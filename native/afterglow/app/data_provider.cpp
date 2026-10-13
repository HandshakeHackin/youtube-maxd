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
    waiting_.push_back({ticket, id, page, std::move(done), false, {}});
    return ticket;
}

unsigned ProviderSource::answer(const std::string &row, std::string_view text, Done done)
{
    if (row.size() >= EVO_PROVIDER_MAX_ITEM_ID || text.size() > 2400)
        return 0;
    const unsigned ticket = next_ticket_++;
    Request request;
    request.ticket = ticket;
    request.id = row;
    request.done = std::move(done);
    request.typed = true;
    request.text = std::string(text);
    waiting_.push_back(std::move(request));
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
    // Another profile, an account signed in or out. A sign-in in progress and the phone link change more often
    // than pages go stale: the screens that show them read their own catalogs again.
    return profile_changes_ * 65536u + tube_accounts_revision();
}

std::string ProviderSource::menu(const std::string &row, const std::string &title)
{
    // A video's actions are the provider's menu of that video; a comment's what the profile may do with it
    // (like, answer); a folder's (a channel, a playlist) its section menu.
    if (is_video_id(row))
        return "account:menu:" + row;
    if (const char *comment = tube_comment_rating(row.c_str()); comment != nullptr && *comment != '\0')
        return comment;
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
    if (ok && source->typed_stage_ == 1)
    {
        // The row that asks for a text was listed: its prompt is armed. The text is handed over from
        // pump(), never from inside the provider's own call.
        source->typed_stage_ = 2;
        return;
    }
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
    source->finish(std::move(page));
}

void ProviderSource::finish(Page &&page)
{
    if (!flying_dropped_)
        finished_.push_back({flying_, std::move(flying_done_), std::move(page)});
    flying_ = 0;
    flying_dropped_ = false;
    flying_done_ = nullptr;
    typed_stage_ = 0;
    // A typed text (it may be a password) is overwritten, not only let go.
    typed_text_.assign(typed_text_.size(), '\0');
    typed_text_.clear();
}

void ProviderSource::list(const std::string &id, int page)
{
    const unsigned ticket = flying_;
    g_waiting = this;
    // The provider may answer before it returns (a local list) or later, from the network poll.
    const int rc = evo_provider_youtube.list_catalog(id.c_str(), page, &ProviderSource::answered, this);
    if (rc < 0 && flying_ == ticket)
        answered(0, nullptr, 0, 0, this);
}

void ProviderSource::hand_over_text()
{
    // The prompt the provider armed takes the text and names the catalog to list next: the choice of a
    // profile, the review of a comment, the page of the preference that was typed.
    typed_stage_ = 3;
    Page refused;
    refused.load = model::Load::failed;
    tube_playlist_prompt_t prompt;
    if (!tube_take_playlist_prompt(&prompt))
    {
        refused.error = tr("Nothing is waiting for a text here.");
        finish(std::move(refused));
        return;
    }
    char next[EVO_PROVIDER_MAX_ITEM_ID] = "";
    if (tube_submit_playlist_name(prompt.ticket, typed_text_.c_str(), next, sizeof next) != 0)
    {
        const char *why = tube_prompt_error();
        refused.error = why != nullptr && *why != '\0' ? tr(why) : tr("Enter a name of 1 to 150 characters for the current profile");
        finish(std::move(refused));
        return;
    }
    typed_text_.assign(typed_text_.size(), '\0');
    typed_text_.clear();
    list(next, 0);
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
        typed_stage_ = 0;
        if (request.typed)
        {
            typed_text_ = std::move(request.text);
            typed_stage_ = request.id.empty() ? 2 : 1;
        }
        if (typed_stage_ != 2)
            list(request.id, request.page);
        if (flying_ == request.ticket && typed_stage_ == 2)
            hand_over_text();
    }
}

void ProviderSource::pump()
{
    if (flying_ != 0 && typed_stage_ == 2)
        hand_over_text(); // the listing that armed the prompt answered from the network poll
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
