// ProsperoTube Afterglow - Home, drawn.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The picture in focus dissolving into the page, the hero's title and play
// line, a Guest's invitation and the shelves under the ring; the page while it
// loads, is empty, failed or has no connection; and attract mode, where the
// picture takes the whole screen.

#include "afterglow/screens/home.hpp"

#include <algorithm>
#include <cmath>

namespace afterglow::home_page
{

void Home::draw(const DrawContext &context, const Phase &phase) const
{
    G w = context.g_world();
    G g = context.g();
    const Look &k = g.k;
    const float fade = tween::clamp01(phase.in) * (1.0f - tween::clamp01(phase.out));
    const float attract = attract_amount_.value;

    // ---- the page: the picture in focus, dissolving into the wash ----
    const float backdrop_in = arrive(0, 0.0f, 0.9f) * fade;
    const float cross = hero_fade_.running ? hero_fade_.progress() : 1.0f;
    const auto backdrop = [&](const model::Video &video, float alpha)
    {
        if (alpha <= 0.004f || video.thumb.empty())
            return;
        const Picture picture = context.images.get(video.thumb, PictureSize::hero);
        // Alone with the screen, the picture takes all of it and drifts.
        const float a = tween::smoothstep(attract);
        const Rect r = lerp_rect(kHeroImage, {0.0f, 0.0f, gfx::kVirtualWidth, gfx::kVirtualHeight}, a);
        const float drift = g.reduced ? 0.0f : attract_step_ / kAttractStep;
        const float arrived = g.reduced ? 1.0f : tween::clamp01(picture.age / 0.35f);
        w.l.push_transform(1.0f + 0.05f * a * drift, 960.0f, 540.0f, 0.0f, 0.0f);
        faded_picture(w.l, picture, r, alpha * arrived * (k.dark ? 0.94f : 1.0f), tween::lerp(480.0f, 0.0f, a),
                      std::floor(tween::lerp(348.0f, 1080.0f, a) / 12.0f) * 12.0f);
        w.l.pop_transform();
    };
    if (cross < 1.0f)
        backdrop(hero_prev_, backdrop_in * (1.0f - tween::smoothstep(cross * 1.4f)));
    backdrop(hero_now_, backdrop_in * tween::smoothstep(cross));
    // Under the title the page is evened out, so text never depends on the picture.
    w.l.shadow({kRail - 160.0f, 190.0f, 760.0f, 250.0f}, 120.0f, 260.0f, k.base.with_alpha((k.dark ? 0.74f : 0.34f) * (1.0f - attract)));
    if (attract > 0.01f)
    {
        w.l.gradient_rect({0.0f, 600.0f, gfx::kVirtualWidth, 480.0f}, 0.0f, kBlack.with_alpha(0.0f), kBlack.with_alpha(0.8f * attract));
        w.l.gradient_rect({0.0f, 0.0f, gfx::kVirtualWidth, 220.0f}, 0.0f, kBlack.with_alpha(0.5f * attract), kBlack.with_alpha(0.0f));
    }

    g.l.push_opacity(fade * furniture());

    // ---- the wordmark, where other pages have their title ----
    {
        Enter enter(g, arrive(1, 0.05f, 0.5f), -14.0f);
        const float ww = text(g, g.f.display, "PROSPERO", kLeft, 102.0f, 23.0f, k.ink2, gfx::Align::left, 6.5f);
        text(g, g.f.display, "TUBE", kLeft + ww + 6.5f, 102.0f, 23.0f, k.ink, gfx::Align::left, 6.5f);
    }

    if (rows_.empty())
    {
        draw_state(g);
        g.l.pop_opacity();
        return;
    }

    // ---- the hero: the old title leaves quickly, the new one lands a beat later ----
    {
        const float in = arrive(2, 0.07f, 0.6f);
        g.l.push_opacity(in);
        if (hero_fade_.running)
        {
            draw_hero(context, g, hero_prev_, hero_label_prev_, 1.0f - tween::smoothstep(cross * 2.4f),
                      g.reduced ? 0.0f : -30.0f * tween::cubic_in(tween::clamp01(cross * 2.4f)));
            const float land = tween::clamp01((cross - 0.28f) / 0.72f);
            draw_hero(context, g, hero_now_, hero_label_, tween::smoothstep(land), g.reduced ? 0.0f : 38.0f * (1.0f - tween::quint_out(land)));
        }
        else
        {
            draw_hero(context, g, hero_now_, hero_label_, 1.0f, g.reduced ? 0.0f : 28.0f * (1.0f - in));
        }
        g.l.pop_opacity();
    }

    // ---- Guest: an invitation, not a wall ----
    if (invite_)
    {
        const float in = arrive(4, 0.07f, 0.6f);
        Enter enter(g, in, 0.0f, 40.0f);
        const Rect card{1304.0f, 150.0f, 520.0f, 290.0f};
        lift(g, card, kRadiusPanel, 0.8f);
        glass_panel(g, card, kRadiusPanel);
        overline(g, tr("You are watching as Guest"), card.x + 36.0f, card.y + 52.0f, k.accent);
        text(g, g.f.display, fit(g.f.display, tr("Make it yours"), 38.0f, card.w - 68.0f), card.x + 34.0f, card.y + 106.0f, 38.0f, k.ink);
        paragraph(g, g.f.regular,
                  tr("Sign in to YouTube for your subscriptions, your history and recommendations that know you. Everything here works without it."),
                  card.x + 36.0f, card.y + 148.0f, 21.0f, card.w - 72.0f, 30.0f, k.ink2, 3);
        const ui::GlyphStyle glyphs = k.dark ? ui::GlyphStyle::dark() : ui::GlyphStyle::light();
        ui::draw_button(g.l, g.f, glyphs, ui::Button::options, card.x + 36.0f, card.y + 252.0f, 30.0f);
        text(g, g.f.semibold, fit(g.f.semibold, tr("Sign in with a code"), 22.0f, card.w - 116.0f), card.x + 80.0f, card.y + 260.0f, 22.0f, k.ink);
    }

    // ---- the shelves ----
    Rect ring = ring_.value();
    ring.x += ui::shake(nudge_.value, g.time, 14.0f, 8.0f) * nudge_dir_;
    const float ring_cx = ring_.value().cx();
    for (int r = 0; r < static_cast<int>(rows_.size()); ++r)
    {
        const float distance = static_cast<float>(r) - row_pos_.value;
        // Shelves above the focused one are gone; the one below waits, dimmed.
        const float visible = tween::clamp01(1.0f + distance * 2.6f) * (distance > 0.0f ? 1.0f - 0.4f * tween::clamp01(distance) : 1.0f);
        if (visible <= 0.01f || distance > 2.2f)
            continue;
        draw_shelf(context, g, rows_[static_cast<std::size_t>(r)], card_rect(r, 0).y, visible, r == row_, ring_cx, 4 + r * 3);
    }
    {
        const Row &row = rows_[static_cast<std::size_t>(row_)];
        if (row.count() > 0)
        {
            const float in = arrive(5, 0.06f, 0.5f);
            const Picture picture = hero_now_.thumb.empty() ? Picture{} : context.images.get(hero_now_.thumb, PictureSize::card);
            focus_ring(g, ring, row.topics() ? 20.0f : kRadiusThumb, picture.ready() ? picture.glow : k.accent, in);
        }
    }

    // ---- the foot of the screen: the hint bar reads over whatever scrolls under it ----
    g.l.gradient_rect({kRail, 944.0f, gfx::kVirtualWidth - kRail, 60.0f}, 0.0f, k.base.with_alpha(0.0f), k.base.with_alpha(0.95f));
    g.l.rounded_rect({kRail, 1004.0f, gfx::kVirtualWidth - kRail, 76.0f}, 0.0f, k.base.with_alpha(0.95f));
    g.l.pop_opacity();

    // ---- attract: the picture, its name, and how to get back ----
    if (attract > 0.01f && !hero_now_.id.empty())
    {
        const float a = tween::smoothstep((attract - 0.3f) / 0.7f) * fade;
        g.l.push_opacity(a);
        mark(g, 120.0f, 92.0f, 46.0f, 1.0f, kWhite, k.accent);
        const float ww = text(g, g.f.display, "PROSPERO", 166.0f, 101.0f, 23.0f, kWhite.with_alpha(0.72f), gfx::Align::left, 6.5f);
        text(g, g.f.display, "TUBE", 166.0f + ww + 6.5f, 101.0f, 23.0f, kWhite, gfx::Align::left, 6.5f);
        const float swap = hero_fade_.running ? tween::smoothstep(cross) : 1.0f;
        g.l.push_opacity(swap);
        overline(g, hero_label_, 98.0f, 838.0f, k.accent);
        text(g, g.f.display, fit(g.f.display, hero_now_.title, 46.0f, 1700.0f), 96.0f, 902.0f, 46.0f, kWhite);
        avatar(g, 114.0f, 946.0f, 18.0f, hero_now_.channel, hero_now_.channel_id, context.images.get(hero_now_.avatar, PictureSize::avatar));
        const float cw = text(g, g.f.semibold, fit(g.f.semibold, hero_now_.channel, kTypeBody, 700.0f), 144.0f, 954.0f, kTypeBody, kWhite);
        text(g, g.f.regular, fit(g.f.regular, meta_of(hero_now_), kTypeBody, 700.0f), 144.0f + cw + 18.0f, 954.0f, kTypeBody, kWhite.with_alpha(0.72f));
        g.l.pop_opacity();
        // Time until the next one.
        g.l.rounded_rect({96.0f, 1004.0f, 1728.0f, 3.0f}, 1.5f, kWhite.with_alpha(0.16f));
        g.l.rounded_rect({96.0f, 1004.0f, 1728.0f * tween::clamp01(attract_step_ / kAttractStep), 3.0f}, 1.5f, k.accent);
        const ui::Hint items[] = {{ui::Button::dpad, tr("Back to Home")}};
        ui::HintLayout layout;
        layout.size = 34.0f;
        layout.text_size = 22.0f;
        layout.cy = 946.0f;
        layout.item_gap = 36.0f;
        ui::draw_hints(g.l, g.f, ui::GlyphStyle::dark(), items, 1, kRight, true, layout);
        g.l.pop_opacity();
    }
}

std::string Home::meta_of(const model::Video &video)
{
    std::string meta = video.views;
    if (!video.age.empty())
        meta += (meta.empty() ? "" : " \xC2\xB7 ") + video.age;
    return meta;
}

void Home::draw_state(G &g) const
{
    const Look &k = g.k;
    if (load_ == model::Load::loading || load_ == model::Load::idle)
    {
        // Bones in the shape of the page: a title, a play line, a shelf.
        bone(g, {kLeft, 250.0f, 720.0f, 46.0f}, 12.0f, 0);
        bone(g, {kLeft, 312.0f, 520.0f, 46.0f}, 12.0f, 1);
        bone(g, {kLeft, 412.0f, 330.0f, 60.0f}, 30.0f, 2);
        bone(g, {kLeft, kShelfTop - 46.0f, 260.0f, 26.0f}, 8.0f, 3);
        for (int i = 0; i < kBones; ++i)
            bone(g, {kLeft + static_cast<float>(i) * kCardPitch, kShelfTop, kCardW, kCardH}, kRadiusThumb, 4 + i);
        return;
    }
    const bool offline = load_ == model::Load::offline;
    const bool failed = load_ == model::Load::failed;
    Enter enter(g, arrive(2, 0.06f, 0.5f), 18.0f);
    const Rect panel{kLeft, 300.0f, 980.0f, 300.0f};
    glass_panel(g, panel, kRadiusPanel);
    icon(g, offline ? Icon::offline : failed ? Icon::alert : Icon::home, panel.x + 70.0f, panel.y + 84.0f, 52.0f, offline || failed ? k.accent : k.ink2);
    const char *title = offline ? tr("No connection to YouTube") : failed ? tr("Home could not be loaded") : tr("Nothing here yet");
    text(g, g.f.display, fit(g.f.display, title, kTypeTitle + 4.0f, panel.w - 170.0f), panel.x + 124.0f, panel.y + 98.0f, kTypeTitle + 4.0f, k.ink);
    const std::string body = offline  ? std::string(tr("The built-in service cannot reach YouTube. Check the network of this PS5, then try again."))
                             : failed ? (error_.empty() ? std::string(tr("Try again in a moment.")) : error_)
                                      : std::string(tr("Search for something, or open a topic from the rail."));
    paragraph(g, g.f.regular, body, panel.x + 46.0f, panel.y + 168.0f, kTypeBody, panel.w - 92.0f, 36.0f, k.ink2, 3);
}

void Home::draw_hero(const DrawContext &context, G &g, const model::Video &video, const std::string &label, float alpha, float slide) const
{
    if (alpha <= 0.01f || video.id.empty())
        return;
    const Look &k = g.k;
    const float x = kLeft + slide;
    const float play_y = rows_.front().topics() ? 372.0f : 412.0f;
    g.l.push_opacity(tween::clamp01(alpha));

    // The block is anchored at its foot: a longer title grows upward.
    const float title_w = invite_ ? 1060.0f : 900.0f;
    const TextBlock block = measure_block(g.f.display, video.title, 52.0f, title_w, 60.0f, 2);
    const int lines = std::max(1, block.lines);
    const float last = play_y - 88.0f;
    const float first = last - 60.0f * static_cast<float>(lines - 1);
    if (!label.empty())
        overline(g, label, x + 2.0f, first - 64.0f, k.accent);
    paragraph(g, g.f.display, video.title, x - 3.0f, first, 52.0f, title_w, 60.0f, k.ink, 2);

    // Channel, numbers, what the video offers.
    const float my = last + 46.0f;
    avatar(g, x + 18.0f, my - 8.0f, 18.0f, video.channel, video.channel_id, context.images.get(video.avatar, PictureSize::avatar));
    float cursor = x + 48.0f;
    cursor += text(g, g.f.semibold, fit(g.f.semibold, video.channel, kTypeBody, 340.0f), cursor, my, kTypeBody, k.ink) + 14.0f;
    const std::string meta = meta_of(video);
    if (!meta.empty())
    {
        g.l.circle(cursor, my - 8.0f, 2.5f, k.ink3);
        cursor += 14.0f;
        cursor += text(g, g.f.regular, fit(g.f.regular, meta, kTypeBody, std::max(120.0f, x + 800.0f - cursor)), cursor, my, kTypeBody, k.ink2) + 18.0f;
    }
    // The line ends before the picture begins: tags only where they still stand on the page.
    if ((video.badges & model::kBadge4K) != 0 && cursor < x + 820.0f)
        cursor += tag(g, cursor, my - 8.0f, "4K", k.raised, k.ink) + 8.0f;
    if ((video.badges & model::kBadgeCaptions) != 0 && cursor < x + 820.0f)
        cursor += tag(g, cursor, my - 8.0f, "CC", k.raised, k.ink) + 8.0f;

    // The play line: what Cross does, and how much is left.
    const bool live = (video.badges & model::kBadgeLive) != 0;
    const bool resume = video.progress > 0.02f && video.progress < 0.97f && video.duration_s > 0;
    const char *verb = live ? tr("Watch live") : resume ? tr("Resume") : tr("Play");
    std::string right;
    if (live)
        right = video.views;
    else if (resume)
        right = tr_n("{0} min left", "{0} min left", std::max(1, static_cast<int>((1.0f - video.progress) * static_cast<float>(video.duration_s) / 60.0f + 0.5f)));
    else
        right = duration_text(video.duration_s);
    const float verb_w = measure(g.f.semibold, verb, kTypeBody);
    const float right_w = measure(g.f.regular, right, 22.0f);
    const float bar_w = resume ? 190.0f : 0.0f;
    const float width = 66.0f + verb_w + (resume ? 24.0f + bar_w : 0.0f) + (right.empty() ? 0.0f : 22.0f + right_w) + 30.0f + (live ? 22.0f : 0.0f);
    const Rect line{x, play_y, width, 60.0f};
    lift(g, line, 30.0f, 0.5f);
    glass_panel(g, line, 30.0f);
    const ui::GlyphStyle glyphs = k.dark ? ui::GlyphStyle::dark() : ui::GlyphStyle::light();
    ui::draw_button(g.l, g.f, glyphs, ui::Button::cross, line.x + 14.0f, line.cy(), 36.0f);
    float px = line.x + 62.0f;
    px += text(g, g.f.semibold, verb, px, line.cy() + 8.5f, kTypeBody, k.ink) + 24.0f;
    if (resume)
    {
        g.l.rounded_rect({px, line.cy() - 3.0f, bar_w, 6.0f}, 3.0f, k.ink.with_alpha(0.2f));
        g.l.rounded_rect({px, line.cy() - 3.0f, bar_w * video.progress, 6.0f}, 3.0f, k.accent);
        px += bar_w + 22.0f;
    }
    if (live)
    {
        const float pulse = g.reduced ? 0.5f : ui::breathe(g.time, 1.6f);
        g.l.circle(px + 6.0f, line.cy(), 6.0f + 3.0f * pulse, k.accent.with_alpha(0.3f));
        g.l.circle(px + 6.0f, line.cy(), 5.0f, k.accent);
        px += 22.0f;
    }
    text(g, g.f.regular, right, px, line.cy() + 7.5f, 22.0f, k.ink2);
    g.l.pop_opacity();
}

void Home::draw_shelf(const DrawContext &context, G &g, const Row &row, float y, float alpha, bool focused_row, float ring_cx, int stagger_base) const
{
    // A shelf that only peeks over the foot of the screen shows its pictures,
    // not text that the edge would cut.
    const int with_text = y + kCardH + 110.0f < 960.0f ? 1 : (y + kCardH + 44.0f < 992.0f ? 3 : 0);
    if (alpha <= 0.01f)
        return;
    const Look &k = g.k;
    const RowState &s = state_of(row);
    g.l.push_opacity(alpha);
    {
        Enter enter(g, arrive(stagger_base, 0.06f, 0.5f), 18.0f);
        text(g, g.f.semibold, fit(g.f.semibold, row.shelf.title, kTypeShelf, 900.0f), kLeft, y - 24.0f, kTypeShelf, focused_row ? k.ink : k.ink2);
    }
    const float pitch = row.topics() ? kTilePitch : kCardPitch;
    const float w = row.topics() ? kTileW : kCardW;
    const float h = row.topics() ? kTileH : kCardH;
    int focused = -1;
    int shown = 0;
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int c = 0; c < row.count(); ++c)
        {
            Rect r{kLeft + static_cast<float>(c) * pitch - s.scroll.offset(), y, w, h};
            if (r.x > gfx::kVirtualWidth + 40.0f || r.x + r.w < kRail - 40.0f)
                continue;
            const float focus = focused_row ? tween::clamp01(1.0f - std::fabs(ring_cx - r.cx()) / (pitch * 0.7f)) : 0.0f;
            const bool is_focused = focused_row && c == s.col;
            if (pass == 0 && is_focused)
            {
                focused = c;
                continue; // drawn last, on top of its neighbours
            }
            if (pass == 1 && c != focused)
                continue;
            const float in = arrive(stagger_base + 1 + (pass == 0 ? shown++ : 0), 0.055f, 0.5f);
            Enter enter(g, in, 30.0f);
            // The focused card grows about its centre and lifts.
            const float grow = 0.06f * focus;
            r = {r.x - r.w * grow * 0.5f, r.y - r.h * grow * 0.5f - 6.0f * focus, r.w * (1.0f + grow), r.h * (1.0f + grow)};
            // Cards slide under the rail's edge rather than over it.
            const float edge = tween::clamp01((r.x + r.w - kRail) / 120.0f);
            // What the edge of the screen cuts is a picture leaving the page: it is clipped there
            // with whatever is on it, and it carries no words underneath.
            const bool cut = r.x + r.w > gfx::kVirtualWidth - 4.0f || r.y + r.h > gfx::kVirtualHeight - 4.0f;
            if (cut)
                g.l.push_clip({0.0f, 0.0f, gfx::kVirtualWidth, gfx::kVirtualHeight});
            if (row.topics())
            {
                topic_tile(g, r, row.shelf.topics[static_cast<std::size_t>(c)], focus, edge);
            }
            else
            {
                const model::Video &video = row.shelf.videos[static_cast<std::size_t>(c)];
                video_card(g, r, video, context.images.get(video.thumb, PictureSize::card), focus, edge, cut ? 0 : with_text);
            }
            if (cut)
                g.l.pop_clip();
        }
    }
    g.l.pop_opacity();
}

} // namespace afterglow::home_page
