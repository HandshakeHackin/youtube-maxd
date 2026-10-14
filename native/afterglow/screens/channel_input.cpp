// ProsperoTube Afterglow - the channel page: what the controller does.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Three zones stand one above the other: the buttons, the tabs (with the order
// of the section at their right end) and the section's content. L1 and R1 turn
// the sections from anywhere, Square searches in the channel, Touchpad opens
// the actions of the video in focus, or of the channel.

#include "afterglow/screens/channel.hpp"

namespace afterglow::channel_page
{

void Channel::search(const InputFrame &input, UpdateContext &context)
{
    if (!head_.can_search)
    {
        refuse(context, input, nudge_);
        return;
    }
    // The search page takes the scope as its argument ("channel-search:<id>", the provider's catalog of a
    // search in a channel); what it finds is the grid of that search.
    context.nav.push(Route{ScreenId::search, "channel-search:" + id_, fill(tr("Search in {0}"), head_.name)}, Move::push);
}

void Channel::retry(UpdateContext &context)
{
    context.feedback.play(audio::Cue::select);
    context.services.data.refresh();
    age_ = 0.0f;
}

void Channel::go(Zone zone, UpdateContext &context)
{
    if (zone == zone_)
        return;
    zone_ = zone;
    const float row = zone == Zone::actions ? 0.0f : zone == Zone::content ? 2.0f : 1.0f;
    context.feedback.play(audio::Cue::tab, 1.10f - 0.07f * row);
}

void Channel::turn(int direction, const InputFrame &input, UpdateContext &context)
{
    if (tabs_.size() < 2)
    {
        refuse(context, input, nudge_);
        return;
    }
    if (bar_.step(direction, input, context.feedback) != ui::Event::changed)
        return;
    active_ = bar_.active();
    if (zone_ == Zone::sort || (zone_ == Zone::content && tab().feed.open && !can_focus(tab())))
        zone_ = Zone::tabs;
}

void Channel::handle(const InputFrame &input, UpdateContext &context)
{
    if (sort_open_)
    {
        handle_sort_menu(input, context);
        return;
    }
    if (pressed(input, Action::back))
    {
        context.nav.back();
        return;
    }
    if (!head_.known)
    {
        if (zone_ == Zone::trouble && pressed(input, Action::confirm))
            retry(context);
        else if (input.nav == Direction::left)
            context.nav.open_rail();
        else if (input.nav != Direction::none && zone_ == Zone::trouble)
            refuse(context, input, nudge_);
        return;
    }
    if (pressed(input, Action::page_next) || pressed(input, Action::page_prev))
    {
        turn(pressed(input, Action::page_next) ? 1 : -1, input, context);
        return;
    }
    if (pressed(input, Action::west))
    {
        search(input, context);
        return;
    }
    if (!has_tab())
        return;
    // A zone that has gone (the order of a section that offers none, an empty section) gives the focus to the tabs.
    if ((zone_ == Zone::content && !can_focus(tab())) || (zone_ == Zone::sort && tab().sorts.size() < 2) ||
        (zone_ == Zone::actions && buttons_.empty()) || zone_ == Zone::trouble)
        zone_ = Zone::tabs;
    const Cell *cell = zone_ == Zone::content ? focused_cell() : nullptr;
    if (pressed(input, Action::touch))
    {
        // The actions of the video in focus; anywhere else, those of the channel itself.
        if (cell != nullptr && cell->video)
            context.nav.actions(cell->v.id, cell->v.title, [data = &context.services.data, catalog = base_.catalog] { data->forget(catalog); });
        else if (cell == nullptr)
            context.nav.actions("channel:" + id_, head_.name, [data = &context.services.data, catalog = base_.catalog] { data->forget(catalog); });
        return;
    }
    switch (zone_)
    {
    case Zone::actions:
        handle_actions(input, context);
        break;
    case Zone::tabs:
        handle_tabs(input, context);
        break;
    case Zone::sort:
        handle_sort(input, context);
        break;
    case Zone::content:
        if (tab().cells.empty())
            handle_retry(input, context);
        else if (tab().home())
            handle_home(tab(), input, context);
        else
            handle_grid(tab(), input, context);
        break;
    case Zone::trouble:
        break;
    }
}

void Channel::handle_actions(const InputFrame &input, UpdateContext &context)
{
    const int count = static_cast<int>(buttons_.size());
    button_ = std::clamp(button_, 0, count - 1);
    const Slot slot = buttons_[static_cast<std::size_t>(button_)];
    if (input.nav == Direction::left && button_ == 0)
        context.nav.open_rail();
    else if (input.nav == Direction::left || input.nav == Direction::right)
    {
        const int next = button_ + (input.nav == Direction::right ? 1 : -1);
        if (next < count)
        {
            button_ = next;
            context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(buttons_[static_cast<std::size_t>(next)].rect.cx()));
        }
        else
        {
            refuse(context, input, nudge_, slot.rect.cx());
        }
    }
    else if (input.nav == Direction::down)
        go(Zone::tabs, context);
    else if (input.nav == Direction::up)
        refuse(context, input, nudge_, slot.rect.cx());
    else if (pressed(input, Action::confirm))
        press_button(slot, input, context);
}

