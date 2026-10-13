// ProsperoTube Afterglow - interface sound and vibration inside the app.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The console's devices behind sound/worker.hpp, the thread that takes its
// turns, and the calls the mount and the app make. Console only.
//
// The port is the interface's own: type MAIN, stereo S16 at 48 kHz in grains
// of 256 frames, exactly as the app opens the port of its own effects. The
// player's port is never touched. The port is opened by the worker's thread
// when the first voice waits, written only while something sounds, and given
// back when the app parks or exits. If it cannot be opened, or refuses grains,
// the interface stays silent for this run and carries on.
//
// Vibration goes to the pad the app opened, in the two-motor mode the UI kit's
// own pad code asks for. EVO's notes (docs/ui/ui-handoff.md) say no vibration
// call had any effect on the firmware it was tried on: whether a pulse is felt
// at all is the first thing to check with a console in hand.

#include "afterglow/sound/app/sound_app.hpp"

#include "afterglow/frame.hpp"
#include "afterglow/sound/app/tube_afterglow_sound.h"
#include "afterglow/sound/sound.hpp"
#include "afterglow/sound/worker.hpp"

extern "C"
{
#include "evo_boot_log.h"
#include "evo_data_path.h"

    int sceAudioOutInit(void);
    int sceAudioOutOpen(int user, int type, int index, unsigned int grain, unsigned int rate, unsigned int format);
    int sceAudioOutOutput(int handle, const void *samples);
    int sceAudioOutClose(int handle);
}

#include <atomic>
#include <cstdio>
#include <ctime>
#include <pthread.h>
#include <unistd.h>

extern "C"
{
    // scePadSetVibration's argument, as the kit's pad code types it.
    struct TubeAfterglowPadVibration
    {
        unsigned char large_motor;
        unsigned char small_motor;
    };
    int scePadSetVibrationMode(int handle, int mode);
    int scePadSetVibration(int handle, const struct TubeAfterglowPadVibration *vibration);
}

namespace afterglow::sound
{

namespace
{

constexpr int kSystemUser = 0xff;
constexpr int kPortMain = 0;
constexpr unsigned int kFormatS16Stereo = 1;
constexpr int kVibrationCompatible = 2; // the two motors, not the voice-coil haptics
constexpr float kParkSeconds = 1.0f;
constexpr std::int64_t kRefreshEvery = 2000000; // microseconds between looks at the session's files

std::atomic<int> g_pad{-1};
TubeAfterglowSound g_preferences = {1, 7, 1, -1, -1, 1, 0};

std::int64_t now_us()
{
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<std::int64_t>(now.tv_sec) * 1000000 + now.tv_nsec / 1000;
}

bool file_exists(const char *name)
{
    std::FILE *file = std::fopen(evo_data_path(name), "rb");
    if (file == nullptr)
        return false;
    std::fclose(file);
    return true;
}

class Port final : public AudioDevice
{
  public:
    bool open() override
    {
        sceAudioOutInit(); // the app did this at its start; "already initialised" is the usual answer
        const int handle = sceAudioOutOpen(kSystemUser, kPortMain, 0, Worker::kGrain, audio::kSampleRate, kFormatS16Stereo);
        code_.store(handle, std::memory_order_relaxed);
        if (handle < 1)
            return false;
        handle_ = handle;
        return true;
    }
    bool write(const std::int16_t *grain) override
    {
        const int result = sceAudioOutOutput(handle_, grain);
        if (result < 0)
            code_.store(result, std::memory_order_relaxed);
        return result >= 0;
    }
    void close() override
    {
        if (handle_ < 1)
            return;
        sceAudioOutOutput(handle_, nullptr); // what is queued plays out
        sceAudioOutClose(handle_);
        handle_ = -1;
    }
    // The handle the port opened with, or the last refusal.
    int code() const
    {
        return code_.load(std::memory_order_relaxed);
    }

