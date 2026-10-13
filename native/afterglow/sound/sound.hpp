// ProsperoTube Afterglow - interface sound and controller vibration.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The frame collects what its screens ask for in a ui::Feedback: cues and one
// rumble per step. Whatever runs the frame hands that to a Sound after every
// Frame::update(). The Sound decides what is heard and felt by the rules of
// docs/afterglow/sound-design.md and section 9 of design-notes.md, and posts
// it: voices to the kit's mixer, pulses to an Output. On the console the
// Output is a worker thread with its own AudioOut port (app/sound_app.cpp); in
// tests it is a Recorder.
//
// Nothing here allocates, blocks or reads a file once the bank is loaded:
// step() runs on the thread that runs the frame.

#pragma once

#include "afterglow/sound/bank.hpp"

#include "audio/mixer.hpp"
#include "ui/feedback.hpp"

#include <array>
#include <cstdint>

namespace afterglow::sound
{

namespace ui = hui::ui;

// Settings > Interface > Sounds (sound-design.md, section 6).
struct Options
{
    enum class Playback : std::uint8_t
    {
        all,     // every cue, 6 dB down
        notices, // skip, countdown and dialogs only
        off,
    };
    bool sounds = true;      // Interface sounds
    int volume = 7;          // Interface volume, 0 to 10 on the kit's curve
    Playback playback = Playback::notices; // Sounds during playback
    int quiet_from = -1;     // Quiet hours: minutes after midnight; -1 for none
    int quiet_until = -1;
    bool vibration = true;   // Vibration
    bool reduced_motion = false; // with a volume of 3 or lower: the calm set
};

// What has the pad while a video plays.
enum class Layer : std::uint8_t
{
    page,   // the page, or the player's controls
    sheet,  // a sheet over the video: captions, options, up next, chat
    dialog, // a question that needs an answer
};

struct Situation
{
    bool playing = false; // a video's own sound is running (not paused)
    Layer layer = Layer::page;
    bool attract = false; // attract mode is on
};

struct Step
{
    float dt = 1.0f / 60.0f;
    bool held = false; // this step's direction is an auto-repeat (InputFrame::nav_repeat)
    int minute = -1;   // the time of day in minutes after midnight, for quiet hours; -1 when unknown
    Situation situation;
};

// One voice as it was started.
struct Heard
{
    audio::Cue cue = audio::Cue::count;
    const char *name = "";   // the cue's name in cues.hpp
    const char *sample = ""; // the take that served it: "glass/focus"
    float pitch = 1.0f;
    float pan = 0.0f;
    float gain = 1.0f;       // before the interface volume
    float seconds = 0.0f;    // how long it sounds
};

// Where the decisions go: the console's worker, or a recorder.
class Output
{
  public:
    virtual ~Output() = default;
    // False once the audio device has failed: no voice is posted any more.
    virtual bool audio() const
    {
        return true;
    }
    // A voice was posted to the mixer. Must not block.
    virtual void voice(const Heard &heard) = 0;
    // A vibration pulse: strength 0..1 for seconds. Must not block.
    virtual void pulse(float strength, float seconds) = 0;
};

struct Stats
{
    unsigned heard = 0;          // voices started
    unsigned merged = 0;         // gave way to another cue of the same action
    unsigned limited = 0;        // too soon after the same cue, or too many voices at once
    unsigned held = 0;           // thinned or silenced on a held direction
    unsigned hushed = 0;         // silent for the situation: a video plays, attract mode
    unsigned off = 0;            // interface sounds are off, or the device failed
    unsigned missing = 0;        // no recording serves the cue
    unsigned unknown = 0;        // not a cue of the interface
    unsigned dropped = 0;        // the mixer took no more
    unsigned pulses = 0;         // vibration pulses started
    unsigned pulses_dropped = 0; // ... and not started
};

class Sound
{
  public:
    Sound(const Bank &bank, Output &output);
    Sound(const Sound &) = delete;
    Sound &operator=(const Sound &) = delete;

    void set_options(const Options &options);
    const Options &options() const
    {
        return options_;
    }

    // The app opened with its opening moment: the next step plays the welcome,
    // once per start whatever is asked later.
    void opened();
    // After every Frame::update(): what that step asked for.
    void step(const ui::Feedback &feedback, const Step &step);
    // The app closes: the parting sound, then nothing more.
    void closing();

    // True while a cue can be heard: sounds are on and the device has not failed.
    // (The countdown's last second vibrates only when this is false.)
    bool audible() const;
    // What the video's own sound is multiplied by now: 1, or 3 dB down while a
    // dialog waits for an answer over a video that keeps playing. For whoever
    // owns the player's volume; nothing here touches it.
    float video_gain() const;

    // The mixer the voices are posted to; the Output's device renders it.
    audio::Mixer &mixer()
    {
        return mixer_;
    }
    const Stats &stats() const
    {
        return stats_;
    }
    // Voices that have not ended yet, by their lengths.
    int voices() const;
    // The interface volume as the bus gain it sets.
    static float level(int volume);

  private:
    int resolve(audio::Cue cue) const;
    void hear(int index, const audio::CueEvent &event, bool held, bool *held_refusal);
    void pulse(const ui::Feedback &feedback, bool held_refusal);

    const Bank &bank_;
    Output &output_;
    audio::Mixer mixer_;
    Options options_;
    Stats stats_;

    double clock_ = 0.0;
    Situation situation_;
    bool quiet_ = false;  // inside quiet hours
    bool calm_ = false;   // the calm set: quiet hours, or reduced motion with a low volume
    bool attract_ = false;
    bool welcome_due_ = false;
    bool welcomed_ = false;
    bool closed_ = false;

    std::array<double, kMaxSpecs> last_{};       // when each cue was last heard
    std::array<std::uint8_t, kMaxSpecs> turn_{}; // the next variation of each cue
    std::array<double, audio::Mixer::kVoices> ends_{};
    std::uint32_t jitter_ = 0x2545f491u;
    double last_nav_ = -1.0e9;       // the last tick of a direction, heard or not
    int run_ = 0;                    // repeats of the held direction so far
    bool thinned_ = false;           // the last tick of a long hold was left out
    double last_confirmed_ = -1.0e9; // the last chime of a button that confirmed itself
    double last_pulse_ = -1.0e9;
};

} // namespace afterglow::sound
