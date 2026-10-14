// ProsperoTube Afterglow - interface sound and vibration, tested on the PC.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The module of native/afterglow/sound against the pinned UI kit's own sound
// sets, with a Recorder for an Output and counted devices behind the Worker:
// which recording serves which cue, the rules of docs/afterglow/sound-design.md
// (one sound per action, held directions, rare moments, beside a playing
// video, quiet hours, off), vibration, and what happens when a device fails.
// Nothing is played: scripts/test_afterglow_sound.sh builds and runs it.
//
// usage: sound_test <repo dir>            runs the checks
//        sound_test --takes              the recordings the vocabulary names, one per line

#include "afterglow/sound/recorder.hpp"
#include "afterglow/sound/worker.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <new>
#include <set>
#include <string>
#include <vector>

#ifdef AFTERGLOW_COUNT_ALLOCATIONS
namespace
{
unsigned long g_allocations = 0;
}
void *operator new(std::size_t size)
{
    ++g_allocations;
    if (void *memory = std::malloc(size != 0 ? size : 1))
        return memory;
    throw std::bad_alloc();
}
void *operator new[](std::size_t size)
{
    return operator new(size);
}
void operator delete(void *memory) noexcept
{
    std::free(memory);
}
void operator delete[](void *memory) noexcept
{
    std::free(memory);
}
void operator delete(void *memory, std::size_t) noexcept
{
    std::free(memory);
}
void operator delete[](void *memory, std::size_t) noexcept
{
    std::free(memory);
}
#endif

