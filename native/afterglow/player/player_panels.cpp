// ProsperoTube Afterglow - the player's drawing: what opens over the video.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The notice above the timeline, the shelf of what is next, the sheets from the
// right (captions, playback options and their lists), the chat column with the
// theatre layout, and the card of the next video. Boards 18 to 23 of the
// proposal; the stop confirmation of board 24 is the kit's Dialog (player.cpp).

#include "afterglow/player/parts.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace afterglow::player
{

namespace
{

// A glyph and its label, ending at right. Returns where it began.
float action(G &g, const Hint &hint, float right, float cy)
{
    if (hint.button == ui::Button::none || hint.label.empty())
        return right;
    const float label_w = measure(g.f.semibold, hint.label, kTypeClock);
    text(g, g.f.semibold, hint.label, right, cy + 8.0f, kTypeClock, g.k.ink, gfx::Align::right);
    const float glyph_w = ui::button_width(hint.button, 34.0f);
    const float x = right - label_w - 12.0f - glyph_w;
    ui::draw_button(g.l, g.f, g.k.dark ? ui::GlyphStyle::dark() : ui::GlyphStyle::light(), hint.button, x, cy, 34.0f);
    return x;
}

// A name in a chat takes a colour of its own; on a light panel it leans toward the ink.
Color name_color(const G &g, std::string_view author)
{
    Color top;
    Color bottom;
    avatar_tint(author, &top, &bottom);
    return gfx::mix(top, g.k.ink, g.k.dark ? 0.12f : 0.5f);
}

} // namespace

void Player::draw_notice(G &g, const Motion &m) const
{
    if (m.notice <= 0.01f)
        return;
    const Look &k = *look_;
    Rect notice = notice_rect(m);
    notice.x += m.notice_slide;
    const Color mark = notice_.rgb != 0 ? Color::rgb(notice_.rgb) : notice_.kind == "error" ? k.theme.danger : k.accent;
    g.l.push_opacity(m.notice);
    lift(g, notice, kRadiusPanel, 0.8f);
    glass_panel(g, notice, kRadiusPanel, 1.2f);
    g.l.circle(notice.x + 42.0f, notice.cy(), 9.0f, mark);
    g.l.ring(notice.x + 42.0f, notice.cy(), 16.0f, 2.0f, mark.with_alpha(0.5f));
    // What it offers, at the right: the way back first.
    float right = notice.x + notice.w - 44.0f;
    if (notice_.action.button != ui::Button::none && !notice_.action.label.empty())
    {
        right = action(g, notice_.action, right, notice.cy());
        if (notice_.second.button != ui::Button::none && !notice_.second.label.empty())
            right = action(g, notice_.second, right - 28.0f, notice.cy());
        right -= 24.0f;
    }
    const float room = right - (notice.x + 74.0f);
    if (!notice_.label.empty())
    {
        overline(g, fit_tracked(g.f.semibold, ui::upper(notice_.label), kTypeOver, room, 3.2f), notice.x + 74.0f, notice.y + 34.0f, k.ink2);
        text(g, g.f.semibold, fit(g.f.semibold, notice_.title, kTypeRow, room), notice.x + 74.0f, notice.y + 68.0f, kTypeRow, k.ink);
    }
    else if (notice_.detail.empty())
    {
        text(g, g.f.semibold, fit(g.f.semibold, notice_.title, kTypeRow, room), notice.x + 74.0f, notice.cy() + 9.0f, kTypeRow, k.ink);
    }
    else
    {
        text(g, g.f.semibold, fit(g.f.semibold, notice_.title, kTypeRow, room), notice.x + 74.0f, notice.y + 40.0f, kTypeRow, k.ink);
        text(g, g.f.regular, fit(g.f.regular, notice_.detail, kTypeMeta, room), notice.x + 74.0f, notice.y + 70.0f, kTypeMeta, k.ink2);
    }
    // The notice drains: how long the offer stands.
    if (notice_.left >= 0.0f)
        g.l.rounded_rect({notice.x + 28.0f, notice.y + notice.h - 8.0f, (notice.w - 56.0f) * tween::clamp01(notice_.left), 3.0f}, 1.5f,
                         mark.with_alpha(0.9f));
    g.l.pop_opacity();
}

void Player::draw_up_next(G &g, const Motion &m) const
{
    const float p = m.up_next;
    if (p <= 0.01f)
        return;
    const Look &k = *look_;
    const float travel = g.reduced ? 0.0f : 1.0f - p;
    g.l.push_opacity(p);
    g.l.push_transform(1.0f, 0.0f, 0.0f, 0.0f, 150.0f * travel);
    glass_panel(g, kUpNextSheet, 44.0f, 1.25f);
    const std::string title = up_next_.title.empty() ? std::string(tr("Up next")) : up_next_.title;
    float x = kEdge - 2.0f + text(g, g.f.display, fit(g.f.display, title, kTypePanelTitle, 520.0f), kEdge - 2.0f, 712.0f, kTypePanelTitle, k.ink) + 34.0f;
    x = std::max(x, kEdge + 190.0f);
    if (!up_next_.chip.empty())
        x += chip(g, x, 700.0f, up_next_.chip, true, 0.0f, 40.0f) + 20.0f;
    const float hints_w = hint_row(g, up_next_.hints, kRight, 702.0f, true, false, kRight - x - 80.0f);
    const float note_room = kRight - hints_w - 40.0f - x;
    if (!up_next_.note.empty() && note_room > 320.0f) // a note cut to a few letters says nothing
        text(g, g.f.regular, fit(g.f.regular, up_next_.note, kTypeNote, note_room), x, 708.0f, kTypeNote, k.ink2);

    const int count = static_cast<int>(up_next_.cards.size());
    if (count == 0)
    {
        // Nothing to show yet, or nothing at all: say which.
        const std::string message = up_next_.message.empty() ? std::string(up_next_.loading ? tr("Finding what comes next...") : tr("Nothing is lined up after this video."))
                                                              : up_next_.message;
        if (up_next_.loading)
        {
            // The bones of the row: five pictures and the two lines under each, breathing one after another.
            for (int i = 0; i < kUpNextShown; ++i)
            {
                const Rect r = card_rect(i, 0.0f);
                g.l.push_opacity(g.reduced ? 0.8f : 0.5f + 0.5f * ui::breathe(g.time + 0.13f * static_cast<float>(i), 1.7f));
                picture_placeholder(g, r, kRadiusThumb);
                g.l.rounded_rect({r.x + 2.0f, r.y + r.h + 16.0f, r.w * 0.78f, 18.0f}, 9.0f, k.raised);
                g.l.rounded_rect({r.x + 2.0f, r.y + r.h + 46.0f, r.w * 0.46f, 14.0f}, 7.0f, k.raised);
                g.l.pop_opacity();
            }
        }
        else
        {
            icon(g, Icon::queue, kEdge + 22.0f, 800.0f, 44.0f, k.ink2);
            paragraph(g, g.f.regular, message, kEdge + 64.0f, 808.0f, kTypeBody, 1100.0f, 34.0f, k.ink2, 3);
        }
    }
    else
    {
        // The row keeps to the shelf: a card past the edge is cut by it.
        g.l.push_clip({0.0f, 716.0f, gfx::kVirtualWidth, 364.0f});
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int i = 0; i < count; ++i)
            {
                if ((pass == 1) != (i == up_next_.focus))
                    continue;
                const Card &card = up_next_.cards[static_cast<std::size_t>(i)];
                const float focus = tween::clamp01(1.0f - std::fabs(up_next_pos_.value - static_cast<float>(i)));
                const Rect rest = card_rect(i, up_next_pos_.value);
                if (rest.x > gfx::kVirtualWidth || rest.x + rest.w < 0.0f)
                    continue;
                const Rect r = grown(rest, 0.05f * focus, 5.0f * focus);
                const Picture shot = picture(card.thumb, PictureSize::card);
                video_card(g, r, video_of(card), shot, focus, 1.0f, 0);
                text(g, g.f.semibold, fit(g.f.semibold, card.title, kTypeNote, r.w - 4.0f), r.x + 2.0f, r.y + r.h + 32.0f, kTypeNote,
                     k.ink.with_alpha(0.82f + 0.18f * focus));
                text(g, g.f.regular, fit(g.f.regular, card.channel.empty() ? card.meta : card.channel, kTypeSmall, r.w - 4.0f), r.x + 2.0f, r.y + r.h + 60.0f,
                     kTypeSmall, k.ink2);
                if (up_next_.marks_next && up_next_.first + i == 0)
                    tag(g, r.x + 12.0f, r.y + r.h - 26.0f, tr("NEXT"), k.accent, k.on_accent);
                if (pass == 1)
                    focus_ring(g, r, kRadiusThumb, shot.ready() ? shot.glow : k.accent);
            }
        }
        g.l.pop_clip();
    }
    g.l.pop_transform();
    g.l.pop_opacity();
}

