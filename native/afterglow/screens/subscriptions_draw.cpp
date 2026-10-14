// ProsperoTube Afterglow - new videos from the channels a profile subscribes to, drawn.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Board 26 of the proposal: the strip of faces, the feed by day under it, and the filter in the header.

#include "afterglow/screens/subscriptions.hpp"

namespace afterglow::subscriptions_page
{

void Subscriptions::draw(const DrawContext &context, const Phase &phase) const
{
    G g = context.g();
    HandOver travel(g, phase);
    if (mode_ == Mode::loading)
    {
        draw_bones(g);
        return;
    }
    const float scroll = scroll_.value;
    const bool clipped = scroll > 0.5f;
    if (clipped)
        g.l.push_clip({0.0f, kClipTop, gfx::kVirtualWidth, gfx::kVirtualHeight - kClipTop});
    g.l.push_transform(1.0f, 0.0f, 0.0f, 0.0f, -scroll);
    draw_strip(g, context);
    if (mode_ == Mode::feed)
        draw_feed(g, context);
    else
    {
        Enter enter(g, arrive(age_, 4, 0.07f, 0.55f), 26.0f);
        notice_.draw(g, notice_icon(), notice_title(), notice_body(), choices_, choice_pos_.value, zone_ == Zone::notice);
    }
    g.l.pop_transform();
    if (clipped)
        g.l.pop_clip();
    foot_scrim(g);
    if (mode_ == Mode::feed)
    {
        Enter enter(g, arrive(age_, 1, 0.05f, 0.5f), -14.0f);
        ui::TabBar bar = bar_;
            ui::Canvas canvas{g.l, g.f, g.glass, g.time};
        bar.draw(canvas);
    }
}

void Subscriptions::draw_bones(G &g) const
{
    Enter enter(g, arrive(age_, 1, 0.05f, 0.4f), 18.0f);
    for (int i = 0; i < kStripShown; ++i)
    {
        const float cx = kStripX + kStripPitch * static_cast<float>(i);
        bone(g, {cx - 40.0f, kStripY - 40.0f, 80.0f, 80.0f}, 40.0f);
        bone(g, {cx - 44.0f, 296.0f, 88.0f, 16.0f}, 8.0f);
    }
    bone(g, {kLeft, 352.0f, 120.0f, 22.0f}, 11.0f);
    for (int i = 0; i < kColumns * 2; ++i)
        card_bone(g, {kLeft + static_cast<float>(i % kColumns) * kCellPitchX, kFeedY + static_cast<float>(i / kColumns) * (kCellPitchY + kSectionGap), kCellW, kCellH},
                  i < kColumns);
}

void Subscriptions::draw_strip(G &g, const DrawContext &context) const
{
    if (strip_.empty())
        return;
    if (!has_all())
    {
        Enter enter(g, arrive(age_, 1, 0.05f, 0.5f), 14.0f);
        overline(g, tr("Followed on this PS5"), kLeft, 172.0f, g.k.ink3);
    }
    g.l.push_clip({kRail + 8.0f, kStripY - 90.0f, gfx::kVirtualWidth - kRail - 8.0f, 190.0f});
    const float scroll = strip_scroll_.value;
    const int count = static_cast<int>(strip_.size());
    for (int i = 0; i < count; ++i)
    {
        const float cx = strip_x(i, scroll);
        if (cx < kRail - 80.0f || cx > gfx::kVirtualWidth + 80.0f)
            continue;
        const Entry &face = strip_[static_cast<std::size_t>(i)];
        const model::Channel *channel = strip_channel(i);
        const float near = tween::clamp01(1.0f - std::fabs(pick_pos_.value - static_cast<float>(i)));
        const float focus = near * strip_focus_.value;
        const bool picked = i == pick_ && has_all();
        // A face at the edge of the screen fades: there are more, and none is cut in half.
        const float edge = tween::clamp01((gfx::kVirtualWidth - 16.0f - cx) / 90.0f) * tween::clamp01((cx - kRail - 20.0f) / 60.0f);
        Enter enter(g, arrive(age_, 2 + std::min(i, kStripShown), 0.03f, 0.4f), 18.0f);
        g.l.push_opacity(edge);
        g.l.push_transform(1.0f + 0.06f * focus, cx, kStripY, 0.0f, 0.0f);
        Color glow = g.k.focus;
        if (channel == nullptr)
        {
            // All of them: a plate while it is the pick, a quiet disc otherwise.
            const float on = picked ? 1.0f : 0.0f;
            g.l.circle(cx, kStripY, 44.0f, gfx::mix(g.k.raised, g.k.plate, on));
            icon(g, Icon::grid, cx, kStripY, 34.0f, gfx::mix(g.k.ink, g.k.on_plate, on));
        }
        else
        {
            const Picture picture = context.images.get(channel->avatar, PictureSize::avatar);
            if (picture.ready())
                glow = picture.glow;
            avatar(g, cx, kStripY, 40.0f, channel->name, channel->id, picture);
            if (face.live)
            {
                // Live: a ring in the signature colour and the word.
                g.l.ring(cx, kStripY, 47.0f, 3.5f, g.k.accent);
                const char *label = tr("LIVE");
                tag(g, cx - tag_width(g, label, 24.0f) * 0.5f, kStripY + 40.0f, label, g.k.accent, g.k.on_accent, 24.0f);
            }
            else if (picked && focus < 0.5f)
                g.l.ring(cx, kStripY, 47.0f, 3.0f, g.k.ink); // the pick keeps its mark when the focus is elsewhere
            if (face.unseen && !face.live)
            {
                g.l.circle(cx + 30.0f, kStripY - 30.0f, 9.0f, g.k.base);
                g.l.circle(cx + 30.0f, kStripY - 30.0f, 6.5f, g.k.accent);
            }
        }
        g.l.pop_transform();
        if (focus > 0.01f)
        {
            Rect lit{cx - 46.0f, kStripY - 46.0f, 92.0f, 92.0f}; // inside the name under the face
            lit.x += ui::shake(nudge_.value, g.time, 14.0f, 8.0f);
            focus_ring(g, lit, 46.0f, glow, focus);
        }
        const bool strong = channel == nullptr || picked || focus > 0.5f;
        const std::string name = channel == nullptr ? std::string(tr("All")) : channel->name;
        const Face &face_of = strong ? g.f.semibold : g.f.regular;
        const float size = channel == nullptr ? 19.0f : 18.0f;
        text(g, face_of, fit(face_of, name, size, 128.0f), cx, 310.0f, size, strong ? g.k.ink : g.k.ink2, gfx::Align::center);
        g.l.pop_opacity();
    }
    g.l.pop_clip();
}

void Subscriptions::draw_feed(G &g, const DrawContext &context) const
{
    const float scroll = scroll_.value;
    const int count = static_cast<int>(order_.size());
    const float zone_focus = feed_focus_.value;
    if (count == 0)
    {
        draw_nothing(g);
        return;
    }
    const int focused_line = line_of(focus_);
    for (std::size_t i = 0; i < lines_.size(); ++i)
    {
        const Line &line = lines_[i];
        if (!line.heads || line.y - kHeadAbove - scroll < kClipTop - 40.0f || line.y - scroll > gfx::kVirtualHeight)
            continue;
        Enter enter(g, arrive(age_, 4, 0.06f, 0.5f), 18.0f);
        const bool here = lines_[static_cast<std::size_t>(focused_line)].day == line.day && zone_focus > 0.5f;
        text(g, g.f.semibold, day_name(line.day), kLeft, line.y - kHeadAbove, kTypeShelf, here || i == 0 ? g.k.ink : g.k.ink2);
    }
    const Rect ring = ring_.value();
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int i = 0; i < count; ++i)
        {
            if ((pass == 1) != (i == focus_ && zone_focus > 0.01f))
                continue;
            const Rect r = cell_rect(i);
            const float y = r.y - scroll;
            if (y > gfx::kVirtualHeight || y + r.h + 120.0f < 0.0f)
                continue;
            const model::Video &video = feed_.videos[static_cast<std::size_t>(order_[static_cast<std::size_t>(i)])];
            const float focus = under_ring(ring, r, 260.0f) * zone_focus;
            const bool whole = y + r.h + 104.0f <= kPageBottom;
            const Picture picture = context.images.get(video.thumb, PictureSize::card);
            Enter enter(g, arrive(age_, 6 + (i - lines_[static_cast<std::size_t>(line_of(i))].first), 0.05f, 0.45f), 26.0f);
            video_card(g, grown(r, 0.05f * focus, 5.0f * focus), video, picture, focus, whole ? 1.0f : 0.8f, whole ? 2 : 0);
            if (i == focus_ && focus > 0.01f)
            {
                Rect lit = ring;
                lit.x += ui::shake(nudge_.value, g.time, 14.0f, 8.0f);
                focus_ring(g, lit, kRadiusThumb, picture.ready() ? picture.glow : g.k.focus, zone_focus);
            }
        }
    }
    if (feed_.fetching_more())
    {
        // The next page is on its way: its row waits as bones under the last one.
        const float y = lines_.back().y + kCellPitchY;
        for (int i = 0; i < kColumns; ++i)
            card_bone(g, {kLeft + static_cast<float>(i) * kCellPitchX, y, kCellW, kCellH}, y - scroll + kCellH + 104.0f <= kPageBottom);
    }
}

