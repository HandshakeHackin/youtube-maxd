// ProsperoTube Afterglow - the player's drawing: what belongs to the video itself.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The shade under the text, captions, the title block, the chaptered timeline
// with SponsorBlock's lane, the row of marks, the seek preview, the pause mark
// and the playback details. Boards 16, 17 and 22 of the proposal.

#include "afterglow/player/parts.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace afterglow::player
{

float live_tag(G &g, float x, float cy)
{
    const char *live = tr("LIVE");
    const float width = measure(g.f.semibold, live, 17.4f, 0.8f) + 36.0f;
    g.l.rounded_rect({x, cy - 15.0f, width, 30.0f}, kRadiusChip, g.k.accent);
    const float pulse = g.reduced ? 0.5f : ui::breathe(g.time, 1.6f);
    g.l.circle(x + 15.0f, cy, 4.5f + 1.5f * pulse, g.k.on_accent.with_alpha(0.35f));
    g.l.circle(x + 15.0f, cy, 4.0f, g.k.on_accent);
    text(g, g.f.semibold, live, x + 28.0f, cy + 6.5f, 17.4f, g.k.on_accent, gfx::Align::left, 0.8f);
    return width;
}

void Player::draw_ground(gfx::DrawList &world, const Motion &m) const
{
    const Look &k = *look_;
    if (m.theatre > 0.004f)
    {
        // The video has stepped aside: the page takes the room around it. One
        // ring as thick as the screen covers everything but the picture and
        // rounds its corners, whoever drew the picture underneath.
        const Rect frame = video_rect();
        const float radius = video_radius();
        constexpr float kMat = 2200.0f;
        world.bordered_rect({frame.x - kMat, frame.y - kMat, frame.w + 2.0f * kMat, frame.h + 2.0f * kMat}, radius + kMat, clear(), kMat, k.base);
        world.bordered_rect(frame, radius, clear(), 1.5f, k.hairline.with_alpha(k.hairline.a * m.theatre));
    }
    // Shade for the text. It lives with the video, so panels frost it too.
    const float dim = std::max(0.28f * tween::clamp01(paused_.value), 0.34f * m.next) * (1.0f - m.theatre);
    if (dim > 0.01f)
        world.rounded_rect(kFull, 0.0f, kBlack.with_alpha(dim));
    if (m.hud > 0.01f)
    {
        world.gradient_rect({0.0f, 0.0f, gfx::kVirtualWidth, 300.0f}, 0.0f, kBlack.with_alpha(0.66f * m.hud), kBlack.with_alpha(0.0f));
        world.gradient_rect({0.0f, 600.0f, gfx::kVirtualWidth, 480.0f}, 0.0f, kBlack.with_alpha(0.0f), kBlack.with_alpha(0.86f * m.hud));
    }
}

void Player::draw_captions(G &g, const Motion &m) const
{
    const Captions &c = view_.captions;
    if (c.lines.empty() && c.second.empty())
        return;
    const Rect frame = video_rect();
    // A sheet from the right leaves the captions the room beside it.
    const float free_right = tween::lerp(frame.x + frame.w, std::min(frame.x + frame.w, sheet_rect().x), m.sheet);
    const float cx = (frame.x + free_right) * 0.5f;
    const float room = std::max(200.0f, free_right - frame.x - 96.0f);
    const float size = std::max(12.0f, c.size);
    // Lifted above the timeline while the controls are up.
    float bottom = c.baseline >= 0.0f ? c.baseline : 1000.0f - (c.raised ? 222.0f * m.hud : 0.0f);
    bottom = std::min(bottom, frame.y + frame.h - 36.0f);
    bottom -= std::max(0.0f, bottom - 606.0f) * m.up_next; // the shelf lifts them too
    const auto block = [&](const std::vector<std::string> &lines, float last_baseline, float full, Color ink)
    {
        // A line wider than its room is set smaller, down to three fifths, before anything is cut from it.
        float widest = 0.0f;
        for (const std::string &raw : lines)
            widest = std::max(widest, measure(g.f.semibold, raw, full));
        const float sz = widest > room ? std::max(full * 0.6f, full * room / widest) : full;
        const float line = sz * 1.25f;
        float y = last_baseline - static_cast<float>(lines.size() - 1) * line;
        for (const std::string &raw : lines)
        {
            const std::string value = fit(g.f.semibold, raw, sz, room);
            const float width = measure(g.f.semibold, value, sz);
            const Rect plate{cx - width * 0.5f - 18.0f, y - sz * 0.95f, width + 36.0f, line};
            if (!c.custom)
            {
                g.l.rounded_rect(plate, 10.0f, kBlack.with_alpha(0.55f));
            }
            else
            {
                if (c.plate_alpha > 0.0f)
                    g.l.rounded_rect(plate, 10.0f, Color::rgb(c.plate_rgb, c.plate_alpha));
                const float edge = std::max(2.0f, sz * 0.05f);
                if (c.edge == 2)
                {
                    text(g, g.f.semibold, value, cx + edge, y + edge, sz, kBlack, gfx::Align::center);
                }
                else if (c.edge == 1)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                        for (int dy = -1; dy <= 1; ++dy)
                            if (dx != 0 || dy != 0)
                                text(g, g.f.semibold, value, cx + static_cast<float>(dx) * edge, y + static_cast<float>(dy) * edge, sz, kBlack, gfx::Align::center);
                }
            }
            text(g, g.f.semibold, value, cx, y, sz, ink, gfx::Align::center);
            y += line;
        }
        return last_baseline - static_cast<float>(lines.size()) * line;
    };
    float top = bottom;
    if (!c.lines.empty())
        top = block(c.lines, bottom, size, c.custom ? Color::rgb(c.text_rgb) : kOnVideo);
    if (!c.second.empty())
    {
        const float second = size * 0.85f;
        if (c.second_top)
            block(c.second, frame.y + 120.0f + static_cast<float>(c.second.size() - 1) * second * 1.25f, second, Color::rgb(c.second_rgb));
        else
            block(c.second, top - 12.0f, second, Color::rgb(c.second_rgb));
    }
}