void Player::draw_sheet(G &g, const Motion &m) const
{
    const float p = m.sheet;
    if (p <= 0.01f)
        return;
    const Look &k = *look_;
    const bool captions = sheet_.kind == "captions";
    const Rect sheet = sheet_rect();
    const float travel = g.reduced ? 0.0f : 1.0f - p;
    g.l.push_opacity(p);
    g.l.push_transform(1.0f, 0.0f, 0.0f, (captions ? 140.0f : 160.0f) * travel, 0.0f);
    // The sheet casts a shadow toward the video; the video is never resized.
    g.l.shadow({sheet.x - 30.0f, 0.0f, 90.0f, gfx::kVirtualHeight}, 0.0f, 90.0f, kBlack.with_alpha(0.45f));
    glass_panel(g, sheet, 44.0f, 1.5f);
    const float left = sheet.x + 40.0f;
    const float title_x = captions ? left : left + 24.0f;
    text(g, g.f.display, fit(g.f.display, sheet_.title, kTypePanelTitle, (captions ? 508.0f : 760.0f)), title_x, 108.0f, kTypePanelTitle, k.ink);

    ui::Canvas canvas{g.l, fonts_, g.glass, time_};
    if (captions && !sheet_.tabs.empty())
    {
        tabs_.draw(canvas);
        std::string settings;
        for (const std::string& pill : sheet_.pills) {
            if (!settings.empty()) settings += "   ";
            settings += pill;
        }
        text(g,g.f.regular,fit(g.f.regular,settings,kTypeSmall,508.0f),left,234.0f,kTypeSmall,k.ink2);
    }
    else if (captions && !sheet_.pills.empty())
    {
        // Size and sync of the captions, as pills under the title.
        float x = left;
        for (const std::string &pill : sheet_.pills)
        {
            if (pill.empty())
                continue;
            const float width = measure(g.f.semibold, pill, kTypeOver, 1.0f) + 30.0f;
            if (x + width > left + 508.0f)
                break;
            g.l.rounded_rect({x, 150.0f, width, 36.0f}, 18.0f, k.raised);
            text(g, g.f.semibold, pill, x + width * 0.5f, 174.0f, kTypeOver, k.ink2, gfx::Align::center, 1.0f);
            x += width + 10.0f;
        }
    }

    const Rect list = list_.bounds();
    const int focus = sheet_focus_ >= 0 && sheet_focus_ < static_cast<int>(sheet_.rows.size()) ? sheet_focus_ : -1;
    if (sheet_.rows.empty())
    {
        // A list with nothing in it says why.
        icon(g, Icon::info, left + 22.0f, list.y + 30.0f, 40.0f, k.ink2);
        text(g, g.f.semibold, fit(g.f.semibold, sheet_.empty_title.empty() ? tr("Nothing to choose here") : sheet_.empty_title.c_str(), kTypeRow, list.w - 60.0f),
             left + 58.0f, list.y + 39.0f, kTypeRow, k.ink);
        if (!sheet_.empty_hint.empty())
            paragraph(g, g.f.regular, sheet_.empty_hint, left, list.y + 92.0f, kTypeNote, list.w, 30.0f, k.ink2, 4);
    }
    else
    {
        list_.draw(canvas);
        // More rows than the sheet shows: where the window is in the whole list.
        const int total = sheet_.total > 0 ? sheet_.total : static_cast<int>(sheet_.rows.size());
        if (total > static_cast<int>(sheet_.rows.size()))
        {
            // Beside the captions' list; on the line between the two columns of the options.
            const Rect track = captions ? Rect{list.x + list.w + 8.0f, list.y + 8.0f, 5.0f, list.h - 16.0f} : Rect{left + 480.0f + 14.25f, 148.0f, 5.0f, 660.0f};
            const float part = std::max(0.08f, static_cast<float>(sheet_.rows.size()) / static_cast<float>(total));
            const float at = static_cast<float>(sheet_.first) / static_cast<float>(std::max(1, total - static_cast<int>(sheet_.rows.size())));
            if (captions)
                g.l.rounded_rect(track, 2.5f, k.raised);
            g.l.rounded_rect({track.x, track.y + (track.h - track.h * part) * tween::clamp01(at), track.w, track.h * part}, 2.5f, k.ink3);
        }
    }

    const Row *row = focus >= 0 ? &sheet_.rows[static_cast<std::size_t>(focus)] : nullptr;
    if (captions)
    {
        const int total = sheet_.total > 0 ? sheet_.total : static_cast<int>(sheet_.rows.size());
        if (total > 0 && focus >= 0)
            text(g,g.f.mono,fill(tr("{0} of {1}"),std::to_string(sheet_.first+focus+1),std::to_string(total)),
                 left+508.0f,880.0f,kTypeSmall,k.ink3,gfx::Align::right);
        const std::string &note = sheet_.note.empty() && row != nullptr ? row->detail : sheet_.note;
        if (!note.empty())
            paragraph(g, g.f.regular, note, left + 4.0f, 908.0f, kTypeMeta, 500.0f, 28.0f, k.ink2, 2);
        if (sheet_.hints.size() > 4) {
            const std::vector<Hint> main(sheet_.hints.begin(),sheet_.hints.begin()+3);
            const std::vector<Hint> adjust(sheet_.hints.begin()+3,sheet_.hints.end());
            hint_row(g, main, left + 2.0f, 976.0f, false, false, 508.0f, 32.0f, kTypeNote, 30.0f);
            hint_row(g, adjust, left + 2.0f, 1024.0f, false, false, 508.0f, 32.0f, kTypeNote, 30.0f);
        } else hint_row(g, sheet_.hints, left + 2.0f, 1000.0f, false, false, 508.0f, 32.0f, kTypeNote, 30.0f);
    }
    else
    {
        // What there is on the left, its values on the right.
        const float column = left + 480.0f + 16.0f;
        g.l.rounded_rect({column, 148.0f, 1.5f, 660.0f}, 0.0f, k.hairline);
        const float cx = column + 24.0f;
        if (!sheet_.choices.empty())
        {
            overline(g, fit_tracked(g.f.semibold, ui::upper(sheet_.choices_title), kTypeOver, 300.0f, 3.2f), cx + 12.0f, 180.0f, k.accent);
            for (std::size_t i = 0; i < sheet_.choices.size() && i < 9; ++i)
            {
                const Choice &choice = sheet_.choices[i];
                const float y = 206.0f + 62.0f * static_cast<float>(i);
                if (choice.current)
                {
                    g.l.rounded_rect({cx, y, 320.0f, 54.0f}, 27.0f, k.raised);
                    icon(g, Icon::check, cx + 292.0f, y + 27.0f, 22.0f, k.accent);
                }
                const std::string label = fit(g.f.semibold, choice.label, kTypeClock, 240.0f);
                const float label_w = text(g, g.f.semibold, label, cx + 24.0f, y + 35.0f, kTypeClock, choice.current ? k.ink : k.ink2);
                const float note_room = 250.0f - label_w;
                if (!choice.note.empty() && note_room > 50.0f)
                    text(g, g.f.regular, fit(g.f.regular, choice.note, kTypeSmall, note_room), cx + 24.0f + label_w + 14.0f, y + 35.0f, kTypeSmall, k.ink3);
            }
        }
        else if (row != nullptr)
        {
            // No list of values to show: what the focused row is for.
            overline(g, fit_tracked(g.f.semibold, ui::upper(row->label), kTypeOver, 300.0f, 3.2f), cx + 12.0f, 180.0f, k.accent);
            float y = 222.0f;
            if (!row->value.empty())
                y = paragraph(g, g.f.semibold, row->value, cx + 12.0f, y, kTypeClock, 300.0f, 32.0f, k.ink, 2) + 8.0f;
            const std::string &detail = row->detail.empty() ? sheet_.note : row->detail;
            if (!detail.empty())
                paragraph(g, g.f.regular, detail, cx + 12.0f, y, kTypeNote, 300.0f, 30.0f, k.ink2, 8);
        }
        if (sheet_.choices_total > static_cast<int>(sheet_.choices.size()))
            text(g, g.f.mono, fill(tr("{0} of {1}"), std::to_string(sheet_.choices_first + 1) + "-" + std::to_string(sheet_.choices_first + sheet_.choices.size()),
                 std::to_string(sheet_.choices_total)), cx + 12.0f, 790.0f, kTypeSmall, k.ink3);
        const int total = sheet_.total > 0 ? sheet_.total : static_cast<int>(sheet_.rows.size());
        if (sheet_.choices_total <= static_cast<int>(sheet_.choices.size()) && total > static_cast<int>(sheet_.rows.size()) && focus >= 0)
            text(g, g.f.mono, fill(tr("{0} of {1}"), std::to_string(sheet_.first + focus + 1), std::to_string(total)), cx + 12.0f, 790.0f, kTypeSmall, k.ink3);
        hint_row(g, sheet_.hints, left + 26.0f, 1000.0f, false, false, 760.0f, 32.0f, kTypeNote, 30.0f);
    }
    g.l.pop_transform();
    if (captions && !sheet_.preview.empty())
    {
        // A line in the size that is chosen, where captions show: beside the sheet, over the picture.
        const float cx = sheet.x * 0.5f;
        const float size = std::clamp(sheet_.preview_size, 16.0f, 120.0f);
        const std::string sample = fit(g.f.semibold, sheet_.preview, size, sheet.x - 160.0f);
        const float width = measure(g.f.semibold, sample, size);
        overline(g, tr("Preview"), cx, 800.0f - size * 1.5f, kOnVideo.with_alpha(0.6f), gfx::Align::center);
        g.l.rounded_rect({cx - width * 0.5f - 18.0f, 800.0f - size * 0.95f, width + 36.0f, size * 1.25f}, 10.0f, kBlack.with_alpha(0.55f));
        text(g, g.f.semibold, sample, cx, 800.0f, size, kOnVideo, gfx::Align::center);
    }
    g.l.pop_opacity();
}

