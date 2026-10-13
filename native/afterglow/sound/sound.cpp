// ProsperoTube Afterglow - interface sound and controller vibration.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/sound/sound.hpp"

#include "core/settings.hpp"

#include <algorithm>

namespace afterglow::sound
{

namespace
{

using audio::Cue;

// Held navigation (sound-design.md, section 4). The app's pad repeats a held
// direction 220 ms after the press and then every 65 to 28 ms, and hands every
// repeat over as a press: a tick this soon after the last belongs to a hold.
constexpr double kHeldGap = 0.25;
constexpr int kThinFrom = 6;         // from this repeat every second tick is left out ...
constexpr float kThinGain = 0.631f;  // ... and the others play 4 dB down
// Beside the video (section 5) and quiet hours (section 6).
constexpr float kBesideGain = 0.501f; // 6 dB down
constexpr float kQuietGain = 0.355f;  // 9 dB down
constexpr float kVideoDuck = 0.708f;  // 3 dB down
// A toast that follows a button which confirmed itself says nothing (cue 22).
constexpr double kToastAfterConfirm = 0.3;
// Ticks give way when this many voices still sound.
constexpr int kVoiceBudget = 12;
// Cues of one step that are looked at; a step that asks for more is a fault.
constexpr std::size_t kScan = 16;
constexpr float kPan = 0.55f;
constexpr float kPitchLow = 0.7f;
constexpr float kPitchHigh = 1.4f;
// Vibration: short and light, never a buzz (design-notes.md, section 9).
constexpr float kPulseStrength = 0.6f;
constexpr float kPulseShortest = 0.02f;
constexpr float kPulseLongest = 0.2f;
constexpr double kPulseGap = 0.12;
constexpr double kNever = -1.0e9;

bool quiet_hours(const Options &options, int minute)
{
    if (minute < 0 || options.quiet_from < 0 || options.quiet_until < 0 || options.quiet_from == options.quiet_until)
        return false;
    if (options.quiet_from < options.quiet_until)
        return minute >= options.quiet_from && minute < options.quiet_until;
    return minute >= options.quiet_from || minute < options.quiet_until; // across midnight
}

bool confirms(Cue cue)
{
    return cue == Cue::favorite_on || cue == Cue::favorite_off || cue == Cue::saved || cue == kSignedIn;
}

} // namespace

Sound::Sound(const Bank &bank, Output &output) : bank_(bank), output_(output)
{
    last_.fill(kNever);
    ends_.fill(kNever);
}

float Sound::level(int volume)
{
    return hui::Settings::gain(std::clamp(volume, 0, 10));
}

void Sound::set_options(const Options &options)
{
    options_ = options;
    options_.volume = std::clamp(options_.volume, 0, 10);
}

bool Sound::audible() const
{
    return options_.sounds && options_.volume > 0 && output_.audio();
}

float Sound::video_gain() const
{
    return situation_.playing && situation_.layer == Layer::dialog ? kVideoDuck : 1.0f;
}

int Sound::voices() const
{
    int count = 0;
    for (const double end : ends_)
        count += end > clock_ ? 1 : 0;
    return count;
}

void Sound::opened()
{
    if (!welcomed_)
        welcome_due_ = true;
}

void Sound::closing(bool parting)
{
    if (closed_)
        return;
    if (parting)
    {
        audio::CueEvent goodbye;
        goodbye.cue = kGoodbye;
        bool unused = false;
        hear(resolve(goodbye.cue), goodbye, false, &unused);
    }
    closed_ = true;
}

// The cue as the rules see it. The calm set has no big moments: the welcome,
// the start of a video and the sign-in arpeggio become a plain confirmation.
int Sound::resolve(audio::Cue cue) const
{
    if (calm_ && (cue == Cue::welcome || cue == Cue::launch || cue == kPlayStart || cue == kSignedIn))
        return index_of(Cue::select);
    return index_of(cue);
}

void Sound::step(const ui::Feedback &feedback, const Step &step)
{
    clock_ += static_cast<double>(std::max(0.0f, step.dt));
    situation_ = step.situation;
    quiet_ = quiet_hours(options_, step.minute);
    calm_ = quiet_ || (options_.reduced_motion && options_.volume <= 3);
    const bool woke = attract_ && !step.situation.attract;
    attract_ = step.situation.attract;
    if (closed_)
        return;

    bool held_refusal = false;
    if (welcome_due_)
    {
        welcome_due_ = false;
        welcomed_ = true;
        audio::CueEvent welcome;
        welcome.cue = Cue::welcome;
        hear(resolve(welcome.cue), welcome, false, &held_refusal);
    }
    if (woke)
    {
        // A button ended attract mode; the press itself was swallowed by the frame.
        audio::CueEvent back_in;
        back_in.cue = Cue::resume;
        hear(resolve(back_in.cue), back_in, false, &held_refusal);
    }

    // One sound per action: of the step's cues that are not ticks, only the
    // highest in rank is heard (the first of equals). Opening a dialog plays
    // modal_open, not select and modal_open.
    const std::size_t count = std::min(feedback.cues.size(), kScan);
    int chosen = -1;
    Rank chosen_rank = Rank::tick;
    for (std::size_t i = 0; i < count; ++i)
    {
        const int index = resolve(feedback.cues[i].cue);
        if (index < 0)
            continue;
        const Rank rank = spec_at(static_cast<std::size_t>(index)).rank;
        if (rank != Rank::tick && (chosen < 0 || rank > chosen_rank))
        {
            chosen = static_cast<int>(i);
            chosen_rank = rank;
        }
    }
    std::uint64_t ticked = 0; // a tick sounds once however often a step asks for it
    for (std::size_t i = 0; i < count; ++i)
    {
        const audio::CueEvent &event = feedback.cues[i];
        const int index = resolve(event.cue);
        if (index < 0)
        {
            ++stats_.unknown;
            continue;
        }
        if (spec_at(static_cast<std::size_t>(index)).rank == Rank::tick)
        {
            const std::uint64_t bit = std::uint64_t{1} << index;
            if ((ticked & bit) != 0)
            {
                ++stats_.merged;
                continue;
            }
            ticked |= bit;
        }
        else if (static_cast<int>(i) != chosen)
        {
            ++stats_.merged;
            continue;
        }
        hear(index, event, step.held, &held_refusal);
    }
    stats_.merged += static_cast<unsigned>(feedback.cues.size() - count);
    pulse(feedback, held_refusal);
}

void Sound::hear(int index, const audio::CueEvent &event, bool held, bool *held_refusal)
{
    if (index < 0)
        return;
    const CueSpec &spec = spec_at(static_cast<std::size_t>(index));
    const std::size_t at = static_cast<std::size_t>(index);
    float gain = event.gain;

    // A hold goes on whether or not its ticks are heard, so it is counted first.
    const bool refusal = spec.cue == Cue::error;
    if (spec.navigation || refusal)
    {
        const bool holding = held || clock_ - last_nav_ < kHeldGap;
        last_nav_ = clock_;
        if (spec.navigation)
            run_ = holding ? run_ + 1 : 0;
        if (refusal && holding)
        {
            // The end of a list under a held direction is silent, and still.
            ++stats_.held;
            *held_refusal = true;
            return;
        }
    }

    if (!options_.sounds || options_.volume <= 0 || !output_.audio())
    {
        ++stats_.off; // off means off: nothing is posted, the opening included
        return;
    }
    if (attract_)
    {
        ++stats_.hushed; // nobody is there to hear it
        return;
    }
    if (situation_.playing)
    {
        // The video's own sound is the product: toasts never, a dialog in
        // full, notices 6 dB down, everything else only when asked for.
        if (options_.playback == Options::Playback::off || spec.beside == Beside::toast)
        {
            ++stats_.hushed;
            return;
        }
        if (situation_.layer != Layer::dialog)
        {
            if (spec.beside != Beside::notice && options_.playback != Options::Playback::all)
            {
                ++stats_.hushed;
                return;
            }
            gain *= kBesideGain;
        }
    }
    if (spec.cue == Cue::notify && clock_ - last_confirmed_ < kToastAfterConfirm)
    {
        ++stats_.merged; // the button already said it
        return;
    }
    if (clock_ - last_[at] < static_cast<double>(spec.gap) - 0.0005)
    {
        ++stats_.limited;
        return;
    }
    if (spec.navigation)
    {
        if (run_ < kThinFrom)
        {
            thinned_ = true; // so that the first tick of the long part of a hold is heard
        }
        else
        {
            thinned_ = !thinned_;
            if (thinned_)
            {
                ++stats_.held;
                return;
            }
            gain *= kThinGain;
        }
    }
    if (spec.rank == Rank::tick && voices() >= kVoiceBudget)
    {
        ++stats_.limited;
        return;
    }
    const Bank::Entry &entry = bank_.entry(index);
    if (entry.count <= 0)
    {
        ++stats_.missing; // a cue without a recording is silent, not an error
        return;
    }
    if (quiet_)
        gain *= kQuietGain;
    gain = std::clamp(gain * entry.take->gain, 0.0f, 2.0f);
    if (gain < 0.001f)
    {
        ++stats_.hushed;
        return;
    }

    const std::size_t variations = static_cast<std::size_t>(entry.count);
    const Bank::Recording &recording = *entry.recordings[turn_[at] % variations];
    turn_[at] = static_cast<std::uint8_t>((turn_[at] + 1u) % variations);
    // The kit's own touch: up to 3 % of detune, so that a repeat never sounds mechanical.
    jitter_ = jitter_ * 1664525u + 1013904223u;
    const float jitter = 0.97f + 0.06f * static_cast<float>(jitter_ >> 8) / 16777216.0f;
    // The interface volume is part of each voice: a voice starts at its level from its first sample, whatever
    // the volume was a moment ago (a gain on the bus would glide there while a tick is already over).
    audio::PlayParams params;
    params.bus = audio::Bus::ui;
    params.gain = gain * level(options_.volume);
    params.pitch = std::clamp(event.pitch * entry.take->pitch, kPitchLow, kPitchHigh) * jitter;
    params.pan = std::clamp(event.pan, -kPan, kPan);
    if (!mixer_.play_clip(&recording.clip, params))
    {
        ++stats_.dropped;
        return;
    }

    last_[at] = clock_;
    if (confirms(spec.cue))
        last_confirmed_ = clock_;
    const double ends = clock_ + static_cast<double>(recording.seconds / params.pitch);
    *std::min_element(ends_.begin(), ends_.end()) = ends;
    ++stats_.heard;

    Heard heard;
    heard.cue = spec.cue;
    heard.name = spec.name;
    heard.sample = entry.take->file;
    heard.pitch = params.pitch;
    heard.pan = params.pan;
    heard.gain = gain;
    heard.seconds = recording.seconds / params.pitch;
    output_.voice(heard);
}

// A pulse is punctuation: one per step at most, capped in strength and length,
// never two in a buzz, none in quiet hours or the calm set, none in attract
// mode, and none for the refusal a held direction keeps running into.
void Sound::pulse(const ui::Feedback &feedback, bool held_refusal)
{
    if (feedback.rumble_strength <= 0.0f || feedback.rumble_seconds <= 0.0f)
        return;
    if (!options_.vibration || calm_ || attract_ || held_refusal || clock_ - last_pulse_ < kPulseGap)
    {
        ++stats_.pulses_dropped;
        return;
    }
    last_pulse_ = clock_;
    ++stats_.pulses;
    output_.pulse(std::min(feedback.rumble_strength, kPulseStrength),
                  std::clamp(feedback.rumble_seconds, kPulseShortest, kPulseLongest));
}

} // namespace afterglow::sound
