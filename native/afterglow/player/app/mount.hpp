// ProsperoTube Afterglow - the player's mount: EVO's player state, drawn in the Afterglow design.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The player stays EVO's PlayerScreen with ProsperoTube's overlays: it owns
// playback, the pad and every menu, and it hands its state to EVO's kit
// application (ui_kit/src/evo_hui_app.cpp) as it always has. While the
// Interface preference is Afterglow, scripts/afterglow_overlay.py sends that
// state here as well and lets these functions draw it instead of EVO's own
// kit screens (OsdScreen, SubtitlePicker, the list of the options, the dialog,
// the toast). With the preference on Classic none of this is called.
//
// Console only: nothing here is compiled for the host. Everything is called
// on the main thread, inside an AGC frame.

#pragma once

#include "evo_rmlui_bridge.h"

#include "gfx/draw_list.hpp"
#include "ui/fonts.hpp"

namespace evo::kit
{
class TextureCache;
}

namespace afterglow
{

// What EVO says, kept until it is drawn. The strings are copied: EVO's
// pointers are valid only during its own call.
void player_set_playback(const evo_playback_osd_params_t &params);
void player_set_details(const evo_perf_hud_t &hud);
void player_set_captions_picker(const evo_rmlui_subtitles_params_t &params);
void player_set_list(const evo_rmlui_list_params_t &params);
void player_set_dialog(const evo_rmlui_dialog_params_t &params);
void player_set_toast(const evo_rmlui_toast_params_t &params);

// True when the list EVO is about to draw is the player's own (its options,
// their choices, the videos that are related): a sheet over the video here,
// a page with the rail in EVO.
bool player_list_is_sheet();

// Each draws its part into the list EVO's kit application then submits.
// frame is that application's count of presented frames (one step of motion
// per frame, however many parts it has). False = nothing was drawn and EVO's
// own drawing stands. page says that the Afterglow frame drew this frame's
// screen: the dialog or toast then belongs to the browser, not to the player.
bool player_draw_controls(hui::gfx::DrawList &list, const hui::ui::Fonts &fonts, evo::kit::TextureCache &textures, float dt, unsigned frame);
bool player_draw_captions_picker(hui::gfx::DrawList &list, const hui::ui::Fonts &fonts, float dt, unsigned frame);
bool player_draw_list(hui::gfx::DrawList &list, const hui::ui::Fonts &fonts, float dt, unsigned frame);
bool player_draw_dialog(hui::gfx::DrawList &list, const hui::ui::Fonts &fonts, float dt, unsigned frame, bool page);
bool player_draw_toast(hui::gfx::DrawList &list, const hui::ui::Fonts &fonts, float dt, unsigned frame, bool page);

// The player's state as one JSON object (Player::snapshot_json) while its
// controls or one of its sheets drew the last frames, "" otherwise. Valid
// until the next call.
const char *player_snapshot(unsigned frame);

} // namespace afterglow
