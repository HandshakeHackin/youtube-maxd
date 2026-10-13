// ProsperoTube Afterglow - the grid, the type scale and the four looks.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A look is one palette of the Afterglow design. The profile's Theme
// preference (native/tube_ui_style.h, g_tube_schemes) picks it: Afterglow,
// Onyx, Daybreak or High contrast; a classic colour scheme, which only the
// RmlUi browser has, shows Afterglow here.

#pragma once

#include "afterglow/base.hpp"

namespace afterglow
{

// ---- the grid ------------------------------------------------------------------
// 1920 x 1080 virtual pixels, an 8 pixel rhythm. The rail takes the first 120;
// content runs from kLeft to kRight; a shelf card is 352 x 198 on a 376 pitch.
constexpr float kRail = 120.0f;
constexpr float kLeft = 176.0f;
constexpr float kRight = 1824.0f;
constexpr float kTop = 60.0f;
constexpr float kHintY = 1026.0f;
constexpr float kCardW = 352.0f;
constexpr float kCardH = 198.0f;
constexpr float kCardGap = 24.0f;
constexpr float kCardPitch = kCardW + kCardGap;
constexpr float kShelfPitch = 404.0f; // from one shelf to the next
// Corner radii: one per kind of thing.
constexpr float kRadiusThumb = 16.0f;
constexpr float kRadiusPanel = 28.0f;
constexpr float kRadiusChip = 8.0f;
// Type scale.
constexpr float kTypeHero = 60.0f;
constexpr float kTypePage = 44.0f;
constexpr float kTypeTitle = 32.0f;
constexpr float kTypeShelf = 26.0f;
constexpr float kTypeBody = 24.0f;
constexpr float kTypeMeta = 20.0f;
constexpr float kTypeOver = 17.0f;

// ---- the look ------------------------------------------------------------------

struct Look
{
    const char *id;
    const char *name;
    bool dark;
    Color base;        // the page under everything
    Color base2;       // its lower tone
    float wash;        // how much of the focused picture colours the page
    Color ink;         // primary text
    Color ink2;        // secondary
    Color ink3;        // tertiary
    Color panel;       // frosted panels: the tint over the blur
    Color panel_solid; // the same where there is nothing to blur
    Color raised;      // resting buttons and chips
    Color hairline;    // light edges
    Color accent;      // the signature colour: progress, live, selected
    Color on_accent;
    Color focus;     // the focus ring
    Color plate;     // the focused or primary plate
    Color on_plate;  // text on it
    ui::Theme theme; // the kit theme the standard controls wear
};

const Look &look_afterglow();
const Look &look_onyx();
const Look &look_daybreak();
// The accessibility variant: no wash, solid panels, strokes that carry.
const Look &look_contrast();

constexpr int kLookCount = 4;
// The look of a Theme preference (an index into g_tube_schemes): 0 Afterglow,
// 1 Onyx, 2 Daybreak, 3 High contrast; anything else is Afterglow.
const Look &look_for_scheme(int scheme);
// The look by its id ("afterglow", "onyx", "daybreak", "contrast"); Afterglow
// for an unknown one.
const Look &look_by_id(std::string_view id);

// WCAG contrast of two opaque colours, 1..21. The harness holds every look to
// 4.5:1 for text on its ground.
float contrast_ratio(Color ink, Color ground);
// ink over ground as one opaque colour (ink's alpha applied).
Color flatten(Color ink, Color ground);

} // namespace afterglow
