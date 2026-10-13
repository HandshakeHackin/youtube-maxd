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
};
constexpr ShelfPlan kSignedIn[] = {
    {"continue", "Continue watching", "history", model::ShelfKind::videos, 12},
    {"recommended", "Recommended", "library:home", model::ShelfKind::videos, 16},
    {"subscriptions", "From your subscriptions", "library:subscriptions", model::ShelfKind::videos, 16},
};
constexpr ShelfPlan kGuest[] = {
    {"topics", "Browse by topic", "discover", model::ShelfKind::topics, 16},
    {"continue", "Continue watching", "history", model::ShelfKind::videos, 12},
    {"trending", "Trending", "section:trending", model::ShelfKind::videos, 16},
};

unsigned language()
{
    return platform() != nullptr ? platform()->language_revision() : 0u;
}

} // namespace

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
    for (auto &pair : slots_)
    {
        if (pair.second->ticket != 0)
            source_.cancel(pair.second->ticket);
    }
}

void Data::pump()
{
    source_.pump();
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
        shelf.load = model::Load::loading;
        home_.shelves.push_back(std::move(shelf));
        slot(plan[i].catalog, 0).seen = 0;
    }
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
    int answered = 0;
    int offline = 0;
    for (std::size_t i = 0; i < home_.shelves.size(); ++i)
    {
        model::Shelf &shelf = home_.shelves[i];
        Slot &source = slot(shelf.catalog, 0);
        if (source.page.load != model::Load::loading)
            ++answered;
        if (source.page.load == model::Load::offline)
            ++offline;
        if (source.version == source.seen && shelf.load == source.page.load)
            continue;
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
    }
    Slot &menu = slot("", 0);
    if (menu.version != menu.seen)
    {
        menu.seen = menu.version;
        home_.pinned = menu.page.channels;
    }
    const int total = static_cast<int>(home_.shelves.size());
    home_.load = total > 0 && offline == total ? model::Load::offline : answered == total ? model::Load::ready : model::Load::loading;
    return home_;
}

} // namespace afterglow
