// ProsperoTube Afterglow - the four looks.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/theme.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace afterglow
{

namespace
{

ui::Theme base_theme()
{
    for (const ui::Theme &theme : ui::themes())
    {
        if (std::string_view(theme.id) == "acrylic")
            return theme;
    }
    return ui::themes()[0];
}

// The kit theme of a look: Acrylic's frosted construction, the look's palette.
ui::Theme theme_for(const Look &k)
{
    ui::Theme t = base_theme();
    t.id = k.id;
    t.name = k.name;
    t.family = "Frosted glass";
    t.summary = "ProsperoTube";
    t.backdrop = {};
    t.page = k.base;
    t.page_text = k.ink;
    t.page_text_muted = k.ink2;
    t.surface = k.panel;
    t.surface_high = k.raised;
    t.text = k.ink;
    t.text_muted = k.ink2;
    t.primary = k.plate;
    t.on_primary = k.on_plate;
    t.secondary = k.raised;
    t.on_secondary = k.ink;
    t.accent = k.accent;
    t.outline = k.hairline;
    t.focus = k.focus;
    t.shadow = Color::rgb(0x000000, k.dark ? 0.45f : 0.18f);
    t.light = k.hairline;
    t.danger = Color::rgb(0xff5d6c);
    t.success = Color::rgb(0x4fd8a0);
    t.warning = Color::rgb(0xf5c451);
    t.radius = 100.0f; // controls are pills
    t.radius_card = kRadiusPanel;
    t.border = 1.5f;
    t.pill_chips = true;
    t.shadow_offset = 14.0f;
    t.shadow_blur = 36.0f;
    t.focus_width = 3.0f;
    t.focus_gap = 4.0f;
    t.heading = ui::FontRole::display;
    t.label = ui::FontRole::semibold;
    t.omega = 18.0f;
    t.damping = 0.9f;
    t.sounds = audio::SoundSet::glass;
    t.dark = k.dark;
    return t;
}

Look make_afterglow()
{
    Look k{};
    k.id = "afterglow";
    k.name = "Afterglow";
    k.dark = true;
    k.base = Color::rgb(0x06070c);
    k.base2 = Color::rgb(0x0a0c14);
    k.wash = 0.62f;
    k.ink = Color::rgb(0xffffff);
    k.ink2 = Color::rgb(0xffffff, 0.72f);
    k.ink3 = Color::rgb(0xffffff, 0.5f);
    k.panel = Color::rgb(0x10121c, 0.55f);
    k.panel_solid = Color::rgb(0x12141e, 0.95f);
    k.raised = Color::rgb(0xffffff, 0.09f);
    k.hairline = Color::rgb(0xffffff, 0.14f);
    k.accent = Color::rgb(0xff5a47);
    k.on_accent = Color::rgb(0x1a0605);
    k.focus = Color::rgb(0xffffff);
    k.plate = Color::rgb(0xffffff);
    k.on_plate = Color::rgb(0x0b0c12);
    k.theme = theme_for(k);
    return k;
}

Look make_onyx()
{
    Look k{};
    k.id = "onyx";
    k.name = "Onyx";
    k.dark = true;
    k.base = Color::rgb(0x000000);
    k.base2 = Color::rgb(0x000000);
    k.wash = 0.14f;
    k.ink = Color::rgb(0xf5f1e8);
    k.ink2 = Color::rgb(0xf5f1e8, 0.7f);
    k.ink3 = Color::rgb(0xf5f1e8, 0.54f);
    k.panel = Color::rgb(0x0a0a0a, 0.6f);
    k.panel_solid = Color::rgb(0x0d0d0d, 0.97f);
    k.raised = Color::rgb(0xffffff, 0.07f);
    k.hairline = Color::rgb(0xffffff, 0.16f);
    k.accent = Color::rgb(0xf2c14e);
    k.on_accent = Color::rgb(0x1a1200);
    k.focus = Color::rgb(0xf5f1e8);
    k.plate = Color::rgb(0xf5f1e8);
    k.on_plate = Color::rgb(0x0a0a0a);
    k.theme = theme_for(k);
    return k;
}

Look make_daybreak()
{
    Look k{};
    k.id = "daybreak";
    k.name = "Daybreak";
    k.dark = false;
    k.base = Color::rgb(0xf3ede4);
    k.base2 = Color::rgb(0xe9e0d3);
    k.wash = 0.42f;
    k.ink = Color::rgb(0x1c1714);
    k.ink2 = Color::rgb(0x1c1714, 0.78f);
    k.ink3 = Color::rgb(0x1c1714, 0.66f);
    k.panel = Color::rgb(0xffffff, 0.58f);
    k.panel_solid = Color::rgb(0xfbf8f3, 0.96f);
    k.raised = Color::rgb(0x1c1714, 0.07f);
    k.hairline = Color::rgb(0x1c1714, 0.12f);
    k.accent = Color::rgb(0xe2533d);
    k.on_accent = Color::rgb(0x1c1714);
    k.focus = Color::rgb(0x1c1714);
    k.plate = Color::rgb(0x1c1714);
    k.on_plate = Color::rgb(0xfbf8f3);
    k.theme = theme_for(k);
    return k;
}

Look make_contrast()
{
    Look k{};
    k.id = "contrast";
    k.name = "High contrast";
    k.dark = true;
    k.base = Color::rgb(0x000000);
    k.base2 = Color::rgb(0x000000);
    k.wash = 0.0f;
    k.ink = Color::rgb(0xffffff);
    k.ink2 = Color::rgb(0xffffff);
    k.ink3 = Color::rgb(0xffffff, 0.82f);
    k.panel = Color::rgb(0x000000, 0.94f);
    k.panel_solid = Color::rgb(0x000000);
    k.raised = Color::rgb(0xffffff, 0.2f);
    k.hairline = Color::rgb(0xffffff, 0.75f);
    k.accent = Color::rgb(0xffd60a);
    k.on_accent = Color::rgb(0x000000);
    k.focus = Color::rgb(0xffd60a);
    k.plate = Color::rgb(0xffffff);
    k.on_plate = Color::rgb(0x000000);
    k.theme = theme_for(k);
    k.theme.focus_width = 5.0f;
    return k;
}

} // namespace

const Look &look_afterglow()
{
    static const Look kLook = make_afterglow();
    return kLook;
}
const Look &look_onyx()
{
    static const Look kLook = make_onyx();
    return kLook;
}
const Look &look_daybreak()
{
    static const Look kLook = make_daybreak();
    return kLook;
}
const Look &look_contrast()
{
    static const Look kLook = make_contrast();
    return kLook;
}

const Look &look_for_scheme(int scheme)
{
    switch (scheme)
    {
    case 1:
        return look_onyx();
    case 2:
        return look_daybreak();
    case 3:
        return look_contrast();
    default:
        return look_afterglow();
    }
}

const Look &look_by_id(std::string_view id)
{
    for (int scheme = 0; scheme < kLookCount; ++scheme)
    {
        if (id == look_for_scheme(scheme).id)
            return look_for_scheme(scheme);
    }
    return look_afterglow();
}

Color flatten(Color ink, Color ground)
{
    const float a = tween::clamp01(ink.a);
    return Color{ink.r * a + ground.r * (1.0f - a), ink.g * a + ground.g * (1.0f - a),
                 ink.b * a + ground.b * (1.0f - a), 1.0f};
}

float contrast_ratio(Color ink, Color ground)
{
    const auto channel = [](float c)
    { return c <= 0.03928f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); };
    const auto luminance = [&](Color c)
    { return 0.2126f * channel(c.r) + 0.7152f * channel(c.g) + 0.0722f * channel(c.b); };
    const float a = luminance(flatten(ink, ground));
    const float b = luminance(ground);
    return (std::max(a, b) + 0.05f) / (std::min(a, b) + 0.05f);
}

} // namespace afterglow
