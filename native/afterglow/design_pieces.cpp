// ProsperoTube Afterglow - the design language: hand-overs, bones, cards and the page with nothing to show.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The pieces every screen package first wrote for itself, kept once here.

#include "afterglow/design.hpp"

#include <algorithm>
#include <cmath>

namespace afterglow
{

namespace
{

// The message of a page that has nothing to show (boards 37 and 38).
constexpr float kNoticeLine = 33.0f;
constexpr float kNoticeWidth = 680.0f;
constexpr float kNoticeDisc = 48.0f;

} // namespace

// ---- hand-overs -------------------------------------------------------------------

Motion hand_over(const Phase &phase, bool reduced)
{
    Motion m;
    const float in = tween::clamp01(phase.in);
    const float out = tween::clamp01(phase.out);
    if (out > 0.0f)
    {
        switch (phase.move)
        {
        case Move::push:
        case Move::pop:
            // Deeper and back out: the old page slides 90 pixels the way the move goes and fades.
            m.alpha = 1.0f - tween::cubic_in(out);
            m.dx = (phase.move == Move::push ? -90.0f : 90.0f) * tween::cubic_in(out);
            break;
        case Move::zoom:
        case Move::shrink:
            // A picture travels: everything else of this page is gone in the first 150 ms.
            m.furniture = 1.0f - tween::clamp01(out / 0.29f);
            break;
        case Move::fade:
            // A section change: the old page sinks back and is gone by 40 %.
            m.alpha = 1.0f - tween::clamp01(out / 0.4f);
            m.scale = 1.0f - 0.025f * tween::clamp01(out / 0.4f);
            break;
        case Move::cut:
            m.alpha = 1.0f - out;
            break;
        }
    }
    else if (in < 1.0f)
    {
        switch (phase.move)
        {
        case Move::push:
        case Move::pop:
            m.alpha = tween::clamp01(in * 2.2f);
            m.dx = (phase.move == Move::push ? 110.0f : -110.0f) * (1.0f - tween::quint_out(in));
            break;
        case Move::zoom:
            // The page's words and buttons start 160 ms behind the picture.
            m.furniture = tween::clamp01((in - 0.31f) / 0.5f);
            break;
        case Move::shrink:
            // The page the picture returns to is there again under it.
            m.furniture = tween::clamp01(in / 0.5f);
            break;
        case Move::fade:
            m.alpha = tween::clamp01((in - 0.3f) / 0.5f);
            break;
        case Move::cut:
            m.alpha = in;
            break;
        }
    }
    if (reduced)
    {
        m.dx = 0.0f;
        m.scale = 1.0f;
    }
    return m;
}

HandOver::HandOver(G &g, const Phase &phase, bool keeps_picture) : motion(hand_over(phase, g.reduced)), l_(g.l)
{
    alpha = keeps_picture ? motion.alpha : motion.alpha * motion.furniture;
    l_.push_opacity(alpha);
    l_.push_transform(motion.scale, gfx::kVirtualWidth * 0.5f, gfx::kVirtualHeight * 0.5f, motion.dx, 0.0f);
}

HandOver::~HandOver()
{
    l_.pop_transform();
    l_.pop_opacity();
}

void wear(ui::ComponentStyle &style, const Look &look, bool reduced)
{
    style.theme = look.theme;
    style.sounds.set = look.theme.sounds;
    style.reduced_motion = reduced;
}

// ---- states ------------------------------------------------------------------------

void card_bone(G &g, const Rect &r, bool with_text)
{
    bone(g, r, kRadiusThumb);
    if (!with_text)
        return;
    bone(g, {r.x + 2.0f, r.y + r.h + 20.0f, r.w * 0.88f, 20.0f}, 10.0f);
    bone(g, {r.x + 2.0f, r.y + r.h + 50.0f, r.w * 0.6f, 20.0f}, 10.0f);
    bone(g, {r.x + 2.0f, r.y + r.h + 82.0f, r.w * 0.72f, 16.0f}, 8.0f);
}

void spinner(G &g, float cx, float cy, float radius, Color color)
{
    const float thickness = std::max(2.5f, radius * 0.22f);
    g.l.ring(cx, cy, radius, thickness, g.k.raised);
    if (g.reduced)
    {
        // Still, and breathing: something is being waited for all the same.
        g.l.arc(cx, cy, radius, thickness, -1.5708f, 4.2f, color.with_alpha(color.a * (0.55f + 0.45f * ui::breathe(g.time, 2.4f))), true);
        return;
    }
    const float turn = g.time * 5.2f;
    const float sweep = 2.2f + 1.5f * ui::breathe(g.time, 1.9f);
    g.l.arc(cx, cy, radius, thickness, turn, sweep, color, true);
}

void foot_scrim(G &g)
{
    g.l.gradient_rect({kRail, 944.0f, gfx::kVirtualWidth - kRail, 60.0f}, 0.0f, g.k.base.with_alpha(0.0f), g.k.base.with_alpha(0.95f));
    g.l.rounded_rect({kRail, 1004.0f, gfx::kVirtualWidth - kRail, 76.0f}, 0.0f, g.k.base.with_alpha(0.95f));
}

void picture_box(G &g, const Rect &r, float radius, const Picture &picture, Color tint)
{
    const float shown = picture.ready() ? (g.reduced ? 1.0f : tween::clamp01(picture.age / 0.25f)) : 0.0f;
    if (shown < 1.0f)
        picture_placeholder(g, r, radius);
    if (shown > 0.0f)
        g.l.image(picture.texture, r, picture.cover(r.w / r.h), tint.with_alpha(shown), radius);
}

void glyph(G &g, ui::Button button, float cx, float cy, float size)
{
    const ui::Hint one{button, ""};
    ui::HintLayout layout;
    layout.size = size;
    layout.text_size = 1.0f;
    layout.cy = cy;
    layout.item_gap = 0.0f;
    ui::GlyphStyle style = g.k.dark ? ui::GlyphStyle::dark() : ui::GlyphStyle::light();
    const float width = ui::measure_hints(g.f, &one, 1, layout);
    ui::draw_hints(g.l, g.f, style, &one, 1, cx - width * 0.5f, false, layout);
}

// ---- the cards beside the video card ------------------------------------------------

void playlist_card(G &g, const Rect &r, const CardText &card, const Picture &picture, float focus, float alpha)
{
    if (alpha <= 0.01f)
        return;
    const float f = tween::clamp01(focus);
    g.l.push_opacity(tween::clamp01(alpha));
    // The pictures behind the first one, as two edges above it.
    const Color tone = picture.ready() ? picture.tone : gfx::mix(g.k.base2, g.k.ink, 0.22f);
    g.l.rounded_rect({r.x + 28.0f, r.y - 16.0f, r.w - 56.0f, 30.0f}, 12.0f, tone.with_alpha(0.5f));
    g.l.rounded_rect({r.x + 14.0f, r.y - 8.0f, r.w - 28.0f, 30.0f}, 14.0f, tone.with_alpha(0.85f));
    if (f > 0.01f)
        g.l.shadow({r.x, r.y + 20.0f, r.w, r.h}, kRadiusThumb, 40.0f, kBlack.with_alpha(0.55f * f));
    const float shown = picture.ready() ? (g.reduced ? 1.0f : tween::clamp01(picture.age / 0.25f)) : 0.0f;
    if (shown < 1.0f)
        picture_placeholder(g, r, kRadiusThumb);
    if (shown > 0.0f)
    {
        const float dim = 0.86f + 0.14f * f;
        g.l.image(picture.texture, r, picture.cover(r.w / r.h), Color{dim, dim, dim, shown}, kRadiusThumb);
    }
    g.l.bordered_rect(r, kRadiusThumb, clear(), 1.5f, kWhite.with_alpha(0.1f));
    const Rect side{r.x + r.w - 132.0f, r.y, 132.0f, r.h};
    g.l.rounded_rect(side, kRadiusThumb, kBlack.with_alpha(0.66f));
    icon(g, Icon::queue, side.cx(), side.cy() - (card.count.empty() ? 0.0f : 18.0f), 34.0f, kWhite);
    if (!card.count.empty())
        text(g, g.f.semibold, fit(g.f.semibold, card.count, kTypeMeta, side.w - 8.0f), side.cx(), side.cy() + 32.0f, kTypeMeta, kWhite,
             gfx::Align::center);
    const float after = paragraph(g, g.f.semibold, card.title, r.x + 2.0f, r.y + r.h + 36.0f, kTypeCard, r.w - 4.0f, 30.0f,
                                  g.k.ink.with_alpha(0.82f + 0.18f * f), 2);
    if (!card.line.empty())
        text(g, g.f.regular, fit(g.f.regular, card.line, kTypeMeta, r.w - 4.0f), r.x + 2.0f, after, kTypeMeta, g.k.ink2);
    g.l.pop_opacity();
}

void folder_card(G &g, const Rect &r, const CardText &card, const Picture &picture, float focus, float alpha)
{
    if (alpha <= 0.01f)
        return;
    const float f = tween::clamp01(focus);
    g.l.push_opacity(tween::clamp01(alpha));
    if (f > 0.01f)
        lift(g, r, kRadiusPanel, f);
    const float shown = picture.ready() ? (g.reduced ? 1.0f : tween::clamp01(picture.age / 0.25f)) : 0.0f;
    Color ink = g.k.ink;
    if (shown < 1.0f)
        glass_panel(g, r, kRadiusPanel, 1.0f + 0.2f * f);
    if (shown > 0.0f)
    {
        // Over a picture the words are white on a shade, whatever the look.
        const float dim = 0.5f + 0.12f * f;
        g.l.image(picture.texture, r, picture.cover(r.w / r.h), Color{dim, dim, dim, shown}, kRadiusPanel);
        g.l.gradient_rect(r, kRadiusPanel, kBlack.with_alpha(0.0f), kBlack.with_alpha(0.55f * shown));
        g.l.bordered_rect(r, kRadiusPanel, clear(), 1.5f, kWhite.with_alpha(0.12f * shown));
        ink = gfx::mix(ink, kWhite, shown);
    }
    icon(g, card.icon, r.x + 46.0f, r.y + 48.0f, 36.0f, ink);
    icon(g, Icon::chevron_right, r.x + r.w - 40.0f, r.y + 48.0f, 26.0f, ink.with_alpha(0.6f + 0.4f * f));
    // The title sits at the tile's foot: two lines grow upward.
    const TextBlock block = measure_block(g.f.semibold, card.title, kTypeShelf, r.w - 64.0f, 34.0f, 2);
    paragraph(g, g.f.semibold, card.title, r.x + 32.0f, r.y + r.h - 30.0f - 34.0f * static_cast<float>(std::max(1, block.lines) - 1), kTypeShelf,
              r.w - 64.0f, 34.0f, ink, 2);
    if (!card.line.empty())
        text(g, g.f.regular, fit(g.f.regular, card.line, kTypeMeta, r.w - 4.0f), r.x + 2.0f, r.y + r.h + 36.0f, kTypeMeta, g.k.ink2);
    g.l.pop_opacity();
}

void channel_card(G &g, const Rect &r, const CardText &card, const Picture &picture, float focus, float alpha)
{
    if (alpha <= 0.01f)
        return;
    const float f = tween::clamp01(focus);
    g.l.push_opacity(tween::clamp01(alpha));
    if (f > 0.01f)
        g.l.shadow({r.x, r.y + 20.0f, r.w, r.h}, kRadiusPanel, 40.0f, kBlack.with_alpha(0.5f * f));
    glass_panel(g, r, kRadiusPanel, 1.0f + 0.2f * f);
    avatar(g, r.cx(), r.y + 86.0f, 60.0f, card.title, card.id, picture);
    text(g, g.f.semibold, fit(g.f.semibold, card.title, kTypeShelf, r.w - 40.0f), r.cx(), r.y + 190.0f, kTypeShelf, g.k.ink, gfx::Align::center);
    text(g, g.f.regular, fit(g.f.regular, card.line, kTypeMeta, r.w - 40.0f), r.cx(), r.y + 222.0f, kTypeMeta, g.k.ink2, gfx::Align::center);
    if (card.marked)
    {
        // Selected is said with a mark, not with the focus.
        const char *label = tr("Subscribed");
        const float label_w = std::min(measure(g.f.semibold, label, kTypeMeta), r.w - 120.0f);
        const Rect pill{r.cx() - label_w * 0.5f - 34.0f, r.y + 244.0f, label_w + 68.0f, 44.0f};
        g.l.rounded_rect(pill, 22.0f, g.k.raised);
        g.l.bordered_rect(pill, 22.0f, clear(), 1.5f, g.k.hairline);
        icon(g, Icon::check, pill.x + 26.0f, pill.cy(), 20.0f, g.k.accent);
        text(g, g.f.semibold, fit(g.f.semibold, label, kTypeMeta, label_w + 2.0f), pill.x + 44.0f, pill.cy() + 7.0f, kTypeMeta, g.k.ink);
    }
    g.l.pop_opacity();
}

// ---- the page that has nothing to show ----------------------------------------------

void Notice::lay_out(const ui::Fonts &fonts, float centre, float y, std::string_view body, const Choice *choices, int choice_count)
{
    cx = centre;
    top = y;
    count = std::clamp(choice_count, 0, 2);
    lines = body.empty() ? 0 : measure_block(fonts.regular, body, kTypeBody, kNoticeWidth, kNoticeLine, 3).lines;
    float widths[2] = {0.0f, 0.0f};
    float total = 0.0f;
    for (int i = 0; i < count; ++i)
    {
        widths[i] = std::max(220.0f, button_width(fonts, choices[i].label));
        total += widths[i] + (i > 0 ? 20.0f : 0.0f);
    }
    float x = cx - total * 0.5f;
    const float row = top + 190.0f + static_cast<float>(lines) * kNoticeLine;
    for (int i = 0; i < count; ++i)
    {
        buttons[static_cast<std::size_t>(i)] = {x, row, widths[i], 60.0f};
        x += widths[i] + 20.0f;
    }
}

float Notice::height() const
{
    return 190.0f + static_cast<float>(lines) * kNoticeLine + (count > 0 ? 60.0f : -30.0f);
}

void Notice::draw(G &g, Icon mark, std::string_view title, std::string_view body, const Choice *choices, float focus_at, bool focused) const
{
    g.l.circle(cx, top + kNoticeDisc, kNoticeDisc, g.k.raised);
    icon(g, mark, cx, top + kNoticeDisc, 50.0f, g.k.ink);
    text(g, g.f.semibold, fit(g.f.semibold, title, kTypeMessage, 1200.0f), cx, top + 142.0f, kTypeMessage, g.k.ink, gfx::Align::center);
    if (!body.empty())
        paragraph(g, g.f.regular, body, cx, top + 181.0f, kTypeBody, kNoticeWidth, kNoticeLine, g.k.ink2, 3, gfx::Align::center);
    if (count == 1)
    {
        // One way on is said as the boards say it: the button that takes it and its words.
        const Rect &r = buttons[0];
        const float label_w = measure(g.f.semibold, choices[0].label, kTypeBody);
        const float x = r.cx() - (label_w + 44.0f) * 0.5f;
        glyph(g, ui::Button::cross, x + 17.0f, r.cy(), 34.0f);
        text(g, g.f.semibold, choices[0].label, x + 44.0f, r.cy() + 8.5f, kTypeBody, focused ? g.k.ink : g.k.ink2);
        return;
    }
    for (int i = 0; i < count; ++i)
    {
        const Rect &r = buttons[static_cast<std::size_t>(i)];
        const float focus = focused ? tween::clamp01(1.0f - std::fabs(focus_at - static_cast<float>(i))) : 0.0f;
        button(g, r, choices[i].label, choices[i].icon, focus, i == 0);
        if (focus > 0.5f)
            focus_ring(g, grown(r, 0.035f * focus), r.h * 0.5f, g.k.focus, focus);
    }
}

const char *trouble_title(model::Load load)
{
    return load == model::Load::offline ? tr("No connection to YouTube") : tr("This could not be loaded");
}

Icon trouble_icon(model::Load load)
{
    return load == model::Load::offline ? Icon::offline : Icon::alert;
}

} // namespace afterglow
