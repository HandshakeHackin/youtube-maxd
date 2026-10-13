// ProsperoTube Afterglow - the turn of the thread that plays and vibrates.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The interface's thread never touches a device: it posts voices to the mixer
// and pulses here, and a worker thread takes turns. A turn opens the audio
// port when the first voice is waiting, writes a grain while something sounds
// (and nothing while nothing does), starts and ends vibration pulses, and lets
// go of both devices when the app parks or closes. Every failure ends in
// silence: a port that does not open or keeps refusing grains is given up for
// this run, and so is a pad that refuses.
//
// The devices are behind two small interfaces so that the same turns run on
// the console (app/sound_app.cpp) and, with a clock of their own, in the test.

#pragma once

#include "audio/mixer.hpp"

#include <atomic>
#include <cstdint>

namespace afterglow::sound
{

namespace audio = hui::audio;

// Called on the worker's thread only; every call may block.
class AudioDevice
{
  public:
    virtual ~AudioDevice() = default;
    virtual bool open() = 0;
    // One grain of Worker::kGrain stereo frames; returns when the device has
    // room for the next. False: the device refused it.
    virtual bool write(const std::int16_t *grain) = 0;
    // What is queued plays out, then the port is given back.
    virtual void close() = 0;
};

class PadDevice
{
  public:
    virtual ~PadDevice() = default;
    // The motors at a strength of 0..1; 0 stops them. False: the pad refused.
    virtual bool vibrate(float strength) = 0;
};

class Worker
{
  public:
    static constexpr int kGrain = 256;      // frames per write: 5.3 ms, the grain of the app's own effects port
    static constexpr int kWriteErrors = 8;  // refusals in a row before the port is given up
    static constexpr int kPadErrors = 3;
    static constexpr int kIdle = 4000;      // microseconds between turns while nothing sounds

    enum class Audio : std::uint8_t
    {
        closed, // not needed yet, or let go
        open,
        failed, // for the rest of this run
    };

    Worker(audio::Mixer &mixer, AudioDevice &audio, PadDevice &pad);

    // ---- the interface's thread: nothing here blocks ----
    void wake();                              // a voice was posted to the mixer
    void pulse(float strength, float seconds);
    void park(float seconds);                 // end what sounds within seconds, then let go of the devices
    void quit();                              // let go now
    Audio audio() const
    {
        return audio_.load(std::memory_order_relaxed);
    }
    bool pad_failed() const
    {
        return pad_failed_.load(std::memory_order_relaxed);
    }
    bool finished() const
    {
        return finished_.load(std::memory_order_relaxed);
    }
    std::uint64_t grains() const
    {
        return grains_.load(std::memory_order_relaxed);
    }
    std::uint64_t write_errors() const
    {
        return write_errors_.load(std::memory_order_relaxed);
    }

    // ---- the worker's thread ----
    // One turn at now (a monotonic clock, in microseconds). Returns the
    // microseconds to wait before the next turn: 0 when a grain was written
    // (the device paced the turn), -1 when the worker has let go of its
    // devices and is done.
    int turn(std::int64_t now);

  private:
    void stop_pad();
    void release();

    audio::Mixer &mixer_;
    AudioDevice &device_;
    PadDevice &pad_;

    std::atomic<std::uint32_t> posted_{0};
    std::atomic<std::uint64_t> pulse_{0}; // a request: sequence, strength, milliseconds
    std::atomic<int> park_ms_{0};
    std::atomic<bool> quit_{false};
    std::atomic<Audio> audio_{Audio::closed};
    std::atomic<bool> pad_failed_{false};
    std::atomic<bool> finished_{false};
    std::atomic<std::uint64_t> grains_{0};
    std::atomic<std::uint64_t> write_errors_{0};
    std::uint32_t sequence_ = 0; // the interface's thread

    // The worker's thread.
    std::uint32_t seen_ = 0;
    int tail_ = 0;           // silent grains still owed after the last voice
    int refused_ = 0;        // grains refused in a row
    int pad_errors_ = 0;
    bool rumbling_ = false;
    std::int64_t rumble_until_ = 0;
    bool parking_ = false;
    std::int64_t park_until_ = 0;
    alignas(64) std::int16_t grain_[kGrain * 2] = {};
};

} // namespace afterglow::sound
