// ProsperoTube Afterglow - the design language: the shared pieces.
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
    // The look's raised tone, up to twice over: one layer of it is barely there on a frosted panel.
    // (Color::with_alpha multiplies: it is given the share, not the alpha.)
    g.l.rounded_rect(r, radius, g.k.raised.with_alpha(std::min(2.0f, 1.0f / std::max(g.k.raised.a, 0.01f)) * breath));
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

} // namespace afterglow
