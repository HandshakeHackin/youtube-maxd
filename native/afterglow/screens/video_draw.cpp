// ProsperoTube Afterglow - the video page, drawn.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The approved board: the picture on the right, carried in from the card it
// came from; on the left what the video is (state, title, numbers, channel,
// the description's first lines, what it carries) and what can be done with
// it; the related shelf under both. Over it, when asked: the rest of the
// actions from the right, or the link to take away.

#include "afterglow/screens/video.hpp"

#include <algorithm>
#include <cmath>

namespace afterglow::video_page
{

namespace
{

constexpr float kLate = 0.12f; // the furniture waits a moment for the picture

// A tracked label in capitals takes more room than its letters: it is cut until it fits.
std::string fit_overline(G &g, std::string label, float width)
{
    float room = width;
    while (room > 80.0f && measure(g.f.semibold, ui::upper(label), kTypeOver, 3.2f) > width)
    {
        room -= 40.0f;
        label = fit(g.f.semibold, label, kTypeOver, room);
    }
    return label;
}

// Where the mark of a labelled button sits (design.cpp's button()).
float button_icon_x(G &g, const Rect &r, std::string_view label)
{
    if (label.empty())
        return r.cx();
    return r.cx() - (measure(g.f.semibold, label, kTypeBody) + 38.0f) * 0.5f + 13.0f;
}

} // namespace

// The entrance of the nth group of the page.
float VideoPage::arrive(int index) const
{
    return tween::stagger(age_ - kLate, index, 0.055f, 0.45f);
}

// A bone: the kit's skeleton, one block at a time, all on one clock.
void VideoPage::bone(G &g, const Rect &r, float radius) const
{
    // REQUEST: update() has no Look, so a kit component is given the look's theme on a copy here.
    ui::Skeleton bones = bones_;
    bones.style.theme = g.k.theme;
    bones.style.reduced_motion = g.reduced;
    bones.style.kind = ui::SkeletonKind::block;
    bones.style.radius = radius;
    bones.set_bounds(r);
    ui::Canvas canvas{g.l, g.f, g.glass, g.time};
    bones.draw(canvas);
}

void VideoPage::bone_disc(G &g, float cx, float cy, float radius) const
{
    bone(g, {cx - radius, cy - radius, radius * 2.0f, radius * 2.0f}, radius);
}

void VideoPage::draw_spinner(const DrawContext &context, float cx, float cy, float size, Color color) const
{
    // REQUEST: update() has no Look, so a kit component is given the look's
    // theme on a copy here instead of once when it changes.
    ui::Spinner spinner = spinner_;
    spinner.style.theme = context.look.theme;
    spinner.style.reduced_motion = context.reduced_motion;
    spinner.style.color = color;
    spinner.style.thickness = 2.5f;
    spinner.set_bounds({cx - size * 0.5f, cy - size * 0.5f, size, size});
    ui::Canvas canvas{context.ui, context.fonts, context.glass, context.time};
    spinner.draw(canvas);
}

// ---- what it is ---------------------------------------------------------------------------

void VideoPage::draw_details(G &g, const DrawContext &context) const
{
    const Look &k = g.k;
    const Details &d = details_;
    {
        Enter enter(g, arrive(1), 20.0f);
        std::string label;
        if (d.live)
            label = tr("Live now");
        else if (d.upcoming)
            label = tr("Upcoming");
        else if (d.resume_s > 0 && d.duration_s > 0)
            label = fill(tr("Continue watching  \xC2\xB7  {0} of {1}"), duration_text(d.resume_s), duration_text(d.duration_s));
        if (!label.empty())
            overline(g, fit_overline(g, label, kColumn), kLeft + 2.0f, kOverlineY, k.accent);
        paragraph(g, g.f.display, d.title, kLeft - 2.0f, kTitleY, kTypePage, kColumn, kTitleLine, k.ink, 3);
    }
    {
        // The numbers, and what the stream offers.
        Enter enter(g, arrive(2), 20.0f);
        std::string fps;
        if (d.fps >= 50)
            fps = fill(tr("{0} FPS"), std::to_string(d.fps));
        const char *marks[4] = {d.uhd ? "4K" : nullptr, d.hdr ? "HDR" : nullptr, d.captions > 0 ? "CC" : nullptr,
                                fps.empty() ? nullptr : fps.c_str()};
        float tags = 0.0f;
        for (const char *mark : marks)
            tags += mark != nullptr ? tag_width(g, mark) + 8.0f : 0.0f;
        std::string meta = d.views;
        if (!d.age.empty())
            meta += (meta.empty() ? "" : " \xC2\xB7 ") + d.age;
        const float room = kColumn - tags - (tags > 0.0f ? 18.0f : 0.0f);
        float x = kLeft;
        if (!meta.empty())
            x += text(g, g.f.regular, fit(g.f.regular, meta, kTypeLine, room), kLeft, meta_y_, kTypeLine, k.ink2) + 18.0f;
        for (const char *mark : marks)
        {
            if (mark != nullptr)
                x += tag(g, x, meta_y_ - 8.0f, mark, k.raised, k.ink) + 8.0f;
        }
    }
    if (!d.channel.empty())
    {
        // The channel, with its own button.
        Enter enter(g, arrive(3), 20.0f);
        const float y = channel_y_;
        const Rect sub = subscribe_rect();
        const float room = (can_subscribe() ? sub.x : kLeft + kColumn) - (kLeft + 70.0f) - 20.0f;
        avatar(g, kLeft + 27.0f, y - 8.0f, 27.0f, d.channel, d.channel_id, context.images.get(d.avatar, PictureSize::avatar));
        text(g, g.f.semibold, fit(g.f.semibold, d.channel, kTypeName, room), kLeft + 70.0f, y - 6.0f, kTypeName, k.ink);
        text(g, g.f.regular, fit(g.f.regular, d.subscribers, kTypeFine, room), kLeft + 70.0f, y + 20.0f, kTypeFine, k.ink3);
        if (can_subscribe())
        {
            const bool on = subscribed();
            const bool waiting = request_.kind == Request::Kind::subscribe || request_.kind == Request::Kind::unsubscribe;
            Rect r = sub;
            const float pop = g.reduced ? 0.0f : subscribe_pop_.value;
            if (pop > 0.01f)
            {
                g.l.glow(r, r.h * 0.5f, 40.0f * pop, k.accent.with_alpha(0.5f * pop));
                r = grown(r, 0.1f * pop);
            }
            const char *label = on ? tr("Subscribed") : tr("Subscribe");
            g.l.push_opacity(waiting ? 0.55f : 1.0f);
            // The label is cut while the button is still growing to its width.
            button(g, r, fit(g.f.semibold, label, kTypeBody, r.w - 70.0f), on ? Icon::check : Icon::plus, channel_focus_.value, false, on);
            g.l.pop_opacity();
            if (waiting)
                draw_spinner(context, button_icon_x(g, r, label), r.cy(), 34.0f, channel_focus_.value > 0.5f ? k.on_plate : k.ink);
        }
    }
    {
        Enter enter(g, arrive(4), 20.0f);
        if (description_lines_ > 0)
            paragraph(g, g.f.regular, d.overview, kLeft, description_y_, kTypeRead, kColumn, 32.0f, k.ink2, description_lines_);
        // What the video carries, at a glance.
        const std::string facts[3] = {
            d.chapters > 0 ? counted(d.chapters, tr("{0} chapter"), tr("{0} chapters")) : std::string(),
            d.segments > 0 ? counted(d.segments, tr("{0} segment skipped"), tr("{0} segments skipped")) : std::string(),
            d.captions > 1 ? counted(d.captions, tr("Captions: {0} language"), tr("Captions: {0} languages")) : std::string()};
        const Icon marks[3] = {Icon::chapters, Icon::skip, Icon::captions};
        float x = kLeft;
        for (int i = 0; i < 3; ++i)
        {
            if (facts[i].empty())
                continue;
            const float w = measure(g.f.regular, facts[i], kTypeMeta) + 62.0f;
            if (x + w > kHero.x - 24.0f)
                break; // a longer language shows the facts that fit beside the picture
            const Rect pill{x, chips_y_, w, 40.0f};
            g.l.rounded_rect(pill, 20.0f, k.raised);
            icon(g, marks[i], pill.x + 24.0f, pill.cy(), 22.0f, k.ink2);
            text(g, g.f.regular, facts[i], pill.x + 46.0f, pill.cy() + 7.0f, kTypeMeta, k.ink2);
            x += w + 10.0f;
        }
    }
}

// The page before its video is known: the same places, as bones.
void VideoPage::draw_bones(G &g) const
{
    Enter enter(g, arrive(1), 20.0f);
    bone(g, {kLeft + 2.0f, kOverlineY - 14.0f, 300.0f, 16.0f}, 8.0f);
    bone(g, {kLeft, kTitleY - 36.0f, kColumn - 60.0f, 40.0f}, 12.0f);
    bone(g, {kLeft, kTitleY + kTitleLine - 36.0f, kColumn * 0.62f, 40.0f}, 12.0f);
    const float meta = kTitleY + kTitleLine + 44.0f;
    bone(g, {kLeft, meta - 18.0f, 280.0f, 22.0f}, 11.0f);
    const float channel = meta + 60.0f;
    bone_disc(g, kLeft + 27.0f, channel - 8.0f, 27.0f);
    bone(g, {kLeft + 70.0f, channel - 26.0f, 220.0f, 22.0f}, 11.0f);
    bone(g, {kLeft + 70.0f, channel + 6.0f, 150.0f, 16.0f}, 8.0f);
    for (int i = 0; i < 3; ++i)
        bone(g, {kLeft, channel + 48.0f + 32.0f * static_cast<float>(i), kColumn * (i == 2 ? 0.7f : 1.0f), 20.0f}, 10.0f);
}

// The video could not be read: what the provider says, and one way on.
void VideoPage::draw_failure(G &g) const
{
    Enter enter(g, arrive(1), 20.0f);
    const bool offline = load_ == model::Load::offline;
    g.l.circle(kLeft + 44.0f, 280.0f, 44.0f, g.k.raised);
    icon(g, offline ? Icon::offline : Icon::alert, kLeft + 44.0f, 280.0f, 46.0f, g.k.ink);
    const float under = paragraph(g, g.f.display, offline ? tr("No connection to YouTube") : tr("This video could not be opened"),
                                  kLeft - 2.0f, 392.0f, kTypePage, kColumn, kTitleLine, g.k.ink, 2);
    paragraph(g, g.f.regular, error_.empty() ? tr("Try again in a moment.") : error_.c_str(), kLeft, under + 4.0f, kTypeBody, kColumn, 34.0f,
              g.k.ink2, 3);
}

// ---- what can be done with it ----------------------------------------------------------------

void VideoPage::draw_actions(G &g, const DrawContext &context) const
{
    const Look &k = g.k;
    Enter enter(g, arrive(5), 22.0f);
    const float shake = g.reduced || zone_ != Zone::actions ? 0.0f : ui::shake(nudge_.value, g.time, 14.0f, 8.0f) * nudge_dir_;
    float end = kLeft;
    for (std::size_t i = 0; i < buttons_.size(); ++i)
    {
        const ActionButton &b = buttons_[i];
        const float focus = zone_ == Zone::actions && layer_ == Layer::none
                                ? tween::clamp01(1.0f - std::fabs(action_pos_.value - static_cast<float>(i)))
                                : zone_ == Zone::actions ? (b.act == action_ ? 0.35f : 0.0f) : 0.0f;
        Rect r{b.x + shake * focus, kActionY, b.w, kActionH};
        // A confirmation: the button swells, light goes out from it.
        const float pop = g.reduced ? 0.0f : b.act == Act::like ? like_pop_.value : b.act == Act::later ? saved_pop_.value : 0.0f;
        if (pop > 0.01f)
        {
            g.l.glow(r, r.h * 0.5f, 40.0f * pop, k.accent.with_alpha(0.5f * pop));
            r = grown(r, 0.1f * pop);
        }
        g.l.push_opacity(b.waiting ? 0.55f : 1.0f);
        button(g, r, b.label, b.icon, focus, b.primary, b.selected);
        g.l.pop_opacity();
        if (b.waiting)
            draw_spinner(context, button_icon_x(g, r, b.label), r.cy(), 36.0f, focus > 0.5f || b.primary ? k.on_plate : k.ink);
        end = b.x + b.w + kActionGap;
    }
    if (!have_ && load_ != model::Load::failed && load_ != model::Load::offline)
    {
        // The buttons that need the video's own numbers.
        static constexpr float kWidths[] = {172.0f, 230.0f, 174.0f, 164.0f, kActionH};
        for (std::size_t i = 0; i < std::size(kWidths); ++i)
        {
            bone(g, {end, kActionY, kWidths[i], kActionH}, kActionH * 0.5f);
            end += kWidths[i] + kActionGap;
        }
    }
}

// ---- what to watch after ----------------------------------------------------------------------

void VideoPage::draw_related(G &g, const DrawContext &context) const
{
    const Look &k = g.k;
    const bool failed = !have_ && (load_ == model::Load::failed || load_ == model::Load::offline);
    if (failed)
        return;
    {
        Enter enter(g, arrive(6), 22.0f);
        text(g, g.f.semibold, tr("Related"), kLeft, kRelatedY - 22.0f, kTypeShelf, zone_ == Zone::related ? k.ink : k.ink2);
    }
    const bool waiting = !have_ || (related_.empty() && (related_load_ == model::Load::loading || related_load_ == model::Load::idle) &&
                                    !details_.related.empty());
    if (waiting)
    {
        for (int i = 0; i < 6; ++i)
        {
            Enter enter(g, arrive(7 + i), 26.0f);
            const Rect r = related_rect(i, 0.0f);
            bone(g, r, kRadiusThumb);
            bone(g, {r.x + 2.0f, r.y + r.h + 14.0f, r.w * 0.8f, 18.0f}, 9.0f);
        }
        return;
    }
    if (related_.empty())
    {
        Enter enter(g, arrive(7), 22.0f);
        const bool broken = related_load_ == model::Load::failed || related_load_ == model::Load::offline;
        icon(g, broken ? (related_load_ == model::Load::offline ? Icon::offline : Icon::alert) : Icon::info, kLeft + 16.0f, kRelatedY + 30.0f,
             30.0f, k.ink3);
        text(g, g.f.regular,
             fit(g.f.regular, broken ? tr("The related videos could not be loaded.") : tr("YouTube names no related videos for this one."),
                 kTypeRead, kRight - kLeft - 60.0f),
             kLeft + 46.0f, kRelatedY + 38.0f, kTypeRead, k.ink3);
        return;
    }
    const int count = static_cast<int>(related_.size());
    const Rect ring = ring_.value();
    const float shake = g.reduced || zone_ != Zone::related ? 0.0f : ui::shake(nudge_.value, g.time, 14.0f, 8.0f) * nudge_dir_;
    // The shelf runs off the right edge on purpose: the card that is cut says there is more.
    g.l.push_clip({kRail, kRelatedY - 60.0f, gfx::kVirtualWidth - kRail, 300.0f});
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int i = 0; i < count + (related_more_ ? 1 : 0); ++i)
        {
            const bool focused = zone_ == Zone::related && i == related_col_;
            if ((pass == 1) != focused)
                continue;
            Rect r = related_rect(i, related_scroll_.offset());
            if (r.x > gfx::kVirtualWidth || r.x + r.w < kRail)
                continue;
            Enter enter(g, arrive(7 + std::min(std::abs(i - related_col_), 5)), 26.0f);
            if (i >= count)
            {
                bone(g, r, kRadiusThumb); // the next page of the list, on its way
                continue;
            }
            const float focus = zone_ == Zone::related
                                    ? tween::clamp01(1.0f - std::fabs(ring.x + ring.w * 0.5f - r.cx()) / (kRelatedPitch * 0.7f))
                                    : 0.0f;
            r = grown(r, 0.06f * focus, 5.0f * focus);
            r.x += shake * focus;
            const model::Video &video = related_[static_cast<std::size_t>(i)];
            const float alpha = tween::clamp01((r.x + r.w - kRail) / 120.0f);
            video_card(g, r, video, context.images.get(video.thumb, PictureSize::card), focus, alpha, 0);
            g.l.push_opacity(alpha);
            text(g, g.f.semibold, fit(g.f.semibold, video.title, kTypeMeta, r.w - 4.0f), r.x + 2.0f, r.y + r.h + 30.0f, kTypeMeta,
                 k.ink.with_alpha(0.8f + 0.2f * focus));
            g.l.pop_opacity();
        }
    }
    g.l.pop_clip();
    if (zone_ == Zone::related && layer_ == Layer::none)
    {
        const Picture picture = context.images.get(related_[static_cast<std::size_t>(related_col_)].thumb, PictureSize::card);
        Rect at = ring;
        at.x += shake;
        focus_ring(g, at, kRadiusThumb, picture.ready() ? picture.glow : k.focus, arrive(8));
    }
}

