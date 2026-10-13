// ProsperoTube Afterglow - Settings: a change on its way to the provider.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A change goes to the provider through Data::act(), one at a time. The row
// shows the value that was asked for while it is on its way, the provider's
// when it answered, and returns to what it had when the provider refused.

#include "afterglow/screens/settings_screen.hpp"

namespace afterglow::settings
{

// What a row shows now: its confirmed value, or the newest one asked for.
Settings::Shown Settings::shown(const Item &item) const
{
    Shown now{item.value, item.on, item.index, false, nullptr};
    for (const Change &change : changes_)
    {
        if (change.item != item.row.id || change.catalog != item.catalog)
            continue;
        now.waiting = true;
        if (change.sets_value)
            now.value = change.value;
        if (change.sets_on)
            now.on = change.on;
        if (change.index >= 0)
            now.index = change.index;
        if (change.has_choice)
            now.choice = &change.choice;
    }
    return now;
}

bool Settings::ask(Change change, UpdateContext &context)
{
    if (!change.absolute || changes_.empty() || changes_.back().ticket != 0 || !changes_.back().absolute ||
        changes_.back().item != change.item || changes_.back().catalog != change.catalog)
    {
        if (changes_.size() >= kWaiting)
        {
            context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
            return false;
        }
        changes_.push_back(std::move(change));
    }
    else
    {
        changes_.back() = std::move(change); // the step before it was never sent: this one stands for both
    }
    return true;
}

// Asks first, with the frame's dialog; the change is made when the answer is yes.
void Settings::ask_first(Change change, std::string_view title, std::string_view body, std::string_view yes, UpdateContext &context)
{
    asking_ = std::move(change);
    const std::weak_ptr<int> alive = alive_;
    context.nav.confirm(title, body, yes, [this, alive](bool agreed)
    {
        if (alive.expired())
            return;
        if (agreed && changes_.size() < kWaiting)
            changes_.push_back(std::move(asking_));
        asking_ = Change{};
    });
}

// One change at a time goes to the provider; its answer settles the row.
void Settings::settle(UpdateContext &context)
{
    if (changes_.empty())
        return;
    Data &data = context.services.data;
    Change &front = changes_.front();
    if (front.ticket == 0)
        front.ticket = data.act(front.action);
    const Page &state = data.action(front.ticket);
    if (state.load == model::Load::loading)
        return;
    const Change done = std::move(front);
    changes_.pop_front();
    if (state.load == model::Load::ready)
    {
        const std::vector<model::Row> answer = state.rows;
        confirmed(done, answer, context);
    }
    else if (state.load == model::Load::idle)
    {
        // The data layer no longer has the answer (this page was covered for longer than answers are
        // kept): how it ended is in the lists, which are read again.
        for (const Source &source : sources_)
            data.forget(source.catalog);
        if (done.moved)
        {
            if (Source *source = source_of(done.catalog))
                source->rows = done.before;
            rebuild();
        }
    }
    else
    {
        const std::string error = state.error;
        refused(done, error, context);
    }
}

void Settings::confirmed(const Change &done, const std::vector<model::Row> &answer, UpdateContext &context)
{
    // The provider answers with a list. Where that is a list of this page, it is that list as it now stands.
    bool refreshed = false;
    for (Source &source : sources_)
    {
        if (source.pages == 1 && same_list(source.rows, answer))
        {
            source.rows = answer;
            refreshed = true;
        }
    }
    const auto listed = choices_.find(done.item);
    std::vector<model::Row> before;
    if (listed != choices_.end())
    {
        for (const Item &choice : listed->second.rows)
            before.push_back(choice.row);
        if (same_list(before, answer))
        {
            listed->second.rows.clear();
            for (const model::Row &row : answer)
                listed->second.rows.push_back(item_of(row, done.item));
        }
    }
    if (!refreshed)
        patch(done, answer);
    // The data layer's copies are older than the provider now: the next read asks again.
    Data &data = context.services.data;
    for (const Source &source : sources_)
        data.forget(source.catalog);
    data.forget(kMain);
    if (!done.item.empty())
        data.forget(done.item);
    rebuild();
    if (!done.item.empty() && !done.moved)
        motion_[done.catalog + '\n' + done.item].applied.trigger(1.0f);
    ++applied_;
    last_applied_ = done.action;
    if (!done.done.empty())
        context.nav.toast(done.done, ToastKind::success);
    if (done.leave)
        leave_ = 0.3f;
}

// The answer was another list than the row's: the row takes what the answer says of it, or what was asked.
void Settings::patch(const Change &done, const std::vector<model::Row> &answer)
{
    Source *source = source_of(done.catalog);
    if (source == nullptr || done.item.empty())
        return;
    const auto row = std::find_if(source->rows.begin(), source->rows.end(), [&](const model::Row &r) { return r.id == done.item; });
    if (row == source->rows.end())
        return;
    const Item item = item_of(*row, done.catalog);
    if (item.kind == Kind::choice)
    {
        for (model::Row &other : source->rows)
            set_fact(other, "selected", other.id == done.item ? "1" : "0");
        return;
    }
    if (item.kind == Kind::list || item.kind == Kind::slider)
    {
        // The choice the answer marks is the one that stands.
        int position = 0;
        for (const model::Row &other : answer)
        {
            const Item choice = item_of(other, done.item);
            if (choice.kind != Kind::choice)
                continue;
            if (choice.on)
            {
                set_fact(*row, "value", choice.label);
                set_fact(*row, "index", std::to_string(position));
                for (const char *name : {"number", "look", "background", "accent", "text_rgb", "background_alpha", "edge"})
                {
                    if (!other.fact(name).empty())
                        set_fact(*row, name, other.fact(name));
                }
                return;
            }
            ++position;
        }
    }
    if (done.sets_value)
        set_fact(*row, "value", done.value);
    if (done.sets_on)
        set_fact(*row, "on", done.on ? "1" : "0");
    if (done.index >= 0)
        set_fact(*row, "index", std::to_string(done.index));
    if (done.has_choice)
    {
        for (const char *name : {"number", "look", "background", "accent", "text_rgb", "background_alpha", "edge"})
        {
            if (!done.choice.fact(name).empty())
                set_fact(*row, name, done.choice.fact(name));
        }
    }
}

void Settings::refused(const Change &done, const std::string &error, UpdateContext &context)
{
    // What was asked of the same row after it falls with it: the row is back at what the provider holds.
    changes_.erase(std::remove_if(changes_.begin(), changes_.end(),
                                  [&](const Change &other) { return other.ticket == 0 && other.item == done.item && other.catalog == done.catalog; }),
                   changes_.end());
    if (done.moved)
    {
        if (Source *source = source_of(done.catalog))
            source->rows = done.before;
        rebuild();
    }
    if (!done.item.empty())
        motion_[done.catalog + '\n' + done.item].refused.trigger(1.0f);
    ++refused_;
    last_refused_ = done.action;
    const std::string words = error.empty() ? std::string(tr("That could not be saved. Nothing was changed.")) : error;
    said_row_ = done.catalog + '\n' + done.item; // the row's help says it in full for as long as the focus stays
    said_ = words;
    context.nav.toast(words, ToastKind::error);
    context.feedback.play(audio::Cue::error);
    context.feedback.rumble(0.25f, 0.05f);
}

} // namespace afterglow::settings
