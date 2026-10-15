// ProsperoTube Afterglow - the frame's sheets: a row's actions, and a text that is typed.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/sheet.hpp"

#include <algorithm>
#include <cmath>

namespace afterglow
{

namespace
{

// The sheet of a row's actions (board 14).
constexpr float kSheetX = 1296.0f;
constexpr float kSheetRowX = 1336.0f;
constexpr float kSheetRowW = 536.0f;
constexpr float kSheetRowH = 68.0f;
constexpr float kSheetPitch = 72.0f;
constexpr float kSheetTop = 196.0f;
constexpr int kSheetShown = 10;
constexpr float kSheetPan = 0.45f;
constexpr float kSheetLine = 33.0f;

// The sheet a text is typed on.
constexpr Rect kTextSheet{510.0f, 150.0f, 900.0f, 720.0f};
constexpr Rect kTextKeys{554.0f, 372.0f, 812.0f, 460.0f};

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

void drop_last_character(std::string &text)
{
    while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0u) == 0x80u)
        text.pop_back();
    if (!text.empty())
        text.pop_back();
}

std::size_t characters(std::string_view text)
{
    std::size_t count = 0;
    for (const char c : text)
        count += (static_cast<unsigned char>(c) & 0xC0u) != 0x80u ? 1u : 0u;
    return count;
}

} // namespace

// ---- what every screen does the same way ------------------------------------------

void refuse(UpdateContext &context, const InputFrame &input, ui::Pulse &nudge, float x)
{
    if (input.nav_repeat)
        return;
    context.feedback.play(audio::Cue::error, 1.0f, ui::pan_for_x(x), 0.6f);
    context.feedback.rumble(0.25f, 0.05f);
    if (!context.reduced_motion)
        nudge.trigger();
}

bool play_video(UpdateContext &context, const model::Video &video, const Rect &carried, float x)
{
    const bool playing = carried.w > 0.0f ? context.nav.play(video, carried, video.thumb) : context.services.data.play(video);
    if (playing)
    {
        context.feedback.play(audio::Cue::launch, 1.0f, ui::pan_for_x(x));
        context.feedback.rumble(0.6f, 0.16f);
        return true;
    }
    context.feedback.play(audio::Cue::error, 1.0f, ui::pan_for_x(x));
    context.nav.toast(tr("This video cannot be played right now."), ToastKind::error);
    return false;
}

bool route_of_row(const model::Row &row, Route *route)
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

Icon icon_of_row(std::string_view id)
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
    if (has(":like:") || has("favorite"))
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
    if (has(":reply:"))
        return Icon::reply;
    return Icon::more;
}

// ---- the actions of a row -------------------------------------------------------------

ActionsSheet::ActionsSheet() : alive_(std::make_shared<int>(0)) {}

