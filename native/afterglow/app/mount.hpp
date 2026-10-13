// ProsperoTube Afterglow - the mount: the frame inside EVO's kit application.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "hui_agc_batch.hpp"
#include "ui/fonts.hpp"

namespace afterglow
{

// Called by EVO's kit application inside an AGC frame, with its batch, its
// fonts and the time since its last frame. Runs one step of the interface and
// draws it. False: the interface is off or could not start, and the classic
// browser draws instead.
bool mount_render(evo::hui_agc::AgcBatch &batch, const hui::ui::Fonts &fonts, float dt, int width, int height);

} // namespace afterglow
