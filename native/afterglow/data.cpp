// ProsperoTube Afterglow - the data layer's cache and Home's composition.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/data.hpp"

#include "afterglow/base.hpp"
#include "afterglow/platform.hpp"

#include <algorithm>

namespace afterglow
{

namespace
{

// Home's rows. A row whose catalog answers with nothing is not shown.
struct ShelfPlan
{
    const char *id;
    const char *title; // English; shown through tr()
    const char *catalog;
    model::ShelfKind kind;
    std::size_t limit; // cards kept from the first page
    // Asked in its turn: after every row above it has answered, and only while fewer filled rows
    // stand above it than the page reaches (Data::home_reach). A row of a signed-in Home is asked at once.
    bool in_turn = false;
};
constexpr ShelfPlan kSignedIn[] = {
    {"continue", tr_mark("Continue watching"), "history", model::ShelfKind::videos, 12},
    {"recommended", tr_mark("Recommended for you"), "library:home", model::ShelfKind::videos, 16},
    {"subscriptions", tr_mark("New from your subscriptions"), "library:subscriptions", model::ShelfKind::videos, 16},
};
// Without an account the page begins with videos all the same: what YouTube recommends to the
// session when it does (a new Guest gets a notice instead), then what is popular in a few topics.
// YouTube refuses Trending to a session without an account at the time of writing; the row is
// there for when it answers. Every row is asked in its turn, so a row YouTube leaves empty costs
// one request and the rows further down none until the page reaches them.
constexpr ShelfPlan kGuest[] = {
    {"continue", tr_mark("Continue watching"), "history", model::ShelfKind::videos, 12, true},
    {"recommended", tr_mark("Recommended for you"), "library:home", model::ShelfKind::videos, 16, true},
    {"music", tr_mark("Music"), "home-topic:music", model::ShelfKind::videos, 16, true},
    {"gaming", tr_mark("Gaming"), "home-topic:gaming", model::ShelfKind::videos, 16, true},
    {"topics", tr_mark("Browse by topic"), "discover", model::ShelfKind::topics, 16, true},
    {"trending", tr_mark("Trending now"), "section:trending", model::ShelfKind::videos, 16, true},
    {"news", tr_mark("News"), "home-topic:news", model::ShelfKind::videos, 16, true},
    {"sports", tr_mark("Sports"), "home-topic:sports", model::ShelfKind::videos, 16, true},
};

unsigned language()
{
    return platform() != nullptr ? platform()->language_revision() : 0u;
}

} // namespace

struct Data::Action
{
    Page page;
    unsigned ticket = 0; // the source's
    int pumps_done = -1; // pumps since it finished; -1 while it runs
};

struct Data::Slot
{
    Page page;
    unsigned ticket = 0;
    unsigned version = 0; // bumped when the answer arrives
    unsigned seen = 0;    // the version Home last copied
};

Data::Data(Source &source) : source_(source), revision_(source.revision()), alive_(std::make_shared<int>(0)) {}

Data::~Data()
{
    for (auto &pair : actions_)
    {
        if (pair.second->ticket != 0)
            source_.cancel(pair.second->ticket);
    }
    for (auto &pair : slots_)
    {
        if (pair.second->ticket != 0)
            source_.cancel(pair.second->ticket);
    }
}

void Data::pump()
{
    source_.pump();
    for (auto it = actions_.begin(); it != actions_.end();)
    {
        Action &action = *it->second;
        if (action.pumps_done >= 0 && ++action.pumps_done > 600)
            it = actions_.erase(it);
        else
            ++it;
    }
    if (source_.revision() != revision_)
    {
        revision_ = source_.revision();
        refresh();
    }
}

void Data::refresh()
{
    for (auto &pair : slots_)
    {
        if (pair.second->ticket != 0)
            source_.cancel(pair.second->ticket);
    }
    slots_.clear();
    home_ = model::Home{};
    home_built_ = false;
}

void Data::forget(const std::string &id)
{
    const std::string prefix = id + '\n';
    for (auto it = slots_.begin(); it != slots_.end();)
    {
        if (it->first.compare(0, prefix.size(), prefix) == 0)
        {
            if (it->second->ticket != 0)
                source_.cancel(it->second->ticket);
            it = slots_.erase(it);
        }
        else
        {
            ++it;
        }
    }
    home_built_ = false; // Home copies its shelves from slots: it reads them again
}

unsigned Data::act(const std::string &row)
{
    const unsigned mine = next_action_++;
    Action &action = *actions_.emplace(mine, std::make_unique<Action>()).first->second;
    action.page.load = model::Load::loading;
    const std::weak_ptr<int> alive = alive_;
    action.ticket = source_.catalog(row, 0, [this, alive, mine](Page &&answer)
    {
        if (alive.expired())
            return;
        const auto target = actions_.find(mine);
        if (target == actions_.end())
            return;
        target->second->page = std::move(answer);
        target->second->ticket = 0;
        target->second->pumps_done = 0;
    });
    if (action.ticket == 0)
    {
        action.page.load = model::Load::failed;
        action.page.error = tr("The request could not be queued. Try again.");
        action.pumps_done = 0;
    }
    return mine;
}

unsigned Data::act(const std::string &row, std::string_view text)
{
    const unsigned mine = next_action_++;
    Action &action = *actions_.emplace(mine, std::make_unique<Action>()).first->second;
    action.page.load = model::Load::loading;
    const std::weak_ptr<int> alive = alive_;
    action.ticket = source_.answer(row, text, [this, alive, mine](Page &&answer)
    {
        if (alive.expired())
            return;
        const auto target = actions_.find(mine);
        if (target == actions_.end())
            return;
        target->second->page = std::move(answer);
        target->second->ticket = 0;
        target->second->pumps_done = 0;
    });
    if (action.ticket == 0)
    {
        action.page.load = model::Load::failed;
        action.page.error = tr("This build cannot take typed text here.");
        action.pumps_done = 0;
    }
    return mine;
}

const char *load_name(model::Load load)
{
    switch (load)
    {
    case model::Load::idle:
        return "idle";
    case model::Load::loading:
        return "loading";
    case model::Load::ready:
        return "ready";
    case model::Load::failed:
        return "failed";
    case model::Load::offline:
        return "offline";
    }
    return "idle";
}

const Page &Data::action(unsigned ticket)
{
    static const Page kGone = []
    {
        Page page;
        page.load = model::Load::idle;
        return page;
    }();
    const auto found = actions_.find(ticket);
    return found != actions_.end() ? found->second->page : kGone;
}

Data::Slot &Data::slot(const std::string &id, int page)
{
    const std::string key = id + '\n' + std::to_string(page);
    auto found = slots_.find(key);
    if (found != slots_.end())
        return *found->second;
    Slot &slot = *slots_.emplace(key, std::make_unique<Slot>()).first->second;
    slot.page.load = model::Load::loading;
    const std::weak_ptr<int> alive = alive_;
    slot.ticket = source_.catalog(id, page, [this, alive, key](Page &&answer)
    {
        if (alive.expired())
            return;
        const auto target = slots_.find(key);
        if (target == slots_.end())
            return;
        target->second->page = std::move(answer);
        target->second->ticket = 0;
        ++target->second->version;
    });
    if (slot.ticket == 0 && slot.page.load == model::Load::loading)
    {
        slot.page.load = model::Load::failed;
        slot.page.error = tr("The request could not be queued. Try again.");
        ++slot.version;
    }
    return slot;
}

const Page &Data::catalog(const std::string &id, int page)
{
    return slot(id, page).page;
}

void Data::build_home()
{
    home_ = model::Home{};
    home_.profile = source_.profile();
    home_.invite_sign_in = !home_.profile.signed_in;
    const ShelfPlan *plan = home_.profile.signed_in ? kSignedIn : kGuest;
    const std::size_t count = home_.profile.signed_in ? sizeof kSignedIn / sizeof kSignedIn[0] : sizeof kGuest / sizeof kGuest[0];
    for (std::size_t i = 0; i < count; ++i)
    {
        model::Shelf shelf;
        shelf.id = plan[i].id;
        shelf.title = tr(plan[i].title);
        shelf.catalog = plan[i].catalog;
        shelf.kind = plan[i].kind;
        // A row asked in its turn is idle until then: nothing was asked for it.
        shelf.load = plan[i].in_turn ? model::Load::idle : model::Load::loading;
        home_.shelves.push_back(std::move(shelf));
        if (!plan[i].in_turn)
            slot(plan[i].catalog, 0).seen = 0;
    }
    home_asked_.assign(count, false);
    for (std::size_t i = 0; i < count; ++i)
        home_asked_[i] = !plan[i].in_turn;
    slot("", 0).seen = 0;
    home_built_ = true;
}

const model::Home &Data::home()
{
    if (!home_built_ || language_ != language())
    {
        language_ = language();
        build_home();
    }
    const ShelfPlan *plan = home_.profile.signed_in ? kSignedIn : kGuest;
    int asked = 0;
    int answered = 0;
    int offline = 0;
    int filled = 0;       // rows above that have something to show
    bool waiting = false; // a row above has not answered yet
    for (std::size_t i = 0; i < home_.shelves.size(); ++i)
    {
        model::Shelf &shelf = home_.shelves[i];
        if (!home_asked_[i])
        {
            // Its turn: everything above has answered and the page reaches this far.
            if (waiting || filled >= home_reach_)
                continue;
            home_asked_[i] = true;
            slot(shelf.catalog, 0).seen = 0; // a page kept from before is copied as a new one is
        }
        Slot &source = slot(shelf.catalog, 0);
        ++asked;
        if (source.page.load != model::Load::loading)
            ++answered;
        else
            waiting = true;
        if (source.page.load == model::Load::offline)
            ++offline;
        if (source.version == source.seen && shelf.load == source.page.load)
        {
            filled += !shelf.videos.empty() || !shelf.topics.empty() ? 1 : 0;
            continue;
        }
        source.seen = source.version;
        shelf.load = source.page.load;
        shelf.error = source.page.error;
        shelf.more = source.page.more;
        shelf.videos.clear();
        shelf.topics.clear();
        if (shelf.kind == model::ShelfKind::topics)
        {
            shelf.topics = source.page.topics;
        }
        else
        {
            for (const model::Video &video : source.page.videos)
            {
                // Continue watching holds what was started and not finished.
                if (shelf.id == "continue" && (video.watched || video.progress < 0.02f || video.progress > 0.97f))
                    continue;
                shelf.videos.push_back(video);
            }
        }
        const std::size_t limit = plan[i].limit;
        if (shelf.videos.size() > limit)
        {
            shelf.videos.resize(limit);
            shelf.more = true;
        }
        if (shelf.topics.size() > limit)
            shelf.topics.resize(limit);
        filled += !shelf.videos.empty() || !shelf.topics.empty() ? 1 : 0;
    }
    Slot &menu = slot("", 0);
    if (menu.version != menu.seen)
    {
        menu.seen = menu.version;
        home_.pinned = menu.page.channels;
    }
    // Rows whose turn has not come are not waited for: the page is ready with what it reaches.
    home_.load = asked > 0 && offline == asked ? model::Load::offline : answered == asked ? model::Load::ready : model::Load::loading;
    return home_;
}

} // namespace afterglow