void Player::draw_header(G &g, const Motion &m) const
{
    if (m.hud <= 0.01f)
        return;
    g.l.push_opacity(m.hud);
    {
        Enter enter(g, tween::clamp01(chrome_.value * 1.2f), -18.0f);
        // The hour, and under it what is being decoded, said once, quietly.
        float clock_w = 0.0f;
        if (!view_.clock.empty())
            clock_w = text(g, g.f.display, view_.clock, kRight, 104.0f, 28.0f, kOnVideo.with_alpha(0.92f), gfx::Align::right);
        float tags_left = kRight;
        constexpr float kPillCy = 147.0f;
        if (!view_.quality.empty())
        {
            const float width = measure(g.f.regular, view_.quality, kTypeMeta) + 36.0f;
            const Rect pill{kRight - width, kPillCy - 19.0f, width, 38.0f};
            g.l.rounded_rect(pill, 19.0f, kBlack.with_alpha(0.36f));
            g.l.bordered_rect(pill, 19.0f, clear(), 1.5f, kOnVideo.with_alpha(0.22f));
            text(g, g.f.regular, view_.quality, pill.cx(), pill.cy() + 7.0f, kTypeMeta, kOnVideo.with_alpha(0.9f), gfx::Align::center);
            tags_left = pill.x - 10.0f;
        }
        for (std::size_t i = view_.tags.size(); i-- > 0;)
        {
            const std::string &label = view_.tags[i];
            if (label.empty())
                continue;
            const float width = tag_width(g, label, 28.0f);
            if (tags_left - width - 8.0f < 700.0f)
                break; // the title block keeps its room
            tags_left -= width + 8.0f;
            tag(g, tags_left, kPillCy, label, kOnVideo.with_alpha(0.16f), kOnVideo, 28.0f);
        }

        // What is playing.
        float x = kEdge - 2.0f;
        if (view_.live)
            x += live_tag(g, kEdge, 94.0f) + 18.0f;
        const float title_room = kRight - clock_w - (clock_w > 0.0f ? 64.0f : 0.0f) - x;
        text(g, g.f.display, fit(g.f.display, view_.title, kTypePlayerTitle, title_room), x, 108.0f, kTypePlayerTitle, kOnVideo);
        const float line_room = tags_left - 32.0f - kEdge;
        const std::string &second = view_.live && !view_.live_status.empty() ? view_.live_status : view_.meta;
        if (!view_.channel.empty())
        {
            avatar(g, kEdge + 16.0f, 144.0f, 16.0f, view_.channel, view_.channel_id, picture(view_.avatar, PictureSize::avatar));
            const std::string name = fit(g.f.semibold, view_.channel, kTypeHint, std::min(520.0f, line_room - 44.0f));
            const float name_w = text(g, g.f.semibold, name, kEdge + 44.0f, 152.0f, kTypeHint, kOnVideo);
            const float rest = line_room - 44.0f - name_w - 18.0f;
            if (!second.empty() && rest > 120.0f)
                text(g, g.f.regular, fit(g.f.regular, second, kTypeHint, rest), kEdge + 44.0f + name_w + 18.0f, 152.0f, kTypeHint,
                     kOnVideo.with_alpha(0.72f));
        }
        else if (!second.empty())
        {
            text(g, g.f.regular, fit(g.f.regular, second, kTypeHint, line_room), kEdge, 152.0f, kTypeHint, kOnVideo.with_alpha(0.72f));
        }
    }
    g.l.pop_opacity();
}

