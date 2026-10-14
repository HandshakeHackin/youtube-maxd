// ProsperoTube Afterglow - the interface's sound vocabulary.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A screen asks for a cue (context.feedback.play(audio::Cue::focus, ...)) and
// never for a file. This table says, for every cue the interface uses, which
// asset serves it and how the rules of docs/afterglow/sound-design.md see
// it. All files are original, reproducible procedural ProsperoTube tones.

#pragma once

#include "audio/cues.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace afterglow::sound
{

namespace audio = hui::audio;

// Moments of a video app the kit's vocabulary has no cue for. A screen plays
// them like any other cue: context.feedback.play(sound::kPause). Their values
// lie past the kit's own (audio::Cue::count itself means "no sound" to the
// kit's components), so the kit never mistakes one for a cue of its own.
constexpr audio::Cue extended(int index)
{
    return static_cast<audio::Cue>(audio::kCueCount + 1 + static_cast<std::size_t>(index));
}
inline constexpr audio::Cue kPlayStart = extended(0);     // a video starts
inline constexpr audio::Cue kPause = extended(1);
inline constexpr audio::Cue kSeekCommit = extended(2);    // the scrub is let go
inline constexpr audio::Cue kSkip = extended(3);          // a segment was skipped
inline constexpr audio::Cue kSignedIn = extended(4);
inline constexpr audio::Cue kGoodbye = extended(5);       // the app closes
inline constexpr audio::Cue kCountdown = extended(6);     // a second of the countdown to the next video
inline constexpr audio::Cue kCountdownDone = extended(7); // ... and its end

// One recording a cue may be served by: "<set>/<cue>", found in the package as
// "<set>/<cue>_NN.wav". A stand-in is shaped to its moment by pitch and gain.
struct Take
{
    const char *file = nullptr;
    float pitch = 1.0f;
    float gain = 1.0f;
};

// How much one action's sounds give way to each other: of the cues of one step
// only the highest of ranks 1 to 3 is heard; ticks sound beside it.
enum class Rank : std::uint8_t
{
    tick,     // the quietest and most frequent: a move, a detent, a key
    plain,    // confirm, back
    specific, // what the action did: opened, switched, refused
    chime,    // a moment: saved, liked, started
};

// What becomes of a cue while a video plays (sound-design.md, section 5).
enum class Beside : std::uint8_t
{
    ordinary, // 6 dB down, and only when the player asked for all sounds
    notice,   // 6 dB down, kept with "Notices only"
    toast,    // never over a playing video
};

struct CueSpec
{
    audio::Cue cue;
    const char *name;
    Rank rank;
    Beside beside;
    float gap;       // seconds before the same cue may be heard again
    bool navigation; // a tick of a held direction: thinned and softened as the hold goes on
    Take takes[3];
};

constexpr float kTickGap = 1.0f / 12.0f; // at most 12 ticks a second
constexpr float kOnce = 1.0e9f;          // once per start of the app

constexpr std::size_t kMaxSpecs = 40;
// Every cue of the interface, in a fixed order; index_of() is a cue's place in it.
std::size_t spec_count();
const CueSpec &spec_at(std::size_t index);
// -1 for a cue the interface has no sound for.
int index_of(audio::Cue cue);
// The cue of a name of this table ("pause", "focus").
bool cue_named(std::string_view name, audio::Cue *cue);

} // namespace afterglow::sound
