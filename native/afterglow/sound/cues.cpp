// ProsperoTube Afterglow - the interface's sound vocabulary.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// All files are original procedural ProsperoTube assets; see recordings/manifest.json
// and scripts/generate_afterglow_sounds.py. No UI-kit audio fallback is packaged.

#include "afterglow/sound/cues.hpp"

namespace afterglow::sound
{

namespace
{

using audio::Cue;

constexpr bool kNav = true;

// The cue sheet of sound-design.md; every cue has an original asset.
constexpr CueSpec kSpecs[] = {
    {Cue::focus, "focus", Rank::tick, Beside::ordinary, kTickGap, kNav, {{"prosperotube/focus"}}},
    {Cue::select, "select", Rank::plain, Beside::ordinary, 0.06f, false, {{"prosperotube/select"}}},
    {Cue::back, "back", Rank::plain, Beside::ordinary, 0.06f, false, {{"prosperotube/back"}}},
    {Cue::tab, "tab", Rank::specific, Beside::ordinary, kTickGap, kNav, {{"prosperotube/tab"}}},
    {Cue::open, "open", Rank::specific, Beside::ordinary, 0.12f, false, {{"prosperotube/open"}}},
    {Cue::modal_open, "modal_open", Rank::specific, Beside::ordinary, 0.12f, false, {{"prosperotube/modal_open"}}},
    {Cue::modal_close, "modal_close", Rank::specific, Beside::ordinary, 0.12f, false, {{"prosperotube/modal_close"}}},
    {Cue::toggle, "toggle", Rank::specific, Beside::ordinary, 0.05f, false, {{"prosperotube/toggle"}}},
    {Cue::slider, "slider", Rank::tick, Beside::ordinary, kTickGap, false, {{"prosperotube/slider"}}},
    {Cue::error, "error", Rank::specific, Beside::ordinary, 0.25f, false, {{"prosperotube/error"}}},
    {Cue::notify, "notify", Rank::specific, Beside::toast, 0.3f, false, {{"prosperotube/notify"}}},
    // In this app the kit's "started something big" is a video starting.
    {Cue::launch, "launch", Rank::chime, Beside::notice, 2.0f, false, {{"prosperotube/play_start"}}},
    {Cue::favorite_on, "favorite_on", Rank::chime, Beside::ordinary, 0.3f, false, {{"prosperotube/favorite_on"}}},
    {Cue::favorite_off, "favorite_off", Rank::chime, Beside::ordinary, 0.3f, false, {{"prosperotube/favorite_off"}}},
    {Cue::saved, "saved", Rank::chime, Beside::ordinary, 0.3f, false, {{"prosperotube/saved"}}},
    {Cue::welcome, "welcome", Rank::chime, Beside::ordinary, kOnce, false, {{"prosperotube/welcome"}}},
    {Cue::resume, "resume", Rank::chime, Beside::ordinary, 1.0f, false, {{"prosperotube/resume"}}},
    {Cue::tick, "tick", Rank::tick, Beside::ordinary, kTickGap, kNav, {{"prosperotube/tick"}}},
    {Cue::type, "type", Rank::tick, Beside::ordinary, 0.04f, false, {{"prosperotube/type"}}},
    {Cue::erase, "erase", Rank::tick, Beside::ordinary, 0.06f, false, {{"prosperotube/erase"}}},
    {kPlayStart, "play_start", Rank::chime, Beside::notice, 2.0f, false, {{"prosperotube/play_start"}}},
    {kPause, "pause", Rank::specific, Beside::ordinary, 0.1f, false, {{"prosperotube/pause"}}},
    {kSeekCommit, "seek_commit", Rank::specific, Beside::ordinary, 0.1f, false, {{"prosperotube/seek_commit"}}},
    {kSkip, "skip", Rank::chime, Beside::notice, 0.3f, false, {{"prosperotube/skip"}}},
    {kSignedIn, "signed_in", Rank::chime, Beside::ordinary, 5.0f, false, {{"prosperotube/signed_in"}}},
    {kGoodbye, "goodbye", Rank::chime, Beside::ordinary, 1.0f, false, {{"prosperotube/goodbye"}}},
    {kCountdown, "countdown", Rank::tick, Beside::notice, kTickGap, false, {{"prosperotube/slider"}}},
    {kCountdownDone, "countdown_done", Rank::chime, Beside::notice, 0.3f, false, {{"prosperotube/notify"}}},
};
static_assert(sizeof kSpecs / sizeof kSpecs[0] <= kMaxSpecs);

} // namespace

std::size_t spec_count()
{
    return sizeof kSpecs / sizeof kSpecs[0];
}

const CueSpec &spec_at(std::size_t index)
{
    return kSpecs[index];
}

int index_of(audio::Cue cue)
{
    for (std::size_t index = 0; index < spec_count(); ++index)
    {
        if (kSpecs[index].cue == cue)
            return static_cast<int>(index);
    }
    return -1;
}

bool cue_named(std::string_view name, audio::Cue *cue)
{
    for (const CueSpec &spec : kSpecs)
    {
        if (name == spec.name)
        {
            *cue = spec.cue;
            return true;
        }
    }
    return false;
}

} // namespace afterglow::sound
