// ProsperoTube Afterglow - Settings: tiles, form rows, bones and the previews.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The form is the approved board's (32-settings-captions): a row is a label on
// the left and its control on the right, 70 high on a 76 pitch, under one tinted
// plate that glides with the focus. The controls are drawn here, not by the
// kit's Form, because a row shows more than a value: one that is on its way to
// the provider, confirmed, refused, or held in the move mode.

#include "afterglow/screens/settings.hpp"

#include <algorithm>
#include <cmath>

namespace afterglow::settings
{

namespace
{

constexpr float kLabelSize = 25.0f;
constexpr float kValueSize = 23.0f;
constexpr float kMarkRoom = 40.0f; // kept beside every control for "on its way" and "confirmed"

// An arrow head pointing left (-1) or right (1), as the kit's choice rows draw it.
void chevron(G &g, float x, float cy, int direction, Color color, float size = 9.0f)
{
    const float tip = x + static_cast<float>(direction) * size * 0.5f;
    const float back = x - static_cast<float>(direction) * size * 0.5f;
    g.l.line(back, cy - size, tip, cy, 2.5f, color);
    g.l.line(tip, cy, back, cy + size, 2.5f, color);
}

// ... pointing up (-1) or down (1).
void chevron_v(G &g, float cx, float y, int direction, Color color, float size = 8.0f)
{
    const float tip = y + static_cast<float>(direction) * size * 0.5f;
    const float back = y - static_cast<float>(direction) * size * 0.5f;
    g.l.line(cx - size, back, cx, tip, 2.5f, color);
    g.l.line(cx, tip, cx + size, back, 2.5f, color);
}

float bone_alpha(const G &g, float offset)
{
    return g.reduced ? 0.8f : 0.6f + 0.4f * ui::breathe(g.time + offset, 1.7f);
}

// A bone is the look's raised tone, twice over: on a frosted tile one layer of it is barely there.
Color bone_colour(const G &g, float offset)
{
    return g.k.raised.with_alpha(std::min(1.0f, g.k.raised.a * 2.2f) * bone_alpha(g, offset));
}

void line_bone(G &g, const Rect &r, float offset)
{
    g.l.rounded_rect(r, std::min(r.h * 0.5f, 12.0f), bone_colour(g, offset));
}

// One line of the sample caption, in the style the profile chose.
void caption_line(G &g, const PreviewState &state, std::string_view value, float cx, float baseline, float size)
{
    const float width = measure(g.f.regular, value, size);
    if (state.plate > 0.01f)
        g.l.rounded_rect({cx - width * 0.5f - size * 0.4f, baseline - size * 1.02f, width + size * 0.8f, size * 1.42f}, size * 0.2f,
                         kBlack.with_alpha(state.plate));
    if (state.edge == 1)
    {
        for (int i = 0; i < 8; ++i)
        {
            const float angle = 0.785398f * static_cast<float>(i);
            text(g, g.f.regular, value, cx + std::cos(angle) * size * 0.07f, baseline + std::sin(angle) * size * 0.07f, size, kBlack,
                 gfx::Align::center);
        }
    }
    else if (state.edge == 2)
    {
        text(g, g.f.regular, value, cx + size * 0.07f, baseline + size * 0.08f, size, kBlack.with_alpha(0.85f), gfx::Align::center);
    }
    text(g, g.f.regular, value, cx, baseline, size, state.text, gfx::Align::center);
}

void preview_captions(G &g, const Rect &r, const PreviewState &state, const Picture &picture)
{
    lift(g, r, 22.0f, 1.0f);
    if (picture.ready())
        g.l.image(picture.texture, r, picture.cover(r.w / r.h), kWhite, 22.0f);
    else
        picture_placeholder(g, r, 22.0f);
    g.l.bordered_rect(r, 22.0f, clear(), 1.5f, g.k.hairline);
    if (!state.captions)
    {
        g.l.rounded_rect(r, 22.0f, kBlack.with_alpha(0.3f));
        const char *off = tr("CAPTIONS ARE OFF");
        const float width = tag_width(g, off, 34.0f);
        tag(g, r.cx() - width * 0.5f, r.cy(), off, kBlack.with_alpha(0.6f), kWhite, 34.0f);
        return;
    }
    g.l.push_clip(r);
    // The box is a 744 pixel wide picture of a 1920 pixel wide screen: the letters keep that share.
    const float size = 34.0f * state.scale * (r.w / 1920.0f) * 1.9f;
    const std::string words = tr("The wind did not stop until morning.");
    // A caption wider than the picture takes two lines, as the player breaks it.
    std::string first = words;
    std::string second;
    if (measure(g.f.regular, words, size) > r.w - 56.0f)
    {
        std::size_t cut = words.find(' ', words.size() / 2);
        if (cut == std::string::npos)
            cut = words.rfind(' ');
        if (cut != std::string::npos)
        {
            first = words.substr(0, cut);
            second = words.substr(cut + 1);
        }
    }
    const float line = size * 1.5f;
    const float block = second.empty() ? 0.0f : line;
    const float lowest = r.y + r.h - 30.0f - size * 0.45f;
    const float highest = r.y + 24.0f + size + block;
    float baseline = state.position < 0.0f ? lowest : tween::lerp(lowest, highest, tween::clamp01(state.position));
    baseline = std::max(baseline, highest);
    caption_line(g, state, first, r.cx(), baseline - block, size);
    if (!second.empty())
        caption_line(g, state, second, r.cx(), baseline, size);
    g.l.pop_clip();
}

// A small page in another look: its ground, its type, a card under its focus
// ring, its accent and its plate.
void preview_theme(G &g, const Rect &r, const PreviewState &state)
{
    const bool classic = state.look.empty();
    const Look &look = classic ? g.k : look_by_id(state.look);
    const Color top = classic ? state.ground : look.base;
    const Color bottom = classic ? state.ground : look.base2;
    const Color ink = classic ? kWhite : look.ink;
    const Color ink2 = classic ? kWhite.with_alpha(0.72f) : look.ink2;
    const Color raised = classic ? kWhite.with_alpha(0.1f) : look.raised;
    const Color accent = classic ? state.accent : look.accent;
    const Color focus = classic ? kWhite : look.focus;
    const Color plate = classic ? kWhite : look.plate;
    const Color on_plate = classic ? kBlack : look.on_plate;
    lift(g, r, 22.0f, 1.0f);
    g.l.gradient_rect(r, 22.0f, top, bottom);
    g.l.push_clip(r);
    // The rail.
    g.l.rounded_rect({r.x, r.y, 58.0f, r.h}, 0.0f, raised);
    for (int i = 0; i < 4; ++i)
        g.l.circle(r.x + 29.0f, r.y + 96.0f + 52.0f * static_cast<float>(i), 9.0f, i == 0 ? ink : ink2.with_alpha(ink2.a * 0.6f));
    g.l.rounded_rect({r.x + 4.0f, r.y + 86.0f, 4.0f, 20.0f}, 2.0f, accent);
    // A title, a line under it, the play button.
    const float x = r.x + 92.0f;
    text(g, g.f.display, fit(g.f.display, state.name, 40.0f, r.w - 130.0f), x, r.y + 86.0f, 40.0f, ink);
    text(g, g.f.regular, fit(g.f.regular, tr("The page, its cards and the focus in this theme"), kTypeMeta, r.w - 130.0f), x, r.y + 122.0f,
         kTypeMeta, ink2);
    const Rect button{x, r.y + 148.0f, 150.0f, 46.0f};
    g.l.rounded_rect(button, 23.0f, plate);
    text(g, g.f.semibold, fit(g.f.semibold, tr("Play"), kTypeMeta, 110.0f), button.cx(), button.cy() + 7.0f, kTypeMeta, on_plate,
         gfx::Align::center);
    // Three cards; the first has the focus.
    for (int i = 0; i < 3; ++i)
    {
        const Rect card{x + 204.0f * static_cast<float>(i), r.y + 232.0f, 188.0f, 106.0f};
        g.l.rounded_rect(card, 12.0f, raised);
        g.l.rounded_rect({card.x, card.y + card.h + 14.0f, card.w * (i == 1 ? 0.6f : 0.82f), 10.0f}, 5.0f, ink2.with_alpha(ink2.a * 0.45f));
        if (i == 0)
        {
            g.l.bordered_rect(card.inset(-5.0f), 17.0f, clear(), 3.5f, focus);
            g.l.rounded_rect({card.x + 10.0f, card.y + card.h - 12.0f, card.w - 20.0f, 4.0f}, 2.0f, ink2.with_alpha(0.3f));
            g.l.rounded_rect({card.x + 10.0f, card.y + card.h - 12.0f, (card.w - 20.0f) * 0.62f, 4.0f}, 2.0f, accent);
        }
    }
    g.l.pop_clip();
    g.l.bordered_rect(r, 22.0f, clear(), 1.5f, g.k.hairline);
}

// One card of a shelf at the chosen sizes, beside the outline of the size it has by default.
void preview_card(G &g, const Rect &r, const PreviewState &state, const Picture &picture)
{
    glass_panel(g, r, 22.0f);
    g.l.push_clip(r);
    const float base = 300.0f;
    const float scale = std::clamp(state.interface_scale * state.grid_scale, 0.35f, 2.2f);
    const float width = base * scale;
    const float height = width * 9.0f / 16.0f;
    const float x = r.x + 44.0f;
    const float y = r.y + 44.0f;
    // What the card measures at 1.0x, so a change of scale is seen against something.
    g.l.bordered_rect({x, y, base, base * 9.0f / 16.0f}, state.rounded ? kRadiusThumb : 0.0f, clear(), 1.5f, g.k.hairline);
    const Rect picture_box{x, y, width, height};
    const float radius = state.rounded ? kRadiusThumb * std::min(scale, 1.4f) : 0.0f;
    if (picture.ready())
        g.l.image(picture.texture, picture_box, picture.cover(16.0f / 9.0f), kWhite, radius);
    else
        picture_placeholder(g, picture_box, radius);
    g.l.bordered_rect(picture_box.inset(-5.0f), radius + 5.0f, clear(), g.k.theme.focus_width + 0.5f, g.k.focus);
    const float title = 23.0f * scale;
    const float after = paragraph(g, g.f.semibold, tr("Seven Days Alone on the Ridge Line, a Silent Hiking Film in Four Parts"), x + 2.0f,
                                  y + height + 14.0f + title, title, width - 4.0f, title * 1.3f, g.k.ink, std::clamp(state.title_lines, 1, 4));
    const float small = kTypeMeta * scale;
    paragraph(g, g.f.regular, tr("Northlight Atlas \xC2\xB7 1.2M views \xC2\xB7 3 weeks ago"), x + 2.0f, after + small * 0.1f, small, width - 4.0f,
              small * 1.3f, g.k.ink2, std::clamp(state.subtitle_lines, 1, 2));
    g.l.pop_clip();
}

} // namespace

void draw_plate(G &g, const Rect &r, float amount, float lifted)
{
    if (amount <= 0.01f)
        return;
    const float radius = r.h * 0.5f;
    const float idle = g.reduced ? 0.5f : ui::breathe(g.time, 2.8f);
    if (lifted > 0.01f)
        lift(g, r, radius, lifted);
    g.l.rounded_rect(r, radius, g.k.focus.with_alpha((0.17f + 0.05f * idle + 0.1f * lifted) * amount));
    g.l.bordered_rect(r, radius, clear(), std::max(1.5f, g.k.theme.focus_width * 0.5f), g.k.focus.with_alpha(0.75f * amount));
    if (lifted > 0.01f)
        focus_ring(g, r, radius, g.k.accent, lifted * amount);
}

void draw_heading(G &g, const Rect &r, std::string_view title)
{
    const float baseline = r.y + r.h - 16.0f;
    const float left = r.x + kRowPad;
    const float room = r.w - 2.0f * kRowPad;
    const float width = text(g, g.f.semibold, fit(g.f.semibold, ui::upper(title), 18.0f, room), left, baseline, 18.0f, g.k.ink2);
    if (room - width > 40.0f)
        g.l.rounded_rect({left + width + 16.0f, baseline - 7.0f, room - width - 16.0f, 1.5f}, 0.0f, g.k.ink2.with_alpha(g.k.ink2.a * 0.3f));
}

void draw_row(G &g, const Rect &r, const Item &item, const RowLook &look)
{
    const Look &k = g.k;
    const float cy = r.cy();
    const float focus = tween::clamp01(look.focus);
    const float left = r.x + kRowPad + (g.reduced ? 0.0f : 6.0f * focus);
    const float right = r.x + r.w - kRowPad;
    const float value_y = cy + kValueSize * 0.35f;
    const Color quiet = gfx::mix(k.ink2, k.ink, focus * 0.6f);
    const float waiting = tween::clamp01(look.pending);
    const Color value_ink = k.ink.with_alpha(k.ink.a * (1.0f - 0.45f * waiting));
    g.l.push_opacity(item.disabled ? 0.42f : 1.0f);
    Color label_ink = k.ink;
    float label_end = right; // where the label must stop: each kind moves it to its control
    const std::string &badge = item.row.fact("tag");
    const float badge_width = badge.empty() ? 0.0f : tag_width(g, badge, 26.0f) + 14.0f;
    const float dot = item.attention ? 22.0f : 0.0f;
    // What the label keeps whatever its control would like to take.
    const float label_need = std::min(measure(g.f.semibold, item.label, kLabelSize), 320.0f) + badge_width + dot + kMarkRoom + 16.0f;
    switch (item.kind)
    {
    case Kind::toggle:
    {
        const float on = tween::clamp01(look.on);
        const Rect body{right - 76.0f, cy - 20.0f, 76.0f, 40.0f};
        // Off, the track is a grey of the look's own ink, so the white thumb reads on a light page too.
        g.l.rounded_rect(body, 20.0f, gfx::mix(k.ink3.with_alpha(k.ink3.a * 0.45f), k.accent, on));
        const float travel = std::clamp(look.on, -0.06f, 1.06f);
        const float cx = body.x + 20.0f + 36.0f * travel;
        g.l.shadow({cx - 15.0f, cy - 13.0f, 30.0f, 30.0f}, 15.0f, 5.0f, kBlack.with_alpha(0.28f));
        g.l.circle(cx, cy, 15.0f, kWhite);
        const char *on_text = tr("On");
        const char *off_text = tr("Off");
        const bool is_on = look.on > 0.5f;
        text(g, g.f.semibold, is_on ? on_text : off_text, body.x - 16.0f, value_y, kValueSize, is_on ? value_ink : quiet.with_alpha(quiet.a * (1.0f - 0.45f * waiting)),
             gfx::Align::right);
        label_end = body.x - 16.0f - std::max(measure(g.f.semibold, on_text, kValueSize), measure(g.f.semibold, off_text, kValueSize));
        break;
    }
    case Kind::list:
    case Kind::cycle:
    {
        // 300 wide as on the board; a value that does not fit takes what the label leaves.
        const float wanted = measure(g.f.semibold, look.value, kValueSize) + 72.0f;
        const float room = std::clamp(wanted, kControlW, std::max(kControlW, r.w - 2.0f * kRowPad - label_need));
        const Rect at{right - room, r.y, room, r.h};
        const float arrows = tween::lerp(0.45f, 1.0f, focus);
        const float push = g.reduced ? 0.0f : 5.0f * look.nudge;
        if (item.kind == Kind::list)
            chevron(g, at.x + 10.0f + std::min(push, 0.0f), cy, -1, k.ink.with_alpha(k.ink.a * arrows));
        chevron(g, right - 10.0f + std::max(push, 0.0f), cy, 1, k.ink.with_alpha(k.ink.a * arrows));
        text(g, g.f.semibold, fit(g.f.semibold, look.value, kValueSize, room - 64.0f), at.cx(), value_y, kValueSize, value_ink, gfx::Align::center);
        label_end = at.x;
        break;
    }
    case Kind::slider:
    {
        // The track is 180 long as on the board; a value longer than "120 %" moves it left instead of being cut.
        const float number = std::clamp(measure(g.f.semibold, look.value, kValueSize) + 12.0f, 86.0f,
                                        std::max(86.0f, r.w - 2.0f * kRowPad - label_need - 214.0f));
        const Rect at{right - 214.0f - number, r.y, 214.0f + number, r.h};
        const Rect track{at.x + 17.0f, cy - 4.0f, 180.0f, 8.0f};
        const float fill = tween::clamp01(look.fill);
        g.l.rounded_rect(track, 4.0f, k.ink3.with_alpha(k.ink3.a * 0.5f));
        if (fill > 0.005f)
            g.l.rounded_rect({track.x, track.y, track.w * fill, track.h}, 4.0f, k.accent);
        const float size = 17.0f + 2.0f * std::fabs(look.nudge);
        const float cx = track.x + track.w * fill;
        g.l.shadow({cx - size, cy - size + 2.0f, size * 2.0f, size * 2.0f}, size, 5.0f, kBlack.with_alpha(0.28f));
        g.l.circle(cx, cy, size, kWhite);
        g.l.ring(cx, cy, size, 2.5f, k.accent);
        text(g, g.f.semibold, fit(g.f.semibold, look.value, kValueSize, number - 8.0f), right, value_y, kValueSize, value_ink, gfx::Align::right);
        label_end = at.x;
        break;
    }
    case Kind::choice:
    {
        float end = right;
        if (look.on > 0.5f && waiting < 0.5f)
        {
            g.l.circle(right - 15.0f, cy, 15.0f, k.accent);
            icon(g, Icon::check, right - 15.0f, cy, 20.0f, k.on_accent);
            end = right - 44.0f;
        }
        else
        {
            g.l.ring(right - 15.0f, cy, 15.0f, 2.0f, k.ink3);
            end = right - 44.0f;
        }
        if (!look.value.empty())
            end -= text(g, g.f.regular, fit(g.f.regular, look.value, kValueSize, 300.0f), end, value_y, kValueSize, quiet, gfx::Align::right) + 12.0f;
        label_end = end;
        break;
    }
    case Kind::order:
    {
        const bool shown = look.on > 0.5f;
        const float held = tween::clamp01(look.grabbed);
        // Three short lines say "this can be taken"; held, they become the two ways it can go.
        if (held < 0.99f)
        {
            for (int i = -1; i <= 1; ++i)
                g.l.line(right - 22.0f, cy + 7.0f * static_cast<float>(i), right - 2.0f, cy + 7.0f * static_cast<float>(i), 2.5f,
                         k.ink3.with_alpha(k.ink3.a * (1.0f - held)));
        }
        if (held > 0.01f)
        {
            const float bob = g.reduced ? 0.0f : 2.0f * ui::breathe(g.time, 1.2f);
            chevron_v(g, right - 12.0f, cy - 11.0f - bob, -1, k.ink.with_alpha(held * (look.can_up ? 1.0f : 0.3f)));
            chevron_v(g, right - 12.0f, cy + 11.0f + bob, 1, k.ink.with_alpha(held * (look.can_down ? 1.0f : 0.3f)));
        }
        float end = right - 44.0f;
        end -= text(g, g.f.regular, fit(g.f.regular, look.value, kValueSize, 220.0f), end, value_y, kValueSize,
                    shown ? quiet : k.ink3.with_alpha(k.ink3.a * (1.0f - 0.45f * waiting)), gfx::Align::right);
        if (!shown)
            icon(g, Icon::hide, end - 24.0f, cy, 24.0f, k.ink3);
        label_end = end - (shown ? 12.0f : 48.0f);
        if (!shown)
            label_ink = k.ink2;
        break;
    }
    default: // action, text, page, link, info
    {
        float end = right;
        if (item.kind == Kind::page || item.kind == Kind::link || item.kind == Kind::text)
        {
            chevron(g, right - 7.0f + (g.reduced ? 0.0f : 4.0f * focus), cy, 1, quiet);
            end -= 30.0f;
        }
        if (item.kind == Kind::action && item.danger)
            label_ink = k.theme.danger;
        if (!look.value.empty())
        {
            const float room = std::max(160.0f, (right - left) * 0.52f);
            end -= text(g, g.f.regular, fit(g.f.regular, look.value, kValueSize, room), end, value_y, kValueSize,
                        quiet.with_alpha(quiet.a * (1.0f - 0.45f * waiting)), gfx::Align::right) + 12.0f;
        }
        label_end = end;
        if (item.kind == Kind::info)
            label_ink = k.ink2;
        break;
    }
    }
    // Beside the control: the value is on its way, or the provider has just confirmed it.
    const float mark_x = label_end - kMarkRoom * 0.5f;
    if (waiting > 0.02f)
        spinner(g, mark_x, cy, 10.0f, k.ink2.with_alpha(k.ink2.a * waiting));
    else if (look.applied > 0.02f)
        icon(g, Icon::check, mark_x, cy, 22.0f + 6.0f * look.applied, k.accent.with_alpha(tween::clamp01(look.applied * 2.2f)));
    label_end -= kMarkRoom;

    float x = left;
    const std::string label = fit(g.f.semibold, item.label, kLabelSize, std::max(label_end - left - badge_width - dot, 60.0f));
    x += text(g, g.f.semibold, label, x, cy + kLabelSize * 0.35f, kLabelSize, label_ink);
    if (item.attention)
    {
        g.l.circle(x + 14.0f, cy, 5.0f, k.accent);
        x += dot;
    }
    if (!badge.empty())
        tag(g, x + 14.0f, cy, badge, k.raised, k.ink2, 26.0f);
    g.l.pop_opacity();
}

void draw_tile(G &g, const Rect &base, const Tile &tile, std::string_view name, std::string_view summary, float focus, bool attention,
               bool summary_loading)
{
    const Look &k = g.k;
    const float f = tween::clamp01(focus);
    const Rect r = grown(base, 0.03f * f, 4.0f * f);
    if (f > 0.01f)
        lift(g, r, kRadiusPanel, f);
    glass_panel(g, r, kRadiusPanel, 1.0f + 0.2f * f);
    // The icon sits on a disc that lights when the tile has the focus.
    g.l.circle(r.x + 54.0f, r.y + 56.0f, 30.0f, gfx::mix(k.raised, k.plate, f));
    icon(g, tile.icon, r.x + 54.0f, r.y + 56.0f, 30.0f, gfx::mix(k.ink, k.on_plate, f));
    if (attention)
        g.l.circle(r.x + r.w - 34.0f, r.y + 34.0f, 7.0f, k.accent);
    const float width = r.w - 56.0f;
    // A name of two lines leaves its summary one.
    const int name_lines = measure_block(g.f.semibold, name, 27.0f, width, 32.0f, 2).lines;
    const float after = paragraph(g, g.f.semibold, name, r.x + 28.0f, r.y + 122.0f, 27.0f, width, 32.0f, k.ink, 2);
    if (summary_loading)
        line_bone(g, {r.x + 28.0f, after - 18.0f, width * 0.62f, 16.0f}, 0.1f * (base.x + base.y) / 100.0f);
    else
        paragraph(g, g.f.regular, summary, r.x + 28.0f, after - 2.0f, kTypeMeta, width, 26.0f, k.ink2, name_lines > 1 ? 1 : 2);
}

void draw_tile_bones(G &g, const Rect &r, float phase)
{
    glass_panel(g, r, kRadiusPanel, 0.8f);
    g.l.circle(r.x + 54.0f, r.y + 56.0f, 30.0f, bone_colour(g, phase));
    line_bone(g, {r.x + 28.0f, r.y + 102.0f, r.w * 0.46f, 22.0f}, phase);
    line_bone(g, {r.x + 28.0f, r.y + 138.0f, r.w * 0.66f, 16.0f}, phase + 0.1f);
}

void draw_form_bones(G &g, float amount)
{
    g.l.push_opacity(tween::clamp01(amount));
    float y = kFormTop;
    for (int section = 0; section < 2; ++section)
    {
        line_bone(g, {kLeft, y + 22.0f, 150.0f, 16.0f}, 0.0f);
        y += kHeadingH + kRowGap;
        for (int i = 0; i < 4; ++i)
        {
            const float phase = 0.12f * static_cast<float>(section * 4 + i);
            line_bone(g, {kLeft, y + 24.0f, 190.0f + 46.0f * static_cast<float>((i * 3 + section) % 4), 22.0f}, phase);
            line_bone(g, {kFormX + kFormW - kRowPad - (i % 2 == 0 ? 76.0f : 240.0f), y + 20.0f, i % 2 == 0 ? 76.0f : 240.0f, 30.0f}, phase + 0.05f);
            y += kRowH + kRowGap;
        }
    }
    g.l.rounded_rect(kPreviewBox, 22.0f, bone_colour(g, 0.3f));
    g.l.pop_opacity();
}

void draw_notice(G &g, Icon glyph, std::string_view title, std::string_view body, std::string_view button_label, float focus,
                 const Rect &button_rect)
{
    const Look &k = g.k;
    const Rect panel{kLeft, 232.0f, 880.0f, button_label.empty() ? 250.0f : 330.0f};
    glass_panel(g, panel, kRadiusPanel);
    g.l.circle(panel.x + 76.0f, panel.y + 80.0f, 36.0f, k.raised);
    icon(g, glyph, panel.x + 76.0f, panel.y + 80.0f, 38.0f, k.ink);
    const float x = panel.x + 136.0f;
    const float width = panel.w - 136.0f - 44.0f;
    const float after = paragraph(g, g.f.semibold, title, x, panel.y + 76.0f, kTypeTitle, width, 40.0f, k.ink, 2);
    paragraph(g, g.f.regular, body, x, after + 4.0f, kTypeBody, width, 34.0f, k.ink2, 3);
    if (!button_label.empty())
        button(g, button_rect, button_label, Icon::chevron_right, focus, true);
}

void draw_path(G &g, const std::vector<std::string> &path, float amount)
{
    const Look &k = g.k;
    Enter enter(g, amount, -14.0f);
    float x = kLeft + 2.0f;
    x += overline(g, tr("Settings"), x, 84.0f, k.ink3);
    // The page itself is the last name, in the accent; a long way down keeps its last two.
    const std::size_t first = path.size() > 2 ? path.size() - 2 : 0;
    for (std::size_t i = first; i < path.size(); ++i)
    {
        icon(g, Icon::chevron_right, x + 16.0f, 78.0f, 16.0f, k.ink3);
        x += 34.0f;
        const std::string name = fit(g.f.semibold, ui::upper(path[i]), kTypeOver, 520.0f);
        x += text(g, g.f.semibold, name, x, 84.0f, kTypeOver, i + 1 == path.size() ? k.accent : k.ink3, gfx::Align::left, 3.2f);
    }
    if (!path.empty())
        text(g, g.f.display, fit(g.f.display, path.back(), kTypePage, 1300.0f), kLeft - 2.0f, 146.0f, kTypePage, k.ink);
}

void draw_help(G &g, const Rect &r, std::string_view heading, std::string_view words, std::string_view more, bool alert)
{
    const Look &k = g.k;
    glass_panel(g, r, 24.0f);
    if (alert)
        g.l.bordered_rect(r, 24.0f, clear(), 2.0f, k.theme.danger);
    g.l.circle(r.x + 44.0f, r.y + 46.0f, 20.0f, k.raised);
    icon(g, alert ? Icon::alert : Icon::info, r.x + 44.0f, r.y + 46.0f, 24.0f, k.ink);
    const float width = r.w - 116.0f;
    int room = static_cast<int>((r.h - 40.0f) / 31.0f);
    float top = r.y + 52.0f;
    if (!heading.empty())
    {
        const int taken = std::min(measure_block(g.f.semibold, heading, 22.0f, width, 31.0f, 2).lines, 2);
        top = paragraph(g, g.f.semibold, heading, r.x + 82.0f, top, 22.0f, width, 31.0f, k.ink, 2);
        room -= taken;
    }
    const TextBlock first = measure_block(g.f.regular, words, 22.0f, width, 31.0f, std::max(room, 1));
    const float after = paragraph(g, g.f.regular, words, r.x + 82.0f, top, 22.0f, width, 31.0f, alert ? k.ink : k.ink2, std::max(room, 1));
    const int left = static_cast<int>((r.y + r.h - 22.0f - after) / 28.0f) + 1;
    if (!more.empty() && first.lines > 0 && left > 0)
        paragraph(g, g.f.regular, more, r.x + 82.0f, after + 8.0f, kTypeMeta, width, 28.0f, k.ink3, left);
}

void draw_preview(G &g, const Rect &r, const PreviewState &state, const Picture &picture, float amount)
{
    if (state.kind == PreviewKind::none || amount <= 0.01f)
        return;
    g.l.push_opacity(tween::clamp01(amount));
    overline(g, tr("Preview"), r.x + 2.0f, r.y - 18.0f, g.k.ink3);
    switch (state.kind)
    {
    case PreviewKind::captions:
        preview_captions(g, r, state, picture);
        break;
    case PreviewKind::theme:
        preview_theme(g, r, state);
        break;
    case PreviewKind::card:
        preview_card(g, r, state, picture);
        break;
    case PreviewKind::none:
        break;
    }
    g.l.pop_opacity();
}

} // namespace afterglow::settings