// ---- the picture: it came from a card ----------------------------------------------------------

void VideoPage::draw_hero(G &g, const DrawContext &context, const Phase &phase, const Motion &motion) const
{
    const Look &k = g.k;
    if (!have_ && (load_ == model::Load::failed || load_ == model::Load::offline))
        return;
    const bool leaving = phase.out > 0.0f;
    const bool entering = !leaving && phase.in < 1.0f;
    const bool carried = phase.carried.w > 0.0f && phase.carried.h > 0.0f && !g.reduced;
    Rect hero = kHero;
    float alpha = g.reduced ? 1.0f : tween::clamp01((age_ - 0.05f) / 0.3f); // assembling with the page
    if (entering && phase.move == Move::zoom && carried)
    {
        hero = lerp_rect(phase.carried, kHero, tween::quint_out(phase.in)); // the card's picture grows into the page's
        alpha = 1.0f;
    }
    else if (leaving && phase.move == Move::shrink && carried)
    {
        hero = lerp_rect(phase.carried, kHero, 1.0f - tween::cubic_in_out(phase.out)); // ... and returns to its card
        alpha = 1.0f - tween::smoothstep((phase.out - 0.84f) / 0.16f);
    }
    else if (leaving)
    {
        alpha = motion.furniture;
    }
    if (alpha <= 0.01f)
        return;
    const float settled = tween::clamp01((hero.w - kRelatedW) / (kHero.w - kRelatedW));
    const float radius = tween::lerp(kRadiusThumb, kRadiusHero, settled);
    // REQUEST: a hand-over carries a rectangle and no picture, so a page that
    // arrives before its details draws the design's resting box where the
    // card's picture should already be.
    const Picture large = context.images.get(details_.thumb, PictureSize::hero);
    const float sharp = large.ready() ? (g.reduced ? 1.0f : tween::clamp01(large.age / 0.25f)) : 0.0f;
    Picture small;
    if (sharp < 1.0f)
        small = context.images.get(details_.thumb, PictureSize::card); // what the card showed, until the larger one is there
    const Color glow = large.ready() ? large.glow : small.ready() ? small.glow : k.raised;
    g.l.push_opacity(alpha);
    if (large.ready() || small.ready())
        g.l.glow(hero.inset(40.0f), 60.0f, 110.0f, glow.with_alpha(0.22f * settled));
    if (!details_.thumb.empty())
        lift(g, hero, radius, 1.2f);
    if (details_.thumb.empty())
        bone(g, hero, radius); // the video is not known yet
    else if (!small.ready() && sharp < 1.0f)
        picture_placeholder(g, hero, radius);
    if (small.ready() && sharp < 1.0f)
        g.l.image(small.texture, hero, small.cover(hero.w / hero.h), kWhite, radius);
    if (sharp > 0.0f)
        g.l.image(large.texture, hero, large.cover(hero.w / hero.h), kWhite.with_alpha(sharp), radius);
    g.l.bordered_rect(hero, radius, clear(), 1.5f, kWhite.with_alpha(0.14f));
    if (settled > 0.6f && details_.resume_s > 0 && details_.duration_s > 0)
    {
        g.l.push_opacity(tween::clamp01((settled - 0.6f) / 0.4f));
        progress(g, {hero.x + 20.0f, hero.y + hero.h - 14.0f, hero.w - 40.0f, 6.0f},
                 static_cast<float>(details_.resume_s) / static_cast<float>(details_.duration_s), k.accent);
        g.l.pop_opacity();
    }
    g.l.pop_opacity();
}