void Player::draw_timeline(G &g, const Rect &lane, float alpha, bool on_page) const
{
    if (alpha <= 0.01f || view_.duration <= 0.0)
        return;
    const Look &k = *look_;
    const Color ink = on_page ? k.ink : kOnVideo;
    const float scrub = tween::clamp01(scrub_.value);
    const float y = lane.y;
    const float h = 6.0f + 5.0f * scrub;
    const float played = tween::clamp01(progress_.value);
    const float target = tween::clamp01(target_.value);
    const bool seeking = scrub > 0.01f;
    const double now = view_.scrubbing ? view_.scrub_target : view_.position;
    const int chapter = chapter_at(now);
    const int count = static_cast<int>(view_.chapters.size());
    g.l.push_opacity(alpha);

    // ---- where you are: the chapter, by name ----
    if (chapter >= 0 && !on_page)
    {
        overline(g, fill(tr("Chapter {0} of {1}"), std::to_string(chapter + 1), std::to_string(count)), lane.x, y - 62.0f, k.accent);
        const std::string &title = view_.chapters[static_cast<std::size_t>(chapter)].title;
        if (!title.empty())
            text(g, g.f.semibold, fit(g.f.semibold, title, kTypeChapter, lane.w * 0.62f), lane.x, y - 26.0f, kTypeChapter, ink);
    }

    // ---- the bar: one piece per chapter ----
    const int pieces = std::max(1, count);
    for (int i = 0; i < pieces; ++i)
    {
        const float a = count == 0 || i == 0 ? 0.0f : share(view_.chapters[static_cast<std::size_t>(i)].start);
        const float b = i + 1 < count ? share(view_.chapters[static_cast<std::size_t>(i) + 1].start) : 1.0f;
        const float x0 = lane.x + lane.w * a + (i > 0 ? 2.0f : 0.0f);
        const float x1 = lane.x + lane.w * b - (i + 1 < pieces ? 2.0f : 0.0f);
        if (x1 - x0 < 1.0f)
            continue;
        const float grow = i == chapter && seeking ? 3.0f * scrub : 0.0f; // the chapter under the thumb swells
        const Rect piece{x0, y - h * 0.5f - grow * 0.5f, x1 - x0, h + grow};
        g.l.rounded_rect(piece, 2.0f, ink.with_alpha(0.26f));
        const auto fill_to = [&](float upto, Color color)
        {
            const float end = std::min(x1, lane.x + lane.w * upto);
            if (end > x0)
                g.l.rounded_rect({x0, piece.y, end - x0, piece.h}, 2.0f, color);
        };
        if (view_.buffered >= 0.0)
            fill_to(share(view_.buffered), ink.with_alpha(0.22f));
        if (seeking && target > played)
            fill_to(target, ink.with_alpha(0.75f * scrub));
        fill_to(seeking ? std::min(played, target) : played, k.accent);
    }
    // ---- SponsorBlock: a lane of its own, in the categories' colours ----
    for (const Segment &segment : view_.segments)
    {
        const float from = static_cast<float>(std::clamp(segment.from, 0.0, 1.0));
        const float to = static_cast<float>(std::clamp(segment.to, segment.from, 1.0));
        const float width = std::max(6.0f, lane.w * (to - from));
        const float x = std::min(lane.x + lane.w * from, lane.x + lane.w - width);
        g.l.rounded_rect({x, y + h * 0.5f + 7.0f, width, 4.0f}, 2.0f, Color::rgb(segment.rgb, 0.95f));
    }
    // ---- the thumb ----
    const float tx = lane.x + lane.w * target;
    if (seeking)
        g.l.rounded_rect({lane.x + lane.w * played - 1.5f, y - 12.0f, 3.0f, 24.0f}, 1.5f, k.accent.with_alpha(scrub));
    const Color thumb = on_page ? k.plate : kOnVideo;
    g.l.glow({tx - 6.0f, y - 6.0f, 12.0f, 12.0f}, 6.0f, 18.0f + 10.0f * scrub, thumb.with_alpha(0.35f));
    g.l.circle(tx, y, 9.0f + 4.0f * scrub, thumb);

    // ---- the clock of the video ----
    text(g, g.f.mono, clock_text(now), lane.x, y + 50.0f, kTypeClock, ink);
    const float whole_w = text(g, g.f.mono, clock_text(view_.duration), lane.x + lane.w, y + 50.0f, kTypeClock, ink.with_alpha(0.7f), gfx::Align::right);
    if (view_.show_remaining)
    {
        const double rate = view_.rate >= 0.25 && view_.rate <= 4.0 ? view_.rate : 1.0;
        text(g, g.f.mono, clock_text(std::max(0.0, view_.duration - now) / rate, true), lane.x + lane.w - whole_w - 28.0f, y + 50.0f, kTypeClock, ink,
             gfx::Align::right);
    }
    g.l.pop_opacity();
}

