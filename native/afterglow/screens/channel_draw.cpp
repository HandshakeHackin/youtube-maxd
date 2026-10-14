// ProsperoTube Afterglow - the channel page, drawn.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Boards 11 and 12 of the proposal: the banner with the avatar standing on its
// edge, the name and the buttons, the tabs, and under them the section in
// view: Home as a featured video beside the latest uploads, every other
// section as a grid.

#include "afterglow/screens/channel.hpp"

namespace afterglow::channel_page
{

void Channel::draw(const DrawContext &context, const Phase &phase) const
{
    G g = context.g();
    HandOver travel(g, phase);
    if (!head_.known)
    {
        if (base_.load == model::Load::failed || base_.load == model::Load::offline)
        {
            const Choice choices[1] = {{tr("Try again"), Icon::update}};
            Enter enter(g, arrive(age_, 1), 26.0f);
            trouble_.draw(g, trouble_icon(base_.load), trouble_title(base_.load),
                          base_.error.empty() ? tr("The channel did not arrive. Check the connection and try again.") : base_.error.c_str(), choices,
                          0.0f, true);
        }
        else
        {
            draw_bones(g);
        }
        return;
    }
    draw_content(g, context);
    draw_head(g, context);
    draw_tabs(g, context);
    foot_scrim(g);
    draw_sort_menu(g);
}

void Channel::draw_bones(G &g) const
{
    Enter enter(g, arrive(age_, 0, 0.05f, 0.4f), 18.0f);
    bone(g, kBanner, 24.0f);
    g.l.circle(kAvatarX, kAvatarY, 74.0f, g.k.base.with_alpha(0.9f));
    bone(g, {kAvatarX - 66.0f, kAvatarY - 66.0f, 132.0f, 132.0f}, 66.0f);
    bone(g, {kNameX, 304.0f, 420.0f, 34.0f}, 17.0f);
    bone(g, {kNameX + 2.0f, 356.0f, 520.0f, 20.0f}, 10.0f);
    bone(g, {kRight - 60.0f, kActionY, 60.0f, kActionH}, 30.0f);
    bone(g, {kRight - 60.0f - 16.0f - 220.0f, kActionY, 220.0f, kActionH}, 30.0f);
    for (int i = 0; i < 4; ++i)
        bone(g, {kLeft + 142.0f * static_cast<float>(i), kTabsY + 16.0f, 104.0f, 24.0f}, 12.0f);
    draw_home_bones(g);
}

void Channel::draw_home_bones(G &g) const
{
    bone(g, kFeatured, kRadiusThumb);
    bone(g, {kFeaturedTextX, 524.0f, 130.0f, 16.0f}, 8.0f);
    bone(g, {kFeaturedTextX, 562.0f, kFeaturedTextW, 30.0f}, 15.0f);
    bone(g, {kFeaturedTextX, 606.0f, kFeaturedTextW * 0.7f, 30.0f}, 15.0f);
    bone(g, {kFeaturedTextX, 716.0f, 260.0f, 18.0f}, 9.0f);
    for (int i = 0; i < 3; ++i)
        bone(g, {kFeaturedTextX, 760.0f + 32.0f * static_cast<float>(i), kFeaturedTextW * (i == 2 ? 0.6f : 1.0f), 18.0f}, 9.0f);
    bone(g, {kLatestX, 520.0f, 200.0f, 22.0f}, 11.0f);
    for (int i = 0; i < kLatestShown; ++i)
    {
        const float y = kLatestY + kLatestPitch * static_cast<float>(i);
        bone(g, {kLatestX, y, 160.0f, 90.0f}, 12.0f);
        bone(g, {kLatestX + 178.0f, y + 10.0f, 330.0f, 18.0f}, 9.0f);
        bone(g, {kLatestX + 178.0f, y + 38.0f, 250.0f, 18.0f}, 9.0f);
        bone(g, {kLatestX + 178.0f, y + 68.0f, 180.0f, 14.0f}, 7.0f);
    }
}

// The banner, the name and the buttons: they rise and fade as a grid scrolls under the tabs.
void Channel::draw_head(G &g, const DrawContext &context) const
{
    const float scroll = scroll_.value;
    const float shown = 1.0f - tween::clamp01(scroll / 170.0f);
    if (shown > 0.01f)
    {
        g.l.push_opacity(shown);
        g.l.push_transform(1.0f, 0.0f, 0.0f, 0.0f, g.reduced ? 0.0f : -scroll * 0.6f);
        {
            Enter enter(g, arrive(age_, 0, 0.05f, 0.6f), -18.0f);
            const Picture picture = context.images.get(head_.banner, PictureSize::hero);
            const float in = picture.ready() ? (g.reduced ? 1.0f : tween::clamp01(picture.age / 0.3f)) : 0.0f;
            lift(g, kBanner, 24.0f, 0.6f);
            picture_box(g, kBanner, 24.0f, picture);
            g.l.gradient_rect(kBanner, 24.0f, kBlack.with_alpha(0.0f), kBlack.with_alpha(0.42f * in));
            // The clock stands on the banner's corner: the picture gives way to the page there, so the time reads in every look.
            g.l.gradient_rect_h({kBanner.x + kBanner.w - 300.0f, kBanner.y, 300.0f, kBanner.h}, 24.0f, g.k.base.with_alpha(0.0f),
                                g.k.base.with_alpha((g.k.dark ? 0.34f : 0.78f) * in));
            g.l.bordered_rect(kBanner, 24.0f, clear(), 1.5f, kWhite.with_alpha(0.12f));
        }
        {
            Enter enter(g, arrive(age_, 1, 0.06f, 0.5f), 18.0f);
            g.l.circle(kAvatarX, kAvatarY, 74.0f, g.k.base.with_alpha(0.9f));
            avatar(g, kAvatarX, kAvatarY, 66.0f, head_.name, id_, context.images.get(head_.avatar, PictureSize::avatar));
            text(g, g.f.display, fit(g.f.display, head_.name, kTypePage, name_width_), kNameX, 338.0f, kTypePage, g.k.ink);
            const std::string line = dotted({head_.handle, head_.subscribers, head_.videos});
            text(g, g.f.regular, fit(g.f.regular, line, 22.0f, name_width_), kNameX + 2.0f, 374.0f, 22.0f, g.k.ink2);
        }
        {
            Enter enter(g, arrive(age_, 2, 0.06f, 0.5f), 18.0f);
            draw_buttons(g);
        }
        g.l.pop_transform();
        g.l.pop_opacity();
    }
    // Scrolled: the channel's name stays, small, where the page title of other screens is.
    const float small = tween::clamp01((scroll - 150.0f) / 90.0f);
    if (small > 0.01f)
    {
        g.l.push_opacity(small);
        avatar(g, kLeft + 26.0f, 104.0f, 26.0f, head_.name, id_, context.images.get(head_.avatar, PictureSize::avatar));
        text(g, g.f.display, fit(g.f.display, head_.name, kTypeTitle, 1100.0f), kLeft + 68.0f, 115.0f, kTypeTitle, g.k.ink);
        g.l.pop_opacity();
    }
}

void Channel::draw_buttons(G &g) const
{
    const float sub = sub_amount_.value;
    const float waiting = g.reduced ? 0.7f : 0.5f + 0.4f * ui::breathe(g.time, 0.9f);
    for (std::size_t i = 0; i < buttons_.size(); ++i)
    {
        const Slot &slot = buttons_[i];
        const float focus = tween::clamp01(1.0f - std::fabs(button_pos_.value - static_cast<float>(i))) * actions_focus_.value;
        Rect r = slot.rect;
        // Asked, not yet answered: the control that was pressed waits, dimmed.
        const bool asked = (slot.button == Button::subscribe && (doing_ == Doing::subscribe || doing_ == Doing::unsubscribe)) ||
                           (slot.button == Button::notify && doing_ == Doing::level);
        const bool answered = (slot.button == Button::subscribe && last_ != Doing::level && last_ != Doing::nothing) ||
                              (slot.button == Button::notify && last_ == Doing::level);
        if (answered)
        {
            r.x += ui::shake(shake_.value, g.time, 10.0f, 9.0f);
            if (pop_.value > 0.01f)
            {
                // Confirmed: the button swells and light goes out from it.
                g.l.glow(r, r.h * 0.5f, 40.0f * pop_.value, g.k.accent.with_alpha(0.5f * pop_.value));
                r = grown(r, 0.1f * pop_.value);
            }
        }
        g.l.push_opacity(asked ? waiting : 1.0f);
        switch (slot.button)
        {
        case Button::subscribe:
            button(g, r, slot.label, sub > 0.5f ? Icon::check : Icon::plus, focus, sub < 0.5f, sub > 0.5f);
            break;
        case Button::sign_in:
            button(g, r, slot.label, Icon::user, focus, true);
            break;
        case Button::notify:
            // The level is the account's to choose once it subscribes; until then it waits, dimmed.
            g.l.push_opacity(0.35f + 0.65f * sub);
            button(g, r, slot.label, Icon::bell, focus);
            g.l.pop_opacity();
            break;
        case Button::search:
            button(g, r, "", Icon::search, focus);
            break;
        }
        g.l.pop_opacity();
        if (focus > 0.5f)
            focus_ring(g, grown(r, 0.035f * focus), r.h * 0.5f, g.k.focus, focus);
    }
}

void Channel::draw_tabs(G &g, const DrawContext &context) const
{
    if (tabs_.empty())
        return;
    Enter enter(g, arrive(age_, 3, 0.06f, 0.5f), 18.0f);
    const float y = tabs_y(scroll_.value);
    ui::TabBar bar = bar_;
    bar.set_bounds({kLeft - 22.0f, y, 900.0f, kTabsH});
    ui::Canvas canvas{g.l, g.f, g.glass, g.time};
    bar.draw(canvas);
    if (tab().sorts.size() > 1 && sort_chip_.w > 0.0f)
    {
        // The order belongs to the section in view; it fades while another slides in.
        const float settled = tween::clamp01(1.0f - std::fabs(bar_.active_value() - static_cast<float>(active_)) * 2.0f);
        g.l.push_opacity(settled);
        const float cy = y + 28.0f;
        chip(g, kRight - sort_chip_.w, cy, sort_text(tab()), false, std::max(sort_focus_.value, sort_amount_.value), 44.0f, true);
        text(g, g.f.regular, tr("Sort by"), kRight - sort_chip_.w - 18.0f, cy + 8.0f, 21.0f, g.k.ink3, gfx::Align::right);
        g.l.pop_opacity();
    }
}

void Channel::draw_sort_menu(G &g) const
{
    const float open = tween::clamp01(sort_amount_.value);
    if (open <= 0.01f || !has_tab())
        return;
    const Rect menu = sort_menu();
    g.l.push_opacity(open);
    g.l.push_transform(1.0f, 0.0f, 0.0f, 0.0f, g.reduced ? 0.0f : -14.0f * (1.0f - open));
    lift(g, menu, 24.0f, 1.0f);
    glass_panel(g, menu, 24.0f, 1.5f);
    const Rect first = sort_row(0);
    g.l.rounded_rect({first.x, first.y + kSortRow * sort_pos_.value + 4.0f, first.w, first.h - 8.0f}, (first.h - 8.0f) * 0.5f, g.k.plate);
    for (std::size_t i = 0; i < tab().sorts.size(); ++i)
    {
        const Rect row = sort_row(static_cast<int>(i));
        const float focus = tween::clamp01(1.0f - std::fabs(sort_pos_.value - static_cast<float>(i)));
        const Color ink = gfx::mix(g.k.ink, g.k.on_plate, focus);
        text(g, g.f.semibold, fit(g.f.semibold, order_label(tab().sorts[i]), kTypeBody, row.w - 84.0f), row.x + 24.0f, row.cy() + 8.5f, kTypeBody, ink);
        if (static_cast<int>(i) == tab().sort)
            icon(g, Icon::check, row.x + row.w - 30.0f, row.cy(), 24.0f, gfx::mix(g.k.accent, g.k.on_plate, focus));
    }
    g.l.pop_transform();
    g.l.pop_opacity();
}

// Every section near the one in view: the content slides with the bar's indicator.
void Channel::draw_content(G &g, const DrawContext &context) const
{
    const float at = bar_.active_value();
    const float in = arrive(age_, 4, 0.06f, 0.5f);
    const float scroll = scroll_.value;
    const bool clipped = scroll > 0.5f; // a scrolled grid passes under the tabs
    if (clipped)
    {
        const float top = tabs_y(scroll) + kTabsH + 8.0f;
        g.l.push_clip({0.0f, top, gfx::kVirtualWidth, gfx::kVirtualHeight - top});
    }
    for (std::size_t i = 0; i < tabs_.size(); ++i)
    {
        const float distance = static_cast<float>(i) - at;
        const float visible = tween::clamp01(1.0f - std::fabs(distance) * 1.6f);
        if (visible <= 0.01f)
            continue;
        g.l.push_opacity(visible * in);
        g.l.push_transform(1.0f, 0.0f, 0.0f, g.reduced ? 0.0f : distance * 220.0f, (g.reduced ? 0.0f : 26.0f * (1.0f - in)) - scroll);
        const Tab &each = tabs_[i];
        const float focus = static_cast<int>(i) == active_ ? content_focus_.value * visible : 0.0f;
        const bool waiting = each.feed.load == model::Load::loading || each.feed.load == model::Load::idle;
        if (!each.feed.open || (waiting && each.cells.empty()))
        {
            if (each.home())
                draw_home_bones(g);
            else
                for (int cell = 0; cell < kColumns * 2; ++cell)
                    card_bone(g, cell_rect(each, cell, false), cell < kColumns);
        }
        else if (each.cells.empty())
            draw_nothing(g, each, focus);
        else if (each.home())
            draw_home(g, context, each, focus);
        else
            draw_grid(g, context, each, focus);
        g.l.pop_transform();
        g.l.pop_opacity();
    }
    if (clipped)
        g.l.pop_clip();
}

// A section with nothing in it, or one that did not arrive.
void Channel::draw_nothing(G &g, const Tab &of, float focus) const
{
    const bool trouble = tab_trouble(of);
    icon(g, trouble ? trouble_icon(of.feed.load) : Icon::info, kLeft + 24.0f, kContentY + 22.0f, 44.0f, g.k.ink2);
    const char *title = trouble ? trouble_title(of.feed.load) : tr("Nothing here yet");
    text(g, g.f.semibold, fit(g.f.semibold, title, kTypeShelf, 1200.0f), kLeft + 64.0f, kContentY + 31.0f, kTypeShelf, g.k.ink);
    const std::string body = trouble ? (of.feed.error.empty() ? std::string(tr("This section did not arrive.")) : of.feed.error)
                                     : fill(tr("{0} has nothing in this section."), head_.name);
    paragraph(g, g.f.regular, body, kLeft + 64.0f, kContentY + 68.0f, kTypeBody, 1100.0f, 34.0f, g.k.ink2, 1);
    if (!trouble)
        return;
    button(g, retry_, tr("Try again"), Icon::update, focus, true);
    if (focus > 0.5f)
        focus_ring(g, grown(retry_, 0.035f * focus), retry_.h * 0.5f, g.k.focus, focus);
}

void Channel::draw_home(G &g, const DrawContext &context, const Tab &of, float zone_focus) const
{
    const Rect ring = ring_.value();
    const Cell &first = of.cells.front();
    {
        const float focus = under_ring(ring, kFeatured, 300.0f) * zone_focus;
        const Rect big = grown(kFeatured, 0.025f * focus, 4.0f * focus);
        const Picture picture = context.images.get(first.video ? first.v.thumb : first.thumb, PictureSize::hero);
        if (first.video)
            video_card(g, big, first.v, picture, focus, 1.0f, 0);
        else
            draw_playlist(g, big, first, picture, focus, false);
        overline(g, tr("Featured"), kFeaturedTextX, 540.0f, g.k.accent);
        paragraph(g, g.f.display, first.video ? first.v.title : first.title, kFeaturedTextX - 2.0f, 590.0f, 34.0f, kFeaturedTextW, 44.0f, g.k.ink, 3);
        const std::string meta = first.video ? dotted({first.v.views, first.v.age}) : first.count;
        text(g, g.f.regular, fit(g.f.regular, meta, 21.0f, kFeaturedTextW), kFeaturedTextX, 734.0f, 21.0f, g.k.ink2);
        if (first.video && !first.v.overview.empty())
            paragraph(g, g.f.regular, first.v.overview, kFeaturedTextX, 778.0f, 22.0f, kFeaturedTextW, 32.0f, g.k.ink2, 3);
        if (of.focus == 0 && focus > 0.01f)
            focus_ring(g, nudged(g, ring), kRadiusThumb, picture.ready() ? picture.glow : g.k.focus, focus);
    }
    if (of.cells.size() < 2)
        return;
    text(g, g.f.semibold, tr("Latest uploads"), kLatestX, 540.0f, kTypeShelf, g.k.ink);
    // Three rows and the focus plate around them; a row that has scrolled out is gone whole.
    const Rect window{kLatestX - 30.0f, kLatestY - 13.0f, kRight - kLatestX + 60.0f, kLatestPitch * static_cast<float>(kLatestShown) + 12.0f};
    g.l.push_clip(window);
    if (of.focus >= 1 && zone_focus > 0.01f)
    {
        // In a list the focus is a plate that glides under the row.
        const Rect plate = nudged(g, ring);
        g.l.push_opacity(zone_focus);
        glass_panel(g, plate, 18.0f);
        g.l.bordered_rect(plate.inset(-4.0f), 22.0f, clear(), g.k.theme.focus_width + 0.5f, g.k.focus);
        g.l.pop_opacity();
    }
    for (std::size_t i = 1; i < of.cells.size(); ++i)
    {
        const Cell &cell = of.cells[i];
        const Rect row = cell_rect(of, static_cast<int>(i), false);
        if (row.y + row.h < window.y || row.y > window.y + window.h)
            continue;
        const float focus = of.focus >= 1 ? tween::clamp01(1.0f - std::fabs(ring.cy() - row.cy()) / kLatestPitch) * zone_focus : 0.0f;
        const Rect thumb{kLatestX, row.y + 8.0f, 160.0f, 90.0f};
        picture_box(g, thumb, 12.0f, context.images.get(cell.video ? cell.v.thumb : cell.thumb, PictureSize::card));
        const float width = kRight - kLatestX - 178.0f - (of.cells.size() > kLatestShown + 1 ? 18.0f : 0.0f);
        paragraph(g, g.f.semibold, cell.video ? cell.v.title : cell.title, kLatestX + 178.0f, thumb.y + 28.0f, 21.0f, width, 27.0f,
                  g.k.ink.with_alpha(0.86f + 0.14f * focus), 2);
        const std::string meta = cell.video ? dotted({cell.v.views, cell.v.age}) : cell.count;
        text(g, g.f.regular, fit(g.f.regular, meta, 19.0f, width), kLatestX + 178.0f, thumb.y + 82.0f, 19.0f, g.k.ink3);
    }
    g.l.pop_clip();
    // More uploads than show: a thumb says how far the list has been read.
    const int rows = static_cast<int>(of.cells.size()) - 1;
    if (rows > kLatestShown)
    {
        const float height = kLatestPitch * static_cast<float>(kLatestShown) - 22.0f;
        const Rect track{kRight - 5.0f, kLatestY + 4.0f, 5.0f, height};
        const float thumb = height * static_cast<float>(kLatestShown) / static_cast<float>(rows);
        const float limit = kLatestPitch * static_cast<float>(rows - kLatestShown);
        g.l.rounded_rect(track, 2.5f, g.k.raised);
        g.l.rounded_rect({track.x, track.y + (height - thumb) * tween::clamp01(of.list_scroll.value / limit), 5.0f, thumb}, 2.5f, g.k.ink3);
    }
}

void Channel::draw_grid(G &g, const DrawContext &context, const Tab &of, float zone_focus) const
{
    const Rect ring = ring_.value();
    const int count = static_cast<int>(of.cells.size());
    const float scroll = scroll_.value;
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int i = 0; i < count; ++i)
        {
            if ((pass == 1) != (i == of.focus && zone_focus > 0.01f))
                continue;
            const Rect r = cell_rect(of, i, false);
            const float y = r.y - scroll;
            if (y > gfx::kVirtualHeight || y + r.h + 120.0f < 0.0f)
                continue;
            const Cell &cell = of.cells[static_cast<std::size_t>(i)];
            const float focus = under_ring(ring, r, 260.0f) * zone_focus;
            const Rect big = grown(r, 0.05f * focus, 5.0f * focus);
            // A row that only peeks over the foot shows its pictures, never half a text block.
            const bool whole = y + r.h + 104.0f <= kPageBottom;
            const Picture picture = context.images.get(cell.video ? cell.v.thumb : cell.thumb, PictureSize::card);
            Enter enter(g, arrive(age_, 5 + i % kColumns, 0.045f, 0.45f), 26.0f);
            if (cell.video)
                video_card(g, big, cell.v, picture, focus, whole ? 1.0f : 0.75f, whole ? 2 : 0);
            else
                draw_playlist(g, big, cell, picture, focus, whole);
            if (i == of.focus && focus > 0.01f)
                focus_ring(g, nudged(g, ring), kRadiusThumb, picture.ready() ? picture.glow : g.k.focus, zone_focus);
        }
    }
    if (of.feed.fetching_more())
    {
        // The next page is on its way: its row waits as bones where it will land.
        const int from = (count + kColumns - 1) / kColumns * kColumns;
        for (int i = 0; i < kColumns; ++i)
        {
            const Rect r = cell_rect(of, from + i, false);
            card_bone(g, r, r.y - scroll + r.h + 104.0f <= kPageBottom);
        }
    }
}