void Player::draw_chat(G &g, const Motion &m) const
{
    const float p = m.chat;
    if (p <= 0.01f)
        return;
    const Look &k = *look_;
    const Rect theatre = theatre_rect();
    g.l.push_opacity(p);
    if (m.theatre > 0.01f)
    {
        // What is on, above the picture; under it the timeline, in the page's own ink.
        g.l.push_opacity(m.theatre);
        float x = theatre.x;
        if (view_.live)
            x += live_tag(g, theatre.x, 91.0f) + 18.0f;
        text(g, g.f.display, fit(g.f.display, view_.title, kTypeTitle, theatre.x + theatre.w - x), x, 102.0f, kTypeTitle, k.ink);
        const std::string &second = view_.live && !view_.live_status.empty() ? view_.live_status : view_.meta;
        float line_x = theatre.x;
        if (!view_.channel.empty())
        {
            avatar(g, theatre.x + 16.0f, 134.0f, 14.0f, view_.channel, view_.channel_id, picture(view_.avatar, PictureSize::avatar));
            line_x = theatre.x + 40.0f + text(g, g.f.semibold, fit(g.f.semibold, view_.channel, kTypeMeta, 460.0f), theatre.x + 40.0f, 141.0f, kTypeMeta, k.ink) + 16.0f;
        }
        if (!second.empty())
            text(g, g.f.regular, fit(g.f.regular, second, kTypeMeta, theatre.x + theatre.w - line_x), line_x, 141.0f, kTypeMeta, k.ink2);
        const Rect lane{theatre.x, theatre.y + theatre.h + 44.0f, theatre.w, 0.0f};
        if (view_.live)
            draw_live_bar(g, lane, 1.0f, true);
        else
            draw_timeline(g, lane, 1.0f, true);
        hint_row(g, chat_.hints, theatre.x, kHintY, false, false, theatre.w, 34.0f, kTypeHint, 36.0f);
        g.l.pop_opacity();
    }

    // The column: beside the video in the theatre layout, over it otherwise.
    const bool on_left = chat_.side == 1;
    const Rect over{on_left ? 48.0f : 1408.0f, 184.0f, 464.0f, 604.0f};
    Rect column=kChatColumn;
    if(on_left) column.x=gfx::kVirtualWidth-column.x-column.w;
    const Rect side = lerp_rect(over, column, m.theatre);
    const float travel = g.reduced ? 0.0f : (1.0f - p) * (on_left ? -120.0f : 120.0f);
    g.l.push_transform(1.0f, 0.0f, 0.0f, travel, 0.0f);
    lift(g, side, kRadiusPanel, 0.8f);
    glass_panel(g, side, kRadiusPanel, 1.1f);
    float title_room = side.w - 60.0f;
    if (!chat_.filter.empty())
    {
        const float width = chip_width(g, chat_.filter, 38.0f);
        chip(g, side.x + side.w - width - 30.0f, side.y + 48.0f, chat_.filter, false, 0.0f, 38.0f);
        title_room -= width + 16.0f;
    }
    const std::string title = chat_.title.empty() ? std::string(tr("Live chat")) : chat_.title;
    text(g, g.f.display, fit(g.f.display, title, 30.0f, title_room), side.x + 30.0f, side.y + 58.0f, 30.0f, k.ink);
    g.l.rounded_rect({side.x + 24.0f, side.y + 86.0f, side.w - 48.0f, 1.5f}, 0.0f, k.hairline);

    const float foot = chat_.compose.empty() ? 20.0f : 96.0f;
    const float text_w = side.w - 110.0f;
    if (chat_.lines.empty())
    {
        paragraph(g, g.f.regular, chat_.status.empty() ? tr("No messages yet.") : chat_.status.c_str(), side.x + 30.0f, side.y + 140.0f, kTypeNote,
                  side.w - 60.0f, 30.0f, k.ink2, 6);
    }
    else
    {
        // Messages stack from the foot upward; the newest arrives with a rise.
        g.l.push_clip({side.x, side.y + 92.0f, side.w, side.h - 92.0f - foot});
        const float fresh = tween::cubic_out(chat_fresh_ / 0.35f);
        float y = side.y + side.h - foot - 16.0f;
        float newest_h = 0.0f;
        for (std::size_t i = 0; i < chat_.lines.size() && y > side.y + 60.0f; ++i)
        {
            const ChatLine &line = chat_.lines[chat_.lines.size() - 1 - i];
            const TextBlock block = measure_block(g.f.regular, line.text, kTypeNote, text_w, 28.0f, 4);
            const float height = 34.0f + 28.0f * static_cast<float>(std::max(1, block.lines));
            if (i == 0)
                newest_h = height + 12.0f;
            y -= height + 12.0f;
            // The lines above make room first; the new one fades into it over the second half of the move.
            const float dy = g.reduced ? 0.0f : i == 0 ? (1.0f - fresh) * 14.0f : (1.0f - fresh) * newest_h;
            const float top = y + dy;
            g.l.push_opacity((i == 0 ? tween::smoothstep(fresh * 2.0f - 1.0f) : 1.0f) * tween::clamp01((top - side.y - 70.0f) / 60.0f));
            avatar(g, side.x + 44.0f, top + 22.0f, 16.0f, line.author, line.author, picture(line.avatar, PictureSize::avatar));
            const float badge_room = line.badge == 2 ? 70.0f : line.badge == 3 ? 34.0f : 0.0f;
            const float name_w = text(g, g.f.semibold, fit(g.f.semibold, line.author, kTypeSmall, text_w - badge_room), side.x + 72.0f, top + 24.0f, kTypeSmall,
                                      line.badge == 3 ? k.accent : name_color(g, line.author));
            if (line.badge == 2)
                tag(g, side.x + 72.0f + name_w + 10.0f, top + 17.0f, tr("MOD"), k.raised, k.ink, 22.0f);
            if (line.badge == 3)
                icon(g, Icon::check, side.x + 72.0f + name_w + 18.0f, top + 17.0f, 18.0f, k.accent);
            paragraph(g, g.f.regular, line.text, side.x + 72.0f, top + 52.0f, kTypeNote, text_w, 28.0f, k.ink.with_alpha(0.92f), 4);
            g.l.pop_opacity();
        }
        g.l.pop_clip();
    }
    if (!chat_.compose.empty())
    {
        const Rect compose{side.x + 20.0f, side.y + side.h - 84.0f, side.w - 40.0f, 60.0f};
        g.l.rounded_rect(compose, 30.0f, k.raised);
        g.l.bordered_rect(compose, 30.0f, clear(), 1.5f, k.hairline);
        text(g, g.f.regular, fit(g.f.regular, chat_.compose, kTypeNote, compose.w - 100.0f), compose.x + 26.0f, compose.cy() + 7.5f, kTypeNote, k.ink2);
        ui::draw_button(g.l, g.f, k.dark ? ui::GlyphStyle::dark() : ui::GlyphStyle::light(), ui::Button::square, compose.x + compose.w - 50.0f, compose.cy(), 30.0f);
    }
    g.l.pop_transform();
    g.l.pop_opacity();
}