// A live stream has an edge, not a length.
void Player::draw_live_bar(G &g, const Rect &lane, float alpha, bool on_page) const
{
    if (alpha <= 0.01f)
        return;
    const Look &k = *look_;
    const Color ink = on_page ? k.ink : kOnVideo;
    g.l.push_opacity(alpha);
    g.l.rounded_rect({lane.x, lane.y - 3.0f, lane.w, 6.0f}, 3.0f, ink.with_alpha(0.2f));
    g.l.rounded_rect({lane.x, lane.y - 3.0f, lane.w - 6.0f, 6.0f}, 3.0f, k.accent);
    g.l.circle(lane.x + lane.w - 6.0f, lane.y, 9.0f, on_page ? k.plate : kOnVideo);
    if (view_.position > 0.0)
        text(g, g.f.mono, clock_text(view_.position), lane.x, lane.y + 44.0f, kTypeHint, ink);
    text(g, g.f.semibold, tr("At the live edge"), lane.x + lane.w, lane.y + 44.0f, kTypeNote, on_page ? k.ink2 : ink.with_alpha(0.72f), gfx::Align::right);
    g.l.pop_opacity();
}

void Player::draw_foot(G &g, const Motion &m) const
{
    if (m.foot <= 0.01f)
        return;
    g.l.push_transform(1.0f, 0.0f, 0.0f, 0.0f, m.rise);
    if (view_.live)
        draw_live_bar(g, {kEdge, kBarY, kBarWidth, 0.0f}, m.foot, false);
    else
        draw_timeline(g, {kEdge, kBarY, kBarWidth, 0.0f}, m.foot, false);

    // The marks: what Cross does is the lit one. They give way to the seek's own hints.
    const float marks = m.foot * (1.0f - tween::clamp01(scrub_.value));
    if (marks > 0.01f)
    {
        g.l.push_opacity(marks);
        static const char *const kMarks[] = {"play_pause", "prev", "next", "captions", "quality", "speed", "chat", "queue", "more"};
        const int count = view_.controls.empty() ? 9 : std::min(12, static_cast<int>(view_.controls.size()));
        float right = kEdge;
        float x = kEdge;
        bool transport = true; // play, previous and next stand together; a gap, then what opens something
        for (int i = 0; i < count; ++i)
        {
            const std::string name = view_.controls.empty() ? kMarks[i] : view_.controls[static_cast<std::size_t>(i)];
            const Icon mark = name == "play_pause" ? (view_.paused ? Icon::play : Icon::pause) : icon_by_name(name);
            const bool moves = name == "play_pause" || name == "play" || name == "pause" || name == "prev" || name == "next";
            if (transport && !moves && i > 0)
                x += 24.0f;
            transport = transport && moves;
            const float focus = i == 0 ? 1.0f : 0.0f;
            const Rect r{x, kMarksY, kMark, kMark};
            x += 70.0f;
            g.l.rounded_rect(r, kMark * 0.5f, gfx::mix(kOnVideo.with_alpha(0.12f), kOnVideo, focus));
            icon(g, mark, r.cx(), r.cy(), 28.0f, gfx::mix(kOnVideo, kOnLit, focus));
            right = r.x + r.w;
        }
        hint_row(g, view_.hints, kRight, kFootHintY, true, true, kRight - right - 48.0f);
        g.l.pop_opacity();
    }
    g.l.pop_transform();
}

