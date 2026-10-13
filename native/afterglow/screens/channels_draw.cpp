// ProsperoTube Afterglow - the channels a profile follows, as tiles, drawn.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Board 28 of the proposal: the order chips in the header, the tiles, and the notice of an account that cannot be reached above them.

#include "afterglow/screens/channels.hpp"

namespace afterglow::channels_page
{

void Channels::draw(const DrawContext &context, const Phase &phase) const
{
    G g = context.g();
    Travel travel(g, phase);
    draw_head(g);
    switch (mode_)
    {
    case Mode::loading:
        draw_bones(g);
        break;
    case Mode::empty:
    case Mode::invite:
    case Mode::trouble:
    {
        Enter enter(g, arrive(age_, 2, 0.07f, 0.55f), 26.0f);
        notice_.draw(g, notice_icon(), notice_title(), notice_body(), choices_, choice_pos_.value, zone_ == Zone::notice && !sheet_.open());
        break;
    }
    default:
        draw_tiles(g, context);
        break;
    }
    foot_scrim(g);
    sheet_.draw(g);
}

void Channels::draw_head(G &g) const
{
    Enter enter(g, arrive(age_, 0, 0.05f, 0.5f), -14.0f);
    if (!subtitle_.empty())
        text(g, g.f.regular, subtitle_, kLeft, 166.0f, 22.0f, g.k.ink2);
    const float waiting = g.reduced ? 0.7f : 0.5f + 0.4f * ui::breathe(g.time, 0.9f);
    for (std::size_t i = 0; i < chips_.size(); ++i)
    {
        const Chip &each = chips_[i];
        const float focus = tween::clamp01(1.0f - std::fabs(chip_pos_.value - static_cast<float>(i))) * order_focus_.value;
        // The order that was asked for waits, dimmed, until the provider has taken it.
        g.l.push_opacity(doing_ == 1 && each.number == wanted_ ? waiting : 1.0f);
        chip(g, each.rect.x, each.rect.cy(), order_label(each.number), each.selected, focus);
        g.l.pop_opacity();
    }
}

void Channels::draw_bones(G &g) const
{
    Enter enter(g, arrive(age_, 1, 0.05f, 0.4f), 18.0f);
    bone(g, {kLeft, 148.0f, 360.0f, 20.0f}, 10.0f);
    for (int i = 0; i < kAcross * 2; ++i)
    {
        const Rect r = tile_rect(i, 0.0f);
        g.l.bordered_rect(r, kRadiusPanel, clear(), 1.5f, g.k.hairline);
        bone(g, {r.cx() - 74.0f, r.y + 36.0f, 148.0f, 148.0f}, 74.0f);
        bone(g, {r.cx() - 84.0f, r.y + 212.0f, 168.0f, 22.0f}, 11.0f);
        bone(g, {r.cx() - 60.0f, r.y + 246.0f, 120.0f, 16.0f}, 8.0f);
    }
}

void Channels::draw_tiles(G &g, const DrawContext &context) const
{
    const float scroll = scroll_.value;
    const float top = tile_rect(0, 0.0f).y;
    if (mode_ == Mode::kept)
    {
        // The account cannot be reached; what the console keeps is still here.
        Enter enter(g, arrive(age_, 1, 0.06f, 0.5f), -30.0f);
        const Rect banner{kLeft, kBannerY, kRight - kLeft, kBannerH};
        glass_panel(g, banner, 24.0f);
        g.l.rounded_rect({banner.x, banner.y, 8.0f, banner.h}, 4.0f, g.k.theme.warning);
        icon(g, Icon::offline, banner.x + 48.0f, banner.cy(), 28.0f, g.k.theme.warning);
        const char *title = account_.load == model::Load::offline ? tr("Offline") : tr("Not loaded");
        const float used = text(g, g.f.semibold, title, banner.x + 84.0f, banner.cy() + 8.5f, kTypeBody, g.k.ink);
        const std::string why = account_.error.empty() ? std::string(tr("Your subscriptions could not be read.")) : account_.error;
        text(g, g.f.regular, fit(g.f.regular, why, 22.0f, banner.w - 84.0f - used - 60.0f), banner.x + 84.0f + used + 20.0f, banner.cy() + 8.0f, 22.0f,
             g.k.ink2);
    }
    const bool clipped = scroll > 0.5f;
    if (clipped)
        g.l.push_clip({0.0f, top - 20.0f, gfx::kVirtualWidth, gfx::kVirtualHeight - top + 20.0f});
    const Rect ring = ring_.value();
    const int count = static_cast<int>(tiles_.size());
    const float zone_focus = tiles_focus_.value;
    const bool local = uses_local();
    g.l.push_transform(1.0f, 0.0f, 0.0f, 0.0f, -scroll);
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int i = 0; i < count; ++i)
        {
            if ((pass == 1) != (i == focus_ && zone_focus > 0.01f))
                continue;
            const Rect base = tile_rect(i, 0.0f);
            if (base.y - scroll > gfx::kVirtualHeight || base.y - scroll + base.h < 0.0f)
                continue;
            const Tile &tile = tiles_[static_cast<std::size_t>(i)];
            const float focus = under_ring(ring, base, 200.0f) * zone_focus;
            Enter enter(g, arrive(age_, 2 + i % kAcross + std::min(i / kAcross, 2), 0.05f, 0.45f), 26.0f);
            const Rect r = grown(base, 0.04f * focus, 4.0f * focus);
            if (focus > 0.01f)
                lift(g, r, kRadiusPanel, focus);
            glass_panel(g, r, kRadiusPanel);
            const Picture picture = context.images.get(tile.channel.avatar, PictureSize::avatar);
            avatar(g, r.cx(), r.y + 110.0f, 74.0f, tile.channel.name, tile.channel.id, picture);
            text(g, g.f.semibold, fit(g.f.semibold, tile.channel.name, 23.0f, r.w - 32.0f), r.cx(), r.y + 230.0f, 23.0f, g.k.ink, gfx::Align::center);
            const std::string line = local ? std::string(tr("On this PS5")) : tile.line;
            text(g, g.f.regular, fit(g.f.regular, line, 19.0f, r.w - 32.0f), r.cx(), r.y + 258.0f, 19.0f, g.k.ink3, gfx::Align::center);
            if (tile.live)
            {
                const char *label = tr("Live now");
                tag(g, r.cx() - tag_width(g, label) * 0.5f, r.y + 298.0f, label, g.k.accent, g.k.on_accent);
            }
            else if (tile.unseen > 0 && marks_.value > 0.01f)
            {
                const std::string label = fill(tr("{0} new"), std::to_string(tile.unseen));
                const float asked = doing_ == 2 ? (g.reduced ? 0.6f : 0.45f + 0.4f * ui::breathe(g.time, 0.9f)) : 1.0f;
                g.l.push_opacity(marks_.value * asked);
                tag(g, r.cx() - tag_width(g, label) * 0.5f, r.y + 298.0f, label, g.k.raised, g.k.ink);
                g.l.pop_opacity();
            }
            if (i == focus_ && focus > 0.01f)
            {
                Rect lit = ring;
                lit.x += ui::shake(nudge_.value, g.time, 14.0f, 8.0f);
                focus_ring(g, lit, kRadiusPanel, picture.ready() ? picture.glow : g.k.focus, zone_focus);
            }
        }
    }
    if (list().fetching_more())
    {
        const int from = (count + kAcross - 1) / kAcross * kAcross;
        for (int i = 0; i < kAcross; ++i)
        {
            const Rect r = tile_rect(from + i, 0.0f);
            g.l.bordered_rect(r, kRadiusPanel, clear(), 1.5f, g.k.hairline);
            bone(g, {r.cx() - 74.0f, r.y + 36.0f, 148.0f, 148.0f}, 74.0f);
        }
    }
    g.l.pop_transform();
    if (clipped)
        g.l.pop_clip();
}

} // namespace afterglow::channels_page
