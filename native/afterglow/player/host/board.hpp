// ProsperoTube Afterglow - the player in the host harness: what a board of the player is held to.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Host only (the app's build takes player/*.cpp and player/app/*.cpp, not this
// folder). tests/afterglow/host_main.cpp brings a player board up, draws it
// and writes its picture; the checks themselves are here so that its block
// stays small.

#pragma once

#include "afterglow/player/player.hpp"

#include <string>
#include <vector>

struct cJSON;

namespace afterglow::player::host
{

// What every board is held to: something has the focus and it is on the
// screen, and no text crosses the edge of the screen outside a clip rectangle.
// (A player with its controls hidden draws nothing, so an empty list is no
// fault here; the harness still asks for a picture with colours in it.)
// Returns what failed, one line each.
std::vector<std::string> standing_faults(const FocusInfo &focus, const gfx::DrawList &world, const gfx::DrawList &ui);

// A board's "expect" against the player's snapshot: "focus" (the focus id),
// "state" (values of the snapshot's state), any other text or number at the
// top of the snapshot, and "pixels" ([x, y, r, g, b] in virtual pixels,
// against the RGBA picture, top row first).
std::vector<std::string> expect_faults(const std::string &snapshot, const cJSON *expect, const unsigned char *pixels, int width, int height);

// A step's "set" object applied to the view.
void apply(const cJSON *set, PlayerView *view);

} // namespace afterglow::player::host