void Player::draw_seek(G &g, const Motion &m) const
{
    const float scrub = m.scrub * (1.0f - m.up_next);
    if (scrub <= 0.01f || view_.live || view_.duration <= 0.0)
        return;
    const float tx = kEdge + kBarWidth * tween::clamp01(target_.value);
    const Tile *target = nullptr;
    int reach = 0;
    for (const Tile &tile : view_.preview)
    {
        if (tile.offset == 0 && target == nullptr)
            target = &tile;
        reach = std::max(reach, std::abs(tile.offset));
    }
    g.l.push_opacity(tween::clamp01(scrub * 1.3f));
    const std::string at = clock_text(view_.scrub_target);
    const float chip_w = measure(g.f.mono, at, 26.0f) + 30.0f;
    constexpr float kBottom = 809.0f;
    const float rise = g.reduced ? 0.0f : 22.0f * (1.0f - scrub);
    if (target != nullptr)
    {
        // A picture of where you would land: the storyboard's frame for that moment.
        const float aspect = std::clamp(target->aspect, 0.5f, 3.0f);
        float w = 336.0f;
        float h = w / aspect;
        if (h > 240.0f)
        {
            w *= 240.0f / h;
            h = 240.0f;
        }
        const float side_w = w * 0.6f;
        const float side_h = h * 0.6f;
        const float spread = w * 0.5f + static_cast<float>(reach) * (side_w + 8.0f);
        const float px = std::clamp(tx, kEdge + spread + 2.0f, kRight - spread - 2.0f);
        const float bottom = kBottom + rise;
        const auto draw_tile = [&](const Tile &tile, const Rect &box, float radius, Color tint)
        {
            if (tile.texture != 0)
            {
                g.l.image(tile.texture, box, tile.uv, tint, radius);
                return;
            }
            const Picture shot = picture(tile.art, PictureSize::card);
            if (shot.ready())
                g.l.image(shot.texture, box, shot.cover(box.w / box.h), tint, radius);
            else
                bone(g, box, radius, tile.offset);
        };
        // Neighbours first, the target's tile over them.
        for (const Tile &tile : view_.preview)
        {
            if (tile.offset == 0)
                continue;
            const float edge = w * 0.5f + 8.0f + static_cast<float>(std::abs(tile.offset) - 1) * (side_w + 8.0f);
            const Rect box{tile.offset < 0 ? px - edge - side_w : px + edge, bottom - side_h, side_w, side_h};
            g.l.rounded_rect(box.inset(-2.0f), 12.0f, kBlack.with_alpha(0.6f));
            draw_tile(tile, box, 10.0f, kWhite.with_alpha(0.72f));
        }
        const Rect tile{px - w * 0.5f, bottom - h, w, h};
        g.l.shadow({tile.x, tile.y + 16.0f, tile.w, tile.h}, kRadiusThumb, 40.0f, kBlack.with_alpha(0.6f));
        draw_tile(*target, tile, kRadiusThumb, kWhite);
        g.l.bordered_rect(tile.inset(-3.0f), kRadiusThumb + 3.0f, clear(), 3.0f, kOnVideo);
        g.l.rounded_rect({px - chip_w * 0.5f, tile.y + tile.h - 52.0f, chip_w, 40.0f}, 12.0f, kBlack.with_alpha(0.74f));
        text(g, g.f.mono, at, px, tile.y + tile.h - 23.0f, 26.0f, kOnVideo, gfx::Align::center);
        // A stem from the tile to the thumb.
        g.l.rounded_rect({tx - 1.5f, tile.y + tile.h + 6.0f, 3.0f, kBarY - tile.y - tile.h - 24.0f}, 1.5f, kOnVideo.with_alpha(0.5f));
    }
    else
    {
        // No storyboard for this video, or none yet: the time alone, over the thumb.
        const float px = std::clamp(tx, kEdge + chip_w * 0.5f, kRight - chip_w * 0.5f);
        const Rect chip{px - chip_w * 0.5f, kBarY - 84.0f + rise * 0.5f, chip_w, 44.0f};
        g.l.shadow({chip.x, chip.y + 10.0f, chip.w, chip.h}, 14.0f, 26.0f, kBlack.with_alpha(0.5f));
        g.l.rounded_rect(chip, 14.0f, kBlack.with_alpha(0.74f));
        g.l.bordered_rect(chip, 14.0f, clear(), 1.5f, kOnVideo.with_alpha(0.3f));
        text(g, g.f.mono, at, px, chip.y + 31.0f, 26.0f, kOnVideo, gfx::Align::center);
    }
    hint_row(g, view_.seek_hints, kEdge, kFootHintY, false, true, kBarWidth);
    g.l.pop_opacity();
}

