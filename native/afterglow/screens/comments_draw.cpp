// ProsperoTube Afterglow - the comments of a video, drawn.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The approved boards: the thread on the left under a plate that glides from
// comment to comment, the video it is about on the right with the way to write,
// and, while writing, a sheet that rises from the foot with the comment being
// answered, the field and the kit's keyboard.

#include "afterglow/screens/comments.hpp"

#include <algorithm>
#include <cmath>

namespace afterglow::comments_page
{

namespace
{

// REQUEST: tr() has no plural forms (as video.hpp).
std::string counted(int count, const char *one, const char *other)
{
    return fill(count == 1 ? one : other, std::to_string(count));
}

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

// The end of a text that is longer than its field: what is being typed stays in view.
std::string tail_fit(const Face &face, const std::string &value, float size, float width)
{
    if (measure(face, value, size) <= width)
        return value;
    std::vector<std::size_t> starts; // where a character begins
    for (std::size_t at = 0; at < value.size(); ++at)
    {
        if ((static_cast<unsigned char>(value[at]) & 0xc0) != 0x80)
            starts.push_back(at);
    }
    std::size_t low = 0;
    std::size_t high = starts.size() - 1;
    while (low < high)
    {
        const std::size_t middle = (low + high) / 2;
        if (measure(face, "..." + value.substr(starts[middle]), size) <= width)
            high = middle;
        else
            low = middle + 1;
    }
    return "..." + value.substr(starts[low]);
}

// One controller button, by the design language's own hint row.
void hint_glyph(G &g, ui::Button button, float right, float cy)
{
    const ui::Hint item{button, ""};
    g.l.push_transform(1.0f, 0.0f, 0.0f, 0.0f, cy - kHintY);
    afterglow::hints(g, &item, 1, 1.0f, false, right);
    g.l.pop_transform();
}

} // namespace

// A bone: the kit's skeleton, one block at a time, all on one clock.
void Comments::bone(G &g, const Rect &r, float radius) const
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

// The entrance of the nth group of the screen.
float Comments::arrive(int index) const
{
    return tween::stagger(age_, index, 0.05f, 0.45f);
}

// ---- one comment ----------------------------------------------------------------------------

void Comments::draw_comment(G &g, const DrawContext &context, const Item &item, const Rect &r, float focus) const
{
    const Look &k = g.k;
    const float x = r.x + 20.0f;
    const float y = r.y;
    const float right = r.x + r.w - 40.0f;
    if (item.kind == Item::Kind::waiting)
    {
        bone(g, {x + 8.0f, y + 14.0f, 40.0f, 40.0f}, 20.0f);
        bone(g, {x + 72.0f, y + 16.0f, 220.0f, 16.0f}, 8.0f);
        bone(g, {x + 72.0f, y + 44.0f, r.w - 260.0f, 16.0f}, 8.0f);
        return;
    }
    if (item.kind == Item::Kind::note)
    {
        icon(g, Icon::alert, x + 28.0f, y + 32.0f, 28.0f, k.ink3);
        text(g, g.f.regular, fit(g.f.regular, item.text.empty() ? tr("More comments could not be loaded.") : item.text.c_str(), kTypeNote,
                                 right - x - 72.0f),
             x + 72.0f, y + 39.0f, kTypeNote, k.ink3);
        return;
    }
    // The same name is the same colour wherever it writes.
    avatar(g, x + 28.0f, y + 40.0f, 24.0f, item.author, {}, context.images.get(item.avatar, PictureSize::avatar));

    // Who, when, and what the channel made of it.
    float cursor = x + 72.0f;
    cursor += text(g, g.f.semibold, fit(g.f.semibold, item.author, kTypeAuthor, (right - cursor) * 0.5f), cursor, y + 38.0f, kTypeAuthor, k.ink) +
              14.0f;
    if (cursor + measure(g.f.regular, item.age, kTypeFine) <= right)
        cursor += text(g, g.f.regular, item.age, cursor, y + 38.0f, kTypeFine, k.ink3) + 16.0f;
    const auto mark = [&](Icon which, float size, Color color, const char *words)
    {
        if (cursor + 24.0f + measure(g.f.regular, words, kTypeFine) > right)
            return; // a long name leaves no room: the mark is dropped, never cut
        icon(g, which, cursor + 9.0f, y + 31.0f, size, color);
        cursor += 24.0f;
        cursor += text(g, g.f.regular, words, cursor, y + 38.0f, kTypeFine, k.ink2) + 16.0f;
    };
    if (item.pinned)
        mark(Icon::pin, 18.0f, k.ink2, tr("Pinned by the channel"));
    if (item.hearted)
        mark(Icon::heart, 17.0f, k.accent, tr("Loved by the channel"));
    if (item.written_here)
        mark(Icon::check, 18.0f, k.accent, tr("Posted to YouTube"));

    // What they wrote: two lines at rest, the whole of it under the focus.
    const float tx = x + 72.0f;
    const float width = kThreadW - 72.0f - 40.0f - kIndent * static_cast<float>(item.depth);
    const Color ink = k.ink.with_alpha(0.78f + 0.22f * focus);
    const float grown = full_height(item) - kPitch;
    const float open = grown > 0.5f ? tween::smoothstep((item.h - kPitch) / grown) : 0.0f;
    if (open < 1.0f)
    {
        g.l.push_opacity(1.0f - open);
        paragraph(g, g.f.regular, item.text, tx, y + 70.0f, kTypeSaid, width, kTextLine, ink, kLinesAtRest);
        g.l.pop_opacity();
    }
    if (open > 0.0f)
    {
        g.l.push_clip({tx - 6.0f, y + 44.0f, width + 12.0f, item.h - 84.0f});
        g.l.push_opacity(open);
        paragraph(g, g.f.regular, item.text, tx, y + 70.0f, kTypeSaid, width, kTextLine, ink, kLinesInFocus);
        g.l.pop_opacity();
        g.l.pop_clip();
    }

    // Likes, and where the conversation continues.
    const float foot = y + item.h - 23.0f;
    const bool rating = (job_.kind == Job::Kind::like || job_.kind == Job::Kind::unlike) && job_.target == item.id;
    const float pop = !g.reduced && like_pop_id_ == item.id ? like_pop_.value : 0.0f;
    if (pop > 0.01f)
        g.l.glow({x + 70.0f, foot - 18.0f, 24.0f, 24.0f}, 12.0f, 26.0f * pop, k.accent.with_alpha(0.55f * pop));
    if (rating)
    {
        // REQUEST: update() has no Look, so the kit's spinner is given the look's theme on a copy here.
        ui::Spinner spinner = spinner_;
        spinner.style.theme = k.theme;
        spinner.style.reduced_motion = g.reduced;
        spinner.style.color = k.ink2;
        spinner.style.thickness = 2.0f;
        spinner.set_bounds({x + 72.0f, foot - 16.0f, 20.0f, 20.0f});
        ui::Canvas canvas{g.l, g.f, g.glass, g.time};
        spinner.draw(canvas);
    }
    else if (!item.written_here)
    {
        icon(g, Icon::heart, x + 82.0f, foot - 6.0f, 16.0f * (1.0f + 0.5f * pop), item.liked ? k.accent : k.ink3);
    }
    float at = x + 98.0f;
    if (!item.likes.empty())
        at += text(g, g.f.regular, item.likes, at, foot, kTypeFine, item.liked ? k.ink2 : k.ink3);
    at += 28.0f;
    if (item.has_replies())
    {
        const Color tone = focus > 0.5f ? k.ink : k.ink3;
        const bool opened = open_.find(item.id) != open_.end();
        icon(g, Icon::reply, at + 9.0f, foot - 6.0f, 18.0f, tone);
        at += 26.0f;
        at += text(g, g.f.semibold, counted(item.replies, tr("{0} reply"), tr("{0} replies")), at, foot, kTypeFine, tone);
        // The way in is to the right; an open thread points down at its replies.
        if (focus > 0.5f)
            icon(g, opened ? Icon::chevron_down : Icon::chevron_right, at + 16.0f, foot - 6.0f, 16.0f, tone.with_alpha(tone.a * focus));
    }
}

// ---- the thread -----------------------------------------------------------------------------

void Comments::draw_thread(G &g, const DrawContext &context) const
{
    const Look &k = g.k;
    const int total = static_cast<int>(items_.size());
    const int at = focused();
    const float view_top = kThreadTop;
    const float view_bottom = kThreadTop + kThreadView;
    g.l.push_clip({kRail, kThreadTop - 14.0f, 1080.0f, kThreadView + 28.0f});
    if (at >= 0 && !writing_)
    {
        // The plate under the comment in focus glides with the focus.
        const int from = std::clamp(static_cast<int>(std::floor(focus_pos_.value)), 0, total - 1);
        const int to = std::min(from + 1, total - 1);
        Rect plate = lerp_rect(item_rect(from), item_rect(to), tween::clamp01(focus_pos_.value - static_cast<float>(from)));
        if (!g.reduced)
            plate.y += ui::shake(nudge_.value, g.time, 10.0f, 8.0f);
        g.l.push_opacity(arrive(3));
        lift(g, plate, kRadiusPlate, 0.5f);
        glass_panel(g, plate, kRadiusPlate);
        g.l.bordered_rect(plate.inset(-4.0f), kRadiusPlate + 4.0f, clear(), k.theme.focus_width, k.focus);
        g.l.pop_opacity();
    }
    else if (at >= 0)
    {
        // While writing the comment being answered keeps a quiet plate.
        const Rect plate = item_rect(at);
        glass_panel(g, plate, kRadiusPlate);
        g.l.bordered_rect(plate.inset(-4.0f), kRadiusPlate + 4.0f, clear(), k.theme.focus_width, k.focus.with_alpha(0.7f));
    }
    const int first = first_shown();
    for (int i = 0; i < total; ++i)
    {
        const Item &item = items_[static_cast<std::size_t>(i)];
        const Rect r = item_rect(i);
        // A comment is shown whole or not at all.
        const float cut = std::max({view_top - 8.0f - r.y, r.y + r.h - (view_bottom + 8.0f), 0.0f});
        const float alpha = 1.0f - tween::clamp01(cut / 40.0f);
        if (alpha <= 0.01f)
            continue;
        const float focus = tween::clamp01(1.0f - std::fabs(focus_pos_.value - static_cast<float>(i)));
        Enter enter(g, arrive(2 + std::clamp(i - first, 0, 6)) * alpha, 22.0f);
        draw_comment(g, context, item, r, writing_ ? 0.0f : focus);
    }
    g.l.pop_clip();
}

// The thread has nothing to show: it is on its way, empty, or could not be read.
void Comments::draw_state(G &g) const
{
    const Look &k = g.k;
    const float x = kLeft;
    if (load_ == model::Load::loading || load_ == model::Load::idle)
    {
        for (int i = 0; i < 5; ++i)
        {
            Enter enter(g, arrive(2 + i), 22.0f);
            const float y = kThreadTop + kPitch * static_cast<float>(i);
            bone(g, {x + 4.0f, y + 16.0f, 48.0f, 48.0f}, 24.0f);
            bone(g, {x + 72.0f, y + 22.0f, 260.0f, 18.0f}, 9.0f);
            bone(g, {x + 72.0f, y + 54.0f, 800.0f, 18.0f}, 9.0f);
            bone(g, {x + 72.0f, y + 85.0f, 560.0f, 18.0f}, 9.0f);
            bone(g, {x + 72.0f, y + 116.0f, 120.0f, 14.0f}, 7.0f);
        }
        return;
    }
    Enter enter(g, arrive(2), 22.0f);
    const bool broken = load_ == model::Load::failed || load_ == model::Load::offline;
    g.l.circle(x + 44.0f, kThreadTop + 60.0f, 44.0f, k.raised);
    icon(g, !broken ? Icon::chat : load_ == model::Load::offline ? Icon::offline : Icon::alert, x + 44.0f, kThreadTop + 60.0f, 46.0f, k.ink);
    const char *headline = !broken                           ? tr("No comments yet")
                           : load_ == model::Load::offline ? tr("No connection to YouTube")
                                                             : tr("The comments could not be loaded");
    const float under = paragraph(g, g.f.display, headline, x - 2.0f, kThreadTop + 168.0f, kTypePage, 900.0f, 52.0f, k.ink, 2);
    std::string words;
    if (broken)
        words = error_.empty() ? std::string(tr("Try again in a moment.")) : error_;
    else if (can_write())
        words = tr("Nobody has written under this video. Press Cross to be the first.");
    else if (!profile_.signed_in)
        words = tr("Nobody has written under this video. Writing needs a YouTube account on this profile.");
    else
        words = tr("Comments cannot be added to this video.");
    const float after = paragraph(g, g.f.regular, words, x, under + 4.0f, kTypeBody, 900.0f, 34.0f, k.ink2, 3);
    if (broken)
    {
        const char *label = tr("Try again");
        const Rect retry{x, after + 12.0f, button_width(g, label, true) + 24.0f, 64.0f};
        button(g, retry, label, Icon::update, 1.0f, true);
    }
}

// ---- what it is about, and the way in --------------------------------------------------------

void Comments::draw_about(G &g, const DrawContext &context) const
{
    const Look &k = g.k;
    Enter enter(g, arrive(2), 0.0f, 36.0f);
    const Rect &thumb = kThumb;
    const Picture picture = context.images.get(video_thumb_, PictureSize::hero);
    const bool unknown = video_thumb_.empty() && video_title_.empty();
    const bool coming = video_load_ == model::Load::loading || video_load_ == model::Load::idle;
    const float shown = picture.ready() ? (g.reduced ? 1.0f : tween::clamp01(picture.age / 0.25f)) : 0.0f;
    if (unknown && coming)
        bone(g, thumb, kRadiusPicture);
    else if (!unknown)
        lift(g, thumb, kRadiusPicture, 0.8f);
    if (!unknown && shown < 1.0f)
        picture_placeholder(g, thumb, kRadiusPicture);
    if (shown > 0.0f)
        g.l.image(picture.texture, thumb, picture.cover(thumb.w / thumb.h), kWhite.with_alpha(shown), kRadiusPicture);
    if (!video_title_.empty())
    {
        const float after = paragraph(g, g.f.semibold, video_title_, thumb.x, thumb.y + thumb.h + 44.0f, kTypeName, thumb.w, 33.0f, k.ink, 2);
        text(g, g.f.regular, fit(g.f.regular, video_channel_, kTypeNote, thumb.w), thumb.x, after + 4.0f, kTypeNote, k.ink2);
    }
    else if (coming)
    {
        bone(g, {thumb.x, thumb.y + thumb.h + 24.0f, thumb.w - 40.0f, 22.0f}, 11.0f);
        bone(g, {thumb.x, thumb.y + thumb.h + 58.0f, thumb.w * 0.5f, 22.0f}, 11.0f);
    }

    // The way to write, and what writing means: once the thread has said whether it is open.
    const Rect &compose = kCompose;
    if (load_ == model::Load::loading || load_ == model::Load::idle)
    {
        bone(g, compose, compose.h * 0.5f);
        return;
    }
    if (load_ != model::Load::ready)
        return;
    const bool lone = focused() < 0 && load_ == model::Load::ready; // the thread is empty: this has the focus
    glass_panel(g, compose, compose.h * 0.5f);
    if (lone && !writing_)
        g.l.bordered_rect(compose.inset(-4.0f), compose.h * 0.5f + 4.0f, clear(), k.theme.focus_width, k.focus);
    const float room = compose.w - 72.0f - 76.0f;
    std::string note;
    if (can_write())
    {
        const std::string name = profile_.name.empty() ? std::string(tr("You")) : profile_.name;
        avatar(g, compose.x + 34.0f, compose.cy(), 22.0f, name, profile_.id, Picture{});
        text(g, g.f.regular, fit(g.f.regular, fill(tr("Add a comment as {0}"), name), kTypeSaid, room), compose.x + 72.0f, compose.cy() + 8.0f,
             kTypeSaid, k.ink3);
        hint_glyph(g, ui::Button::square, compose.x + compose.w - 14.0f, compose.cy());
        note = tr("Comments and replies are posted to YouTube from your account, once, after you confirm.");
    }
    else if (!profile_.signed_in)
    {
        icon(g, Icon::user, compose.x + 36.0f, compose.cy(), 28.0f, k.ink3);
        text(g, g.f.regular, fit(g.f.regular, tr("Sign in to write and to like"), kTypeSaid, room), compose.x + 72.0f, compose.cy() + 8.0f,
             kTypeSaid, k.ink3);
        hint_glyph(g, ui::Button::square, compose.x + compose.w - 14.0f, compose.cy());
        note = tr("Reading needs no account. Writing, answering and liking use a YouTube account on this profile.");
    }
    else
    {
        icon(g, Icon::chat, compose.x + 36.0f, compose.cy(), 28.0f, k.ink3);
        text(g, g.f.regular, fit(g.f.regular, tr("Comments cannot be added here"), kTypeSaid, compose.w - 100.0f), compose.x + 72.0f,
             compose.cy() + 8.0f, kTypeSaid, k.ink3);
    }
    if (!note.empty())
        paragraph(g, g.f.regular, note, thumb.x, 826.0f, kTypeMeta, thumb.w, 28.0f, k.ink3, 3);
}

// ---- writing: the keyboard rises, the thread steps back ----------------------------------------

void Comments::draw_writing(G &g, const DrawContext &context, float amount) const
{
    const Look &k = g.k;
    const float rise = g.reduced ? 0.0f : (1.0f - amount) * 160.0f;
    g.l.push_opacity(amount);
    g.l.push_transform(1.0f, 0.0f, 0.0f, 0.0f, rise);
    g.l.shadow({kSheet.x, kSheet.y - 10.0f, kSheet.w, 80.0f}, 40.0f, 80.0f, kBlack.with_alpha(0.5f));
    glass_panel(g, kSheet, kRadiusSheet, 1.3f);
    const float x = kLeft + 20.0f;
    const Item *to = writing_to_ >= 0 && writing_to_ < static_cast<int>(items_.size()) ? &items_[static_cast<std::size_t>(writing_to_)] : nullptr;
    const std::string label = to != nullptr ? fill(tr("Reply to {0}"), to->author) : std::string(tr("New comment"));
    overline(g, fit_overline(g, label, 740.0f), x, 650.0f, k.accent);
    paragraph(g, g.f.regular, to != nullptr ? to->text : video_title_, x, 692.0f, kTypeNote, 740.0f, 29.0f, k.ink3, 2);

    const Rect &field = kField;
    const bool sending = job_.kind == Job::Kind::post || job_.kind == Job::Kind::reply;
    g.l.rounded_rect(field, kRadiusField, k.raised);
    g.l.bordered_rect(field, kRadiusField, clear(), 2.0f, k.focus.with_alpha(sending ? 0.3f : 0.8f));
    if (text_.empty())
    {
        text(g, g.f.regular, fit(g.f.regular, to != nullptr ? tr("Write your reply") : tr("Write your comment"), kTypeName, field.w - 60.0f),
             field.x + 26.0f, field.cy() + 9.0f, kTypeName, k.ink3);
    }
    const float typed = text(g, g.f.regular, tail_fit(g.f.regular, text_, kTypeName, field.w - 60.0f), field.x + 26.0f, field.cy() + 9.0f,
                             kTypeName, k.ink.with_alpha(sending ? 0.6f : 1.0f));
    if (!sending && (g.reduced || std::fmod(g.time, 1.0f) < 0.55f))
        g.l.rounded_rect({field.x + 28.0f + typed, field.cy() - 16.0f, 3.0f, 32.0f}, 1.5f, k.accent);

    // Under the field: who posts, how much room is left, or why it did not go out.
    const float under = field.y + field.h + 34.0f;
    int characters = 0;
    for (const char c : text_)
        characters += (static_cast<unsigned char>(c) & 0xc0) != 0x80 ? 1 : 0;
    text(g, g.f.regular, fill(tr("{0} / {1}"), std::to_string(characters), std::to_string(kTextLimit)), field.x + field.w, under, kTypeFine,
         k.ink3, gfx::Align::right);
    if (sending)
    {
        // REQUEST: update() has no Look, so the kit's spinner is given the look's theme on a copy here.
        ui::Spinner spinner = spinner_;
        spinner.style.theme = k.theme;
        spinner.style.reduced_motion = g.reduced;
        spinner.style.color = k.accent;
        spinner.style.thickness = 2.5f;
        spinner.set_bounds({field.x + 4.0f, under - 19.0f, 24.0f, 24.0f});
        ui::Canvas canvas{g.l, g.f, g.glass, g.time};
        spinner.draw(canvas);
        text(g, g.f.regular, tr("Posting to YouTube"), field.x + 40.0f, under, kTypeFine, k.ink2);
    }
    else if (!trouble_.empty())
    {
        icon(g, Icon::alert, field.x + 14.0f, under - 7.0f, 22.0f, k.theme.danger);
        paragraph(g, g.f.regular, trouble_, field.x + 36.0f, under, kTypeFine, field.w - 190.0f, 26.0f, k.ink, 3);
    }
    else
    {
        const std::string name = profile_.name.empty() ? std::string(tr("You")) : profile_.name;
        text(g, g.f.regular, fit(g.f.regular, fill(tr("Posting as {0}"), name), kTypeFine, field.w - 190.0f), field.x + 4.0f, under, kTypeFine,
             k.ink3);
    }

    // REQUEST: update() has no Look, so the kit's keyboard is given the look's theme on a copy here.
    ui::Keyboard keys = keys_;
    keys.style.theme = k.theme;
    keys.style.reduced_motion = g.reduced;
    keys.set_active(!sending);
    ui::Canvas canvas{g.l, g.f, context.glass, g.time};
    keys.draw(canvas);

    static ui::Hint items[4];
    items[0] = {ui::Button::options, tr("Post")};
    items[1] = {ui::Button::square, tr("Delete")};
    items[2] = {ui::Button::triangle, tr("Space")};
    items[3] = {ui::Button::circle, tr("Cancel")};
    afterglow::hints(g, items, 4, sending ? 0.4f : 1.0f, true, x);
    g.l.pop_transform();
    g.l.pop_opacity();
}

// ---- the screen --------------------------------------------------------------------------------

void Comments::draw(const DrawContext &context, const Phase &phase) const
{
    G g = context.g();
    const Look &k = g.k;
    const Motion motion = hand_over(phase, context.reduced_motion);
    const float writing = tween::clamp01(writing_amount_.value);
    g.l.push_opacity(motion.alpha * motion.furniture);
    g.l.push_transform(motion.scale, gfx::kVirtualWidth * 0.5f, gfx::kVirtualHeight * 0.5f, motion.dx, 0.0f);
    {
        // The header's second line: how many there are.
        Enter enter(g, arrive(0), -14.0f);
        if (!count_.empty())
            text(g, g.f.regular, fit(g.f.regular, fill(tr("{0} on this video"), count_), kTypeAuthor, 700.0f), kLeft, 166.0f, kTypeAuthor,
                 k.ink2);
    }
    {
        // The order: the chosen one is a filled plate.
        Enter enter(g, arrive(1), -14.0f);
        const char *orders[2] = {tr("Top"), tr("Newest")};
        float x = kLeft - 20.0f + kThreadW;
        for (int i = 1; i >= 0; --i)
        {
            x -= chip_width(g, orders[i]);
            chip(g, x, 150.0f, orders[i], i == order_, 0.0f);
            x -= 12.0f;
        }
    }
    g.l.push_opacity(1.0f - 0.55f * writing);
    if (items_.empty())
        draw_state(g);
    else
        draw_thread(g, context);
    draw_about(g, context);
    g.l.pop_opacity();
    if (writing > 0.01f)
        draw_writing(g, context, writing);
    g.l.pop_transform();
    g.l.pop_opacity();
}

} // namespace afterglow::comments_page