void Subscriptions::draw_nothing(G &g) const
{
    if (feed_.fetching_more())
    {
        for (int i = 0; i < kColumns; ++i)
            card_bone(g, {kLeft + static_cast<float>(i) * kCellPitchX, kFeedY, kCellW, kCellH});
        return;
    }
    Enter enter(g, arrive(age_, 4, 0.06f, 0.5f), 18.0f);
    const model::Channel *picked = strip_channel(pick_);
    std::string title;
    if (picked != nullptr)
        title = fill(filter_ == 0 ? tr("Nothing from {0} in your feed yet") : tr("Nothing from {0} under this filter"), picked->name);
    else
        title = filter_ == 1 ? tr("Nothing new today") : filter_ == 2 ? tr("Nobody is live right now") : filter_ == 3 ? tr("You are all caught up") : tr("Nothing in your feed yet");
    icon(g, Icon::info, kLeft + 24.0f, kFeedY + 2.0f, 44.0f, g.k.ink2);
    text(g, g.f.semibold, fit(g.f.semibold, title, kTypeShelf, kRight - kLeft - 80.0f), kLeft + 64.0f, kFeedY + 11.0f, kTypeShelf, g.k.ink);
    const char *way = picked != nullptr ? tr("Open the channel to see everything it has.") : tr("Another filter shows more.");
    paragraph(g, g.f.regular, way, kLeft + 64.0f, kFeedY + 48.0f, kTypeBody, 1100.0f, 34.0f, g.k.ink2, 1);
}

} // namespace afterglow::subscriptions_page
