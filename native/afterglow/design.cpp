// ProsperoTube Afterglow - the design language: icons and the shared pieces.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/design.hpp"

#include "tube_qr.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace afterglow
{

namespace
{

float rad(float degrees)
{
    return degrees * 0.01745329252f;
}

// Two colours for a channel that has no picture, always the same for one id.
struct Tint
{
    Color top;
    Color bottom;
};
Tint tint_of(std::string_view key)
{
    static constexpr std::uint32_t kPairs[][2] = {
        {0x7a5cff, 0xff8a4c}, {0x2ad0c0, 0x2f6fed}, {0x3a5bd9, 0xc04bd6}, {0xffb347, 0xff5a3d},
        {0xff3d8b, 0x7a5cff}, {0xf2c94c, 0xff8a5c}, {0x39d0c0, 0x1f4f8f}, {0x39d98a, 0x1f8f6f},
        {0xff8a5c, 0xb0387a}, {0x9ece6a, 0x2f80ed}, {0x5ce1ff, 0x8a6cff}, {0xf0a85a, 0x8a3f52},
    };
    std::uint32_t hash = 2166136261u;
    for (const char c : key)
        hash = (hash ^ static_cast<unsigned char>(c)) * 16777619u;
    const auto &pair = kPairs[hash % (sizeof kPairs / sizeof kPairs[0])];
    return {Color::rgb(pair[0]), Color::rgb(pair[1])};
}

// The first letter of a name, as UTF-8, in upper case when it is ASCII.
std::string first_letter(std::string_view name)
{
    std::size_t at = 0;
    while (at < name.size() && (name[at] == ' ' || name[at] == '@'))
        ++at;
    if (at >= name.size())
        return {};
    const unsigned char lead = static_cast<unsigned char>(name[at]);
    const std::size_t length = lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xe ? 3 : 4;
    std::string letter(name.substr(at, std::min(length, name.size() - at)));
    if (letter.size() == 1 && letter[0] >= 'a' && letter[0] <= 'z')
        letter[0] = static_cast<char>(letter[0] - 'a' + 'A');
    return letter;
}

} // namespace

float overline(G &g, std::string_view value, float x, float baseline, Color color, gfx::Align align)
{
    return text(g, g.f.semibold, ui::upper(value), x, baseline, kTypeOver, color, align, 3.2f);
}

void icon(G &g, Icon which, float cx, float cy, float size, Color color)
{
    const float u = size / 24.0f;
    const float w = std::max(2.0f, size * 0.085f);
    const Color none = clear();
    const auto X = [&](float x) { return cx + (x - 12.0f) * u; };
    const auto Y = [&](float y) { return cy + (y - 12.0f) * u; };
    const auto L = [&](float x1, float y1, float x2, float y2) { g.l.line(X(x1), Y(y1), X(x2), Y(y2), w, color); };
    const auto R = [&](float x, float y, float rw, float rh, float rr)
    { g.l.bordered_rect({X(x) - w * 0.5f, Y(y) - w * 0.5f, rw * u + w, rh * u + w}, rr * u + w * 0.5f, none, w, color); };
    const auto RF = [&](float x, float y, float rw, float rh, float rr) { g.l.rounded_rect({X(x), Y(y), rw * u, rh * u}, rr * u, color); };
    const auto C = [&](float x, float y, float r) { g.l.ring(X(x), Y(y), r * u + w * 0.5f, w, color); };
    const auto CF = [&](float x, float y, float r) { g.l.circle(X(x), Y(y), r * u, color); };
    const auto A = [&](float x, float y, float r, float start, float sweep)
    { g.l.arc(X(x), Y(y), r * u + w * 0.5f, w, rad(start), rad(sweep), color, true); };
    const auto TR = [&](float x, float y, float base, float height, float angle)
    { g.l.triangle({X(x) - base * u * 0.5f, Y(y) - height * u * 0.5f, base * u, height * u}, color, 0.0f, rad(angle)); };
    const auto word = [&](const char *letters)
    { g.l.text(*g.f.semibold.font, g.f.semibold.texture, letters, cx, cy + size * 0.13f, size * 0.36f, color, gfx::Align::center, 0.0f); };

    switch (which)
    {
    case Icon::count:
        break;
    case Icon::home:
        L(3.5f, 11.5f, 12, 4);
        L(12, 4, 20.5f, 11.5f);
        L(6, 10.5f, 6, 20);
        L(6, 20, 18, 20);
        L(18, 20, 18, 10.5f);
        L(10.5f, 20, 10.5f, 14.5f);
        L(10.5f, 14.5f, 13.5f, 14.5f);
        L(13.5f, 14.5f, 13.5f, 20);
        break;
    case Icon::search:
        C(10.5f, 10.5f, 6.2f);
        L(15.4f, 15.4f, 20.5f, 20.5f);
        break;
    case Icon::subs:
        L(7, 3.5f, 17, 3.5f);
        R(3.5f, 7.5f, 17, 13, 3);
        TR(12.4f, 14, 6.4f, 5.6f, 90);
        break;
    case Icon::library:
        L(5, 4.5f, 5, 19.5f);
        L(10, 4.5f, 10, 19.5f);
        L(14.5f, 5.5f, 19.3f, 19.5f);
        break;
    case Icon::bell:
        A(12, 11, 6.2f, -90, 180);
        L(5.8f, 11, 5.8f, 16);
        L(18.2f, 11, 18.2f, 16);
        L(4, 17.2f, 20, 17.2f);
        CF(12, 20.6f, 1.7f);
        break;
    case Icon::settings:
        L(4, 7, 20, 7);
        L(4, 12, 20, 12);
        L(4, 17, 20, 17);
        CF(9, 7, 2.6f);
        CF(15.5f, 12, 2.6f);
        CF(8, 17, 2.6f);
        break;
    case Icon::user:
        C(12, 8.6f, 3.9f);
        A(12, 21.5f, 7.6f, -72, 144);
        break;
    case Icon::users:
        C(9, 8.8f, 3.4f);
        A(9, 21, 6.6f, -72, 144);
        A(15.5f, 8.8f, 3.3f, -20, 190);
        A(16.5f, 21, 6.4f, 5, 62);
        break;
    case Icon::play:
        TR(12.8f, 12, 15, 13, 90);
        break;
    case Icon::pause:
        RF(6.5f, 4.5f, 4, 15, 1.4f);
        RF(13.5f, 4.5f, 4, 15, 1.4f);
        break;
    case Icon::next:
        TR(10.5f, 12, 13, 11, 90);
        RF(17.2f, 5.5f, 2.6f, 13, 1.2f);
        break;
    case Icon::prev:
        TR(13.5f, 12, 13, 11, -90);
        RF(4.2f, 5.5f, 2.6f, 13, 1.2f);
        break;
    case Icon::queue:
        L(4, 7, 15, 7);
        L(4, 12, 15, 12);
        L(4, 17, 10.5f, 17);
        TR(17.6f, 16.2f, 7, 6, 90);
        break;
    case Icon::clock:
        C(12, 12, 8.3f);
        L(12, 7.4f, 12, 12);
        L(12, 12, 15.4f, 14);
        break;
    case Icon::heart:
        CF(8.3f, 9.4f, 4.5f);
        CF(15.7f, 9.4f, 4.5f);
        g.l.rotated_rect({X(12) - 5.6f * u, Y(13.2f) - 5.6f * u, 11.2f * u, 11.2f * u}, 1.2f * u, rad(45), color);
        break;
    case Icon::share:
        CF(17.8f, 5.6f, 2.8f);
        CF(6.2f, 12, 2.8f);
        CF(17.8f, 18.4f, 2.8f);
        L(8.2f, 10.9f, 15.8f, 6.7f);
        L(8.2f, 13.1f, 15.8f, 17.3f);
        break;
    case Icon::captions:
        R(3, 5.5f, 18, 13, 3);
        word("CC");
        break;
    case Icon::quality:
        R(3, 5.5f, 18, 13, 3);
        word("HD");
        break;
    case Icon::speed:
        A(12, 14, 8.2f, -120, 240);
        L(12, 14, 16, 9);
        CF(12, 14, 1.6f);
        break;
    case Icon::audio:
        RF(3.5f, 9.5f, 4, 5, 0.8f);
        TR(8.6f, 12, 11, 6.5f, -90);
        A(12.5f, 12, 3.6f, 40, 100);
        A(12.5f, 12, 7.2f, 40, 100);
        break;
    case Icon::chat:
        R(3.5f, 4.5f, 17, 12.5f, 3.5f);
        TR(8.5f, 18.6f, 5, 3.6f, 180);
        CF(8, 10.8f, 1.2f);
        CF(12, 10.8f, 1.2f);
        CF(16, 10.8f, 1.2f);
        break;
    case Icon::block:
        C(12, 12, 8.3f);
        L(6.2f, 6.2f, 17.8f, 17.8f);
        break;
    case Icon::hide:
        C(12, 12, 8.3f);
        L(7.6f, 12, 16.4f, 12);
        break;
    case Icon::playlist_add:
        L(4, 7, 14, 7);
        L(4, 12, 14, 12);
        L(4, 17, 10, 17);
        L(18, 13, 18, 21);
        L(14, 17, 22, 17);
        break;
    case Icon::chevron_right:
        L(9.5f, 6, 15.5f, 12);
        L(15.5f, 12, 9.5f, 18);
        break;
    case Icon::chevron_left:
        L(14.5f, 6, 8.5f, 12);
        L(8.5f, 12, 14.5f, 18);
        break;
    case Icon::chevron_down:
        L(6, 9.5f, 12, 15.5f);
        L(12, 15.5f, 18, 9.5f);
        break;
    case Icon::check:
        L(5, 12.6f, 10, 17.6f);
        L(10, 17.6f, 19.4f, 7);
        break;
    case Icon::plus:
        L(12, 5, 12, 19);
        L(5, 12, 19, 12);
        break;
    case Icon::close:
        L(6, 6, 18, 18);
        L(18, 6, 6, 18);
        break;
    case Icon::info:
        C(12, 12, 8.3f);
        CF(12, 8, 1.3f);
        L(12, 11.6f, 12, 16.4f);
        break;
    case Icon::phone:
        R(7, 3, 10, 18, 2.6f);
        L(10.6f, 17.6f, 13.4f, 17.6f);
        break;
    case Icon::backup:
        L(4, 15, 4, 19.5f);
        L(4, 19.5f, 20, 19.5f);
        L(20, 19.5f, 20, 15);
        L(12, 4, 12, 14.5f);
        L(8, 10.5f, 12, 14.5f);
        L(16, 10.5f, 12, 14.5f);
        break;
    case Icon::update:
        A(12, 12, 7.8f, 50, 280);
        L(13.2f, 3.6f, 17.6f, 5.2f);
        L(17.6f, 5.2f, 15.6f, 9.4f);
        break;
    case Icon::globe:
        C(12, 12, 8.3f);
        L(3.8f, 12, 20.2f, 12);
        L(12, 3.8f, 12, 20.2f);
        L(5.6f, 7.6f, 18.4f, 7.6f);
        L(5.6f, 16.4f, 18.4f, 16.4f);
        break;
    case Icon::grid:
        R(4, 4, 6.4f, 6.4f, 1.8f);
        R(13.6f, 4, 6.4f, 6.4f, 1.8f);
        R(4, 13.6f, 6.4f, 6.4f, 1.8f);
        R(13.6f, 13.6f, 6.4f, 6.4f, 1.8f);
        break;
    case Icon::skip:
        TR(7.2f, 12, 12, 7.6f, 90);
        TR(14, 12, 12, 7.6f, 90);
        RF(18.6f, 6, 2.4f, 12, 1.1f);
        break;
    case Icon::title:
        L(4.5f, 6, 15.5f, 6);
        L(10, 6, 10, 19);
        g.l.star(X(18), Y(15.5f), 4.2f * u, color);
        break;
    case Icon::shorts:
        R(7, 3, 10, 18, 3);
        TR(12.4f, 12, 6, 5.2f, 90);
        break;
    case Icon::live:
        CF(12, 12, 2.7f);
        A(12, 12, 6.2f, 50, 80);
        A(12, 12, 6.2f, 230, 80);
        A(12, 12, 9.8f, 55, 70);
        A(12, 12, 9.8f, 235, 70);
        break;
    case Icon::music:
        CF(8, 17.6f, 3.1f);
        L(10.9f, 17.4f, 10.9f, 5);
        L(10.9f, 5, 18, 7);
        break;
    case Icon::gamepad:
        R(3, 7.5f, 18, 10, 5);
        L(6.9f, 12.5f, 10.5f, 12.5f);
        L(8.7f, 10.7f, 8.7f, 14.3f);
        CF(15, 11.4f, 1.3f);
        CF(17.3f, 13.7f, 1.3f);
        break;
    case Icon::news:
        R(4, 4, 16, 16, 2.6f);
        L(7.6f, 8.6f, 16.4f, 8.6f);
        L(7.6f, 12, 16.4f, 12);
        L(7.6f, 15.4f, 13, 15.4f);
        break;
    case Icon::sports:
        C(12, 12, 8.3f);
        A(12, 25, 10.4f, -34, 68);
        A(12, -1, 10.4f, 146, 68);
        break;
    case Icon::movie:
        R(3, 5, 18, 14, 2.6f);
        L(8, 5, 8, 19);
        L(16, 5, 16, 19);
        L(3, 12, 8, 12);
        L(16, 12, 21, 12);
        break;
    case Icon::flask:
        L(9.5f, 4, 14.5f, 4);
        L(10.2f, 4, 10.2f, 10);
        L(13.8f, 4, 13.8f, 10);
        L(10.2f, 10, 5, 19.6f);
        L(13.8f, 10, 19, 19.6f);
        L(5, 19.6f, 19, 19.6f);
        L(7.8f, 15.2f, 16.2f, 15.2f);
        break;
    case Icon::trending:
        L(3.5f, 17, 9.5f, 11);
        L(9.5f, 11, 13.5f, 14.5f);
        L(13.5f, 14.5f, 20.5f, 7);
        L(15.5f, 7, 20.5f, 7);
        L(20.5f, 7, 20.5f, 12);
        break;
    case Icon::more:
        CF(5.5f, 12, 1.9f);
        CF(12, 12, 1.9f);
        CF(18.5f, 12, 1.9f);
        break;
    case Icon::bookmark:
        L(7, 4, 17, 4);
        L(7, 4, 7, 20);
        L(17, 4, 17, 20);
        L(7, 20, 12, 15.6f);
        L(17, 20, 12, 15.6f);
        break;
    case Icon::sort:
        L(4, 7, 20, 7);
        L(6.6f, 12, 17.4f, 12);
        L(9.6f, 17, 14.4f, 17);
        break;
    case Icon::qr:
        R(4, 4, 6, 6, 1);
        R(14, 4, 6, 6, 1);
        R(4, 14, 6, 6, 1);
        RF(14, 14, 2.6f, 2.6f, 0.4f);
        RF(17.6f, 17.6f, 2.6f, 2.6f, 0.4f);
        RF(14, 18, 2.2f, 2.2f, 0.4f);
        RF(18, 14, 2.2f, 2.2f, 0.4f);
        break;
    case Icon::lock:
        R(6, 11, 12, 9, 2.2f);
        A(12, 11, 4.2f, -90, 180);
        break;
    case Icon::mic:
        R(9.4f, 3.4f, 5.2f, 10.4f, 2.6f);
        A(12, 11.2f, 6.4f, 90, 180);
        L(12, 17.8f, 12, 20.6f);
        break;
    case Icon::offline:
        A(12, 18, 5, -45, 90);
        A(12, 18, 9.6f, -45, 90);
        CF(12, 18, 1.7f);
        L(5, 4.5f, 19, 19.5f);
        break;
    case Icon::alert:
        g.l.triangle({X(3), Y(3.5f), 18 * u, 16.5f * u}, color, w, 0.0f);
        L(12, 9.6f, 12, 14);
        CF(12, 16.8f, 1.2f);
        break;
    case Icon::chapters:
        CF(5, 7, 1.5f);
        CF(5, 12, 1.5f);
        CF(5, 17, 1.5f);
        L(9, 7, 20, 7);
        L(9, 12, 20, 12);
        L(9, 17, 20, 17);
        break;
    case Icon::reply:
        L(9, 6, 4.5f, 10.5f);
        L(4.5f, 10.5f, 9, 15);
        L(4.5f, 10.5f, 14, 10.5f);
        A(14, 15.5f, 5, 0, 110);
        break;
    case Icon::pin:
        CF(12, 8, 4.2f);
        L(12, 12, 12, 20.5f);
        break;
    case Icon::moon:
        A(12, 12, 8, 20, 250);
        A(15.4f, 9, 6.4f, 215, 118);
        break;
    }
}

void glass_panel(G &g, const Rect &r, float radius, float strength)
{
    if (g.glass != 0)
    {
        g.l.glass(g.glass, r, radius, kWhite);
        g.l.rounded_rect(r, radius, g.k.panel.with_alpha(strength));
    }
    else
    {
        g.l.rounded_rect(r, radius, g.k.panel_solid);
    }
    g.l.bordered_rect(r, radius, clear(), 1.5f, g.k.hairline);
}

void lift(G &g, const Rect &r, float radius, float amount)
{
    g.l.shadow({r.x, r.y + 18.0f * amount, r.w, r.h}, radius, 44.0f,
               kBlack.with_alpha((g.k.dark ? 0.5f : 0.2f) * amount));
}

void avatar_tint(std::string_view key, Color *top, Color *bottom)
{
    const Tint tint = tint_of(key);
    *top = tint.top;
    *bottom = tint.bottom;
}

void avatar(G &g, float cx, float cy, float radius, std::string_view name, std::string_view id, const Picture &picture)
{
    const Rect r{cx - radius, cy - radius, radius * 2.0f, radius * 2.0f};
    if (picture.ready())
    {
        g.l.image(picture.texture, r, picture.cover(1.0f), kWhite, radius);
    }
    else
    {
        const Tint tint = tint_of(id.empty() ? name : id);
        g.l.gradient_rect(r, radius, tint.top, tint.bottom);
        const std::string initial = first_letter(name);
        if (covers(g.f.semibold, initial))
            text(g, g.f.semibold, initial, cx, cy + radius * 0.36f, radius * 1.02f, kWhite, gfx::Align::center);
    }
    g.l.bordered_rect(r, radius, clear(), std::max(1.5f, radius * 0.05f), kWhite.with_alpha(0.22f));
}

float tag_width(const ui::Fonts &fonts, std::string_view label, float height)
{
    const float size = height * 0.58f;
    return measure(fonts.semibold, label, size, 0.8f) + height * 0.72f;
}

float tag_width(G &g, std::string_view label, float height)
{
    return tag_width(g.f, label, height);
}

float tag(G &g, float x, float cy, std::string_view label, Color fill, Color ink, float height)
{
    const float size = height * 0.58f;
    const float width = tag_width(g, label, height);
    g.l.rounded_rect({x, cy - height * 0.5f, width, height}, kRadiusChip, fill);
    text(g, g.f.semibold, label, x + height * 0.36f, cy + size * 0.36f, size, ink, gfx::Align::left, 0.8f);
    return width;
}

float button_width(G &g, std::string_view label, bool has_icon)
{
    return measure(g.f.semibold, label, kTypeBody) + (has_icon ? 96.0f : 60.0f);
}

float button_width(const ui::Fonts &fonts, std::string_view label, bool has_icon)
{
    return measure(fonts.semibold, label, kTypeBody) + (has_icon ? 96.0f : 60.0f);
}

void button(G &g, const Rect &r, std::string_view label, Icon glyph, float focus, bool primary, bool selected, bool busy)
{
    const float f = tween::clamp01(focus);
    const float radius = r.h * 0.5f;
    g.l.push_transform(1.0f + 0.035f * f, r.cx(), r.cy(), 0.0f, 0.0f);
    const bool has_label = !label.empty();
    Color fill = primary ? g.k.plate.with_alpha(0.92f) : g.k.raised;
    Color ink = primary ? g.k.on_plate : g.k.ink;
    if (selected && !primary)
    {
        fill = g.k.accent.with_alpha(0.2f);
        ink = g.k.ink;
    }
    if (f > 0.01f)
    {
        g.l.glow(r, radius, 22.0f, g.k.plate.with_alpha(0.3f * f));
        g.l.shadow({r.x, r.y + 12.0f, r.w, r.h}, radius, 28.0f, kBlack.with_alpha(0.4f * f));
    }
    g.l.rounded_rect(r, radius, gfx::mix(fill, g.k.plate, f));
    if (!primary && f < 0.99f)
        g.l.bordered_rect(r, radius, clear(), 1.5f, (selected ? g.k.accent : g.k.hairline).with_alpha((selected ? 0.7f : 1.0f) * (1.0f - f)));
    ink = gfx::mix(ink, g.k.on_plate, f);
    const Color icon_ink = selected && f < 0.5f ? g.k.accent : ink;
    // Pressed and not yet answered: the icon turns and the words wait.
    const auto sign = [&](float cx, float cy, float size)
    {
        if (busy)
            spinner(g, cx, cy, size * 0.4f, icon_ink);
        else
            icon(g, glyph, cx, cy, size, icon_ink);
    };
    if (has_label)
    {
        const float label_w = measure(g.f.semibold, label, kTypeBody);
        const float content = label_w + 38.0f;
        const float x = r.cx() - content * 0.5f;
        sign(x + 13.0f, r.cy(), 26.0f);
        text(g, g.f.semibold, label, x + 38.0f, r.cy() + 8.5f, kTypeBody, busy ? ink.with_alpha(ink.a * 0.6f) : ink);
    }
    else
    {
        sign(r.cx(), r.cy(), std::min(30.0f, r.h * 0.46f));
    }
    g.l.pop_transform();
}

float chip_width(const ui::Fonts &fonts, std::string_view label, float height, bool menu)
{
    return measure(fonts.semibold, label, height * 0.5f) + height * 0.9f + (menu ? height * 0.5f : 0.0f);
}

float chip_width(G &g, std::string_view label, float height, bool menu)
{
    return chip_width(g.f, label, height, menu);
}

float chip(G &g, float x, float cy, std::string_view label, bool selected, float focus, float height, bool menu)
{
    const float f = tween::clamp01(focus);
    const float width = chip_width(g, label, height, menu);
    const Rect r{x, cy - height * 0.5f, width, height};
    const float radius = height * 0.5f;
    if (selected)
        g.l.rounded_rect(r, radius, g.k.plate);
    else
    {
        g.l.rounded_rect(r, radius, g.k.raised);
        g.l.bordered_rect(r, radius, clear(), 1.5f, g.k.hairline);
    }
    if (f > 0.01f)
    {
        g.l.glow(r.inset(-2.0f), radius, 16.0f, g.k.focus.with_alpha(0.22f * f));
        g.l.bordered_rect(r.inset(-5.0f), radius + 5.0f, clear(), 3.0f, g.k.focus.with_alpha(f));
    }
    const Color ink = selected ? g.k.on_plate : (f > 0.5f ? g.k.ink : g.k.ink2);
    if (menu)
    {
        text(g, g.f.semibold, label, r.x + height * 0.45f, cy + height * 0.18f, height * 0.5f, ink);
        icon(g, Icon::chevron_down, r.x + r.w - height * 0.5f, cy + 1.0f, height * 0.4f, ink.with_alpha(ink.a * 0.8f));
    }
    else
    {
        text(g, g.f.semibold, label, r.cx(), cy + height * 0.18f, height * 0.5f, ink, gfx::Align::center);
    }
    return width;
}

void progress(G &g, const Rect &r, float value, Color fill)
{
    g.l.rounded_rect(r, r.h * 0.5f, kWhite.with_alpha(0.3f));
    const float width = std::max(r.h, r.w * tween::clamp01(value));
    g.l.rounded_rect({r.x, r.y, width, r.h}, r.h * 0.5f, fill);
}

void hints(G &g, const ui::Hint *items, int count, float alpha, bool left, float x)
{
    if (alpha <= 0.01f || count <= 0)
        return;
    ui::HintLayout layout;
    layout.size = 34.0f;
    layout.text_size = 22.0f;
    layout.cy = kHintY;
    layout.item_gap = 36.0f;
    ui::GlyphStyle style = g.k.dark ? ui::GlyphStyle::dark() : ui::GlyphStyle::light();
    style.label = g.k.ink2;
    g.l.push_opacity(alpha);
    ui::draw_hints(g.l, g.f, style, items, count, x, !left, layout);
    g.l.pop_opacity();
}

void focus_ring(G &g, const Rect &r, float radius, Color light, float amount)
{
    if (amount <= 0.01f)
        return;
    const float breathe = g.reduced ? 0.5f : ui::breathe(g.time, 2.8f);
    g.l.push_opacity(tween::clamp01(amount));
    // Light spills outward only: strokes of falling opacity keep the picture clean.
    for (int i = 0; i < 5; ++i)
    {
        const float out = 5.0f + 5.0f * static_cast<float>(i);
        const float fall = 1.0f - static_cast<float>(i) / 5.0f;
        g.l.bordered_rect(r.inset(-out), radius + out, clear(), 6.0f, light.with_alpha((0.2f + 0.12f * breathe) * fall * fall));
    }
    g.l.bordered_rect(r.inset(-5.0f), radius + 5.0f, clear(), g.k.theme.focus_width + 0.5f, g.k.focus);
    g.l.pop_opacity();
}

void picture_placeholder(G &g, const Rect &r, float radius, std::string_view initial)
{
    g.l.gradient_rect(r, radius, g.k.raised, g.k.raised.with_alpha(g.k.raised.a * 0.45f));
    if (!initial.empty() && covers(g.f.display, initial))
        text(g, g.f.display, initial, r.x + r.w * 0.5f, r.y + r.h * 0.5f + r.h * 0.13f, r.h * 0.38f, g.k.ink3,
             gfx::Align::center);
}

std::string duration_text(int seconds)
{
    if (seconds <= 0)
        return {};
    char buffer[24];
    if (seconds >= 3600)
        std::snprintf(buffer, sizeof buffer, "%d:%02d:%02d", seconds / 3600, seconds / 60 % 60, seconds % 60);
    else
        std::snprintf(buffer, sizeof buffer, "%d:%02d", seconds / 60, seconds % 60);
    return buffer;
}

void video_card(G &g, const Rect &r, const model::Video &video, const Picture &picture, float focus, float alpha,
                int text_mode)
{
    if (alpha <= 0.01f)
        return;
    const float f = tween::clamp01(focus);
    g.l.push_opacity(tween::clamp01(alpha));
    if (f > 0.01f)
        g.l.shadow({r.x, r.y + 20.0f, r.w, r.h}, kRadiusThumb, 40.0f, kBlack.with_alpha(0.55f * f));
    const float dim = 0.86f + 0.14f * f;
    // The picture fades in over its placeholder when it arrives.
    const float shown = picture.ready() ? (g.reduced ? 1.0f : tween::clamp01(picture.age / 0.25f)) : 0.0f;
    if (shown < 1.0f)
        picture_placeholder(g, r, kRadiusThumb, first_letter(video.title));
    if (shown > 0.0f)
        g.l.image(picture.texture, r, picture.cover(r.w / r.h), Color{dim, dim, dim, shown}, kRadiusThumb);
    g.l.bordered_rect(r, kRadiusThumb, clear(), 1.5f, kWhite.with_alpha(0.1f));

    // Tags, top left.
    float x = r.x + 12.0f;
    const float tag_cy = r.y + 28.0f;
    if ((video.badges & model::kBadgeLive) != 0)
    {
        const float pulse = g.reduced ? 0.6f : ui::breathe(g.time, 1.6f);
        const char *live = tr("LIVE");
        const float w = measure(g.f.semibold, live, 16.2f, 0.8f) + 40.0f;
        g.l.rounded_rect({x, tag_cy - 14.0f, w, 28.0f}, kRadiusChip, g.k.accent);
        g.l.circle(x + 14.0f, tag_cy, 4.0f + 1.5f * pulse, g.k.on_accent.with_alpha(0.25f + 0.25f * pulse));
        g.l.circle(x + 14.0f, tag_cy, 4.0f, g.k.on_accent);
        text(g, g.f.semibold, live, x + 26.0f, tag_cy + 5.8f, 16.2f, g.k.on_accent, gfx::Align::left, 0.8f);
        x += w + 8.0f;
    }
    if ((video.badges & model::kBadgeNew) != 0)
        x += tag(g, x, tag_cy, tr("NEW"), kWhite, Color::rgb(0x0b0c12)) + 8.0f;
    if ((video.badges & model::kBadge4K) != 0)
        x += tag(g, x, tag_cy, "4K", kBlack.with_alpha(0.62f), kWhite) + 8.0f;
    // Length, bottom right.
    const std::string length = duration_text(video.duration_s);
    if (!length.empty())
    {
        const float w = measure(g.f.semibold, length, 17.0f) + 18.0f;
        g.l.rounded_rect({r.x + r.w - w - 10.0f, r.y + r.h - 40.0f, w, 28.0f}, kRadiusChip, kBlack.with_alpha(0.72f));
        text(g, g.f.semibold, length, r.x + r.w - 19.0f, r.y + r.h - 20.0f, 17.0f, kWhite, gfx::Align::right);
    }
    if (video.progress > 0.0f)
        progress(g, {r.x + 12.0f, r.y + r.h - 9.0f, r.w - 24.0f, 5.0f}, video.progress, g.k.accent);

    if (text_mode != 0)
    {
        const float top = r.y + r.h;
        const Color ink = g.k.ink.with_alpha(0.82f + 0.18f * f);
        std::string meta = video.views;
        if (!video.age.empty())
            meta += (meta.empty() ? "" : " \xC2\xB7 ") + video.age;
        if (text_mode == 3)
        {
            text(g, g.f.semibold, fit(g.f.semibold, video.title, 22.0f, r.w - 4.0f), r.x + 2.0f, top + 34.0f, 22.0f, ink);
            g.l.pop_opacity();
            return;
        }
        const float after = paragraph(g, g.f.semibold, video.title, r.x + 2.0f, top + 36.0f, 23.0f, r.w - 4.0f, 30.0f, ink, 2);
        if (text_mode == 1)
        {
            text(g, g.f.regular, fit(g.f.regular, video.channel, kTypeMeta, r.w - 4.0f), r.x + 2.0f, after, kTypeMeta, g.k.ink2);
            text(g, g.f.regular, fit(g.f.regular, meta, kTypeMeta, r.w - 4.0f), r.x + 2.0f, after + 27.0f, kTypeMeta, g.k.ink3);
        }
        else
        {
            const std::string line = video.channel + (meta.empty() || video.channel.empty() ? "" : " \xC2\xB7 ") + meta;
            text(g, g.f.regular, fit(g.f.regular, line, kTypeMeta, r.w - 4.0f), r.x + 2.0f, after, kTypeMeta, g.k.ink2);
        }
    }
    g.l.pop_opacity();
}

void topic_tile(G &g, const Rect &r, const model::Topic &topic, float focus, float alpha)
{
    if (alpha <= 0.01f)
        return;
    const float f = tween::clamp01(focus);
    const Tint tint = tint_of(topic.id);
    g.l.push_opacity(tween::clamp01(alpha));
    if (f > 0.01f)
        g.l.shadow({r.x, r.y + 18.0f, r.w, r.h}, 20.0f, 36.0f, kBlack.with_alpha(0.5f * f));
    const float dim = 0.5f + 0.3f * f;
    g.l.gradient_rect(r, 20.0f, Color{tint.top.r * dim, tint.top.g * dim, tint.top.b * dim, 1.0f},
                      Color{tint.bottom.r * dim * 0.7f, tint.bottom.g * dim * 0.7f, tint.bottom.b * dim * 0.7f, 1.0f});
    g.l.gradient_rect(r, 20.0f, kBlack.with_alpha(0.05f), kBlack.with_alpha(0.5f));
    g.l.bordered_rect(r, 20.0f, clear(), 1.5f, kWhite.with_alpha(0.14f));
    icon(g, icon_by_name(topic.icon), r.x + 36.0f, r.y + 38.0f, 32.0f, kWhite);
    text(g, g.f.semibold, fit(g.f.semibold, topic.title, 27.0f, r.w - 36.0f), r.x + 20.0f, r.y + r.h - 22.0f, 27.0f, kWhite);
    g.l.pop_opacity();
}

// The kit tints an image top to bottom, not left to right, so the left side is
// built from columns of rising opacity. They are 12 pixels wide so that their
// edges fall on whole pixels at 720p, 1080p and 4K alike.
void faded_picture(gfx::DrawList &list, const Picture &picture, const Rect &r, float alpha, float fade_w, float solid_h)
{
    if (alpha <= 0.004f || !picture.ready())
        return;
    const Rect uv = picture.cover(r.w / r.h);
    const auto column = [&](float x0, float x1, float a)
    {
        const float u0 = uv.x + (x0 - r.x) / r.w * uv.w;
        const float u1 = uv.x + (x1 - r.x) / r.w * uv.w;
        const float v = solid_h / r.h;
        list.image(picture.texture, {x0, r.y, x1 - x0, solid_h}, {u0, uv.y, u1 - u0, uv.h * v}, Color{1.0f, 1.0f, 1.0f, a});
        if (solid_h < r.h)
            list.image_gradient(picture.texture, {x0, r.y + solid_h, x1 - x0, r.h - solid_h},
                                {u0, uv.y + uv.h * v, u1 - u0, uv.h * (1.0f - v)}, Color{1.0f, 1.0f, 1.0f, a},
                                Color{1.0f, 1.0f, 1.0f, 0.0f});
    };
    constexpr float kStrip = 12.0f;
    const int strips = static_cast<int>(fade_w / kStrip);
    for (int i = 0; i < strips; ++i)
        column(r.x + static_cast<float>(i) * kStrip, r.x + static_cast<float>(i + 1) * kStrip,
               alpha * tween::smoothstep((static_cast<float>(i) + 0.5f) / static_cast<float>(strips)));
    column(r.x + static_cast<float>(strips) * kStrip, r.x + r.w, alpha);
}

void bone(G &g, const Rect &r, float radius, int index)
{
    // The breath travels across the page from the left, so a row of bones reads as one wave.
    const float phase = g.time + 0.13f * static_cast<float>(index) - r.cx() / gfx::kVirtualWidth * 0.9f;
    const float breath = g.reduced ? 0.7f : 0.55f + 0.45f * ui::breathe(phase, 1.7f);
    g.l.rounded_rect(r, radius, g.k.raised.with_alpha(g.k.raised.a * breath));
}

void qr_code(G &g, const Rect &r, std::string_view value, Color dark, Color light)
{
    tube_qr_code code;
    const std::string copy(value);
    g.l.rounded_rect(r, 18.0f, light);
    if (tube_qr_encode_text(copy.c_str(), &code) != 0 || code.size <= 0)
        return;
    // Four light modules around the symbol, as the standard asks.
    const float cell = std::min(r.w, r.h) / static_cast<float>(code.size + 8);
    const float ox = r.cx() - cell * static_cast<float>(code.size) * 0.5f;
    const float oy = r.cy() - cell * static_cast<float>(code.size) * 0.5f;
    for (int y = 0; y < code.size; ++y)
    {
        int run = 0;
        for (int x = 0; x <= code.size; ++x)
        {
            const bool on = x < code.size && tube_qr_module(&code, x, y) != 0;
            if (on)
            {
                ++run;
                continue;
            }
            if (run > 0) // one rectangle per horizontal run keeps the shape count low
                g.l.rounded_rect({ox + cell * static_cast<float>(x - run), oy + cell * static_cast<float>(y),
                                  cell * static_cast<float>(run) + 0.3f, cell + 0.3f},
                                 0.0f, dark);
            run = 0;
        }
    }
}

void mark(G &g, float cx, float cy, float size, float amount, Color ink, Color glow)
{
    const float a = tween::clamp01(amount);
    if (a <= 0.001f)
        return;
    const float outer = size * 0.5f;
    const float draw = tween::cubic_out(a);
    g.l.glow({cx - outer * 0.5f, cy - outer * 0.5f, outer, outer}, outer * 0.5f, size * 0.55f, glow.with_alpha(0.42f * draw));
    // The mouth of the tube: a ring that does not quite close.
    g.l.arc(cx, cy, outer, size * 0.105f, 0.95f, (6.2831853f - 1.15f) * draw, ink, true);
    // The glass behind it, turned the other way.
    const float inner = tween::cubic_out(tween::clamp01(a * 1.35f - 0.35f));
    if (inner > 0.001f)
        g.l.arc(cx, cy, size * 0.3f, size * 0.085f, 0.95f + 3.14159265f, (6.2831853f - 2.3f) * inner, ink.with_alpha(0.72f), true);
    // The afterglow.
    const float core = tween::back_out(tween::clamp01(a * 2.0f - 1.0f));
    if (core > 0.001f)
    {
        g.l.glow({cx - size * 0.04f, cy - size * 0.04f, size * 0.08f, size * 0.08f}, size * 0.04f, size * 0.2f, glow.with_alpha(0.7f * core));
        g.l.circle(cx, cy, size * 0.088f * core, glow);
    }
}

namespace
{
constexpr const char *kIconNames[] = {
    "home", "search", "subs", "library", "bell", "settings", "user", "users", "play", "pause", "next", "prev",
    "queue", "clock", "heart", "share", "captions", "quality", "speed", "audio", "chat", "block", "hide",
    "playlist_add", "chevron_right", "chevron_left", "chevron_down", "check", "plus", "close", "info", "phone",
    "backup", "update", "globe", "grid", "skip", "title", "shorts", "live", "music", "gamepad", "news", "sports",
    "movie", "flask", "trending", "more", "bookmark", "sort", "qr", "lock", "mic", "offline", "alert", "chapters",
    "reply", "pin", "moon"};
static_assert(sizeof kIconNames / sizeof kIconNames[0] == static_cast<std::size_t>(Icon::count));
} // namespace

const char *icon_name(Icon which)
{
    return which < Icon::count ? kIconNames[static_cast<std::size_t>(which)] : "grid";
}

Icon icon_by_name(std::string_view name)
{
    for (std::size_t i = 0; i < static_cast<std::size_t>(Icon::count); ++i)
    {
        if (name == kIconNames[i])
            return static_cast<Icon>(i);
    }
    return Icon::grid;
}

} // namespace afterglow