void Player::draw_next(G &g, const Motion &m) const
{
    const float p = m.next;
    if (p <= 0.01f)
        return;
    const Look &k = *look_;
    const float travel = g.reduced ? 0.0f : 1.0f - p;
    g.l.push_opacity(p);
    // What just ended, top left, for the moment before the next begins.
    text(g, g.f.regular, tr("You watched"), kEdge, 96.0f, kTypeNote, kOnVideo.with_alpha(0.6f));
    text(g, g.f.display, fit(g.f.display, next_.watched.empty() ? view_.title : next_.watched, kTypePanelTitle, 1100.0f), kEdge - 2.0f, 138.0f, kTypePanelTitle,
         kOnVideo);

    // The end: the next video offers itself, with a way out.
    g.l.push_transform(1.0f, 0.0f, 0.0f, 0.0f, 70.0f * travel);
    const Rect card = kNextCard;
    lift(g, card, 36.0f, 1.2f);
    glass_panel(g, card, 36.0f, 1.25f);
    overline(g, tr("Up next"), card.x + 40.0f, card.y + 58.0f, k.accent);
    ui::Canvas canvas{g.l, fonts_, g.glass, time_};
    ring_.draw(canvas);
    const Rect thumb{card.x + 40.0f, card.y + 86.0f, 288.0f, 162.0f};
    const Picture shot = picture(next_.card.thumb, PictureSize::card);
    if (shot.ready())
        g.l.image(shot.texture, thumb, shot.cover(thumb.w / thumb.h), kWhite, 14.0f);
    else
        picture_placeholder(g, thumb, 14.0f);
    const std::string length = duration_text(next_.card.duration_s);
    if (!length.empty())
        tag(g, thumb.x + thumb.w - tag_width(g, length) - 10.0f, thumb.y + thumb.h - 24.0f, length, kBlack.with_alpha(0.72f), kWhite);
    const float tx = thumb.x + thumb.w + 28.0f;
    const float room = card.x + card.w - 40.0f - tx;
    // The ring takes the corner: the title's first line stops short of it.
    paragraph(g, g.f.semibold, next_.card.title, tx, thumb.y + 34.0f, kTypeShelf, room - 4.0f, 34.0f, k.ink, 3);
    text(g, g.f.regular, fit(g.f.regular, next_.card.channel, kTypeNote, room), tx, thumb.y + 138.0f, kTypeNote, k.ink2);
    text(g, g.f.regular, fit(g.f.regular, next_.card.meta, kTypeSmall, room), tx, thumb.y + 164.0f, kTypeSmall, k.ink3);
    const std::string play = tr("Play now");
    const std::string cancel = tr("Cancel");
    const float play_w = std::max(300.0f, button_width(g, play, true));
    const float cancel_w = std::max(200.0f, button_width(g, cancel, true));
    // One white plate at a time: the primary button rests filled only while it has the focus.
    button(g, {card.x + 40.0f, card.y + 296.0f, play_w, 64.0f}, play, Icon::play, next_.focus == 0 ? 1.0f : 0.0f, next_.focus == 0);
    button(g, {card.x + 56.0f + play_w, card.y + 296.0f, std::min(cancel_w, card.w - 96.0f - play_w), 64.0f}, cancel, Icon::close, next_.focus == 1 ? 1.0f : 0.0f);
    g.l.pop_transform();
    g.l.pop_opacity();
}

} // namespace afterglow::player