void Channel::handle_tabs(const InputFrame &input, UpdateContext &context)
{
    const int count = static_cast<int>(tabs_.size());
    if (input.nav == Direction::left && active_ == 0)
        context.nav.open_rail();
    else if (input.nav == Direction::right && active_ == count - 1 && tab().sorts.size() > 1)
        go(Zone::sort, context);
    else if (input.nav == Direction::left || input.nav == Direction::right)
    {
        if (bar_.handle(input, context.feedback) == ui::Event::changed)
            active_ = bar_.active();
    }
    else if (input.nav == Direction::up && !buttons_.empty())
        go(Zone::actions, context);
    else if ((input.nav == Direction::down || pressed(input, Action::confirm)) && can_focus(tab()))
        enter_content(tab(), tab().home() ? 0 : tab().focus % kColumns, context);
    else if (input.nav != Direction::none)
        refuse(context, input, nudge_, tab_rect_.cx());
}

void Channel::handle_sort(const InputFrame &input, UpdateContext &context)
{
    if (input.nav == Direction::left)
        go(Zone::tabs, context);
    else if (input.nav == Direction::up && !buttons_.empty())
        go(Zone::actions, context);
    else if (input.nav == Direction::down && can_focus(tab()))
        enter_content(tab(), kColumns - 1, context);
    else if (input.nav != Direction::none)
        refuse(context, input, nudge_, sort_chip_.cx());
    else if (pressed(input, Action::confirm))
    {
        sort_open_ = true;
        sort_pick_ = std::max(tab().sort, 0);
        sort_pos_.snap(static_cast<float>(sort_pick_));
        context.feedback.play(audio::Cue::modal_open, 1.0f, ui::pan_for_x(sort_chip_.cx()));
    }
}

// Down from the tabs: the first row of a grid, at the column the focus last had.
void Channel::enter_content(Tab &of, int column, UpdateContext &context)
{
    if (!of.cells.empty())
    {
        const int last = static_cast<int>(of.cells.size()) - 1;
        if (of.home())
            of.focus = column == 0 ? 0 : std::min(of.list_focus, last);
        else
            of.focus = std::min(column, last);
    }
    go(Zone::content, context);
}

void Channel::handle_retry(const InputFrame &input, UpdateContext &context)
{
    if (pressed(input, Action::confirm))
        retry(context);
    else if (input.nav == Direction::up)
        go(Zone::tabs, context);
    else if (input.nav == Direction::left)
        context.nav.open_rail();
    else if (input.nav != Direction::none)
        refuse(context, input, nudge_, retry_.cx());
}

void Channel::handle_home(Tab &of, const InputFrame &input, UpdateContext &context)
{
    const int count = static_cast<int>(of.cells.size());
    int next = of.focus;
    bool edge = false;
    switch (input.nav)
    {
    case Direction::left:
        if (of.focus == 0)
        {
            context.nav.open_rail();
            return;
        }
        of.list_focus = of.focus;
        next = 0;
        break;
    case Direction::right:
        if (of.focus == 0 && count > 1)
            next = std::clamp(of.list_focus, 1, count - 1);
        else
            edge = true;
        break;
    case Direction::up:
        if (of.focus <= 1)
        {
            go(Zone::tabs, context);
            return;
        }
        next = of.focus - 1;
        break;
    case Direction::down:
        if (of.focus >= 1 && of.focus + 1 < count)
            next = of.focus + 1;
        else
            edge = true;
        break;
    case Direction::none:
        break;
    }
    move_focus(of, next, edge, input, context);
    press_cell(of, input, context);
}