namespace
{

using namespace afterglow::sound;
using audio::Cue;

int g_checks = 0;
int g_failures = 0;
const char *g_test = "";

#define CHECK(condition)                                                                    \
    do                                                                                      \
    {                                                                                       \
        ++g_checks;                                                                         \
        if (!(condition))                                                                   \
        {                                                                                   \
            ++g_failures;                                                                   \
            std::fprintf(stderr, "FAIL %s: %s (line %d)\n", g_test, #condition, __LINE__); \
        }                                                                                   \
    } while (0)

bool near(float a, float b, float tolerance = 0.002f)
{
    return std::fabs(a - b) <= tolerance;
}

bool read_file(const std::string &path, std::string *out)
{
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (file == nullptr)
        return false;
    char buffer[65536];
    std::size_t got = 0;
    out->clear();
    while ((got = std::fread(buffer, 1, sizeof buffer, file)) > 0)
        out->append(buffer, got);
    std::fclose(file);
    return true;
}

// A quarter of a second of a 440 Hz tone as a 48 kHz mono 16-bit WAV: a recording that did not exist before.
std::string made_recording()
{
    const int frames = 12000;
    std::string wav = "RIFF";
    const auto u32 = [&](std::uint32_t v) { wav.append({static_cast<char>(v), static_cast<char>(v >> 8), static_cast<char>(v >> 16), static_cast<char>(v >> 24)}); };
    const auto u16 = [&](std::uint16_t v) { wav.append({static_cast<char>(v), static_cast<char>(v >> 8)}); };
    u32(36 + frames * 2);
    wav += "WAVEfmt ";
    u32(16);
    u16(1);
    u16(1);
    u32(48000);
    u32(96000);
    u16(2);
    u16(16);
    wav += "data";
    u32(frames * 2);
    for (int i = 0; i < frames; ++i)
        u16(static_cast<std::uint16_t>(static_cast<std::int16_t>(8000.0 * std::sin(i * 2.0 * 3.14159265 * 440.0 / 48000.0))));
    return wav;
}

Reader g_assets;    // original procedural ProsperoTube assets
Bank g_bank;        // ... decoded

constexpr float kFrame = 1.0f / 60.0f;

struct Rig
{
    Recorder record;
    Sound sound;
    Tape tape;
    Step at;

    explicit Rig(const Bank &bank = g_bank) : sound(bank, record)
    {
        at.dt = kFrame;
    }
    // One step of the frame with what it collected, then what the port would render in that time.
    void step(const ui::Feedback &feedback)
    {
        sound.step(feedback, at);
        tape.run(sound.mixer(), 800);
    }
    void idle(float seconds)
    {
        const ui::Feedback nothing;
        for (int i = 0; i < static_cast<int>(std::lround(seconds * 60.0f)); ++i)
            step(nothing);
    }
    void cue(Cue cue, float pitch = 1.0f, float pan = 0.0f, float gain = 1.0f)
    {
        ui::Feedback feedback;
        feedback.play(cue, pitch, pan, gain);
        step(feedback);
    }
    std::size_t heard() const
    {
        return record.heard.size();
    }
    const Heard &last() const
    {
        return record.heard.back();
    }
};

void vocabulary()
{
    g_test = "vocabulary";
    std::set<std::string> names;
    for (std::size_t i = 0; i < spec_count(); ++i)
    {
        const CueSpec &spec = spec_at(i);
        CHECK(spec.name != nullptr && spec.name[0] != 0);
        CHECK(names.insert(spec.name).second);
        CHECK(spec.takes[0].file != nullptr);
        CHECK(index_of(spec.cue) == static_cast<int>(i));
        audio::Cue found = Cue::count;
        CHECK(cue_named(spec.name, &found) && found == spec.cue);
    }
    // Every interface cue of the kit, the three feedback cues the design uses, and the moments of a video app.
    for (int cue = 0; cue <= static_cast<int>(Cue::resume); ++cue)
        CHECK(index_of(static_cast<Cue>(cue)) >= 0);
    for (const Cue cue : {Cue::tick, Cue::type, Cue::erase})
        CHECK(index_of(cue) >= 0);
    for (const Cue cue : {Cue::place, Cue::mark, Cue::merge, Cue::complete})
        CHECK(index_of(cue) < 0);
    for (const Cue cue : {kPlayStart, kPause, kSeekCommit, kSkip, kSignedIn, kGoodbye, kCountdown, kCountdownDone})
    {
        CHECK(index_of(cue) >= 0);
        CHECK(static_cast<int>(cue) > static_cast<int>(Cue::count)); // Cue::count silences a kit component
        CHECK(std::strlen(audio::cue_name(cue)) == 0);               // and the kit has no name for it
    }
    CHECK(index_of(Cue::game_over) < 0);
    CHECK(index_of(Cue::count) < 0);
    CHECK(spec_count() <= kMaxSpecs);
}

void bank_of_the_app()
{
    g_test = "original application sound bank";
    Bank bank;
    const Bank::Report report = bank.load(g_assets);
    for (const std::string &error : report.errors)
        std::fprintf(stderr, "  %s\n", error.c_str());
    CHECK(report.errors.empty());
    CHECK(report.served == static_cast<int>(spec_count()));
    CHECK(report.silent == 0);
    CHECK(report.files == 34);
    for (std::size_t i=0;i<spec_count();++i) {
        const auto &entry=bank.entry(static_cast<int>(i));
        CHECK(std::string(entry.take->file).find("prosperotube/")==0);
        CHECK(near(entry.take->pitch,1) && near(entry.take->gain,1));
    }
    CHECK(bank.entry(index_of(Cue::focus)).count==2);
    for(auto cue:{Cue::toggle,Cue::slider,Cue::tick,Cue::type}) CHECK(bank.entry(index_of(cue)).count==3);
    CHECK(bank.bytes()>1500000 && bank.bytes()<3000000);

}

void bank_takes_new_recordings()
{
    g_test = "new recordings by file name";
    // Replacing an asset by file name changes its decoded samples without source edits.
    const std::set<std::string> added = {"prosperotube/type_01.wav", "prosperotube/type_02.wav", "prosperotube/tick_01.wav", "prosperotube/play_start_01.wav",
                                         "prosperotube/pause_01.wav",  "prosperotube/favorite_on_01.wav", "prosperotube/goodbye_01.wav"};
    const std::string recording = made_recording();
    Bank bank;
    const Bank::Report report = bank.load([&](const std::string &file, std::string *bytes)
                                          {
                                              if (added.count(file) == 0)
                                                  return g_assets(file, bytes);
                                              *bytes = recording;
                                              return true;
                                          });
    CHECK(report.errors.empty() && report.silent == 0);
    const auto take = [&](Cue cue) { return std::string(bank.entry(index_of(cue)).take->file); };
    CHECK(take(Cue::type) == "prosperotube/type" && bank.entry(index_of(Cue::type)).count == 3);
    CHECK(take(Cue::tick) == "prosperotube/tick" && bank.entry(index_of(Cue::tick)).count == 3);
    CHECK(take(Cue::launch) == "prosperotube/play_start" && take(kPlayStart) == "prosperotube/play_start");
    CHECK(take(kPause) == "prosperotube/pause" && near(bank.entry(index_of(kPause)).take->pitch, 1.0f));
    CHECK(take(Cue::favorite_on) == "prosperotube/favorite_on" && take(Cue::favorite_off) == "prosperotube/favorite_off");
    CHECK(take(kGoodbye) == "prosperotube/goodbye");
    Rig rig(bank);
    rig.cue(Cue::type);
    CHECK(rig.heard() == 1 && std::string(rig.last().sample) == "prosperotube/type" && near(rig.last().seconds, 0.25f, 0.01f));
}

void missing_recordings()
{
    g_test = "missing recordings";
    Bank none;
    const Bank::Report report = none.load([](const std::string &, std::string *) { return false; });
    CHECK(report.files == 0 && report.served == 0 && report.silent == static_cast<int>(spec_count()) && report.errors.empty());
    Rig silent(none);
    silent.sound.opened();
    for (const Cue cue : {Cue::focus, Cue::select, Cue::launch, kSkip, Cue::notify})
        silent.cue(cue);
    silent.idle(0.2f);
    CHECK(silent.heard() == 0 && silent.sound.stats().missing == 6 && silent.tape.peak() == 0);
    // A file that is there and is not a recording is named, and its cue is silent.
    Bank broken;
    const Bank::Report told = broken.load([&](const std::string &file, std::string *bytes)
                                          {
                                              if (file != "prosperotube/select_01.wav")
                                                  return g_assets(file, bytes);
                                              *bytes = "this is not a RIFF file";
                                              return true;
                                          });
    CHECK(told.errors.size() == 1 && told.errors[0].find("prosperotube/select_01.wav") == 0);
    CHECK(broken.entry(index_of(Cue::select)).count == 0 && broken.entry(index_of(Cue::focus)).count == 2);
    Rig rig(broken);
    rig.cue(Cue::select);
    rig.cue(Cue::focus);
    CHECK(rig.record.names() == "focus" && rig.sound.stats().missing == 1);
    // A reader that is not there at all.
    Bank unread;
    CHECK(unread.load(Reader()).served == 0);
}

void a_cue_is_a_voice()
{
    g_test = "a cue is a voice";
    Rig rig;
    rig.cue(Cue::focus, 1.0f, 0.3f);
    CHECK(rig.heard() == 1);
    CHECK(std::string(rig.last().name) == "focus" && std::string(rig.last().sample) == "prosperotube/focus");
    CHECK(near(rig.last().pan, 0.3f) && near(rig.last().gain, 1.0f));
    CHECK(rig.last().pitch >= 0.97f && rig.last().pitch <= 1.03f); // the kit's detune
    CHECK(near(rig.last().seconds, 0.045f, 0.003f));
    CHECK(rig.sound.voices() == 1 && rig.sound.mixer().active_voices() == 1 && rig.tape.peak() > 0);
    rig.idle(0.1f);
    CHECK(rig.sound.voices() == 0 && rig.sound.mixer().active_voices() == 0);
    // Space and pitch stay inside what the design allows.
    rig.cue(Cue::select, 3.0f, 0.9f);
    CHECK(near(rig.last().pan, 0.55f) && rig.last().pitch <= 1.4f * 1.03f + 0.001f && rig.last().pitch >= 1.4f * 0.97f - 0.001f);
    rig.idle(0.3f);
    rig.cue(Cue::back, 0.1f, -0.9f);
    CHECK(near(rig.last().pan, -0.55f) && rig.last().pitch >= 0.7f * 0.97f - 0.001f && rig.last().pitch <= 0.7f * 1.03f + 0.001f);
    // A cue the interface has no sound for, and a gain of nothing.
    rig.idle(0.3f);
    const std::size_t before = rig.heard();
    rig.cue(Cue::explode);
    rig.cue(Cue::select, 1.0f, 0.0f, 0.0f);
    CHECK(rig.heard() == before && rig.sound.stats().unknown == 1);
    // The interface volume is the kit's curve, on the interface bus.
    CHECK(near(Sound::level(7), 0.49f) && near(Sound::level(10), 1.0f) && near(Sound::level(0), 0.0f) && near(Sound::level(99), 1.0f));
    Rig loud;
    Rig soft;
    Options options;
    options.volume = 10;
    loud.sound.set_options(options);
    options.volume = 3;
    soft.sound.set_options(options);
    for (Rig *each : {&loud, &soft})
    {
        each->cue(Cue::welcome);
        each->idle(0.6f);
    }
    CHECK(loud.tape.peak() > soft.tape.peak() * 8 && soft.tape.peak() > 0);
    // A voice has its level from its first sample: a tick right after the volume was turned down is quiet.
    Rig turned;
    options.volume = 10;
    turned.sound.set_options(options);
    turned.cue(Cue::focus);
    turned.idle(0.2f);
    const int full = turned.tape.peak();
    Rig down;
    down.sound.set_options(options);
    options.volume = 2;
    down.sound.set_options(options);
    down.cue(Cue::focus);
    down.idle(0.2f);
    CHECK(full > 500 && down.tape.peak() > 0 && down.tape.peak() * 15 < full);
}

void twelve_a_second()
{
    g_test = "twelve a second";
    Rig rig;
    ui::Feedback step;
    step.play(Cue::slider);
    for (int frame = 0; frame < 60; ++frame)
        rig.step(step);
    CHECK(rig.heard() == 12 && rig.sound.stats().limited == 48);
    // A tick asked for three times in one step sounds once.
    Rig twice;
    ui::Feedback triple;
    triple.play(Cue::focus);
    triple.play(Cue::focus);
    triple.play(Cue::focus);
    twice.step(triple);
    CHECK(twice.heard() == 1 && twice.sound.stats().merged == 2);
    // Ticks give way when the mixer is busy; a step that asks for a flood is cut.
    Rig flood;
    ui::Feedback many;
    for (int i = 0; i < 40; ++i)
        many.play(Cue::select);
    flood.step(many);
    CHECK(flood.heard() == 1 && flood.sound.stats().merged == 39);
}

void held_direction()
{
    g_test = "held direction";
    // The app's pad: a repeat 220 ms after the press, then every 65 ms, each handed over as a press.
    Rig rig;
    ui::Feedback tick;
    tick.play(Cue::focus, 1.0f, 0.2f);
    const ui::Feedback nothing;
    std::vector<int> heard_at;
    for (int frame = 0; frame < 150; ++frame)
    {
        const bool fires = frame == 0 || (frame >= 13 && (frame - 13) % 4 == 0);
        const std::size_t before = rig.heard();
        rig.step(fires ? tick : nothing);
        if (rig.heard() > before)
            heard_at.push_back(frame);
    }
    CHECK(heard_at.size() >= 8 && heard_at.size() <= 14); // 36 moves were asked for
    for (std::size_t i = 1; i < heard_at.size(); ++i)
        CHECK(heard_at[i] - heard_at[i - 1] >= 5); // never more than 12 a second
    CHECK(near(rig.record.heard[0].gain, 1.0f) && near(rig.record.heard[1].gain, 1.0f) && near(rig.record.heard[2].gain, 1.0f));
    CHECK(near(rig.last().gain, 0.631f)); // the long part of a hold plays 4 dB down ...
    int soft = 0;
    for (const Heard &voice : rig.record.heard)
        soft += near(voice.gain, 0.631f) ? 1 : 0;
    CHECK(soft >= 4 && rig.sound.stats().held >= 4); // ... and every second tick of it is left out
    for (std::size_t i = heard_at.size() - 3; i + 1 < heard_at.size(); ++i)
        CHECK(heard_at[i + 1] - heard_at[i] >= 12); // about four a second by then
    // Let go, press again: a fresh move sounds in full.
    rig.idle(0.4f);
    rig.step(tick);
    CHECK(near(rig.last().gain, 1.0f));

    // The same when the input says it is a repeat, however slowly it repeats.
    Rig told;
    for (int press = 0; press < 12; ++press)
    {
        told.at.held = press > 0;
        told.step(tick);
        told.at.held = false;
        told.idle(0.3f);
    }
    CHECK(told.heard() == 9 && told.sound.stats().held == 3);
    CHECK(near(told.record.heard[5].gain, 1.0f) && near(told.record.heard[6].gain, 0.631f) && near(told.record.heard[8].gain, 0.631f));
    // Deliberate presses, one every 350 ms, are never thinned.
    Rig taps;
    for (int press = 0; press < 12; ++press)
    {
        taps.step(tick);
        taps.idle(0.33f);
    }
    CHECK(taps.heard() == 12 && taps.sound.stats().held == 0 && near(taps.last().gain, 1.0f));
}

void held_refusal()
{
    g_test = "the end of a list";
    ui::Feedback refusal;
    refusal.play(Cue::error, 1.0f, 0.0f, 0.6f);
    refusal.rumble(0.25f, 0.05f);
    ui::Feedback move;
    move.play(Cue::focus);
    // A press against the edge answers: a soft error and a short pulse.
    Rig rig;
    rig.step(refusal);
    CHECK(rig.record.names() == "error" && near(rig.last().gain, 0.6f) && rig.record.pulses.size() == 1);
    CHECK(near(rig.record.pulses[0].strength, 0.25f) && near(rig.record.pulses[0].seconds, 0.05f));
    // A held direction that runs into the edge stays silent and still, repeat after repeat.
    rig.idle(1.0f);
    rig.step(move);
    for (int repeat = 0; repeat < 6; ++repeat)
    {
        rig.idle(0.05f);
        rig.step(refusal);
    }
    CHECK(rig.record.names() == "error focus" && rig.record.pulses.size() == 1);
    CHECK(rig.sound.stats().held == 6 && rig.sound.stats().pulses_dropped == 6);
    // ... and answers again once the direction was let go.
    rig.idle(0.5f);
    rig.step(refusal);
    CHECK(rig.record.names() == "error focus error" && rig.record.pulses.size() == 2);
    // The input's own word for it.
    Rig told;
    told.at.held = true;
    told.step(refusal);
    CHECK(told.heard() == 0 && told.record.pulses.empty());
}

void one_sound_per_action()
{
    g_test = "one sound per action";
    const auto heard = [](std::initializer_list<Cue> cues)
    {
        Rig rig;
        ui::Feedback feedback;
        for (const Cue cue : cues)
            feedback.play(cue);
        rig.step(feedback);
        return rig.record.names();
    };
    CHECK(heard({Cue::select, Cue::modal_open}) == "modal_open"); // confirming opens a dialog
    CHECK(heard({Cue::modal_open, Cue::select}) == "modal_open");
    CHECK(heard({Cue::select, Cue::open}) == "open");
    CHECK(heard({Cue::back, Cue::modal_close}) == "modal_close");
    CHECK(heard({Cue::select}) == "select");
    CHECK(heard({Cue::select, Cue::saved}) == "saved");
    CHECK(heard({Cue::select, Cue::launch}) == "launch");
    CHECK(heard({Cue::toggle, Cue::favorite_on}) == "favorite_on");
    CHECK(heard({Cue::open, Cue::tab}) == "open");         // the first of equals
    CHECK(heard({Cue::focus, Cue::open}) == "focus open"); // a tick sounds beside it
    CHECK(heard({Cue::focus, Cue::slider, Cue::select}) == "focus slider select");
}

void toast_after_a_confirmation()
{
    g_test = "toast after a confirmation";
    Rig rig;
    rig.cue(Cue::favorite_on, 1.12f);
    rig.cue(Cue::notify); // the toast that says the same
    CHECK(rig.record.names() == "favorite_on" && rig.sound.stats().merged == 1);
    rig.idle(0.5f);
    rig.cue(Cue::notify);
    CHECK(rig.record.names() == "favorite_on notify");
    // A toast on its own, and two in a burst.
    Rig toasts;
    toasts.cue(Cue::notify, 1.06f);
    toasts.idle(0.1f);
    toasts.cue(Cue::notify, 0.94f);
    CHECK(toasts.heard() == 1 && toasts.sound.stats().limited == 1);
}

void rare_moments()
{
    g_test = "rare moments";
    Rig rig;
    rig.sound.opened();
    rig.sound.opened();
    CHECK(rig.heard() == 0); // it sounds with the first step, when the time of day is known
    rig.idle(0.1f);
    CHECK(rig.record.names() == "welcome" && std::string(rig.last().sample) == "prosperotube/welcome");
    rig.sound.opened(); // the interface came up again in the same run
    rig.cue(Cue::welcome);
    rig.idle(3.0f);
    rig.cue(Cue::welcome);
    CHECK(rig.record.names() == "welcome");
    // One start of playback per video.
    rig.cue(Cue::launch);
    rig.idle(1.0f);
    rig.cue(Cue::launch);
    rig.idle(1.2f);
    rig.cue(Cue::launch);
    CHECK(rig.record.names() == "welcome launch launch" && std::string(rig.last().sample) == "prosperotube/play_start");
    // The interface was away while that video played; time went on, and the next video has its start.
    rig.at.dt = 1800.0f;
    rig.idle(kFrame);
    rig.at.dt = kFrame;
    rig.cue(Cue::launch);
    CHECK(rig.record.names() == "welcome launch launch launch" && rig.sound.voices() == 1);
    rig.record.heard.pop_back();
    rig.cue(kSignedIn);
    rig.idle(3.0f);
    rig.cue(kSignedIn);
    CHECK(rig.record.names() == "welcome launch launch signed_in" && std::string(rig.last().sample) == "prosperotube/signed_in");
    // The parting sound, and nothing after it.
    rig.sound.closing();
    rig.sound.closing();
    rig.cue(Cue::focus);
    ui::Feedback pulse;
    pulse.rumble(0.3f, 0.06f);
    rig.step(pulse);
    CHECK(rig.record.names() == "welcome launch launch signed_in goodbye" && std::string(rig.last().sample) == "prosperotube/goodbye");
    CHECK(rig.record.pulses.empty());
    // Closed from somewhere else (the player, a timer): no parting sound, and nothing after it either.
    Rig away;
    away.sound.closing(false);
    away.cue(Cue::focus);
    CHECK(away.heard() == 0);
}

void beside_the_video()
{
    g_test = "beside the video";
    const auto run = [](Options::Playback mode, Layer layer, std::initializer_list<Cue> cues, std::vector<float> *gains = nullptr)
    {
        Rig rig;
        Options options;
        options.playback = mode;
        rig.sound.set_options(options);
        rig.at.situation.playing = true;
        rig.at.situation.layer = layer;
        for (const Cue cue : cues)
        {
            rig.cue(cue);
            rig.idle(0.4f);
        }
        if (gains != nullptr)
            for (const Heard &voice : rig.record.heard)
                gains->push_back(voice.gain);
        return rig.record.names();
    };
    const std::initializer_list<Cue> all = {Cue::focus, Cue::tick, kSeekCommit, Cue::modal_open, Cue::modal_close, kSkip, kCountdown, kCountdownDone, Cue::notify, kPause};
    std::vector<float> gains;
    // The default: notices only, 6 dB down.
    CHECK(run(Options::Playback::notices, Layer::page, all, &gains) == "skip countdown countdown_done");
    CHECK(gains.size() == 3 && near(gains[0], 0.501f) && near(gains[1], 0.501f) && near(gains[2], 0.501f));
    CHECK(run(Options::Playback::notices, Layer::sheet, {Cue::focus, Cue::select, Cue::modal_close}) == "");
    // Everything, 6 dB down; a toast still says nothing over a video.
    gains.clear();
    CHECK(run(Options::Playback::all, Layer::page, all, &gains) == "focus tick seek_commit modal_open modal_close skip countdown countdown_done pause");
    for (std::size_t i = 0; i < gains.size(); ++i)
        CHECK(near(gains[i], 0.501f));
    CHECK(run(Options::Playback::all, Layer::sheet, {Cue::focus, Cue::toggle}) == "focus toggle");
    // A dialog that needs an answer is heard in full, unless sounds during playback are off.
    gains.clear();
    CHECK(run(Options::Playback::notices, Layer::dialog, {Cue::modal_open, Cue::focus, Cue::select, Cue::notify}, &gains) == "modal_open focus select");
    CHECK(gains.size() == 3 && near(gains[0], 1.0f) && near(gains[1], 1.0f));
    CHECK(run(Options::Playback::off, Layer::dialog, {Cue::modal_open, kSkip, Cue::focus}) == "");
    CHECK(run(Options::Playback::off, Layer::page, all) == "");
    // The next video starting is part of the notice.
    CHECK(run(Options::Playback::notices, Layer::page, {kCountdownDone, kPlayStart}) == "countdown_done play_start");
    // Paused, or browsing: the interface is on its own.
    Rig paused;
    paused.cue(Cue::focus);
    paused.cue(Cue::notify);
    CHECK(paused.record.names() == "focus notify" && near(paused.last().gain, 1.0f));
    // The video's own sound: 3 dB down only under a dialog while it keeps playing. Nothing here applies it.
    Rig duck;
    CHECK(near(duck.sound.video_gain(), 1.0f));
    duck.at.situation.playing = true;
    duck.idle(0.05f);
    CHECK(near(duck.sound.video_gain(), 1.0f));
    duck.at.situation.layer = Layer::sheet;
    duck.idle(0.05f);
    CHECK(near(duck.sound.video_gain(), 1.0f));
    duck.at.situation.layer = Layer::dialog;
    duck.idle(0.05f);
    CHECK(near(duck.sound.video_gain(), 0.708f));
    duck.at.situation.playing = false;
    duck.idle(0.05f);
    CHECK(near(duck.sound.video_gain(), 1.0f));
}

void attract_mode()
{
    g_test = "attract mode";
    Rig rig;
    rig.cue(Cue::focus);
    rig.at.situation.attract = true;
    ui::Feedback asked;
    asked.play(Cue::notify);
    asked.play(Cue::focus);
    asked.rumble(0.3f, 0.06f);
    rig.step(asked);
    rig.idle(2.0f);
    CHECK(rig.record.names() == "focus" && rig.sound.stats().hushed == 2 && rig.record.pulses.empty());
    // A button wakes the page: one sound, whatever the page itself asks for in that step.
    rig.at.situation.attract = false;
    ui::Feedback woke;
    woke.play(Cue::resume);
    rig.step(woke);
    CHECK(rig.record.names() == "focus resume");
    rig.idle(1.0f);
    rig.cue(Cue::focus);
    CHECK(rig.record.names() == "focus resume focus");
}

void quiet_hours()
{
    g_test = "quiet hours";
    Options options;
    options.quiet_from = 22 * 60;
    options.quiet_until = 7 * 60;
    ui::Feedback refusal;
    refusal.play(Cue::error, 1.0f, 0.0f, 0.6f);
    refusal.rumble(0.25f, 0.05f);
    const auto at = [&](int minute, const Options &with)
    {
        Rig rig;
        rig.sound.set_options(with);
        rig.at.minute = minute;
        rig.sound.opened();
        rig.idle(0.1f);
        rig.cue(Cue::launch);
        rig.idle(0.4f);
        rig.cue(kSignedIn);
        rig.idle(0.4f);
        rig.cue(Cue::focus);
        rig.idle(0.4f);
        rig.step(refusal);
        // ... with the pulses felt and the gain of the focus tick in thousandths.
        return rig.record.names() + " | " + std::to_string(rig.record.pulses.size()) + " pulses, focus " +
               std::to_string(std::lround(rig.record.heard[3].gain * 1000.0f));
    };
    // Inside: 9 dB down, no big moments, no vibration.
    CHECK(at(23 * 60 + 30, options) == "select select select focus error | 0 pulses, focus 355");
    CHECK(at(6 * 60 + 59, options) == "select select select focus error | 0 pulses, focus 355");
    CHECK(at(7 * 60, options) == "welcome launch signed_in focus error | 1 pulses, focus 1000");
    CHECK(at(12 * 60, options) == "welcome launch signed_in focus error | 1 pulses, focus 1000");
    CHECK(at(-1, options) == "welcome launch signed_in focus error | 1 pulses, focus 1000"); // the time is not known
    Options day = options;
    day.quiet_from = 13 * 60;
    day.quiet_until = 15 * 60;
    CHECK(at(14 * 60, day) == "select select select focus error | 0 pulses, focus 355");
    CHECK(at(15 * 60, day) == "welcome launch signed_in focus error | 1 pulses, focus 1000");
    // The calm set of somebody who reduced motion and turned the interface down: the same moments,
    // no vibration, at the volume that was chosen.
    Options calm;
    calm.reduced_motion = true;
    calm.volume = 3;
    CHECK(at(12 * 60, calm) == "select select select focus error | 0 pulses, focus 1000");
    calm.volume = 4;
    CHECK(at(12 * 60, calm) == "welcome launch signed_in focus error | 1 pulses, focus 1000");
}

void off_means_off()
{
    g_test = "off means off";
    for (int way = 0; way < 2; ++way)
    {
        Rig rig;
        Options options;
        options.sounds = way != 0;
        options.volume = way == 0 ? 7 : 0;
        rig.sound.set_options(options);
        CHECK(!rig.sound.audible());
        rig.sound.opened();
        rig.idle(0.1f);
        ui::Feedback asked;
        asked.play(Cue::error, 1.0f, 0.0f, 0.6f);
        asked.rumble(0.25f, 0.05f);
        rig.step(asked);
        rig.cue(Cue::launch);
        rig.idle(0.5f);
        CHECK(rig.heard() == 0 && rig.sound.stats().off == 3 && rig.tape.peak() == 0);
        CHECK(rig.record.pulses.size() == 1); // vibration has its own switch
    }
    Rig still;
    Options options;
    options.vibration = false;
    still.sound.set_options(options);
    ui::Feedback asked;
    asked.play(Cue::launch);
    asked.rumble(0.6f, 0.16f);
    still.step(asked);
    CHECK(still.record.names() == "launch" && still.record.pulses.empty() && still.sound.audible());
}

void a_failed_device()
{
    g_test = "a failed device";
    Rig rig;
    rig.cue(Cue::focus);
    rig.record.broken = true; // the port stopped taking grains
    CHECK(!rig.sound.audible());
    ui::Feedback asked;
    asked.play(Cue::select);
    asked.play(Cue::focus);
    for (int frame = 0; frame < 2000; ++frame)
        rig.sound.step(asked, rig.at); // nothing renders the mixer any more
    CHECK(rig.heard() == 1 && rig.sound.mixer().dropped_commands() == 0 && rig.sound.stats().dropped == 0);
}

void never_too_loud()
{
    g_test = "never too loud";
    Rig rig;
    Options options;
    options.volume = 10;
    rig.sound.set_options(options);
    rig.sound.opened();
    // Everything long at once, each asked for at twice its level.
    for (const Cue cue : {Cue::launch, Cue::notify, Cue::saved, Cue::resume, Cue::open, Cue::favorite_on, kSkip, kCountdownDone})
    {
        rig.cue(cue, 1.0f, 0.0f, 4.0f);
        CHECK(rig.last().gain <= 2.0f);
    }
    rig.idle(1.5f);
    CHECK(rig.heard() == 9);
    CHECK(rig.tape.peak() > 20000);                                                      // it was loud enough to need the limiter
    CHECK(rig.tape.peak() <= static_cast<int>(audio::Mixer::kCeiling * 32767.0f) + 1); // and never passed -1 dBFS
}

void vibration()
{
    g_test = "vibration";
    const auto pulse = [](float strength, float seconds)
    {
        ui::Feedback feedback;
        feedback.rumble(strength, seconds);
        return feedback;
    };
    Rig rig;
    rig.step(pulse(0.25f, 0.05f)); // the edge of a list
    rig.idle(0.3f);
    rig.step(pulse(0.6f, 0.16f)); // playback starts
    rig.idle(0.3f);
    rig.step(pulse(1.0f, 2.0f)); // nothing the design asks for: held to what it allows
    rig.idle(0.3f);
    rig.step(pulse(0.18f, 0.001f));
    CHECK(rig.record.pulses.size() == 4);
    CHECK(near(rig.record.pulses[0].strength, 0.25f) && near(rig.record.pulses[0].seconds, 0.05f));
    CHECK(near(rig.record.pulses[1].strength, 0.6f) && near(rig.record.pulses[1].seconds, 0.16f));
    CHECK(near(rig.record.pulses[2].strength, 0.6f) && near(rig.record.pulses[2].seconds, 0.2f));
    CHECK(near(rig.record.pulses[3].strength, 0.18f) && near(rig.record.pulses[3].seconds, 0.02f));
    // Detents while scrubbing fast never blur into a buzz: one per 120 ms.
    Rig scrub;
    for (int frame = 0; frame < 60; ++frame)
        scrub.step(pulse(0.18f, 0.03f));
    CHECK(scrub.record.pulses.size() == 8 && scrub.sound.stats().pulses_dropped == 52);
    CHECK(rig.heard() == 0); // a pulse is not a sound
}

#ifdef AFTERGLOW_COUNT_ALLOCATIONS
void nothing_is_allocated()
{
    g_test = "nothing is allocated per cue";
    struct Counted final : Output
    {
        unsigned voices = 0;
        unsigned pulses = 0;
        void voice(const Heard &) override
        {
            ++voices;
        }
        void pulse(float, float) override
        {
            ++pulses;
        }
    } counted;
    Sound sound(g_bank, counted);
    ui::Feedback steps[4];
    steps[0].play(Cue::focus, 1.0f, 0.2f);
    steps[1].play(Cue::select);
    steps[1].play(Cue::open);
    steps[2].play(Cue::favorite_on, 1.12f);
    steps[2].rumble(0.3f, 0.06f);
    steps[3].play(Cue::launch);
    steps[3].play(Cue::notify);
    std::int16_t grain[1600];
    Step at;
    at.dt = kFrame;
    sound.opened();
    const unsigned long before = g_allocations;
    for (int frame = 0; frame < 3600; ++frame)
    {
        sound.step(steps[frame % 11 < 4 ? frame % 11 : 0], at);
        sound.mixer().render(grain, 800);
    }
    CHECK(g_allocations == before);
    CHECK(counted.voices > 100 && counted.pulses > 10);
}
#endif

// ---- the worker: counted devices and a clock of the test's own -------------------------

struct CountedPort final : AudioDevice
{
    int opens = 0;
    int writes = 0;
    int closes = 0;
    bool refuse_open = false;
    int refuse_writes = 0; // the next writes that fail
    int loudest = 0;
    bool is_open = false;
    bool open() override
    {
        ++opens;
        is_open = !refuse_open;
        return is_open;
    }
    bool write(const std::int16_t *grain) override
    {
        if (refuse_writes > 0)
        {
            --refuse_writes;
            return false;
        }
        ++writes;
        for (int i = 0; i < Worker::kGrain * 2; ++i)
            loudest = std::max(loudest, std::abs(static_cast<int>(grain[i])));
        return true;
    }
    void close() override
    {
        ++closes;
        is_open = false;
    }
};

struct CountedPad final : PadDevice
{
    std::vector<float> calls;
    int refuse = 0; // the next calls that fail
    bool vibrate(float strength) override
    {
        if (refuse > 0)
        {
            --refuse;
            return false;
        }
        calls.push_back(strength);
        return true;
    }
};

// A Sound whose Output is a worker, as on the console, with the thread's loop run here.
struct Console final : Output
{
    CountedPort port;
    CountedPad pad;
    Sound sound;
    Worker worker;
    std::int64_t now = 1000000;
    Step at;

    Console() : sound(g_bank, *this), worker(sound.mixer(), port, pad)
    {
        at.dt = kFrame;
    }
    bool audio() const override
    {
        return !worker.finished() && worker.audio() != Worker::Audio::failed;
    }
    void voice(const Heard &) override
    {
        worker.wake();
    }
    void pulse(float strength, float seconds) override
    {
        worker.pulse(strength, seconds);
    }
    void cue(Cue cue)
    {
        ui::Feedback feedback;
        feedback.play(cue);
        sound.step(feedback, at);
    }
    // The thread's loop for seconds of its clock: a written grain takes its 5.3 ms, a wait is slept.
    int run(float seconds)
    {
        const std::int64_t until = now + static_cast<std::int64_t>(seconds * 1.0e6f);
        int last = 0;
        while (now < until)
        {
            last = worker.turn(now);
            if (last < 0)
                break;
            now += last > 0 ? last : 5333;
        }
        return last;
    }
};

void worker_port()
{
    g_test = "the port";
    Console quiet;
    CHECK(quiet.run(1.0f) == Worker::kIdle && quiet.port.opens == 0 && quiet.port.writes == 0); // a silent interface asks for no port
    Console console;
    console.cue(Cue::focus);
    console.run(0.02f);
    CHECK(console.port.opens == 1 && console.worker.audio() == Worker::Audio::open && console.port.writes >= 3);
    console.run(1.0f);
    // 45 ms of focus plus a final grain, allowing the existing 3% detune.
    CHECK(console.port.writes >= 8 && console.port.writes <= 11 && console.port.loudest > 0);
    const int written = console.port.writes;
    console.run(5.0f);
    CHECK(console.port.writes == written && console.port.opens == 1 && console.port.closes == 0);
    console.cue(Cue::select);
    console.run(0.5f);
    CHECK(console.port.writes >= written + 25 && console.port.writes <= written + 30 && console.port.opens == 1);
    CHECK(console.worker.grains() == static_cast<std::uint64_t>(console.port.writes) && console.worker.write_errors() == 0);

    g_test = "a port that does not open";
    Console shut;
    shut.port.refuse_open = true;
    shut.cue(Cue::focus);
    shut.run(0.1f);
    CHECK(shut.worker.audio() == Worker::Audio::failed && shut.port.opens == 1 && shut.port.writes == 0);
    CHECK(!shut.sound.audible());
    for (int i = 0; i < 600; ++i)
    {
        shut.cue(Cue::select);
        shut.run(0.016f);
    }
    CHECK(shut.port.opens == 1 && shut.sound.stats().heard == 1 && shut.sound.mixer().dropped_commands() == 0); // silent, and carrying on
    ui::Feedback felt;
    felt.rumble(0.3f, 0.06f);
    shut.sound.step(felt, shut.at);
    shut.run(0.2f);
    CHECK(shut.pad.calls.size() == 2); // vibration does not depend on the port

    g_test = "a port that refuses grains";
    Console hiccup;
    hiccup.cue(Cue::welcome);
    hiccup.run(0.05f);
    hiccup.port.refuse_writes = 3;
    hiccup.run(0.3f);
    CHECK(hiccup.worker.audio() == Worker::Audio::open && hiccup.worker.write_errors() == 3 && hiccup.port.closes == 0);
    hiccup.port.refuse_writes = 1000;
    hiccup.run(0.3f);
    CHECK(hiccup.worker.audio() == Worker::Audio::failed && hiccup.port.closes == 1 && hiccup.worker.write_errors() == 3 + Worker::kWriteErrors);
    const int tried = hiccup.port.opens;
    hiccup.cue(Cue::select);
    hiccup.run(1.0f);
    CHECK(hiccup.port.opens == tried && !hiccup.sound.audible());
}

void worker_pad()
{
    g_test = "the pad";
    Console console;
    ui::Feedback felt;
    felt.rumble(0.3f, 0.06f);
    console.sound.step(felt, console.at);
    const std::int64_t started = console.now;
    console.run(0.001f);
    CHECK(console.pad.calls.size() == 1 && near(console.pad.calls[0], 0.3f, 0.001f));
    console.run(0.05f);
    CHECK(console.pad.calls.size() == 1); // still running
    console.run(0.03f);
    CHECK(console.pad.calls.size() == 2 && console.pad.calls[1] == 0.0f); // stopped, once
    CHECK(console.now - started < 90000);
    console.run(1.0f);
    CHECK(console.pad.calls.size() == 2 && console.port.opens == 0); // a pulse opens no port

    g_test = "a pad that refuses";
    Console stuck;
    stuck.sound.step(felt, stuck.at);
    stuck.run(0.001f);
    stuck.pad.refuse = 1; // the stop is refused once: it is asked again
    stuck.run(0.2f);
    CHECK(stuck.pad.calls.size() == 2 && stuck.pad.calls[1] == 0.0f && !stuck.worker.pad_failed());
    Console dead;
    dead.pad.refuse = 1000;
    for (int i = 0; i < 6; ++i)
    {
        dead.at.dt = 0.2f;
        dead.sound.step(felt, dead.at);
        dead.run(0.2f);
    }
    CHECK(dead.worker.pad_failed() && dead.pad.calls.empty() && dead.pad.refuse == 1000 - Worker::kPadErrors);
}

void worker_parks()
{
    g_test = "parking";
    // The parting sound plays out, then the port is given back without another call.
    Console console;
    console.cue(Cue::focus);
    console.run(0.5f);
    console.sound.closing();
    console.worker.park(1.0f);
    const std::int64_t parked = console.now;
    CHECK(console.run(3.0f) == -1 && console.worker.finished());
    CHECK(console.port.closes == 1 && !console.port.is_open && console.worker.audio() == Worker::Audio::closed);
    CHECK(console.now - parked > 150000 && console.now - parked < 400000); // "back" is 200 ms long
    console.cue(Cue::select);
    CHECK(console.worker.turn(console.now) == -1 && console.port.opens == 1 && !console.sound.audible());

    // A long sound is cut at the budget; a pulse stops at once.
    Console late;
    late.cue(Cue::welcome);
    ui::Feedback felt;
    felt.rumble(0.6f, 0.16f);
    late.sound.step(felt, late.at);
    late.run(0.05f);
    CHECK(late.pad.calls.size() == 1);
    late.worker.park(0.2f);
    const std::int64_t from = late.now;
    CHECK(late.run(3.0f) == -1 && late.port.closes == 1);
    CHECK(late.pad.calls.size() == 2 && late.pad.calls[1] == 0.0f);
    CHECK(late.now - from >= 195000 && late.now - from < 230000);

    // Parking with a voice waiting and no port: none is opened for it.
    Console never;
    never.cue(Cue::focus);
    never.worker.park(1.0f);
    CHECK(never.run(1.0f) == -1 && never.port.opens == 0 && never.worker.finished());

    g_test = "exit";
    Console exits;
    exits.cue(Cue::welcome);
    exits.sound.step(felt, exits.at);
    exits.run(0.05f);
    exits.worker.quit();
    CHECK(exits.worker.turn(exits.now) == -1 && exits.port.closes == 1 && exits.pad.calls.back() == 0.0f && exits.worker.finished());
    Console unused;
    unused.worker.quit();
    CHECK(unused.worker.turn(unused.now) == -1 && unused.port.opens == 0 && unused.port.closes == 0 && unused.pad.calls.empty());
}

} // namespace

int main(int argc, char **argv)
{
    if (argc >= 2 && std::strcmp(argv[1], "--takes") == 0)
    {
        std::set<std::string> takes;
        for (std::size_t i = 0; i < spec_count(); ++i)
            for (const Take &take : spec_at(i).takes)
                if (take.file != nullptr)
                    takes.insert(take.file);
        for (const std::string &take : takes)
            std::printf("%s\n", take.c_str());
        return 0;
    }
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: sound_test <repo dir> | --takes\n");
        return 2;
    }
    const std::string sets = std::string(argv[1]) + "/native/afterglow/sound/recordings/";
    g_assets = [sets](const std::string &file, std::string *bytes) { return read_file(sets + file, bytes); };
    const Bank::Report report = g_bank.load(g_assets);
    if (report.files == 0)
    {
        std::fprintf(stderr, "no recordings under %s\n", sets.c_str());
        return 1;
    }

    vocabulary();
    bank_of_the_app();
    bank_takes_new_recordings();
    missing_recordings();
    a_cue_is_a_voice();
    twelve_a_second();
    held_direction();
    held_refusal();
    one_sound_per_action();
    toast_after_a_confirmation();
    rare_moments();
    beside_the_video();
    attract_mode();
    quiet_hours();
    off_means_off();
    a_failed_device();
    never_too_loud();
    vibration();
#ifdef AFTERGLOW_COUNT_ALLOCATIONS
    nothing_is_allocated();
#endif
    worker_port();
    worker_pad();
    worker_parks();

    std::printf("%s: afterglow sound: %d checks, %d failed\n", g_failures == 0 ? "PASS" : "FAIL", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
