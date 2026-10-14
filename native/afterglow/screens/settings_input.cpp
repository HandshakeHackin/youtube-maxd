// ProsperoTube Afterglow - Settings: what the controller does.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The tiles, a page of rows, a row held in the move mode, and the text sheet.

#include "afterglow/screens/settings_screen.hpp"

namespace afterglow::settings
{

void Settings::input_home(const InputFrame &input, UpdateContext &context)
{
    const int count = static_cast<int>(shown_.size());
    const Source &main = sources_.front();
    int next = focus_;
    bool edge = false;
    switch (input.nav)
    {
    case Direction::left:
        if (focus_ % kTileColumns > 0)
            next = focus_ - 1;
        else
            context.nav.open_rail();
        break;
    case Direction::right:
        if (focus_ % kTileColumns < kTileColumns - 1 && focus_ + 1 < count)
            next = focus_ + 1;
        else
            edge = true;
        break;
    case Direction::up:
        if (focus_ >= kTileColumns)
            next = focus_ - kTileColumns;
        else
            edge = true;
        break;
    case Direction::down:
        if (focus_ / kTileColumns < (count - 1) / kTileColumns)
            next = std::min(focus_ + kTileColumns, count - 1); // a last row that is not full takes its last tile
        else
            edge = true;
        break;
    case Direction::none:
        break;
    }
    if (next != focus_)
    {
        focus_ = next;
        context.feedback.play(audio::Cue::focus, 1.08f - 0.04f * static_cast<float>(next / kTileColumns), ui::pan_for_x(tile_rect(next).cx()));
    }
    else if (edge)
    {
        bump(input, context);
    }
    if (count == 0)
        return;
    const int index = shown_[static_cast<std::size_t>(focus_)];
    const Tile &tile = tiles()[static_cast<std::size_t>(index)];
    if (pressed(input, Action::confirm))
    {
        // A tile that is only the door to another package's screen opens that; every other one its page.
        if (tile.link != ScreenId::settings && !tile_has_page(tile, main.rows))
            context.nav.push(Route{tile.link, {}, tr(tile.name)}, Move::push);
        else
            context.nav.push(Route{ScreenId::settings, std::string(kTilePrefix) + tile.key, {}}, Move::push);
    }
    else if (pressed(input, Action::back))
    {
        context.nav.back();
    }
    else if (pressed(input, Action::menu))
    {
        // Options: this part of the settings back to its defaults.
        if (const model::Row *reset = tile_reset(tile, main.rows))
            ask_reset(item_of(*reset, kMain), tr(tile.name), context);
        else if (!input.nav_repeat)
            context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
    }
    else if (pressed(input, Action::north) && main.load != model::Load::ready && main.load != model::Load::loading)
    {
        again(context);
    }
}

void Settings::ask_reset(const Item &reset, std::string_view name, UpdateContext &context)
{
    Change change;
    change.catalog = reset.catalog;
    change.action = reset.row.id;
    change.done = filled(tr("{0} is back at its defaults"), name);
    const std::string &question = reset.row.fact("confirm");
    ask_first(std::move(change), filled(tr("Reset {0}?"), name),
              question.empty() ? std::string(tr("Every setting of this part returns to its default for the profile in use.")) : question, tr("Reset"), context);
}

// Reads the page again after it could not be read.
void Settings::again(UpdateContext &context)
{
    for (Source &source : sources_)
    {
        context.services.data.forget(source.catalog);
        if (source.load != model::Load::ready)
        {
            source.load = model::Load::idle;
            source.mark = 0;
        }
    }
    context.feedback.play(audio::Cue::select);
}

// The end of a row or a list: one soft answer, none while the direction is held.
void Settings::bump(const InputFrame &input, UpdateContext &context)
{
    if (input.nav_repeat)
        return;
    context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
    context.feedback.rumble(0.25f, 0.05f);
    edge_.trigger(1.0f);
}

void Settings::input_page(const InputFrame &input, UpdateContext &context)
{
    if (leave_ > 0.0f)
    {
        // The choice was confirmed: the list closes once the mark has been seen on it.
        leave_ -= context.dt;
        if (leave_ <= 0.0f || pressed(input, Action::back))
        {
            leave_ = 0.0f;
            step_back(context);
        }
        return;
    }
    const model::Load state = page_load();
    const Item *item = focused();
    if (state != model::Load::ready || item == nullptr)
    {
        // Nothing to move through: the one button, the rail and the way back.
        if (pressed(input, Action::back))
            context.nav.back();
        else if (input.nav == Direction::left)
            context.nav.open_rail();
        else if (pressed(input, Action::confirm) && state != model::Load::loading)
        {
            if (state == model::Load::ready)
                context.nav.back();
            else
                again(context);
        }
        else if (input.nav != Direction::none)
            bump(input, context);
        return;
    }
    read_choices(context.services.data);
    if (steps_ != 0 && choices_[item->row.id].ready)
    {
        // Presses made before the choices were here.
        const int owed = steps_;
        steps_ = 0;
        step(*item, owed, input, context);
    }
    // More rows of a long list arrive as the focus nears its end.
    Source &last = sources_.back();
    if (last.more && last.pages < kPagesAtMost && focus_ + 6 >= static_cast<int>(lines_.size()))
    {
        const Page &next = context.services.data.catalog(last.catalog, last.pages);
        if (next.load != model::Load::loading && next.load != model::Load::idle)
            ++last.pages;
    }

    if (grabbed_)
    {
        input_move(*item, input, context);
        return;
    }
    if (input.nav == Direction::up || input.nav == Direction::down)
    {
        go(input.nav == Direction::down ? 1 : -1, 1, input, context);
    }
    else if (pressed(input, Action::jump_next) || pressed(input, Action::jump_prev))
    {
        go(pressed(input, Action::jump_next) ? 1 : -1, 8, input, context);
    }
    else if (input.nav == Direction::left || input.nav == Direction::right)
    {
        const int direction = input.nav == Direction::right ? 1 : -1;
        switch (item->kind)
        {
        case Kind::toggle:
            if (shown(*item).on != (direction > 0))
                flip(*item, context);
            else
                bump(input, context);
            break;
        case Kind::list:
        case Kind::slider:
            step(*item, direction, input, context);
            break;
        case Kind::cycle:
            if (direction > 0)
                activate(*item, input, context);
            else
                bump(input, context);
            break;
        default:
            if (direction < 0)
                context.nav.open_rail();
            else
                bump(input, context);
            break;
        }
    }
    else if (pressed(input, Action::confirm))
    {
        activate(*item, input, context);
    }
    else if (pressed(input, Action::back))
    {
        step_back(context);
    }
    else if (pressed(input, Action::west) && item->kind == Kind::order)
    {
        show_or_hide(*item, input, context);
    }
    else if (pressed(input, Action::north))
    {
        if (item->kind == Kind::order)
            context.nav.push(Route{ScreenId::settings, item->row.id, below(item->label)}, Move::push);
        else if (has_reset_)
            ask_reset(reset_, path_.empty() ? std::string_view() : std::string_view(path_.back()), context);
        else if (!input.nav_repeat)
            context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
    }
}

// Back to the page before. Leaving a page on which something was saved says so once.
void Settings::step_back(UpdateContext &context)
{
    if (applied_ > 0)
        context.feedback.play(audio::Cue::saved, 1.0f, 0.0f, 0.7f);
    context.nav.back();
}

// Moves the focus by rows; a held direction moves one row at a time and stops quietly at the end.
void Settings::go(int direction, int rows, const InputFrame &input, UpdateContext &context)
{
    int at = focus_;
    int reached = focus_;
    for (int left = rows; left > 0;)
    {
        at += direction;
        if (at < 0 || at >= static_cast<int>(lines_.size()))
            break;
        if (lines_[static_cast<std::size_t>(at)].item < 0)
            continue;
        reached = at;
        --left;
    }
    if (reached == focus_)
    {
        bump(input, context);
        return;
    }
    focus_ = reached;
    steps_ = 0;
    said_row_.clear();
    const Item &item = items_[static_cast<std::size_t>(lines_[static_cast<std::size_t>(focus_)].item)];
    focus_id_ = key_of(item);
    reveal();
    const float through = content_ > 0.0f ? lines_[static_cast<std::size_t>(focus_)].y / content_ : 0.0f;
    context.feedback.play(audio::Cue::focus, 1.08f - 0.14f * through, ui::pan_for_x(kFormX + kFormW * 0.5f));
}

void Settings::reveal()
{
    if (focus_ < 0 || focus_ >= static_cast<int>(lines_.size()))
        return;
    const Line &line = lines_[static_cast<std::size_t>(focus_)];
    scroll_.reveal(line.y, line.y + line.h, kView, kRowH + kRowGap + 8.0f, content_);
}

std::string Settings::below(std::string_view name) const
{
    std::string title;
    for (const std::string &part : path_)
    {
        title += part;
        title += kPathSeparator;
    }
    title += name;
    return title;
}

// A row that cannot be changed now: its help beside the form says why.
void Settings::refuse(const Item &item, const InputFrame &input, UpdateContext &context)
{
    (void)item;
    bump(input, context);
}

void Settings::flip(const Item &item, UpdateContext &context)
{
    Change change;
    change.item = item.row.id;
    change.catalog = item.catalog;
    change.action = item.row.id;
    change.on = !shown(item).on;
    change.sets_on = true;
    if (ask(std::move(change), context))
        context.feedback.play(audio::Cue::toggle, shown(item).on ? 1.08f : 0.94f, ui::pan_for_x(kFormX + kFormW));
}

// Left or Right on a row that has choices: the one before or after the one it shows.
void Settings::step(const Item &item, int direction, const InputFrame &input, UpdateContext &context)
{
    if (item.disabled)
    {
        refuse(item, input, context);
        return;
    }
    const Choices &choices = choices_[item.row.id];
    if (!choices.ready)
    {
        steps_ = std::clamp(steps_ + direction, -24, 24); // taken when its choices are here
        return;
    }
    const int count = static_cast<int>(choices.rows.size());
    if (count < 2)
    {
        bump(input, context);
        return;
    }
    int now = shown(item).index;
    if (now < 0 || now >= count)
    {
        now = 0;
        for (int i = 0; i < count; ++i)
        {
            if (choices.rows[static_cast<std::size_t>(i)].on)
                now = i;
        }
    }
    int next = now + direction;
    if (item.kind == Kind::slider)
        next = std::clamp(next, 0, count - 1); // a quantity has two ends
    else
        next = ((next % count) + count) % count; // a list goes round
    Motion &motion = motion_[key_of(item)];
    if (next == now)
    {
        bump(input, context);
        return;
    }
    const Item &choice = choices.rows[static_cast<std::size_t>(next)];
    Change change;
    change.item = item.row.id;
    change.catalog = item.catalog;
    change.action = choice.row.id;
    change.value = choice.label;
    change.sets_value = true;
    change.index = next;
    change.choice = choice.row;
    change.has_choice = true;
    change.absolute = true;
    if (!ask(std::move(change), context))
        return;
    motion.nudge.trigger(1.0f);
    motion.direction = direction > 0 ? 1.0f : -1.0f;
    if (item.kind == Kind::slider)
        context.feedback.play(audio::Cue::slider, 0.9f + 0.3f * static_cast<float>(next) / static_cast<float>(count - 1), ui::pan_for_x(kFormX + kFormW));
    else
        context.feedback.play(audio::Cue::toggle, direction > 0 ? 1.04f : 0.96f, ui::pan_for_x(kFormX + kFormW));
}

void Settings::activate(const Item &item, const InputFrame &input, UpdateContext &context)
{
    if (item.disabled)
    {
        refuse(item, input, context);
        return;
    }
    switch (item.kind)
    {
    case Kind::toggle:
        flip(item, context);
        break;
    case Kind::list:
    case Kind::slider:
    case Kind::page:
        // A row's choices, or a further page: the list that has the row's id.
        context.nav.push(Route{ScreenId::settings, item.row.id, below(item.label)}, Move::push);
        break;
    case Kind::cycle:
    {
        Change change;
        change.item = item.row.id;
        change.catalog = item.catalog;
        change.action = item.row.id;
        if (ask(std::move(change), context))
        {
            Motion &motion = motion_[key_of(item)];
            motion.nudge.trigger(1.0f);
            motion.direction = 1.0f;
            context.feedback.play(audio::Cue::toggle, 1.04f, ui::pan_for_x(kFormX + kFormW));
        }
        break;
    }
    case Kind::choice:
    {
        if (shown(item).on)
        {
            step_back(context); // it already stands
            break;
        }
        Change change;
        change.item = item.row.id;
        change.catalog = item.catalog;
        change.action = item.row.id;
        change.on = true;
        change.sets_on = true;
        change.leave = true;
        if (ask(std::move(change), context))
            context.feedback.play(audio::Cue::select);
        break;
    }
    case Kind::action:
    {
        Change change;
        change.item = item.row.id;
        change.catalog = item.catalog;
        change.action = item.row.id;
        change.done = item.row.fact("done");
        const std::string &question = item.row.fact("confirm");
        if (!question.empty())
        {
            const std::string &yes = item.row.fact("confirm_label");
            ask_first(std::move(change), item.label, question, yes.empty() ? std::string(tr("Confirm")) : yes, context);
        }
        else if (ask(std::move(change), context))
        {
            context.feedback.play(audio::Cue::select);
        }
        break;
    }
    case Kind::text:
    {
        if (changes_.size() >= kWaiting)
        {
            context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
            break;
        }
        typing_ = true;
        TextQuestion question;
        question.title = item.label;
        question.detail = item.detail;
        question.secret = item.row.has("secret");
        question.initial = question.secret ? std::string() : item.row.fact("text").empty() ? item.value : item.row.fact("text");
        question.may_be_empty = true;
        const std::weak_ptr<int> alive = alive_;
        context.nav.ask_text(std::move(question), [this, alive, item](bool taken, std::string text)
        {
            if (alive.expired())
            {
                std::fill(text.begin(), text.end(), '\0');
                return;
            }
            typing_ = false;
            if (taken)
            {
                Change change;
                change.item = item.row.id;
                change.catalog = item.catalog;
                change.action = item.row.id;
                change.typed = true;
                if (!item.row.has("secret"))
                {
                    change.value = text;
                    change.sets_value = true;
                }
                change.text = std::move(text);
                changes_.push_back(std::move(change));
            }
            std::fill(text.begin(), text.end(), '\0');
        });
        break;
    }
    case Kind::link:
    {
        ScreenId target = ScreenId::reader;
        const std::string &argument = item.row.fact("argument");
        if (!screen_from_name(item.row.fact("screen"), &target))
            target = ScreenId::reader; // a text to read: credits, changes, release notes, an address
        if (target == ScreenId::settings)
            context.nav.push(Route{target, argument.empty() ? item.row.id : argument, below(item.label)}, Move::push);
        else
            context.nav.push(Route{target, !argument.empty() ? argument : target == ScreenId::reader ? item.row.id : std::string(), item.label}, Move::push);
        break;
    }
    case Kind::order:
        grabbed_ = true;
        context.feedback.play(audio::Cue::pickup);
        break;
    case Kind::info:
    case Kind::reset:
        if (!input.nav_repeat)
            context.feedback.play(audio::Cue::tick, 1.0f, 0.0f, 0.6f);
        break;
    }
}

void Settings::input_move(const Item &item, const InputFrame &input, UpdateContext &context)
{
    if (item.kind != Kind::order)
    {
        grabbed_ = false;
        return;
    }
    if (input.nav == Direction::up || input.nav == Direction::down)
    {
        move(item, input.nav == Direction::down ? 1 : -1, input, context);
    }
    else if (pressed(input, Action::confirm) || pressed(input, Action::back))
    {
        grabbed_ = false;
        context.feedback.play(audio::Cue::drop);
    }
    else if (pressed(input, Action::west))
    {
        show_or_hide(item, input, context);
    }
    else if (input.nav != Direction::none)
    {
        bump(input, context);
    }
}

bool Settings::can_move(const Item &item, int direction)
{
    return !item.row.fact(direction < 0 ? "up" : "down").empty() && !item.row.has(direction < 0 ? "first" : "last");
}

void Settings::move(const Item &item, int direction, const InputFrame &input, UpdateContext &context)
{
    const int at = lines_[static_cast<std::size_t>(focus_)].item;
    const int other = at + direction;
    Source *source = source_of(item.catalog);
    if (!can_move(item, direction) || source == nullptr || other < 0 || other >= static_cast<int>(items_.size()) ||
        items_[static_cast<std::size_t>(other)].kind != Kind::order || items_[static_cast<std::size_t>(other)].section != item.section)
    {
        bump(input, context);
        return;
    }
    const std::string mine = item.row.id;
    const std::string theirs = items_[static_cast<std::size_t>(other)].row.id;
    const auto a = std::find_if(source->rows.begin(), source->rows.end(), [&](const model::Row &row) { return row.id == mine; });
    const auto b = std::find_if(source->rows.begin(), source->rows.end(), [&](const model::Row &row) { return row.id == theirs; });
    if (a == source->rows.end() || b == source->rows.end())
        return;
    Change change;
    change.item = mine;
    change.catalog = item.catalog;
    change.action = item.row.fact(direction < 0 ? "up" : "down");
    change.moved = true;
    change.before = source->rows;
    if (!ask(std::move(change), context))
        return;
    // The row is where it was put at once; which ends it now stands at follows from the row it passed.
    const bool was_end = !can_move(items_[static_cast<std::size_t>(other)], direction);
    set_fact(*a, direction < 0 ? "first" : "last", was_end ? "1" : "0");
    set_fact(*a, direction < 0 ? "last" : "first", "0");
    set_fact(*b, direction < 0 ? "first" : "last", "0");
    set_fact(*b, direction < 0 ? "last" : "first", can_move(item, -direction) ? "0" : "1");
    std::iter_swap(a, b);
    rebuild();
    reveal();
    context.feedback.play(audio::Cue::slide, direction > 0 ? 0.96f : 1.04f, ui::pan_for_x(kFormX + kFormW * 0.5f));
}

void Settings::show_or_hide(const Item &item, const InputFrame &input, UpdateContext &context)
{
    const std::string &action = item.row.fact("toggle");
    if (action.empty())
    {
        bump(input, context);
        return;
    }
    Change change;
    change.item = item.row.id;
    change.catalog = item.catalog;
    change.action = action;
    change.on = !shown(item).on;
    change.sets_on = true;
    if (ask(std::move(change), context))
        context.feedback.play(audio::Cue::toggle, shown(item).on ? 1.08f : 0.94f, ui::pan_for_x(kFormX + kFormW));
}

} // namespace afterglow::settings