// ---- over it: the rest of the actions, or the link to take away ----------------------------------

void VideoPage::draw_sheet(G &g, const DrawContext &context, float amount) const
{
    const Look &k = g.k;
    const float slide = g.reduced ? 0.0f : (1.0f - amount) * 120.0f;
    g.l.push_opacity(amount);
    g.l.push_transform(1.0f, 0.0f, 0.0f, slide, 0.0f);
    g.l.shadow({kSheet.x - 20.0f, kSheet.y, 80.0f, kSheet.h}, 0.0f, 80.0f, kBlack.with_alpha(0.5f));
    glass_panel(g, kSheet, kRadiusSheet, 1.25f);
    const float x = kSheetList.x + 26.0f;
    text(g, g.f.display, fit(g.f.display, sheet_title_.empty() ? tr("More actions") : sheet_title_.c_str(), kTypePanel, 480.0f), x, 120.0f,
         kTypePanel, k.ink);
    text(g, g.f.regular, fit(g.f.regular, details_.title, kTypeNote, 480.0f), x + 2.0f, 156.0f, kTypeNote, k.ink2);
    if (!sheet_rows_.empty())
    {
        // REQUEST: update() has no Look, so the list is given the look's theme on a copy here.
        ui::ListView list = list_;
        list.style.theme = context.look.theme;
        list.style.reduced_motion = context.reduced_motion;
        list.leading = [&](ui::Canvas &, const Rect &row, const ui::ListItem &item, int, float focus)
        {
            const std::size_t at = static_cast<std::size_t>(std::clamp(item.tag, 0, static_cast<int>(sheet_rows_.size()) - 1));
            icon(g, sheet_rows_[at].icon, row.x + 44.0f, row.cy(), 26.0f, gfx::mix(k.ink2, k.on_plate, focus));
        };
        ui::Canvas canvas{context.ui, context.fonts, context.glass, context.time};
        list.draw(canvas);
    }
    else if (sheet_load_ == model::Load::loading || sheet_load_ == model::Load::idle)
    {
        for (int i = 0; i < 6; ++i)
            bone(g, {kSheetList.x, kSheetList.y + 72.0f * static_cast<float>(i), kSheetList.w, 68.0f}, 34.0f);
    }
    else
    {
        const bool broken = sheet_load_ != model::Load::ready;
        icon(g, broken ? (sheet_load_ == model::Load::offline ? Icon::offline : Icon::alert) : Icon::info, x + 16.0f, kSheetList.y + 34.0f,
             32.0f, k.ink2);
        paragraph(g, g.f.regular,
                  broken ? (sheet_error_.empty() ? tr("The actions could not be loaded.") : sheet_error_.c_str())
                         : tr("Nothing more can be done with this video on this profile."),
                  x + 48.0f, kSheetList.y + 42.0f, kTypeBody, 420.0f, 34.0f, k.ink2, 5);
    }
    g.l.pop_transform();
    g.l.pop_opacity();
}

