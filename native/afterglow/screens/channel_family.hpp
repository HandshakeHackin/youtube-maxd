// ProsperoTube Afterglow - what the channel, channels and subscriptions screens share.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the three screens of this family share beyond the design language and
// the data layer.
//
// The parts marked REQUEST stand in for a piece that belongs to the frame
// owner's files; docs/afterglow/requests/C.md has the exact request for each.
// When the shared piece exists the stand-in goes.

#pragma once

#include "afterglow/data_feed.hpp"
#include "afterglow/screen.hpp"

#include "ui/components/tabs.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace afterglow::family
{


// ---- motion -----------------------------------------------------------------------------

// REQUEST: the frame gives a screen its Phase and moves nothing for it, and
// design.hpp has no piece that turns a Phase into the hand-over of the motion
// table (deeper and back out: 90 and 110 pixels; a section change: the old
// screen sinks to 97.5 %; a card that becomes a video page: the furniture is
// gone in 150 ms). Every screen needs the same one.
struct Travel
{
    gfx::DrawList &l;
    float alpha = 1.0f;

    Travel(G &g, const Phase &phase);
    ~Travel();
    Travel(const Travel &) = delete;
    Travel &operator=(const Travel &) = delete;
};

// Cross on a video: the player takes over, or says why not.
void play(const model::Video &video, float x, UpdateContext &context);

// ---- pieces -----------------------------------------------------------------------------


// How much of the focus a thing has: 1 under the gliding ring, falling to 0 a reach away.
inline float under_ring(const Rect &ring, const Rect &thing, float reach)
{
    return tween::clamp01(1.0f - std::hypot(ring.cx() - thing.cx(), ring.cy() - thing.cy()) / reach);
}



// ---- the actions of a row -----------------------------------------------------------------

// REQUEST: Touchpad opens the actions of whatever has the focus on every
// screen (board 14), so the sheet belongs to the frame: Nav::actions(row,
// title). Until it is there this is the sheet: the catalog Data::menu() names,
// its rows as a list, a row chosen through Data::act(), an answer that is a
// further list shown in place, a row that is another page handed back to the
// screen as a Route.
class Sheet
{
  public:
    Sheet();

    bool open() const
    {
        return open_;
    }
    // True while the sheet is on screen, also on its way out.
    bool visible() const
    {
        return amount_.value > 0.01f;
    }
    // Opens the actions of a row ("ptv...", "channel:UC..."); about is what the sheet is about, in words.
    void show(const std::string &row, std::string_view about, UpdateContext &context);
    // One frame of input while the sheet is open. True when the choice is another page: route says which.
    bool handle(const InputFrame &input, UpdateContext &context, Route *route);
    // Every frame: reads the menu, follows a running action, moves.
    void update(UpdateContext &context);
    void draw(G &g) const;
    // True once after an action went through: the lists of the screen may be older than the account now.
    bool take_changed();

    FocusInfo focus() const;
    std::span<const ui::Hint> hints() const;
    void snapshot(Snapshot &out) const;

  private:
    struct Level
    {
        std::string title;
        std::vector<model::Row> rows;
        int focus = 0;
    };
    void close(UpdateContext &context, bool sound);
    void begin(const model::Row &row, UpdateContext &context);
    Rect row_rect(int index) const;

    bool open_ = false;
    std::string menu_;
    std::string about_;
    std::vector<Level> levels_;
    model::Load load_ = model::Load::idle;
    std::string error_;
    Act act_;
    int acting_ = -1;
    std::string acting_title_;
    std::string confirmed_; // the row the person agreed to in the dialog
    bool changed_ = false;
    tween::Spring amount_;
    tween::Spring pos_;
    tween::Spring scroll_;
    ui::Pulse nudge_;
    std::shared_ptr<int> alive_;
};

} // namespace afterglow::family
