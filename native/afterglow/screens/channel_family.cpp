// ProsperoTube Afterglow - what the channel, channels and subscriptions screens share.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/screens/channel_family.hpp"

namespace afterglow::family
{

namespace
{

// The message of a page that has nothing to show.
constexpr float kNoticeTitle = 36.0f;
constexpr float kNoticeLine = 33.0f;
constexpr float kNoticeWidth = 680.0f;

// The sheet of a row's actions (board 14).
constexpr float kSheetX = 1296.0f;
constexpr float kSheetRowX = 1336.0f;
constexpr float kSheetRowW = 536.0f;
constexpr float kSheetRowH = 68.0f;
constexpr float kSheetPitch = 72.0f;
constexpr float kSheetTop = 196.0f;
constexpr int kSheetShown = 10;
constexpr float kSheetPan = 0.45f;

bool is_notice(std::string_view id)
{
    return starts_with(id, "account:notice");
}

// A change that takes something away is asked once more before it is made.
bool asks_first(std::string_view id)
{
    return id.find(":unsubscribe:") != std::string_view::npos || id.find("remove") != std::string_view::npos ||
           id.find(":block:") != std::string_view::npos || id.find("dont_recommend") != std::string_view::npos ||
           id.find(":forget:") != std::string_view::npos;
}

// A row of a menu that is another page of the interface.
bool route_of(const model::Row &row, Route *route)
{
    const std::string &id = row.id;
    if (starts_with(id, "channel:"))
        *route = Route{ScreenId::channel, id.substr(8), row.subtitle};
    else if (starts_with(id, "details:"))
        *route = Route{ScreenId::video, id.substr(8), {}};
    else if (starts_with(id, "description:"))
        *route = Route{ScreenId::reader, id, row.title};
    else if (starts_with(id, "discussion:"))
        *route = Route{ScreenId::comments, id, row.title};
    else if (starts_with(id, "library:playlist:") || starts_with(id, "library:suggestions:") ||
             (starts_with(id, "playlist:") && !starts_with(id, "playlist:pick:") && !starts_with(id, "playlist:confirm:") &&
              !starts_with(id, "playlist:prompt:")))
        *route = Route{ScreenId::grid, id, row.title};
    else
        return false;
    return true;
}

Icon icon_of(std::string_view id)
{
    const auto has = [&](std::string_view part) { return id.find(part) != std::string_view::npos; };
    if (starts_with(id, "channel:"))
        return Icon::user;
    if (starts_with(id, "description:"))
        return Icon::chapters;
    if (starts_with(id, "details:"))
        return Icon::info;
    if (starts_with(id, "discussion:"))
        return Icon::chat;
    if (starts_with(id, "link:"))
        return Icon::share;
    if (starts_with(id, "notify:") || starts_with(id, "reminder:"))
        return Icon::bell;
    if (has("playlist"))
        return Icon::playlist_add;
    if (has("watch_later"))
        return Icon::clock;
    if (has("unsubscribe") || has("unblock"))
        return Icon::close;
    if (has("subscribe"))
        return Icon::plus;
    if (has(":dislike:") || has("not_interested") || has("dont_recommend") || has(":hide"))
        return Icon::hide;
    if (has(":like:"))
        return Icon::heart;
    if (has("block"))
        return Icon::block;
    if (has("watched"))
        return Icon::check;
    if (has(":next:") || has("queue"))
        return Icon::queue;
    if (has("incognito"))
        return Icon::moon;
    if (has("restart") || has("refresh"))
        return Icon::update;
    if (has("sbexclude") || has("sbinclude"))
        return Icon::skip;
    if (has("suggestions"))
        return Icon::grid;
    if (has("channel-sort") || has(":sort"))
        return Icon::sort;
    return Icon::more;
}

} // namespace

// ---- motion -----------------------------------------------------------------------------

Travel::Travel(G &g, const Phase &phase) : l(g.l)
{
    float dx = 0.0f;
    float scale = 1.0f;
    if (phase.out > 0.0f)
    {
        const float t = tween::clamp01(phase.out);
        switch (phase.move)
        {
        case Move::push:
            alpha = 1.0f - tween::cubic_in(t);
            dx = -90.0f * tween::cubic_in(t);
            break;
        case Move::pop:
            alpha = 1.0f - tween::cubic_in(t);
            dx = 90.0f * tween::cubic_in(t);
            break;
        case Move::zoom:
            alpha = 1.0f - tween::clamp01(t / 0.29f);
            break;
        case Move::fade:
            alpha = 1.0f - tween::clamp01(t / 0.4f);
            scale = 1.0f - 0.025f * tween::clamp01(t / 0.4f);
            break;
        default:
            alpha = 1.0f - t;
            break;
        }
    }
    else if (phase.in < 1.0f)
    {
        const float t = tween::clamp01(phase.in);
        switch (phase.move)
        {
        case Move::push:
            alpha = tween::clamp01(t * 2.2f);
            dx = 110.0f * (1.0f - tween::quint_out(t));
            break;
        case Move::pop:
            alpha = tween::clamp01(t * 2.2f);
            dx = -110.0f * (1.0f - tween::quint_out(t));
            break;
        case Move::fade:
            alpha = tween::clamp01((t - 0.3f) / 0.5f);
            break;
        case Move::shrink:
            alpha = tween::clamp01(t * 2.0f);
            break;
        default:
            alpha = t;
            break;
        }
    }
    if (g.reduced)
    {
        dx = 0.0f;
        scale = 1.0f;
    }
    l.push_opacity(alpha);
    l.push_transform(scale, gfx::kVirtualWidth * 0.5f, gfx::kVirtualHeight * 0.5f, dx, 0.0f);
}

Travel::~Travel()
{
    l.pop_transform();
    l.pop_opacity();
}

void play(const model::Video &video, float x, UpdateContext &context)
{
    if (context.services.data.play(video))
    {
        context.feedback.play(audio::Cue::launch, 1.0f, ui::pan_for_x(x));
        context.feedback.rumble(0.6f, 0.16f);
        return;
    }
    context.feedback.play(audio::Cue::error, 1.0f, ui::pan_for_x(x));
    context.nav.toast(tr("This video cannot be played right now."), ToastKind::error);
}

// ---- pieces -----------------------------------------------------------------------------

// ---- Sheet ------------------------------------------------------------------------------

Sheet::Sheet() : alive_(std::make_shared<int>(0)) {}

void Sheet::show(const std::string &row, std::string_view about, UpdateContext &context)
{
    const std::string menu = context.services.data.menu(row, std::string(about));
    if (menu.empty())
    {
        context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        return;
    }
    menu_ = menu;
    about_ = about;
    levels_.clear();
    levels_.emplace_back();
    levels_.back().title = tr("Actions");
    load_ = model::Load::loading;
    error_.clear();
    acting_ = -1;
    confirmed_.clear();
    open_ = true;
    pos_.snap(0.0f);
    scroll_.snap(0.0f);
    context.feedback.play(audio::Cue::modal_open, 1.0f, kSheetPan);
}

void Sheet::close(UpdateContext &context, bool sound)
{
    open_ = false;
    acting_ = -1;
    if (sound)
        context.feedback.play(audio::Cue::modal_close, 1.0f, kSheetPan);
}

void Sheet::begin(const model::Row &row, UpdateContext &context)
{
    acting_ = levels_.back().focus;
    acting_title_ = row.title;
    act_.start(context.services.data, row.id);
    context.feedback.play(audio::Cue::select, 1.0f, kSheetPan);
}

bool Sheet::take_changed()
{
    const bool changed = changed_;
    changed_ = false;
    return changed;
}

Rect Sheet::row_rect(int index) const
{
    return {kSheetRowX, kSheetTop + kSheetPitch * static_cast<float>(index) - scroll_.target, kSheetRowW, kSheetRowH};
}

bool Sheet::handle(const InputFrame &input, UpdateContext &context, Route *route)
{
    if (!open_)
        return false;
    if (pressed(input, hui::Action::back))
    {
        if (levels_.size() > 1 && acting_ < 0)
        {
            levels_.pop_back(); // back to the list this one was opened from
            context.feedback.play(audio::Cue::back, 1.0f, kSheetPan);
        }
        else
        {
            close(context, true);
        }
        return false;
    }
    if (acting_ >= 0)
        return false; // the answer is on its way
    Level &level = levels_.back();
    const int count = static_cast<int>(level.rows.size());
    if (input.nav == Direction::up || input.nav == Direction::down)
    {
        const int next = level.focus + (input.nav == Direction::down ? 1 : -1);
        if (next >= 0 && next < count)
        {
            level.focus = next;
            context.feedback.play(audio::Cue::focus, 1.06f - 0.02f * static_cast<float>(std::min(next, 8)), kSheetPan);
        }
        else
        {
            refuse(context, input, nudge_, 1600.0f);
        }
    }
    else if (input.nav != Direction::none)
    {
        refuse(context, input, nudge_, 1600.0f);
    }
    else if (pressed(input, hui::Action::confirm))
    {
        if (count == 0)
        {
            if (load_ == model::Load::failed || load_ == model::Load::offline)
            {
                context.services.data.forget(menu_); // asked again at the next read
                context.feedback.play(audio::Cue::select, 1.0f, kSheetPan);
            }
            return false;
        }
        const model::Row row = level.rows[static_cast<std::size_t>(level.focus)];
        if (is_notice(row.id))
        {
            refuse(context, input, nudge_, 1600.0f);
        }
        else if (route_of(row, route))
        {
            close(context, false); // the page that opens brings its own sound
            return true;
        }
        else if (row.id == "settings:refresh")
        {
            context.services.data.refresh();
            context.feedback.play(audio::Cue::select, 1.0f, kSheetPan);
            close(context, false);
        }
        else if (asks_first(row.id))
        {
            const std::weak_ptr<int> alive = alive_;
            const std::string id = row.id;
            context.nav.confirm(row.title, about_, tr("Confirm"), [this, alive, id](bool yes)
            {
                if (!alive.expired() && yes)
                    confirmed_ = id;
            });
        }
        else
        {
            begin(row, context);
        }
    }
    return false;
}

void Sheet::update(UpdateContext &context)
{
    const bool reduced = context.reduced_motion;
    amount_.target = open_ ? 1.0f : 0.0f;
    amount_.update(context.dt, reduced ? 60.0f : 13.0f);
    nudge_.update(context.dt, 10.0f);
    if (!open_)
        return;
    Data &data = context.services.data;
    if (levels_.size() == 1 && acting_ < 0)
    {
        const Page &page = data.catalog(menu_);
        load_ = page.load;
        error_ = page.error;
        if (page.load == model::Load::ready && levels_.front().rows.size() != page.rows.size())
            levels_.front().rows = page.rows;
    }
    if (!confirmed_.empty() && acting_ < 0)
    {
        const std::string id = std::move(confirmed_);
        confirmed_.clear();
        for (const model::Row &row : levels_.back().rows)
        {
            if (row.id == id)
            {
                const model::Row chosen = row;
                begin(chosen, context);
                break;
            }
        }
    }
    if (acting_ >= 0)
    {
        const Page *answer = nullptr;
        const model::Load result = act_.poll(data, &answer);
        if (result == model::Load::ready)
        {
            const bool list = std::any_of(answer->rows.begin(), answer->rows.end(), [](const model::Row &row) { return !is_notice(row.id); });
            if (list)
            {
                // The answer is a further choice (a notification level, a playlist): it takes the sheet.
                Level level;
                level.title = acting_title_;
                level.rows = answer->rows;
                levels_.push_back(std::move(level));
                acting_ = -1;
                context.feedback.play(audio::Cue::open, 1.0f, kSheetPan);
            }
            else
            {
                context.nav.toast(answer->rows.empty() ? acting_title_ : answer->rows.front().title, ToastKind::success);
                changed_ = true;
                data.forget(menu_); // the menu may offer the opposite now
                close(context, false);
            }
        }
        else if (result != model::Load::loading)
        {
            const std::string message = answer != nullptr ? answer->error : std::string();
            context.nav.toast(message.empty() ? tr("YouTube did not confirm the change. Nothing was changed.") : message.c_str(), ToastKind::error);
            context.feedback.play(audio::Cue::error, 1.0f, kSheetPan);
            context.feedback.rumble(0.25f, 0.05f);
            acting_ = -1;
        }
    }
    if (levels_.empty())
        return;
    Level &level = levels_.back();
    const int count = static_cast<int>(level.rows.size());
    level.focus = std::clamp(level.focus, 0, std::max(count - 1, 0));
    pos_.target = static_cast<float>(level.focus);
    pos_.update(context.dt, reduced ? 60.0f : 22.0f);
    // The list keeps the row in focus among the ten that show.
    float target = scroll_.target;
    target = std::min(target, kSheetPitch * static_cast<float>(level.focus));
    target = std::max(target, kSheetPitch * static_cast<float>(level.focus - (kSheetShown - 1)));
    scroll_.target = std::clamp(target, 0.0f, kSheetPitch * static_cast<float>(std::max(count - kSheetShown, 0)));
    scroll_.update(context.dt, reduced ? 60.0f : 14.0f);
}

void Sheet::draw(G &g) const
{
    const float a = tween::clamp01(amount_.value);
    if (a <= 0.01f || levels_.empty())
        return;
    const Level &level = levels_.back();
    g.l.rounded_rect({0.0f, 0.0f, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0.0f, kBlack.with_alpha((g.k.dark ? 0.5f : 0.28f) * a));
    g.l.push_opacity(a);
    g.l.push_transform(1.0f, 0.0f, 0.0f, g.reduced ? 0.0f : (1.0f - a) * 120.0f, 0.0f);
    const Rect panel{kSheetX, -40.0f, 720.0f, 1160.0f};
    g.l.shadow({panel.x - 20.0f, panel.y, 80.0f, panel.h}, 0.0f, 80.0f, kBlack.with_alpha(g.k.dark ? 0.5f : 0.22f));
    glass_panel(g, panel, 40.0f, 1.25f);
    text(g, g.f.display, fit(g.f.display, level.title, 34.0f, 500.0f), kSheetX + 66.0f, 120.0f, 34.0f, g.k.ink);
    text(g, g.f.regular, fit(g.f.regular, about_, 21.0f, 480.0f), kSheetX + 68.0f, 156.0f, 21.0f, g.k.ink2);

    const int count = static_cast<int>(level.rows.size());
    if (count == 0)
    {
        if (load_ == model::Load::failed || load_ == model::Load::offline)
        {
            icon(g, trouble_icon(load_), kSheetX + 88.0f, 236.0f, 40.0f, g.k.ink2);
            paragraph(g, g.f.regular, error_.empty() ? tr("The actions did not arrive. Choose again to ask once more.") : error_.c_str(), kSheetX + 126.0f, 244.0f,
                      kTypeBody, 440.0f, kNoticeLine, g.k.ink2, 4);
        }
        else
        {
            for (int i = 0; i < 6; ++i)
                bone(g, {kSheetX + 70.0f, kSheetTop + 22.0f + kSheetPitch * static_cast<float>(i), i % 2 == 0 ? 380.0f : 300.0f, 24.0f}, 12.0f);
        }
    }
    else
    {
        const float scroll = scroll_.value;
        g.l.push_clip({kSheetX, kSheetTop - 10.0f, gfx::kVirtualWidth - kSheetX, kSheetPitch * static_cast<float>(kSheetShown) + 16.0f});
        Rect plate{kSheetRowX, kSheetTop + kSheetPitch * pos_.value - scroll, kSheetRowW, kSheetRowH};
        plate.x += ui::shake(nudge_.value, g.time, 10.0f, 8.0f);
        g.l.glow(plate, plate.h * 0.5f, 20.0f, g.k.plate.with_alpha(0.22f));
        g.l.rounded_rect(plate, plate.h * 0.5f, g.k.plate);
        for (int i = 0; i < count; ++i)
        {
            const model::Row &row = level.rows[static_cast<std::size_t>(i)];
            const float cy = kSheetTop + kSheetPitch * static_cast<float>(i) - scroll + kSheetRowH * 0.5f;
            if (cy < kSheetTop - 60.0f || cy > kSheetTop + kSheetPitch * static_cast<float>(kSheetShown) + 60.0f)
                continue;
            const float focus = tween::clamp01(1.0f - std::fabs(pos_.value - static_cast<float>(i)));
            const bool notice = is_notice(row.id);
            Route unused;
            const bool opens = !notice && (route_of(row, &unused) || starts_with(row.id, "notify:") || starts_with(row.id, "playlist:pick:") ||
                                           starts_with(row.id, "link:") || starts_with(row.id, "reminder:"));
            Color ink = gfx::mix(notice ? g.k.ink3 : g.k.ink, g.k.on_plate, focus);
            if (i == acting_)
                ink = ink.with_alpha(g.reduced ? 0.6f : 0.45f + 0.4f * ui::breathe(g.time, 0.9f)); // asked, not yet answered
            float x = kSheetRowX + 96.0f;
            if (!notice)
                icon(g, icon_of(row.id), kSheetRowX + 70.0f, cy, 26.0f, ink);
            else
                x = kSheetRowX + 56.0f;
            const float width = kSheetRowX + kSheetRowW - (opens ? 62.0f : 28.0f) - x;
            text(g, g.f.semibold, fit(g.f.semibold, row.title, 25.0f, width), x, cy + 9.0f, 25.0f, ink);
            if (opens)
                icon(g, Icon::chevron_right, kSheetRowX + kSheetRowW - 34.0f, cy, 26.0f, ink);
        }
        g.l.pop_clip();
    }
    g.l.pop_transform();
    g.l.pop_opacity();
}

FocusInfo Sheet::focus() const
{
    FocusInfo info;
    info.id = "sheet/0";
    info.text = levels_.empty() ? std::string() : levels_.back().title;
    info.rect = row_rect(0);
    if (!levels_.empty() && !levels_.back().rows.empty())
    {
        const Level &level = levels_.back();
        info.id = "sheet/" + std::to_string(level.focus);
        info.text = level.rows[static_cast<std::size_t>(level.focus)].title;
        info.rect = row_rect(level.focus);
        info.index = level.focus;
        info.total = static_cast<int>(level.rows.size());
    }
    return info;
}

std::span<const ui::Hint> Sheet::hints() const
{
    static ui::Hint items[2];
    items[0] = {ui::Button::cross, tr("Choose")};
    items[1] = {ui::Button::circle, levels_.size() > 1 ? tr("Back") : tr("Close")};
    return items;
}

void Sheet::snapshot(Snapshot &out) const
{
    out.set("sheet", open_ ? 1 : 0);
    if (!open_ || levels_.empty())
        return;
    out.set("sheet_load", load_name(load_));
    out.set("sheet_level", static_cast<int>(levels_.size()));
    out.set("sheet_acting", acting_ >= 0 ? 1 : 0);
    std::vector<std::string> titles;
    for (const model::Row &row : levels_.back().rows)
        titles.push_back(row.title);
    out.list("sheet", std::move(titles));
}

} // namespace afterglow::family