void VideoPage::draw_share(G &g, float amount) const
{
    const Look &k = g.k;
    const Rect &panel = kSharePanel;
    g.l.push_opacity(amount);
    g.l.push_transform(g.reduced ? 1.0f : 0.94f + 0.06f * amount, 960.0f, 540.0f, 0.0f, g.reduced ? 0.0f : 24.0f * (1.0f - amount));
    lift(g, panel, kRadiusDialog, 1.4f);
    glass_panel(g, panel, kRadiusDialog, 1.3f);
    const Rect code{panel.x + 56.0f, panel.y + 56.0f, 420.0f, 420.0f};
    const float x = panel.x + 532.0f;
    const float room = 432.0f;
    const ShareLink *link = links_.empty() ? nullptr : &links_[static_cast<std::size_t>(link_)];
    overline(g, fit_overline(g, tr("Share"), room), x, panel.y + 92.0f, k.accent);
    text(g, g.f.display, fit(g.f.display, tr("Take it with you"), kTypeDialog, room), x - 2.0f, panel.y + 148.0f, kTypeDialog, k.ink);
    if (link != nullptr)
    {
        // A code is read by a camera: dark on light in every look.
        qr_code(g, code, link->url, kBlack, kWhite);
        const std::string body = link->id == "time"
                                     ? fill(tr("Point your phone's camera at the code, or type the address. The link opens at {0}."),
                                            duration_text(details_.resume_s))
                                     : std::string(tr("Point your phone's camera at the code, or type the address."));
        paragraph(g, g.f.regular, body, x, panel.y + 196.0f, kTypeRead, room - 12.0f, 32.0f, k.ink2, 3);
        const Rect address{x, panel.y + 316.0f, room, 60.0f};
        g.l.rounded_rect(address, kRadiusField, k.raised);
        std::string_view shown = link->url;
        if (starts_with(shown, "https://"))
            shown.remove_prefix(8);
        if (starts_with(shown, "www."))
            shown.remove_prefix(4);
        text(g, g.f.mono, fit(g.f.mono, shown, kTypeRead, address.w - 44.0f), address.x + 22.0f, address.cy() + 7.5f, kTypeRead, k.ink);
        for (std::size_t i = 0; i < links_.size(); ++i)
        {
            const bool chosen = static_cast<int>(i) == link_;
            chip(g, links_[i].chip.x, links_[i].chip.cy(), links_[i].label, chosen, chosen ? 1.0f : 0.0f);
        }
    }
    else if (links_load_ == model::Load::failed || links_load_ == model::Load::offline)
    {
        g.l.rounded_rect(code, 18.0f, k.raised);
        icon(g, links_load_ == model::Load::offline ? Icon::offline : Icon::alert, code.cx(), code.cy(), 72.0f, k.ink3);
        paragraph(g, g.f.regular, links_error_.empty() ? tr("The link could not be loaded.") : links_error_.c_str(), x, panel.y + 196.0f,
                  kTypeRead, room - 12.0f, 32.0f, k.ink2, 4);
    }
    else
    {
        bone(g, code, 18.0f);
        bone(g, {x, panel.y + 178.0f, room - 40.0f, 20.0f}, 10.0f);
        bone(g, {x, panel.y + 210.0f, room - 120.0f, 20.0f}, 10.0f);
        bone(g, {x, panel.y + 316.0f, room, 60.0f}, kRadiusField);
    }
    text(g, g.f.regular, fit(g.f.regular, details_.title, kTypeLine, 420.0f), panel.x + 56.0f, panel.y + 540.0f, kTypeLine, k.ink);
    std::string line = details_.channel;
    const std::string length = duration_text(details_.duration_s);
    if (!length.empty())
        line += (line.empty() ? "" : " \xC2\xB7 ") + length;
    text(g, g.f.regular, fit(g.f.regular, line, kTypeMeta, 420.0f), panel.x + 56.0f, panel.y + 572.0f, kTypeMeta, k.ink3);
    // The panel's own hints: the page's row is under the scrim.
    static ui::Hint items[2];
    items[0] = {ui::Button::dpad, tr("Which link")};
    items[1] = {ui::Button::circle, tr("Close")};
    g.l.push_transform(1.0f, 0.0f, 0.0f, 0.0f, panel.y + panel.h - 56.0f - kHintY);
    afterglow::hints(g, items, links_.size() > 1 ? 2 : 1, 1.0f, false, panel.x + panel.w - 56.0f);
    g.l.pop_transform();
    g.l.pop_transform();
    g.l.pop_opacity();
}

