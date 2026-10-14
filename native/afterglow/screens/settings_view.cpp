// ProsperoTube Afterglow - Settings: what moves, the hints, and the screen's drawing.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The pieces themselves (a tile, a row, the previews) are in settings_draw.cpp;
// this places them: the tiles, a page with its form, preview and help, the sheet.

#include "afterglow/screens/settings_screen.hpp"

namespace afterglow::settings
{

void Settings::advance(UpdateContext &context)
{
    const float dt = context.dt;
    const bool reduced = context.reduced_motion;
    const float quick = reduced ? 60.0f : 20.0f;
    edge_.update(dt, 10.0f);
    if (home_)
    {
        // A fourth row of tiles scrolls the grid; three rows never move.
        const int row = focus_ / kTileColumns;
        tiles_scroll_.target = shown_.size() > 12 ? static_cast<float>(std::max(0, row - 2)) * kTilePitchY : 0.0f;
        tiles_scroll_.update(dt, reduced ? 60.0f : 12.0f);
        const Rect target = grown(tile_rect(focus_, tiles_scroll_.target), 0.03f, 4.0f);
        if (age_ < 0.1f)
            ring_.snap(target);
        else
            ring_.target(target);
        ring_.update(dt, quick);
        return;
    }
    scroll_.update(dt, reduced ? 60.0f : 14.0f);
    if (focus_ >= 0 && focus_ < static_cast<int>(lines_.size()))
        plate_.target = lines_[static_cast<std::size_t>(focus_)].y;
    plate_.update(dt, quick);
    grab_.target = grabbed_ ? 1.0f : 0.0f;
    grab_.update(dt, reduced ? 60.0f : 16.0f);
    for (const Item &item : items_)
    {
        Motion &motion = motion_[key_of(item)];
        const Shown now = shown(item);
        motion.on.target = now.on ? 1.0f : 0.0f;
        float fill = number_of(item.row.fact("fill"), 0.0f);
        int count = item.count;
        int index = now.index;
        const auto listed = choices_.find(item.row.id);
        if (item.kind == Kind::slider && listed != choices_.end() && listed->second.ready && !listed->second.rows.empty())
        {
            // Its choices are here: they say how many steps there are and, where the row does not, which one stands.
            count = static_cast<int>(listed->second.rows.size());
            for (int i = 0; index < 0 && i < count; ++i)
            {
                if (listed->second.rows[static_cast<std::size_t>(i)].on)
                    index = i;
            }
        }
        if (count > 1 && index >= 0)
            fill = static_cast<float>(std::min(index, count - 1)) / static_cast<float>(count - 1);
        motion.fill.target = fill;
        motion.waiting.target = now.waiting ? 1.0f : 0.0f;
        if (!motion.placed)
        {
            motion.on.snap(motion.on.target);
            motion.fill.snap(fill);
            motion.placed = true;
        }
        if (reduced)
            motion.on.snap(motion.on.target);
        else
            motion.on.update(dt, 18.0f, 0.62f);
        motion.fill.update(dt, quick);
        motion.waiting.update(dt, 9.0f);
        motion.applied.update(dt, 2.6f);
        motion.refused.update(dt, 7.0f);
        motion.nudge.update(dt, 9.0f);
    }
    preview(reduced, dt);
}

// What the right-hand picture shows: every row that feeds it says so with its fact "preview".
void Settings::preview(bool reduced, float dt)
{
    PreviewState next;
    const Item *under = focused();
    const auto kind_of = [](const std::string &what)
    {
        return starts_with(what, "caption") ? PreviewKind::captions : what == "theme" ? PreviewKind::theme : starts_with(what, "card") ? PreviewKind::card : PreviewKind::none;
    };
    PreviewKind first = PreviewKind::none;
    for (const Item &item : items_)
    {
        const std::string &what = item.row.fact("preview");
        if (!item.row.fact("preview_art_url").empty())
            next.picture = item.row.fact("preview_art_url");
        if (what.empty())
            continue;
        if (first == PreviewKind::none)
            first = kind_of(what);
        // On a list of choices only the one under the focus speaks: moving through the list tries each on.
        if (item.kind == Kind::choice && &item != under)
            continue;
        const Shown now = shown(item);
        const model::Row &row = now.choice != nullptr ? *now.choice : item.row;
        if (what == "caption.on")
            next.captions = now.on;
        else if (what == "caption.size")
            next.scale = number_of(row.fact("number"), 100.0f) / 100.0f;
        else if (what == "caption.position")
            next.position = number_of(row.fact("number"), -1.0f) < 0.0f ? -1.0f : number_of(row.fact("number"), 0.0f) / 1000.0f;
        else if (what == "caption.style")
        {
            next.text = colour_of(row.fact("text_rgb"), kWhite);
            next.plate = number_of(row.fact("background_alpha"), 140.0f) / 255.0f;
            next.edge = row.fact("edge") == "outline" ? 1 : row.fact("edge") == "shadow" ? 2 : 0;
        }
        else if (what == "theme")
        {
            next.look = row.fact("look");
            next.name = item.kind == Kind::choice ? item.label : now.value;
            next.ground = colour_of(row.fact("background"), kBlack);
            next.accent = colour_of(row.fact("accent"), kWhite);
        }
        else if (what == "card.scale")
            next.interface_scale = number_of(row.fact("number"), 100.0f) / 100.0f;
        else if (what == "card.grid")
            next.grid_scale = number_of(row.fact("number"), 100.0f) / 100.0f;
        else if (what == "card.title_lines")
            next.title_lines = static_cast<int>(number_of(row.fact("number"), 1.0f));
        else if (what == "card.subtitle_lines")
            next.subtitle_lines = item.kind == Kind::toggle ? (now.on ? 2 : 1) : static_cast<int>(number_of(row.fact("number"), 1.0f));
        else if (what == "card.corners")
            next.rounded = now.on;
    }
    // The picture follows the row under the focus, and stays when the focus is on a row that feeds none.
    if (under != nullptr && kind_of(under->row.fact("preview")) != PreviewKind::none)
        sticky_ = kind_of(under->row.fact("preview"));
    else if (sticky_ == PreviewKind::none)
        sticky_ = first;
    next.kind = first == PreviewKind::none ? PreviewKind::none : sticky_;
    // Sizes and places glide to what was chosen.
    look_scale_.target = next.scale;
    look_position_.target = next.position;
    look_plate_.target = next.plate;
    look_interface_.target = next.interface_scale;
    look_grid_.target = next.grid_scale;
    if (!preview_placed_ || reduced || (preview_.position < 0.0f) != (next.position < 0.0f))
    {
        look_scale_.snap(next.scale);
        look_position_.snap(next.position);
        look_plate_.snap(next.plate);
        look_interface_.snap(next.interface_scale);
        look_grid_.snap(next.grid_scale);
        preview_placed_ = next.kind != PreviewKind::none;
    }
    for (tween::Spring *spring : {&look_scale_, &look_position_, &look_plate_, &look_interface_, &look_grid_})
        spring->update(dt, 16.0f);
    preview_ = std::move(next);
    preview_.scale = look_scale_.value;
    preview_.position = look_position_.value;
    preview_.plate = look_plate_.value;
    preview_.interface_scale = look_interface_.value;
    preview_.grid_scale = look_grid_.value;
}

void Settings::make_hints()
{
    hint_count_ = 0;
    const auto add = [&](ui::Button button, const char *label, ui::Button second = ui::Button::none)
    {
        if (hint_count_ < hints_.size())
            hints_[hint_count_++] = {button, label, second};
    };
    if (home_)
    {
        const Source &main = sources_.front();
        add(ui::Button::cross, tr("Open"));
        add(ui::Button::circle, tr("Back"));
        if (main.load != model::Load::ready && main.load != model::Load::loading && main.load != model::Load::idle)
            add(ui::Button::triangle, tr("Try again"));
        else if (!shown_.empty() && tile_reset(tiles()[static_cast<std::size_t>(shown_[static_cast<std::size_t>(focus_)])], main.rows) != nullptr)
            add(ui::Button::options, tr("Reset a section"));
        return;
    }
    const Item *item = page_load() == model::Load::ready ? focused() : nullptr;
    if (item == nullptr)
    {
        if (page_load() != model::Load::loading)
            add(ui::Button::cross, page_load() == model::Load::ready ? tr("Back to Settings") : tr("Try again"));
        add(ui::Button::circle, tr("Back"));
        return;
    }
    if (grabbed_)
    {
        add(ui::Button::dpad, tr("Move"));
        add(ui::Button::square, tr("Show or hide"));
        add(ui::Button::cross, tr("Done"));
        return;
    }
    switch (item->kind)
    {
    case Kind::toggle:
        add(ui::Button::dpad, tr("Change"));
        add(ui::Button::cross, tr("Switch"));
        break;
    case Kind::list:
    case Kind::slider:
        add(ui::Button::dpad, tr("Change"));
        add(ui::Button::cross, tr("All choices"));
        break;
    case Kind::cycle:
        add(ui::Button::cross, tr("Next"));
        break;
    case Kind::choice:
        add(ui::Button::cross, tr("Choose"));
        break;
    case Kind::action:
        add(ui::Button::cross, tr("Select"));
        break;
    case Kind::text:
        add(ui::Button::cross, tr("Edit"));
        break;
    case Kind::page:
    case Kind::link:
        add(ui::Button::cross, tr("Open"));
        break;
    case Kind::order:
        add(ui::Button::cross, tr("Move"));
        add(ui::Button::square, tr("Show or hide"));
        add(ui::Button::triangle, tr("More"));
        break;
    case Kind::info:
    case Kind::reset:
        break;
    }
    if (has_reset_ && item->kind != Kind::order)
        add(ui::Button::triangle, tr("Defaults"));
    add(ui::Button::circle, tr("Back"));
}

float Settings::arrive(int index, float step, float duration) const
{
    return tween::stagger(age_, index, step, duration);
}

// 0..1 while the bones give way to what arrived, in 0.3 s; 1 at once when there were no bones to see.
float Settings::settled() const
{
    return age_ - shown_age_ > 0.08f ? tween::clamp01(shown_age_ / 0.3f) : 1.0f;
}

// The entrance of what arrived late: counted from its arrival, not from the page's.
float Settings::arrive_shown(int index, float step, float duration) const
{
    return age_ - shown_age_ > 0.08f ? tween::stagger(shown_age_, std::max(index - 1, 0), step, duration) : arrive(index, step, duration);
}

void Settings::draw_home(G &g) const
{
    const Look &k = g.k;
    const Source &main = sources_.front();
    const bool loading = main.load == model::Load::idle || main.load == model::Load::loading;
    {
        Enter enter(g, arrive(0, 0.05f, 0.5f), -14.0f);
        const std::string who = profile_.guest || profile_.name.empty() ? std::string(tr("Guest")) : profile_.name;
        text(g, g.f.regular, fit(g.f.regular, filled(tr("For {0} \xC2\xB7 saved on this PS5"), who), 22.0f, 1200.0f), kLeft, 166.0f, 22.0f, k.ink2);
    }
    const bool scrolls = shown_.size() > 12;
    const float scroll = tiles_scroll_.value;
    if (scrolls)
        g.l.push_clip({kRail + 8.0f, 196.0f, gfx::kVirtualWidth - kRail - 8.0f, 772.0f});
    const Rect ring = ring_.value();
    for (int pass = 0; pass < 2; ++pass)
    {
        for (std::size_t i = 0; i < shown_.size(); ++i)
        {
            const bool has_focus = static_cast<int>(i) == focus_;
            if ((pass == 1) != has_focus)
                continue;
            const Tile &tile = tiles()[static_cast<std::size_t>(shown_[i])];
            const Rect base = tile_rect(static_cast<int>(i), scroll);
            const float focus = tween::clamp01(1.0f - std::hypot(ring.cx() - base.cx(), ring.cy() - base.cy() + 4.0f) / 220.0f);
            Enter enter(g, arrive(1 + static_cast<int>(i) % kTileColumns + static_cast<int>(i) / kTileColumns, 0.055f, 0.45f), 28.0f);
            const float here = loading && main.rows.empty() ? 0.0f : settled();
            if (here < 1.0f)
            {
                Faded bones(g, 1.0f - here);
                draw_tile_bones(g, base, 0.11f * static_cast<float>(i));
            }
            if (here > 0.0f)
            {
                Faded content(g, here);
                draw_tile(g, base, tile, tr(tile.name),
                          shown_[i] + 1 == static_cast<int>(tiles().size()) ? tr_n("{0} further setting", "{0} further settings", static_cast<long>(unknown_)) : tile_summary(tile, main.rows),
                          focus, tile_attention(tile, main.rows), false);
            }
        }
    }
    const float nudge = g.reduced ? 0.0f : ui::shake(edge_.value, g.time);
    focus_ring(g, {ring.x + nudge, ring.y, ring.w, ring.h}, kRadiusPanel, k.accent, arrive(4, 0.05f, 0.4f));
    {
        Enter enter(g, arrive(6, 0.06f, 0.5f), 16.0f);
        const float y = kTilesTop + static_cast<float>((shown_.size() + kTileColumns - 1) / kTileColumns) * kTilePitchY - scroll + 40.0f;
        if (main.load == model::Load::failed || main.load == model::Load::offline)
        {
            // The tiles stay: what is another screen's still opens. This line says what is missing.
            icon(g, main.load == model::Load::offline ? Icon::offline : Icon::alert, kLeft + 14.0f, y - 8.0f, 28.0f, k.ink2);
            const std::string words = main.error.empty() ? std::string(tr("The settings could not be read. Triangle reads them again.")) : main.error;
            text(g, g.f.regular, fit(g.f.regular, words, 21.0f, kRight - kLeft - 44.0f), kLeft + 44.0f, y, 21.0f, k.ink2);
        }
        else if (!loading && main.rows.empty())
        {
            text(g, g.f.regular, fit(g.f.regular, tr("ProsperoTube listed no settings for this profile. Accounts and Phone still open."), 21.0f, kRight - kLeft), kLeft, y, 21.0f,
                 k.ink2);
        }
        else
        {
            text(g, g.f.regular, fit(g.f.regular, tr("Settings follow the profile in use. Interface and updates apply to the console."), 21.0f, kRight - kLeft), kLeft, y,
                 21.0f, k.ink3);
        }
    }
    if (scrolls)
        g.l.pop_clip();
}

void Settings::draw_page(G &g, const DrawContext &context) const
{
    const Look &k = g.k;
    draw_path(g, path_, arrive(0, 0.05f, 0.5f));
    const model::Load state = page_load();
    if (state == model::Load::loading)
    {
        draw_form_bones(g, arrive(1, 0.06f, 0.4f));
        return;
    }
    if (settled() < 1.0f)
        draw_form_bones(g, 1.0f - settled());
    if (state != model::Load::ready || items_.empty())
    {
        Enter enter(g, arrive_shown(1, 0.06f, 0.5f), 22.0f);
        if (state == model::Load::ready)
            draw_notice(g, Icon::info, tr("Nothing to set here"), tr("This page has no settings for the profile in use. Other pages of Settings are not affected."),
                        tr("Back to Settings"), 1.0f, kNoticeButton);
        else
            draw_notice(g, state == model::Load::offline ? Icon::offline : Icon::alert,
                        state == model::Load::offline ? tr("These settings cannot be reached") : tr("These settings could not be read"),
                        page_error().empty() ? std::string(tr("Nothing was changed. What is saved on this PS5 stays as it is.")) : page_error(), tr("Try again"), 1.0f,
                        kNoticeButton);
        return;
    }

    const float scroll = scroll_.offset();
    const Item *under = focused();
    const float since = age_ - shown_age_ > 0.08f ? shown_age_ : age_;
    {
        Enter enter(g, arrive_shown(1, 0.06f, 0.5f), 22.0f);
        g.l.push_clip({kFormX - 14.0f, kFormTop - 8.0f, kFormW + 28.0f, kView + 16.0f});
        // The plate glides with the focus; a row held in the move mode lifts on it.
        const float nudge = g.reduced ? 0.0f : ui::shake(edge_.value, g.time);
        const float refusal = under != nullptr ? shake_of(*under, g) : 0.0f;
        draw_plate(g, {kFormX + nudge + refusal, kFormTop + plate_.value - scroll, kFormW, kRowH}, 1.0f, grab_.value);
        int seen = 0;
        for (const Line &line : lines_)
        {
            const float y = kFormTop + line.y - scroll;
            if (y + line.h < kFormTop - 30.0f || y > kFormBottom + 30.0f)
                continue;
            // A row the edge of the form cuts fades: gone when half of it is cut. Never the row under the
            // focus: that one is on its way into view.
            const float cut = std::max(kFormTop - 2.0f - y, y + line.h - kFormBottom - 2.0f);
            const bool is_under = line.item >= 0 && under == &items_[static_cast<std::size_t>(line.item)];
            const float whole = is_under ? 1.0f : tween::clamp01((0.5f - cut / line.h) / 0.4f);
            if (whole <= 0.01f)
                continue;
            Faded faded(g, whole);
            const float amount = tween::stagger(since, 2 + seen++, 0.03f, 0.35f);
            Enter row_enter(g, amount, 12.0f);
            if (line.item < 0)
            {
                draw_heading(g, {kFormX, y, kFormW, line.h}, line.heading);
                continue;
            }
            const Item &item = items_[static_cast<std::size_t>(line.item)];
            const Shown now = shown(item);
            const auto found = motion_.find(key_of(item));
            RowLook look;
            look.value = now.value;
            if (item.kind == Kind::order)
                look.value = now.on ? tr("Shown") : tr("Hidden");
            else if (item.kind == Kind::text && item.row.has("secret") && !now.value.empty())
                look.value = "\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2";
            look.focus = tween::clamp01(1.0f - std::fabs(plate_.value - line.y) / 60.0f);
            look.on = now.on ? 1.0f : 0.0f;
            if (found != motion_.end())
            {
                const Motion &motion = found->second;
                look.on = motion.on.value;
                look.fill = motion.fill.value;
                look.pending = tween::clamp01(motion.waiting.value * 1.6f - 0.6f); // a change the provider answers at once never shows as waiting
                look.applied = motion.applied.value;
                look.nudge = motion.nudge.value * motion.direction;
            }
            const bool held = grabbed_ && &item == under;
            look.grabbed = held ? grab_.value : 0.0f;
            look.can_up = can_move(item, -1);
            look.can_down = can_move(item, 1);
            const float shift = &item == under ? nudge + refusal : shake_of(item, g);
            draw_row(g, {kFormX + shift, y, kFormW, kRowH}, item, look);
        }
        // More of a long list is on its way: its shape under the last row.
        if (sources_.back().more)
        {
            const float y = kFormTop + content_ + kRowGap - scroll;
            if (y < kFormBottom)
            {
                const float pulse = g.reduced ? 0.8f : 0.6f + 0.4f * ui::breathe(g.time, 1.7f);
                g.l.rounded_rect({kLeft, y + 24.0f, 260.0f, 22.0f}, 11.0f, k.raised.with_alpha(k.raised.a * pulse));
            }
        }
        g.l.pop_clip();
        if (content_ > kView + 1.0f)
        {
            // How far down the page the view is.
            const Rect track{1039.0f, kFormTop + 8.0f, 3.0f, kView - 16.0f};
            g.l.rounded_rect(track, 1.5f, k.hairline);
            const float thumb = std::max(64.0f, track.h * kView / content_);
            g.l.rounded_rect({track.x - 1.0f, track.y + (track.h - thumb) * tween::clamp01(scroll / (content_ - kView)), 5.0f, thumb}, 2.5f, k.ink3);
        }
    }
    {
        Enter enter(g, arrive_shown(2, 0.07f, 0.55f), 0.0f, 44.0f);
        const bool pictured = preview_.kind != PreviewKind::none;
        if (pictured)
            draw_preview(g, kPreviewBox, preview_, context.images.get(preview_.picture, PictureSize::hero), 1.0f);
        // The focused row explains itself here, in full sentences.
        // A name its row had to cut is said in full here.
        const bool cut = under != nullptr && measure(g.f.semibold, under->label, 25.0f) > 500.0f;
        const std::string_view heading = cut ? std::string_view(under->label) : std::string_view();
        const float tall = (cut ? 62.0f : 0.0f) + 148.0f;
        const Rect help = pictured ? Rect{kPreviewBox.x, kPreviewBox.y + kPreviewBox.h + 28.0f, kPreviewBox.w, tall}
                                   : Rect{kPreviewBox.x, kPreviewBox.y, kPreviewBox.w, under != nullptr && !under->row.overview.empty() ? 330.0f : tall};
        if (under != nullptr && !said_row_.empty() && said_row_ == key_of(*under))
        {
            draw_help(g, help, heading, said_, {}, true);
        }
        else
        {
            const std::string words = under != nullptr && !under->detail.empty() ? under->detail : std::string(tr("Changes are saved as you make them, for this profile."));
            draw_help(g, help, heading, words, under != nullptr ? std::string_view(under->row.overview) : std::string_view());
        }
    }
}

// A refused change shakes its row.
float Settings::shake_of(const Item &item, const G &g) const
{
    const auto found = motion_.find(key_of(item));
    return found == motion_.end() || g.reduced ? 0.0f : ui::shake(found->second.refused.value, g.time, 12.0f);
}

} // namespace afterglow::settings