void Channel::handle_grid(Tab &of, const InputFrame &input, UpdateContext &context)
{
    const int count = static_cast<int>(of.cells.size());
    const int column = of.focus % kColumns;
    int next = of.focus;
    bool edge = false;
    switch (input.nav)
    {
    case Direction::left:
        if (column == 0)
        {
            context.nav.open_rail();
            return;
        }
        next = of.focus - 1;
        break;
    case Direction::right:
        if (column < kColumns - 1 && of.focus + 1 < count)
            next = of.focus + 1;
        else
            edge = true;
        break;
    case Direction::up:
        if (of.focus < kColumns)
        {
            // What is above the first row: the tabs, and over the last column the order of the section.
            go(column == kColumns - 1 && of.sorts.size() > 1 ? Zone::sort : Zone::tabs, context);
            return;
        }
        next = of.focus - kColumns;
        break;
    case Direction::down:
        if (of.focus + kColumns < count)
            next = of.focus + kColumns;
        else if (of.focus / kColumns < (count - 1) / kColumns)
            next = count - 1; // the last row is shorter: its last card
        else
            edge = true;
        break;
    case Direction::none:
        break;
    }
    move_focus(of, next, edge, input, context);
    // The next page is asked for two rows before the end, so it is there when the focus is.
    if ((count - 1) / kColumns - of.focus / kColumns <= 1)
        of.feed.want_more();
    press_cell(of, input, context);
}

void Channel::move_focus(Tab &of, int next, bool edge, const InputFrame &input, UpdateContext &context)
{
    if (edge)
    {
        refuse(context, input, nudge_, on_screen(cell_rect(of, of.focus, true)).cx());
    }
    else if (next != of.focus)
    {
        of.focus = next;
        if (of.home() && next >= 1)
            of.list_focus = next;
        context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(cell_rect(of, next, true).cx()));
    }
}

void Channel::press_cell(Tab &of, const InputFrame &input, UpdateContext &context)
{
    if (!pressed(input, Action::confirm))
        return;
    const Cell &cell = of.cells[static_cast<std::size_t>(of.focus)];
    if (cell.video)
        play_video(context, cell.v, cell_rect(of, of.focus, true), cell_rect(of, of.focus, true).cx());
    else
        context.nav.push(Route{ScreenId::grid, cell.id, cell.title}, Move::push);
}

void Channel::handle_sort_menu(const InputFrame &input, UpdateContext &context)
{
    Tab &of = tab();
    const int count = static_cast<int>(of.sorts.size());
    if (input.nav == Direction::up || input.nav == Direction::down)
    {
        const int next = sort_pick_ + (input.nav == Direction::down ? 1 : -1);
        if (next >= 0 && next < count)
        {
            sort_pick_ = next;
            context.feedback.play(audio::Cue::focus, 1.06f - 0.03f * static_cast<float>(next), ui::pan_for_x(sort_chip_.cx()));
        }
        else
        {
            refuse(context, input, nudge_, sort_chip_.cx());
        }
    }
    else if (pressed(input, Action::confirm))
    {
        sort_open_ = false;
        if (sort_pick_ != of.sort)
        {
            // The section is read again in the order asked for, from its beginning.
            of.sort = sort_pick_;
            of.chosen = true;
            of.focus = 0;
            of.cells.clear();
            of.feed.start(catalog_of(of));
        }
        context.feedback.play(audio::Cue::select, 1.0f, ui::pan_for_x(sort_chip_.cx()));
    }
    else if (pressed(input, Action::back) || input.nav == Direction::left)
    {
        sort_open_ = false;
        context.feedback.play(audio::Cue::modal_close, 1.0f, ui::pan_for_x(sort_chip_.cx()));
    }
}

} // namespace afterglow::channel_page