void Player::draw_centre(G &g, const Motion &m) const
{
    const Look &k = *look_;
    const Rect frame = video_rect();
    const float cx = frame.cx();
    const float cy = frame.cy() - 70.0f * (1.0f - m.theatre);
    const float shown = (1.0f - m.next);
    const float pause = tween::clamp01(paused_.value) * shown;
    if (pause > 0.01f)
    {
        // Paused: one mark in the middle.
        const float pop = 0.16f * pause_pop_.value;
        g.l.push_opacity(pause);
        const Rect disc = grown({cx - 66.0f, cy - 66.0f, 132.0f, 132.0f}, pop);
        glass_panel(g, disc, disc.w * 0.5f, 1.1f);
        icon(g, Icon::pause, cx, cy, 54.0f * (1.0f + pop), k.ink);
        g.l.pop_opacity();
    }
    const float loading = tween::clamp01(loading_.value) * shown;
    if (loading > 0.01f)
    {
        // The first frames, or a seek, are on their way: never a picture that only looks frozen.
        g.l.push_opacity(loading);
        const Rect disc{cx - 66.0f, cy - 66.0f, 132.0f, 132.0f};
        glass_panel(g, disc, 66.0f, 1.1f);
        g.l.arc(cx, cy, 40.0f, 6.0f, 0.0f, 6.2831853f, k.ink.with_alpha(0.16f), false);
        g.l.arc(cx, cy, 40.0f, 6.0f, g.reduced ? 0.0f : std::fmod(g.time * 5.5f, 6.2831853f), 1.6f, k.accent, true);
        g.l.pop_opacity();
    }
}

void Player::draw_stats(G &g, const Motion &m) const
{
    (void)m;
    const float shown = tween::clamp01(stats_.value);
    if (shown <= 0.01f)
        return;
    const Look &k = *look_;
    const Stats &stats = view_.stats;
    g.l.push_opacity(shown);
    const Rect panel{kEdge, 200.0f, 760.0f, 560.0f};
    lift(g, panel, kRadiusPanel, 0.8f);
    glass_panel(g, panel, kRadiusPanel, 1.4f);
    overline(g, tr("Playback details"), panel.x + 30.0f, panel.y + 46.0f, k.accent);
    float y = panel.y + 88.0f;
    for (const std::string &line : stats.lines)
    {
        if (line.empty())
            continue;
        text(g, g.f.mono, fit(g.f.mono, line, 18.0f, panel.w - 60.0f), panel.x + 30.0f, y, 18.0f, k.ink.with_alpha(0.92f));
        y += 32.0f;
    }
    float gx = panel.x + 30.0f;
    const float gy = panel.y + panel.h - 134.0f;
    for (const Graph &graph : stats.graphs)
    {
        if (gx + 220.0f > panel.x + panel.w - 20.0f)
            break;
        const Rect box{gx, gy, 220.0f, 104.0f};
        g.l.rounded_rect(box, 14.0f, k.raised);
        text(g, g.f.semibold, graph.label, box.x + 14.0f, box.y + 28.0f, 16.0f, k.accent, gfx::Align::left, 2.0f);
        text(g, g.f.mono, graph.value, box.x + box.w - 14.0f, box.y + 28.0f, 16.0f, k.ink, gfx::Align::right);
        if (graph.samples.size() > 1)
        {
            const float step = (box.w - 28.0f) / static_cast<float>(graph.samples.size() - 1);
            for (std::size_t i = 1; i < graph.samples.size(); ++i)
            {
                const float x0 = box.x + 14.0f + step * static_cast<float>(i - 1);
                const float x1 = box.x + 14.0f + step * static_cast<float>(i);
                const float y0 = box.y + box.h - 12.0f - 54.0f * tween::clamp01(graph.samples[i - 1]);
                const float y1 = box.y + box.h - 12.0f - 54.0f * tween::clamp01(graph.samples[i]);
                g.l.line(x0, y0, x1, y1, 2.5f, k.accent);
            }
        }
        gx += 236.0f;
    }
    g.l.pop_opacity();
}

} // namespace afterglow::player
