// ProsperoTube Afterglow - the interface's sound vocabulary.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Where the recordings come from: "glass/..." and "paper/..." are the UI kit's
// two sound sets (ps5-homebrew-ui, assets/audio/sfx, Copyright (C) 2026
// BlackBearReloaded, GPL-3.0-or-later; made for ProsperoEden and
// ProsperoPuzzles), taken from the kit pinned in scripts/ui_kit.py when the
// package is built. Nothing is recorded in this repository. A recording placed
// in native/afterglow/sound/recordings/<set>/ is shipped over the kit's file
// of the same name.

#include "afterglow/sound/cues.hpp"

namespace afterglow::sound
{

namespace
{

using audio::Cue;

constexpr bool kNav = true;

// The cue sheet of sound-design.md, section 3. Where a cue names two
// recordings the first is one of the ten still to be made and the second is
// what plays today: "borrowed" from the paper set, or a "stand-in" of the
// glass set with the pitch and gain the sheet gives it.
constexpr CueSpec kSpecs[] = {
    {Cue::focus, "focus", Rank::tick, Beside::ordinary, kTickGap, kNav, {{"glass/focus"}}},
    {Cue::select, "select", Rank::plain, Beside::ordinary, 0.06f, false, {{"glass/select"}}},
    {Cue::back, "back", Rank::plain, Beside::ordinary, 0.06f, false, {{"glass/back"}}},
    {Cue::tab, "tab", Rank::specific, Beside::ordinary, kTickGap, kNav, {{"glass/tab"}}},
    {Cue::open, "open", Rank::specific, Beside::ordinary, 0.12f, false, {{"glass/open"}}},
    {Cue::modal_open, "modal_open", Rank::specific, Beside::ordinary, 0.12f, false, {{"glass/modal_open"}}},
    {Cue::modal_close, "modal_close", Rank::specific, Beside::ordinary, 0.12f, false, {{"glass/modal_close"}}},
    {Cue::toggle, "toggle", Rank::specific, Beside::ordinary, 0.05f, false, {{"glass/toggle"}}},
    {Cue::slider, "slider", Rank::tick, Beside::ordinary, kTickGap, false, {{"glass/slider"}}},
    {Cue::error, "error", Rank::specific, Beside::ordinary, 0.25f, false, {{"glass/error"}}},
    {Cue::notify, "notify", Rank::specific, Beside::toast, 0.3f, false, {{"glass/notify"}}},
    // In this app the kit's "started something big" is a video starting.
    {Cue::launch, "launch", Rank::chime, Beside::notice, 2.0f, false, {{"glass/play_start"}, {"glass/launch"}}},
    {Cue::favorite_on, "favorite_on", Rank::chime, Beside::ordinary, 0.3f, false, {{"glass/favorite_on"}, {"paper/favorite_on"}}},
    {Cue::favorite_off, "favorite_off", Rank::chime, Beside::ordinary, 0.3f, false, {{"glass/favorite_off"}, {"paper/favorite_off"}}},
    {Cue::saved, "saved", Rank::chime, Beside::ordinary, 0.3f, false, {{"glass/saved"}}},
    {Cue::welcome, "welcome", Rank::chime, Beside::ordinary, kOnce, false, {{"glass/welcome"}}},
    {Cue::resume, "resume", Rank::chime, Beside::ordinary, 1.0f, false, {{"glass/resume"}}},
    {Cue::tick, "tick", Rank::tick, Beside::ordinary, kTickGap, kNav, {{"glass/tick"}, {"paper/tick"}}},
    {Cue::type, "type", Rank::tick, Beside::ordinary, 0.04f, false, {{"glass/type"}, {"paper/type"}}},
    {Cue::erase, "erase", Rank::tick, Beside::ordinary, 0.06f, false, {{"glass/erase"}, {"paper/erase"}}},
    {kPlayStart, "play_start", Rank::chime, Beside::notice, 2.0f, false, {{"glass/play_start"}, {"glass/launch"}}},
    {kPause, "pause", Rank::specific, Beside::ordinary, 0.1f, false, {{"glass/pause"}, {"glass/toggle", 0.92f}}},
    {kSeekCommit, "seek_commit", Rank::specific, Beside::ordinary, 0.1f, false, {{"glass/seek_commit"}, {"glass/select", 1.1f}}},
    {kSkip, "skip", Rank::chime, Beside::notice, 0.3f, false, {{"glass/skip"}, {"glass/notify", 1.1f, 0.7f}}},
    {kSignedIn, "signed_in", Rank::chime, Beside::ordinary, 5.0f, false, {{"glass/signed_in"}, {"glass/saved"}}},
    {kGoodbye, "goodbye", Rank::chime, Beside::ordinary, 1.0f, false, {{"glass/goodbye"}, {"glass/back"}}},
    {kCountdown, "countdown", Rank::tick, Beside::notice, kTickGap, false, {{"glass/slider"}}},
    {kCountdownDone, "countdown_done", Rank::chime, Beside::notice, 0.3f, false, {{"glass/notify"}}},
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
