// ProsperoTube Afterglow - a catalog read page by page, and an action on its way.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/data_feed.hpp"

#include "afterglow/base.hpp"

#include <algorithm>

namespace afterglow
{

namespace
{

void append(Lists &into, const Page &page)
{
    // add_row() decided what each row is and left its place in the page with the view model.
    const int base = static_cast<int>(into.rows.size());
    into.rows.insert(into.rows.end(), page.rows.begin(), page.rows.end());
    for (const model::Video &video : page.videos)
    {
        into.videos.push_back(video);
        into.video_row.push_back(base + std::max(video.at, 0));
        into.videos.back().at = into.video_row.back();
    }
    for (const model::Channel &channel : page.channels)
    {
        into.channels.push_back(channel);
        into.channel_row.push_back(base + std::max(channel.at, 0));
        into.channels.back().at = into.channel_row.back();
    }
}

} // namespace

void Feed::start(std::string id)
{
    const unsigned next = version + 1;
    *this = Feed{};
    catalog = std::move(id);
    open = true;
    version = next;
}

void Feed::restart()
{
    if (!open)
        return;
    incoming_ = Lists{};
    again_ = taken > 0 || !rows.empty();
    taken = 0;
    more_failed = false;
}

bool Feed::read(Data &data)
{
    if (!open)
        return false;
    bool changed = false;
    const Page &first = data.catalog(catalog, 0);
    const bool waiting = first.load == model::Load::loading || first.load == model::Load::idle;
    if (!again_ && taken > 0 && first.load != model::Load::ready)
    {
        // The data layer forgot the catalog: what was read stays until it is there again.
        incoming_ = Lists{};
        again_ = true;
        taken = 0;
        more_failed = false;
    }
    const model::Load state = again_ && waiting ? model::Load::ready : first.load;
    if (load != state || error != first.error)
    {
        load = state;
        error = first.error;
        changed = true;
    }
    while (taken < wanted)
    {
        const Page &page = taken == 0 ? first : data.catalog(catalog, taken);
        if (page.load == model::Load::loading || page.load == model::Load::idle)
            break;
        if (page.load != model::Load::ready)
        {
            if (taken > 0 && !more_failed)
            {
                more_failed = true;
                changed = true;
            }
            break;
        }
        append(again_ ? incoming_ : static_cast<Lists &>(*this), page);
        more = page.more;
        ++taken;
        changed = changed || !again_;
    }
    // A list that is read again takes the place of the old one when every page that was shown is there,
    // or when it is clear that it will not come.
    const bool gone = !waiting && first.load != model::Load::ready;
    if (again_ && (gone || (first.load == model::Load::ready && (taken == wanted || more_failed))))
    {
        static_cast<Lists &>(*this) = gone ? Lists{} : std::move(incoming_);
        incoming_ = Lists{};
        again_ = false;
        changed = true;
    }
    if (changed)
        ++version;
    return changed;
}

model::Load Act::poll(Data &data, const Page **answer)
{
    if (!running)
        return model::Load::idle;
    const Page &page = data.action(ticket);
    if (page.load == model::Load::loading)
        return model::Load::loading;
    if (page.load == model::Load::ready && !applying && page.rows.size() == 1 && starts_with(page.rows.front().id, "account:apply:"))
    {
        // The provider named the change and waits for the word: it was given on the control that was pressed.
        const std::string apply = page.rows.front().id;
        applying = true;
        ticket = data.act(apply);
        return model::Load::loading;
    }
    running = false;
    if (answer != nullptr)
        *answer = &page;
    return page.load == model::Load::idle ? model::Load::failed : page.load;
}

} // namespace afterglow