void VideoPage::draw_layers(const DrawContext &context, float fade) const
{
    const float layer = tween::clamp01(layer_amount_.value) * fade;
    if (layer <= 0.01f)
        return;
    // REQUEST: a screen draws under the frame's clock, hint row and rail, so
    // this layer cannot cover them as the boards show; Screen needs a second
    // draw the frame calls after its own furniture.
    G g = context.g();
    g.l.rounded_rect({0.0f, 0.0f, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0.0f, kBlack.with_alpha(0.5f * layer));
    if (shown_layer_ == Layer::menu)
        draw_sheet(g, context, layer);
    else
        draw_share(g, layer);
}

// ---- the page --------------------------------------------------------------------------------

void VideoPage::draw(const DrawContext &context, const Phase &phase) const
{
    G g = context.g();
    const Motion motion = hand_over(phase, context.reduced_motion);
    g.l.push_opacity(motion.opacity);
    g.l.push_transform(motion.scale, gfx::kVirtualWidth * 0.5f, gfx::kVirtualHeight * 0.5f, motion.dx, 0.0f);
    g.l.push_opacity(motion.furniture);
    if (!have_ && (load_ == model::Load::failed || load_ == model::Load::offline))
    {
        draw_failure(g);
    }
    else
    {
        // Bones in the shape of the content, then the content in the same places.
        const float shown = !have_ ? 0.0f : context.reduced_motion ? 1.0f : tween::clamp01(since_ready_ / 0.3f);
        if (shown < 1.0f)
        {
            g.l.push_opacity(1.0f - shown);
            draw_bones(g);
            g.l.pop_opacity();
        }
        if (shown > 0.0f)
        {
            g.l.push_opacity(shown);
            draw_details(g, context);
            g.l.pop_opacity();
        }
    }
    draw_actions(g, context);
    draw_related(g, context);
    g.l.pop_opacity();
    draw_hero(g, context, phase, motion);
    g.l.pop_transform();
    g.l.pop_opacity();
    draw_layers(context, motion.opacity * motion.furniture);
}

} // namespace afterglow::video_page
