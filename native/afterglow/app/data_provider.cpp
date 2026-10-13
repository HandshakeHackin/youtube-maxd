// ProsperoTube Afterglow - the data layer's source in the app: the shipping YouTube provider.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/app/data_provider.hpp"

#include "afterglow/base.hpp"
#include "afterglow/data_rows.hpp"

extern "C"
{
#include "evo_error.h"
#include "tube_provider.h"
}

#include <cstring>

namespace afterglow
{

namespace
{

// The provider answers into whichever source is waiting; there is one.
ProviderSource *g_waiting = nullptr;

std::vector<std::string> split(const char *list)
{
    std::vector<std::string> parts;
    std::string part;
    for (const char *c = list; c != nullptr && *c != '\0'; ++c)
    {
        if (*c == ',')
        {
            if (!part.empty())
                parts.push_back(part);
            part.clear();
        }
        else if (*c != ' ')
            part.push_back(*c);
    }
    if (!part.empty())
        parts.push_back(part);
    return parts;
}

} // namespace

ProviderSource::ProviderSource() : last_profile_(tube_browse_profile()) {}

ProviderSource::~ProviderSource()
{
    if (g_waiting == this)
        g_waiting = nullptr;
}

unsigned ProviderSource::catalog(const std::string &id, int page, Done done)
{
    if (id.size() >= EVO_PROVIDER_MAX_ITEM_ID || page < 0)
        return 0;
    const unsigned ticket = next_ticket_++;
    waiting_.push_back({ticket, id, page, std::move(done)});
    return ticket;
}

void ProviderSource::cancel(unsigned ticket)
{
    for (auto it = waiting_.begin(); it != waiting_.end(); ++it)
    {
        if (it->ticket == ticket)
        {
            waiting_.erase(it);
            return;
        }
    }
    for (auto it = finished_.begin(); it != finished_.end(); ++it)
    {
        if (it->ticket == ticket)
        {
            finished_.erase(it);
            return;
        }
    }
    if (flying_ == ticket)
        flying_dropped_ = true; // the provider finishes it; the answer is thrown away
}

model::Profile ProviderSource::profile()
{
    model::Profile profile;
    profile.id = tube_browse_profile();
    profile.guest = profile.id == "guest";
    profile.name = profile.guest ? std::string() : std::string(tube_browse_profile_name());
    profile.signed_in = tube_browse_profile_account() != 0;
    return profile;
}

unsigned ProviderSource::revision()
{
    const char *now = tube_browse_profile();
    if (last_profile_ != now)
    {
        last_profile_ = now;
        ++profile_changes_;
    }
    return profile_changes_ * 65536u + tube_accounts_revision();
}

std::string ProviderSource::menu(const std::string &row, const std::string &title)
{
    const char *menu = tube_folder_menu(row.c_str(), title.c_str());
    return menu != nullptr ? std::string(menu) : std::string();
}

bool ProviderSource::play(const model::Video &video)
{
    return player_ ? player_(video) : false;
}

void ProviderSource::answered(int ok, const evo_provider_item_t *items, int count, int more, void *self)
{
    ProviderSource *source = g_waiting;
    if (source == nullptr || self != source || source->flying_ == 0)
        return; // an answer nobody waits for any more
    Page page;
    if (ok)
    {
        for (int i = 0; items != nullptr && i < count; ++i)
        {
            const evo_provider_item_t &item = items[i];
            RawRow row;
            row.id = item.id;
            row.channel_id = item.parent_id;
            row.title = item.title;
            row.subtitle = item.subtitle;
            row.overview = item.overview;
            row.art_url = item.art_url;
            row.duration_s = item.duration_sec;
            row.resume_s = item.resume_pos_sec;
            row.folder = item.is_folder != 0;
            row.live = item.is_live != 0;
            row.played = item.played != 0;
            row.interface_text = item.tube_interface != 0;
            const char *name = nullptr;
            const char *value = nullptr;
            for (int k = 0; tube_row_fact_at(item.id, k, &name, &value); ++k)
            {
                if (!std::strcmp(name, "views"))
                    row.views = value;
                else if (!std::strcmp(name, "published"))
                    row.published = value;
                else if (!std::strcmp(name, "channel_art_url"))
                    row.channel_art = value;
                else if (!std::strcmp(name, "badges"))
                    row.badges = split(value);
                else if (!std::strcmp(name, "is_upcoming"))
                    row.upcoming = std::strcmp(value, "1") == 0;
                else if (!std::strcmp(name, "is_short"))
                    row.is_short = std::strcmp(value, "1") == 0;
                else
                    row.facts.emplace_back(name, value);
            }
            if (!row.id.empty())
                add_row(page, row);
        }
        page.more = more != 0;
        page.load = model::Load::ready;
    }
    else
    {
        char message[512] = "";
        evo_error_take(message, sizeof message);
        // The built-in service that did not start is "offline"; everything else is the provider's own message.
        const bool offline = tube_embedded_mode() != 0 && *tube_embedded_error() != '\0';
        page.load = offline ? model::Load::offline : model::Load::failed;
        page.error = *message != '\0' ? tr(message) : tr("This list could not be loaded.");
    }
    if (!source->flying_dropped_)
        source->finished_.push_back({source->flying_, std::move(source->flying_done_), std::move(page)});
    source->flying_ = 0;
    source->flying_dropped_ = false;
    source->flying_done_ = nullptr;
}

void ProviderSource::start_next()
{
    while (flying_ == 0 && !waiting_.empty())
    {
        Request request = std::move(waiting_.front());
        waiting_.pop_front();
        flying_ = request.ticket;
        flying_dropped_ = false;
        flying_done_ = std::move(request.done);
        g_waiting = this;
        // The provider may answer before it returns (a local list) or later, from the network poll.
        const int rc = evo_provider_youtube.list_catalog(request.id.c_str(), request.page, &ProviderSource::answered, this);
        if (rc < 0 && flying_ == request.ticket)
            answered(0, nullptr, 0, 0, this);
    }
}

void ProviderSource::pump()
{
    // Answers reach their owners here, never from inside catalog().
    while (!finished_.empty())
    {
        Finished finished = std::move(finished_.front());
        finished_.pop_front();
        if (finished.done)
            finished.done(std::move(finished.page));
    }
    start_next();
}

} // namespace afterglow