// A playlist: its first picture with the others stacked behind it.
void Channel::draw_playlist(G &g, const Rect &r, const Cell &cell, const Picture &picture, float focus, bool whole) const
{
    g.l.push_opacity(whole ? 1.0f : 0.75f);
    const Color tone = picture.ready() ? picture.tone : g.k.raised;
    g.l.rounded_rect({r.x + 28.0f, r.y - 16.0f, r.w - 56.0f, 30.0f}, 12.0f, tone.with_alpha(0.5f));
    g.l.rounded_rect({r.x + 14.0f, r.y - 8.0f, r.w - 28.0f, 30.0f}, 14.0f, tone.with_alpha(0.85f));
    if (focus > 0.01f)
        g.l.shadow({r.x, r.y + 20.0f, r.w, r.h}, kRadiusThumb, 40.0f, kBlack.with_alpha(0.55f * focus));
    picture_box(g, r, kRadiusThumb, picture, Color{0.9f, 0.9f, 0.9f, 1.0f});
    const Rect side{r.x + r.w - 132.0f, r.y, 132.0f, r.h};
    g.l.rounded_rect(side, kRadiusThumb, kBlack.with_alpha(0.66f));
    icon(g, Icon::queue, side.cx(), side.cy() - 18.0f, 34.0f, kWhite);
    text(g, g.f.semibold, fit(g.f.semibold, cell.count, 20.0f, side.w - 16.0f), side.cx(), side.cy() + 32.0f, 20.0f, kWhite, gfx::Align::center);
    g.l.bordered_rect(r, kRadiusThumb, clear(), 1.5f, kWhite.with_alpha(0.1f));
    if (whole)
    {
        const float after = paragraph(g, g.f.semibold, cell.title, r.x + 2.0f, r.y + r.h + 36.0f, 23.0f, r.w - 4.0f, 30.0f,
                                      g.k.ink.with_alpha(0.82f + 0.18f * focus), 2);
        text(g, g.f.regular, fit(g.f.regular, dotted({tr("Playlist"), cell.note}), kTypeMeta, r.w - 4.0f), r.x + 2.0f, after, kTypeMeta, g.k.ink2);
    }
    g.l.pop_opacity();
}

} // namespace afterglow::channel_page
