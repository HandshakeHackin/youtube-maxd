// ProsperoTube Afterglow - interface sound and vibration inside the app.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Console only: nothing here is compiled for the host. The mount calls
// app_begin() when the frame comes up and app_step() after every update; the
// app's park and exit arrive through tube_afterglow_sound.h.

#pragma once

namespace afterglow
{

class Frame;
class Platform;

namespace sound
{

class Sound;

// The interface came up. The first call reads the recordings; opening says the
// app has just started, which is welcomed once.
void app_begin(Platform &platform, bool opening);
// After Frame::update(): plays and vibrates what that step asked for.
void app_step(const Frame &frame, Platform &platform, float dt);
// The app's Sound once app_begin() ran, for the player's controls when the
// interface draws them (they step it with Situation::playing).
Sound *app_sound();

} // namespace sound
} // namespace afterglow