bool ActionsSheet::show(const std::string &row, std::string_view about, UpdateContext &context)
{
    const std::string menu = context.services.data.menu(row, std::string(about));
    if (menu.empty())
    {
        context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        context.feedback.rumble(0.25f, 0.05f);
        return false;
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
    changed_ = false;
    open_ = true;
    pos_.snap(0.0f);
    scroll_.snap(0.0f);
    context.feedback.play(audio::Cue::modal_open, 1.0f, kSheetPan);
    return true;
}

void ActionsSheet::dismiss()
{
    open_ = false;
    acting_ = -1;
    amount_.snap(0.0f);
}

void ActionsSheet::close(UpdateContext &context, bool sound)
{
    open_ = false;
    acting_ = -1;
    if (sound)
        context.feedback.play(audio::Cue::modal_close, 1.0f, kSheetPan);
}

void ActionsSheet::begin(const model::Row &row, UpdateContext &context)
{
    acting_ = levels_.back().focus;
    acting_title_ = row.title;
    act_.start(context.services.data, row.id);
    context.feedback.play(audio::Cue::select, 1.0f, kSheetPan);
}

bool ActionsSheet::take_changed()
{
    const bool changed = changed_;
    changed_ = false;
    return changed;
}

Rect ActionsSheet::row_rect(int index) const
{
    return {kSheetRowX, kSheetTop + kSheetPitch * static_cast<float>(index) - scroll_.target, kSheetRowW, kSheetRowH};
}

bool ActionsSheet::handle(const InputFrame &input, UpdateContext &context, Route *route)
{
    if (!open_)
        return false;
    if (pressed(input, Action::back))
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
    else if (pressed(input, Action::confirm))
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
        else if (route_of_row(row, route))
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

void ActionsSheet::update(UpdateContext &context)
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
                context.nav.toast(answer->rows.empty() ? acting_title_ : answer->rows.front().title, about_, ToastKind::success);
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

void ActionsSheet::draw(G &g) const
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
    glass_panel(g, panel, kRadiusSheet, 1.25f);
    text(g, g.f.display, fit(g.f.display, level.title, kTypePanel, 500.0f), kSheetX + 66.0f, 120.0f, kTypePanel, g.k.ink);
    text(g, g.f.regular, fit(g.f.regular, about_, kTypeNote, 480.0f), kSheetX + 68.0f, 156.0f, kTypeNote, g.k.ink2);

    const int count = static_cast<int>(level.rows.size());
    if (count == 0)
    {
        if (load_ == model::Load::failed || load_ == model::Load::offline)
        {
            icon(g, trouble_icon(load_), kSheetX + 88.0f, 236.0f, 40.0f, g.k.ink2);
            paragraph(g, g.f.regular, error_.empty() ? tr("The actions did not arrive. Choose again to ask once more.") : error_.c_str(), kSheetX + 126.0f,
                      244.0f, kTypeBody, 440.0f, kSheetLine, g.k.ink2, 4);
        }
        else
        {
            for (int i = 0; i < 6; ++i)
                bone(g, {kSheetX + 70.0f, kSheetTop + 22.0f + kSheetPitch * static_cast<float>(i), i % 2 == 0 ? 380.0f : 300.0f, 24.0f}, 12.0f, i);
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
            const bool opens = !notice && (route_of_row(row, &unused) || starts_with(row.id, "notify:") || starts_with(row.id, "playlist:pick:") ||
                                           starts_with(row.id, "link:") || starts_with(row.id, "reminder:"));
            Color ink = gfx::mix(notice ? g.k.ink3 : g.k.ink, g.k.on_plate, focus);
            if (i == acting_)
                ink = ink.with_alpha(g.reduced ? 0.6f : 0.45f + 0.4f * ui::breathe(g.time, 0.9f)); // asked, not yet answered
            float x = kSheetRowX + 96.0f;
            if (!notice)
                icon(g, icon_of_row(row.id), kSheetRowX + 70.0f, cy, 26.0f, ink);
            else
                x = kSheetRowX + 56.0f;
            const float width = kSheetRowX + kSheetRowW - (opens ? 62.0f : 28.0f) - x;
            text(g, g.f.semibold, fit(g.f.semibold, row.title, kTypeRow, width), x, cy + 9.0f, kTypeRow, ink);
            if (opens)
                icon(g, Icon::chevron_right, kSheetRowX + kSheetRowW - 34.0f, cy, 26.0f, ink);
        }
        g.l.pop_clip();
    }
    // The sheet's own hints stand at its foot: the page's are under the scrim.
    const std::span<const ui::Hint> items = hints();
    afterglow::hints(g, items.data(), static_cast<int>(items.size()));
    g.l.pop_transform();
    g.l.pop_opacity();
}

FocusInfo ActionsSheet::focus() const
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

std::span<const ui::Hint> ActionsSheet::hints() const
{
    static ui::Hint items[2];
    items[0] = {ui::Button::cross, tr("Choose")};
    items[1] = {ui::Button::circle, levels_.size() > 1 ? tr("Back") : tr("Close")};
    return items;
}

void ActionsSheet::snapshot(Snapshot &out) const
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

// ---- a text that is typed -------------------------------------------------------------

void TextSheet::show(TextQuestion question, std::function<void(bool, std::string)> answered, UpdateContext &context)
{
    question_ = std::move(question);
    answered_ = std::move(answered);
    text_ = question_.secret ? std::string() : question_.initial;
    keys_ = ui::Keyboard{};
    keys_.style.bindings = ui::KeyboardBindings::standard();
    keys_.style.panel = false;
    if (context.look != nullptr)
        wear(keys_.style, *context.look, context.reduced_motion);
    worn_ = context.look;
    keys_.set_bounds(kTextKeys);
    keys_.set_length(-1);
    keys_.enter();
    open_ = true;
    context.feedback.play(audio::Cue::modal_open);
}

void TextSheet::dismiss()
{
    open_ = false;
    amount_.snap(0.0f);
    answered_ = nullptr;
    text_.clear();
}

void TextSheet::finish(bool taken, UpdateContext &context)
{
    open_ = false;
    context.feedback.play(taken ? audio::Cue::select : audio::Cue::modal_close);
    auto answered = std::move(answered_);
    answered_ = nullptr;
    std::string text = std::move(text_);
    text_.clear(); // a password is not kept here either
    if (answered)
        answered(taken, taken ? std::move(text) : std::string());
}

void TextSheet::handle(const InputFrame &input, UpdateContext &context)
{
    if (!open_)
        return;
    for (const char ch : context.typed)
    {
        if (ch == '\x1b') { finish(false, context); return; }
        if (ch == '\r')
        {
            if (!text_.empty() || question_.may_be_empty) finish(true, context);
            else refuse(context, input, nudge_);
            return;
        }
        if (ch == '\b') drop_last_character(text_);
        else text_.push_back(ch);
    }
    if (pressed(input, Action::back))
    {
        finish(false, context);
        return;
    }
    ui::Event event = ui::Event::none;
    if (pressed(input, Action::menu))
        event = ui::Event::activated; // Options takes the text from anywhere on the keyboard
    else
        event = keys_.handle(input, context.feedback);
    for (int i = 0; i < keys_.erased(); ++i)
        drop_last_character(text_);
    text_ += keys_.typed();
    if (event != ui::Event::activated)
        return;
    if (text_.empty() && !question_.may_be_empty)
    {
        refuse(context, input, nudge_);
        return;
    }
    finish(true, context);
}

void TextSheet::update(UpdateContext &context)
{
    amount_.target = open_ ? 1.0f : 0.0f;
    amount_.update(context.dt, context.reduced_motion ? 60.0f : 13.0f);
    nudge_.update(context.dt, 10.0f);
    if (amount_.value <= 0.01f)
        return;
    if (context.look != nullptr && (worn_ != context.look || keys_.style.reduced_motion != context.reduced_motion))
    {
        wear(keys_.style, *context.look, context.reduced_motion);
        worn_ = context.look;
    }
    keys_.update(context.dt);
}

void TextSheet::draw(G &g) const
{
    const float amount = tween::clamp01(amount_.value);
    if (amount <= 0.01f)
        return;
    const Look &k = g.k;
    g.l.push_opacity(amount);
    g.l.rounded_rect({0.0f, 0.0f, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0.0f, kBlack.with_alpha(k.dark ? 0.62f : 0.3f));
    g.l.push_transform(1.0f, 0.0f, 0.0f, 0.0f, g.reduced ? 0.0f : 40.0f * (1.0f - amount));
    const Rect &sheet = kTextSheet;
    lift(g, sheet, 36.0f, 1.0f);
    glass_panel(g, sheet, 36.0f, 1.5f);
    text(g, g.f.display, fit(g.f.display, question_.title, kTypePanel, sheet.w - 88.0f), sheet.x + 44.0f, sheet.y + 66.0f, kTypePanel, k.ink);
    text(g, g.f.regular, fit(g.f.regular, question_.detail, kTypeMeta, sheet.w - 88.0f), sheet.x + 44.0f, sheet.y + 100.0f, kTypeMeta, k.ink2);
    Rect field{sheet.x + 44.0f, sheet.y + 124.0f, sheet.w - 88.0f, 64.0f};
    field.x += ui::shake(nudge_.value, g.time, 10.0f, 8.0f);
    g.l.rounded_rect(field, 16.0f, k.raised);
    g.l.bordered_rect(field, 16.0f, clear(), 2.0f, k.focus.with_alpha(0.6f));
    std::string typed = text_;
    if (question_.secret)
    {
        typed.clear();
        for (std::size_t i = characters(text_); i > 0; --i)
            typed += "\xE2\x80\xA2";
    }
    // The end of a long text stays in view: what does not fit leaves on the left.
    g.l.push_clip(field.inset(14.0f));
    const float width = measure(g.f.mono, typed, kTypeBody);
    const float room = field.w - 44.0f;
    const float x = field.x + 20.0f - std::max(0.0f, width - room);
    text(g, g.f.mono, typed, x, field.cy() + 8.5f, kTypeBody, k.ink);
    if (g.reduced || std::fmod(g.time, 1.0f) < 0.55f)
        g.l.rounded_rect({x + width + 3.0f, field.cy() - 16.0f, 2.5f, 32.0f}, 1.0f, k.accent);
    g.l.pop_clip();
    ui::Canvas canvas{g.l, g.f, g.glass, g.time};
    keys_.draw(canvas);
    g.l.pop_transform();
    const std::span<const ui::Hint> items = hints();
    afterglow::hints(g, items.data(), static_cast<int>(items.size()));
    g.l.pop_opacity();
}

FocusInfo TextSheet::focus() const
{
    FocusInfo info;
    info.id = "sheet/keys";
    info.text = question_.title;
    info.rect = keys_.key_rect(keys_.focus_row(), keys_.focus_key());
    return info;
}

std::span<const ui::Hint> TextSheet::hints() const
{
    static ui::Hint items[4];
    items[0] = {ui::Button::cross, tr("Type")};
    items[1] = {ui::Button::square, tr("Delete")};
    items[2] = {ui::Button::options, question_.done.empty() ? tr("Save") : question_.done.c_str()};
    items[3] = {ui::Button::circle, tr("Cancel")};
    return items;
}

void TextSheet::snapshot(Snapshot &out) const
{
    out.set("typing", open_ ? 1 : 0);
    if (!open_)
        return;
    // A password is never part of a report: only how much of it there is.
    out.set("typed", question_.secret ? std::string(characters(text_), '*') : text_);
}

} // namespace afterglow