  private:
    int handle_ = -1;
    std::atomic<int> code_{0};
};

class Pad final : public PadDevice
{
  public:
    bool vibrate(float strength) override
    {
        const int handle = g_pad.load(std::memory_order_relaxed);
        if (handle < 0)
            return true; // no pad: nothing to do, and nothing wrong
        if (handle != mode_set_for_)
        {
            mode_.store(scePadSetVibrationMode(handle, kVibrationCompatible), std::memory_order_relaxed);
            mode_set_for_ = handle;
        }
        // The small motor gives the short tick an interface wants; the large
        // one would only join above the strength the interface is held to.
        const float level = strength < 0.0f ? 0.0f : strength > 1.0f ? 1.0f : strength;
        const TubeAfterglowPadVibration vibration{static_cast<unsigned char>(level > 0.6f ? (level - 0.6f) * 2.5f * 255.0f : 0.0f),
                                                  static_cast<unsigned char>(level * 255.0f)};
        const int result = scePadSetVibration(handle, &vibration);
        if (level > 0.0f)
        {
            result_.store(result, std::memory_order_relaxed);
            pulses_.fetch_add(1, std::memory_order_relaxed);
        }
        return result == 0;
    }
    int mode() const
    {
        return mode_.load(std::memory_order_relaxed);
    }
    int result() const
    {
        return result_.load(std::memory_order_relaxed);
    }
    unsigned pulses() const
    {
        return pulses_.load(std::memory_order_relaxed);
    }

  private:
    int mode_set_for_ = -1;
    std::atomic<int> mode_{0};
    std::atomic<int> result_{0};
    std::atomic<unsigned> pulses_{0};
};

// One per run of the app, made when the interface first comes up and never
// destroyed: the audio thread reads the mixer and the recordings to the end.
struct App final : public Output
{
    Bank bank;
    Port port;
    Pad pad;
    Sound sound{bank, *this};
    Worker worker{sound.mixer(), port, pad};

    pthread_t thread{};
    bool started = false;       // the worker's thread exists
    bool joined = false;
    bool thread_failed = false;
    bool parked = false;        // the app parked or exits: nothing starts any more
    std::int64_t refreshed = 0;
    int minute = -1;
    Worker::Audio logged = Worker::Audio::closed;
    bool logged_pad = false;
    bool logged_pulse = false;

    static void *run(void *self)
    {
        Worker &worker = static_cast<App *>(self)->worker;
        for (;;)
        {
            const int wait = worker.turn(now_us());
            if (wait < 0)
                break;
            if (wait > 0)
                usleep(static_cast<useconds_t>(wait));
        }
        return nullptr;
    }

    // The thread is made by the first voice or pulse, so an interface with
    // sounds and vibration off never has one. Never named: naming a thread
    // hangs on this platform.
    bool start()
    {
        if (started)
            return !worker.finished();
        if (thread_failed || parked)
            return false;
        if (pthread_create(&thread, nullptr, &App::run, this) != 0)
        {
            thread_failed = true;
            evo_boot_log("afterglow: sound: no thread for the port; the interface stays silent");
            return false;
        }
        started = true;
        return true;
    }

    bool audio() const override
    {
        return !thread_failed && !parked && !worker.finished() && worker.audio() != Worker::Audio::failed;
    }
    void voice(const Heard &heard) override
    {
        (void)heard;
        if (start())
            worker.wake();
    }
    void pulse(float strength, float seconds) override
    {
        if (start())
            worker.pulse(strength, seconds);
    }

    // The profile's preferences as they were last handed over, and the files a
    // console session uses to switch without them: afterglow-sound.off and
    // afterglow-vibration.off in the app's data folder.
    void refresh(Platform &platform)
    {
        const std::int64_t now = now_us();
        if (refreshed != 0 && now - refreshed < kRefreshEvery)
            return;
        refreshed = now;
        Options options;
        options.sounds = g_preferences.sounds != 0 && !file_exists("afterglow-sound.off");
        options.volume = g_preferences.volume;
        options.playback = g_preferences.playback == 0   ? Options::Playback::all
                           : g_preferences.playback == 2 ? Options::Playback::off
                                                         : Options::Playback::notices;
        options.quiet_from = g_preferences.quiet_from;
        options.quiet_until = g_preferences.quiet_until;
        options.vibration = g_preferences.vibration != 0 && !file_exists("afterglow-vibration.off");
        options.reduced_motion = g_preferences.reduced_motion != 0;
        sound.set_options(options);
        int hour = 0;
        int minutes = 0;
        platform.wall_clock(&hour, &minutes);
        minute = hour * 60 + minutes;
    }

