// ProsperoTube Afterglow - Home.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The hero follows the focus: the focused video's picture dissolves into the
// page, its title and its play line stand on the left. Under it the shelves:
// signed in, Continue watching, Recommended and From your subscriptions; as
// Guest, the topics, Trending and an invitation to sign in. Left of the first
// card is the rail. Alone for a while, the page shows off what is on.

#include "afterglow/screens/home.hpp"

#include <algorithm>
#include <cmath>

namespace afterglow
{

namespace home_page
{

void Home::enter(const Route &route, UpdateContext &context, bool restored)
{
    (void)route;
    if (restored)
    {
        // Back from a video or a page: what was watched may have moved shelves.
        context.services.data.forget("history");
        return;
    }
    age_ = 0.0f;
    row_ = 0;
    wanted_.clear();
    shown_.clear();
    row_pos_.snap(0.0f);
    ring_ready_ = false;
    attract_ = false;
    attract_amount_.snap(0.0f);
}

void Home::update(const InputFrame &input, UpdateContext &context)
{
    age_ += context.dt;
    const bool reduced = context.reduced_motion;
    collect(context.services.data.home());
    const int count = static_cast<int>(rows_.size());
    row_ = std::clamp(row_, 0, std::max(0, count - 1));

    if (attract_ && count > 0)
    {
        attract_step_ += context.dt;
        Row &row = rows_[static_cast<std::size_t>(row_)];
        if (attract_step_ > kAttractStep && row.count() > 0)
        {
            attract_step_ = 0.0f;
            state(row).col = (state(row).col + 1) % row.count();
        }
    }
    else if (context.top && count > 0)
    {
        handle(input, context);
    }
    else if (context.top)
    {
        // Nothing to focus: the page is loading, empty or offline.
        if (pressed(input, Action::confirm) && load_ != model::Load::loading)
        {
            context.services.data.refresh();
            context.feedback.play(audio::Cue::select);
        }
        else if (pressed(input, Action::west))
            context.nav.push(Route{ScreenId::search, {}, {}});
        else if (input.nav == Direction::left)
            context.nav.open_rail();
        else if (pressed(input, Action::back))
            context.nav.back();
    }

    follow_later(context);

    // ---- motion ----
    row_pos_.target = static_cast<float>(row_);
    row_pos_.update(context.dt, reduced ? 60.0f : 11.0f);
    for (Row &row : rows_)
    {
        RowState &s = state(row);
        const float pitch = row.topics() ? kTilePitch : kCardPitch;
        const float width = row.topics() ? kTileW : kCardW;
        const float start = static_cast<float>(s.col) * pitch;
        s.scroll.reveal(start, start + width, kRight - kLeft, pitch * 0.5f);
        s.scroll.update(context.dt, reduced ? 60.0f : 12.0f);
    }
    if (count > 0)
    {
        const Rect target = card_rect(row_, state(rows_[static_cast<std::size_t>(row_)]).col);
        if (!ring_ready_)
        {
            ring_.snap(target);
            ring_ready_ = true;
        }
        ring_.target(target);
        ring_.update(context.dt, reduced ? 60.0f : 20.0f);
    }
    nudge_.update(context.dt, 9.0f);
    attract_amount_.target = attract_ ? 1.0f : 0.0f;
    attract_amount_.update(context.dt, reduced ? 40.0f : 3.2f);

    // The hero and the page's colour follow the focus.
    const model::Video *focused = hero_video();
    const std::string now = focused != nullptr ? focused->id : std::string();
    if (now != hero_now_.id)
    {
        hero_prev_ = hero_now_;
        hero_now_ = focused != nullptr ? *focused : model::Video{};
        hero_label_prev_ = hero_label_;
        hero_fade_.start(reduced ? 0.12f : 0.5f);
        context.nav.ambient(hero_now_.thumb);
    }
    hero_label_ = hero_shelf_title();
    hero_fade_.update(context.dt);
}

std::span<const ui::Hint> Home::hints() const
{
    static ui::Hint items[4];
    if (rows_.empty())
    {
        if (load_ == model::Load::loading)
            return {};
        items[0] = {ui::Button::cross, tr("Try again")};
        items[1] = {ui::Button::square, tr("Search")};
        return {items, 2};
    }
    const Row &row = rows_[static_cast<std::size_t>(row_)];
    if (row.topics())
    {
        items[0] = {ui::Button::cross, tr("Open topic")};
        items[1] = {ui::Button::square, tr("Search")};
        items[2] = {ui::Button::options, tr("Sign in")};
        return {items, static_cast<std::size_t>(invite_ ? 3 : 2)};
    }
    items[0] = {ui::Button::cross, tr("Play")};
    items[1] = {ui::Button::triangle, tr("Details")};
    // Watch later is a list of the YouTube account: a Guest's Square searches instead.
    items[2] = {ui::Button::square, signed_in_ ? tr("Watch later") : tr("Search")};
    items[3] = {ui::Button::touchpad, tr("Actions")};
    return items;
}

float Home::furniture() const
{
    return 1.0f - tween::smoothstep(tween::clamp01(attract_amount_.value * 1.6f));
}

bool Home::attract(bool on)
{
    if (on && (rows_.empty() || rows_[static_cast<std::size_t>(row_)].topics() || rows_[static_cast<std::size_t>(row_)].count() == 0))
        return false; // nothing to show off
    attract_ = on;
    attract_step_ = 0.0f;
    return on;
}

FocusInfo Home::focus() const
{
    FocusInfo info;
    if (rows_.empty())
    {
        info.id = load_ == model::Load::loading ? "loading" : load_ == model::Load::offline ? "offline" : "empty";
        info.rect = {kLeft, 360.0f, 900.0f, 200.0f};
        info.total = 1;
        return info;
    }
    const Row &row = rows_[static_cast<std::size_t>(row_)];
    const int col = state_of(row).col;
    info.id = row.shelf.id + "/" + std::to_string(col);
    info.text = row.count() == 0 ? row.shelf.title : row.topics() ? row.shelf.topics[static_cast<std::size_t>(col)].title
                                                                   : row.shelf.videos[static_cast<std::size_t>(col)].title;
    info.rect = card_rect(row_, col);
    info.index = col;
    info.total = row.count();
    if (row.count() > 0)
    {
        info.kind = row.topics() ? "topic" : "video";
        info.live = !row.topics() && (row.shelf.videos[static_cast<std::size_t>(col)].badges & model::kBadgeLive) != 0;
    }
    return info;
}

std::vector<std::pair<std::string, std::string>> Home::focusable() const
{
    std::vector<std::pair<std::string, std::string>> all;
    for (const Row &row : rows_)
    {
        for (int i = 0; i < row.count(); ++i)
            all.emplace_back(row.shelf.id + "/" + std::to_string(i), row.topics() ? row.shelf.topics[static_cast<std::size_t>(i)].title
                                                                                  : row.shelf.videos[static_cast<std::size_t>(i)].title);
    }
    return all;
}

void Home::snapshot(Snapshot &out) const
{
    out.set("load", load_ == model::Load::ready ? "ready" : load_ == model::Load::loading ? "loading" : load_ == model::Load::offline ? "offline" : "failed");
    out.set("profile", guest_ ? "guest" : "account");
    out.set("shelves", static_cast<int>(rows_.size()));
    out.set("row", rows_.empty() ? std::string() : rows_[static_cast<std::size_t>(row_)].shelf.id);
    out.set("hero", hero_now_.title);
    out.set("invite", invite_ ? 1 : 0);
    for (const Row &row : rows_)
    {
        std::vector<std::string> titles;
        for (int i = 0; i < row.count(); ++i)
            titles.push_back(row.topics() ? row.shelf.topics[static_cast<std::size_t>(i)].title : row.shelf.videos[static_cast<std::size_t>(i)].title);
        out.list("shelf:" + row.shelf.id, std::move(titles));
    }
}

// Takes over what Home is now: the shelves to show, in order, keeping the focus where it was.
void Home::collect(const model::Home &home)
{
    const std::string focused = rows_.empty() ? std::string() : rows_[static_cast<std::size_t>(std::clamp(row_, 0, static_cast<int>(rows_.size()) - 1))].shelf.id;
    load_ = home.load;
    error_ = home.error;
    guest_ = home.profile.guest;
    invite_ = home.invite_sign_in;
    signed_in_ = home.profile.signed_in;
    rows_.clear();
    bool any_ready = false;
    for (const model::Shelf &shelf : home.shelves)
    {
        const bool ready = shelf.load == model::Load::ready;
        const bool filled = !shelf.videos.empty() || !shelf.topics.empty();
        any_ready = any_ready || ready;
        if (shelf.load == model::Load::failed && error_.empty())
            error_ = shelf.error;
        if (ready)
        {
            if (filled)
                shown_[shelf.id] = shelf;
            else
                shown_.erase(shelf.id);
        }
        // A shelf that is being read again keeps what it showed until the answer is in.
        const auto kept = shown_.find(shelf.id);
        if (kept != shown_.end() && (ready || shelf.load == model::Load::loading))
        {
            rows_.push_back(Row{kept->second});
            any_ready = true;
        }
    }
    if (!any_ready)
        rows_.clear(); // the whole page waits together; shelves do not pop in around an empty middle
    if (rows_.empty() && load_ == model::Load::ready)
        load_ = error_.empty() ? model::Load::ready : model::Load::failed;
    (void)focused;
    if (wanted_.empty() && !rows_.empty())
        wanted_ = rows_.front().shelf.id;
    for (std::size_t i = 0; i < rows_.size(); ++i)
    {
        RowState &s = state(rows_[i]);
        s.col = std::clamp(s.col, 0, std::max(0, rows_[i].count() - 1));
        if (rows_[i].shelf.id == wanted_)
            row_ = static_cast<int>(i);
    }
}

float Home::top_of(int index) const
{
    float y = 0.0f;
    for (int i = 0; i < index && i < static_cast<int>(rows_.size()); ++i)
        y += rows_[static_cast<std::size_t>(i)].topics() ? kTopicPitch : kShelfPitch;
    return y;
}

Rect Home::card_rect(int row, int col) const
{
    if (rows_.empty())
        return {kLeft, kShelfTop, kCardW, kCardH};
    const int last = static_cast<int>(rows_.size()) - 1;
    const int low = std::clamp(static_cast<int>(std::floor(row_pos_.value)), 0, last);
    const int high = std::min(low + 1, last);
    const float scroll = tween::lerp(top_of(low), top_of(high), row_pos_.value - static_cast<float>(low));
    const Row &r = rows_[static_cast<std::size_t>(std::clamp(row, 0, last))];
    const float first = rows_.front().topics() ? kShelfTopTopics : kShelfTop;
    const float y = first + top_of(row) - scroll;
    const float offset = state_of(r).scroll.offset();
    if (r.topics())
        return {kLeft + static_cast<float>(col) * kTilePitch - offset, y, kTileW, kTileH};
    return {kLeft + static_cast<float>(col) * kCardPitch - offset, y, kCardW, kCardH};
}

// The video the hero shows: the focused one; on the topic tiles, the first of the first shelf that has videos.
const model::Video *Home::hero_video() const
{
    if (rows_.empty())
        return nullptr;
    const Row &row = rows_[static_cast<std::size_t>(row_)];
    if (!row.topics() && row.count() > 0)
        return &row.shelf.videos[static_cast<std::size_t>(state_of(row).col)];
    for (const Row &other : rows_)
    {
        if (!other.topics() && other.count() > 0)
            return &other.shelf.videos[0];
    }
    return nullptr;
}

std::string Home::hero_shelf_title() const
{
    if (rows_.empty())
        return {};
    const Row &row = rows_[static_cast<std::size_t>(row_)];
    if (!row.topics())
        return row.shelf.title;
    for (const Row &other : rows_)
    {
        if (!other.topics() && other.count() > 0)
            return other.shelf.title;
    }
    return {};
}

// Watch later on its way: said in a toast when YouTube has answered.
void Home::follow_later(UpdateContext &context)
{
    const Page *answer = nullptr;
    const model::Load result = later_.poll(context.services.data, &answer);
    if (result == model::Load::ready)
    {
        const bool said = answer != nullptr && !answer->rows.empty() && !answer->rows.front().title.empty();
        context.nav.toast(said ? answer->rows.front().title.c_str() : tr("Added to Watch later"), later_title_, ToastKind::success);
        context.feedback.rumble(0.3f, 0.06f);
        context.services.data.forget("library:watch_later");
    }
    else if (result == model::Load::failed || result == model::Load::offline)
    {
        const bool said = answer != nullptr && !answer->error.empty();
        context.nav.toast(said ? answer->error.c_str() : tr("YouTube did not confirm the change. Nothing was changed."), later_title_, ToastKind::error);
        context.feedback.play(audio::Cue::error);
        context.feedback.rumble(0.25f, 0.05f);
    }
}

} // namespace home_page

std::unique_ptr<Screen> make_home()
{
    return std::make_unique<home_page::Home>();
}

} // namespace afterglow