    // What the worker's thread found, said once in the app's log from this thread.
    void report()
    {
        const Worker::Audio state = worker.audio();
        if (state != logged)
        {
            logged = state;
            if (state == Worker::Audio::open)
                evo_boot_log("afterglow: sound: port open, handle %d", port.code());
            else if (state == Worker::Audio::failed)
                evo_boot_log("afterglow: sound: the port failed (0x%08x); the interface stays silent", static_cast<unsigned>(port.code()));
        }
        if (!logged_pulse && pad.pulses() > 0)
        {
            logged_pulse = true;
            evo_boot_log("afterglow: vibration: mode 0x%08x, first pulse 0x%08x", static_cast<unsigned>(pad.mode()),
                         static_cast<unsigned>(pad.result()));
        }
        if (!logged_pad && worker.pad_failed())
        {
            logged_pad = true;
            evo_boot_log("afterglow: vibration: the pad refused (0x%08x); no more pulses", static_cast<unsigned>(pad.result()));
        }
    }
};

App *g_app = nullptr;

} // namespace

void app_begin(Platform &platform, bool opening)
{
    if (g_app == nullptr)
    {
        g_app = new App();
        const Bank::Report report = g_app->bank.load([&platform](const std::string &file, std::string *bytes)
                                                     { return platform.read_asset("hui/afterglow/sfx/" + file, bytes); });
        evo_boot_log("afterglow: sound: %d recordings, %d cues served, %d silent, %zu KiB", report.files, report.served, report.silent,
                     g_app->bank.bytes() / 1024);
        for (const std::string &error : report.errors)
            evo_boot_log("afterglow: sound: %s", error.c_str());
    }
    if (opening)
        g_app->sound.opened();
}

void app_step(const Frame &frame, Platform &platform, float dt)
{
    if (g_app == nullptr)
        return;
    App &app = *g_app;
    app.refresh(platform);
    Step step;
    step.dt = dt;
    step.minute = app.minute;
    step.situation.attract = frame.attracting();
    app.sound.step(frame.feedback(), step);
    app.report();
}

Sound *app_sound()
{
    return g_app != nullptr ? &g_app->sound : nullptr;
}

} // namespace afterglow::sound

using afterglow::sound::g_app;

extern "C" void tube_afterglow_sound_options(const TubeAfterglowSound *options)
{
    if (options == nullptr)
        return;
    afterglow::sound::g_preferences = *options;
    if (g_app != nullptr)
        g_app->refreshed = 0; // taken at the next step
}

extern "C" void tube_afterglow_sound_pad(int pad_handle)
{
    afterglow::sound::g_pad.store(pad_handle, std::memory_order_relaxed);
}

extern "C" void tube_afterglow_sound_park(void)
{
    if (g_app == nullptr || g_app->parked)
        return;
    if (g_app->started)
    {
        g_app->sound.closing();
        g_app->worker.park(afterglow::sound::kParkSeconds);
    }
    g_app->parked = true;
    evo_boot_log("afterglow: sound: parked");
}

extern "C" void tube_afterglow_sound_close(void)
{
    if (g_app == nullptr)
        return;
    g_app->parked = true;
    g_app->worker.quit();
    if (g_app->started && !g_app->joined)
    {
        pthread_join(g_app->thread, nullptr);
        g_app->joined = true;
    }
    evo_boot_log("afterglow: sound: closed, %llu grains written, %llu refused", static_cast<unsigned long long>(g_app->worker.grains()),
                 static_cast<unsigned long long>(g_app->worker.write_errors()));
}
