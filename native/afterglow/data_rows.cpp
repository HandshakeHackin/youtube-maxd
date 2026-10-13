// ProsperoTube Afterglow - catalog rows as they arrive, and what they become.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/data_rows.hpp"

#include "afterglow/base.hpp"
#include "afterglow/text.hpp"

#include "cJSON.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>

namespace afterglow
{

namespace
{

std::string string_of(const cJSON *object, const char *name)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) && item->valuestring != nullptr ? std::string(item->valuestring) : std::string();
}

double number_of(const cJSON *object, const char *name, double fallback = 0.0)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsNumber(item) ? item->valuedouble : fallback;
}

bool flag_of(const cJSON *object, const char *name)
{
    return cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(object, name));
}

} // namespace

bool is_video_id(std::string_view id)
{
    if (id.size() != 11)
        return false;
    return std::all_of(id.begin(), id.end(), [](char c)
    { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'; });
}

const char *topic_icon(std::string_view catalog)
{
    static constexpr struct
    {
        const char *id;
        const char *icon;
    } kIcons[] = {
        {"search:science", "flask"},      {"search:technology", "grid"},  {"section:gaming", "gamepad"},
        {"section:news", "news"},         {"section:music", "music"},     {"search:documentary", "movie"},
        {"search:education", "bookmark"}, {"section:sports", "sports"},   {"section:live", "live"},
        {"section:movies", "movie"},      {"section:trending", "trending"}, {"section:shorts", "shorts"},
        {"section:kids", "users"},
    };
    for (const auto &entry : kIcons)
    {
        if (catalog == entry.id)
            return entry.icon;
    }
    return "grid";
}

void add_row(Page &page, const RawRow &row)
{
    model::Row plain;
    plain.id = row.id;
    // A row the provider wrote is an interface text: it is shown in the profile's language.
    // What YouTube wrote keeps its words and loses what no face draws.
    plain.title = row.interface_text ? std::string(tr(row.title.c_str())) : clean(row.title);
    plain.subtitle = row.interface_text ? std::string(tr(row.subtitle.c_str())) : clean(row.subtitle);
    plain.overview = row.interface_text ? row.overview : clean(row.overview);
    plain.thumb = row.art_url;
    plain.folder = row.folder;
    plain.interface_text = row.interface_text;
    // Facts keep YouTube's words too and lose what no face draws; an address is left as it is.
    plain.facts.reserve(row.facts.size() + 8);
    for (const auto &fact : row.facts)
    {
        const bool address = fact.first.size() >= 3 && fact.first.compare(fact.first.size() - 3, 3, "url") == 0;
        plain.facts.emplace_back(fact.first, address ? fact.second : clean(fact.second));
    }
    // What a video's fields are filled from stays readable as facts on any row: the first row of
    // "details:<video>" is a folder, and it has views, an age and a channel picture all the same.
    const auto keep = [&](const char *name, std::string value)
    {
        if (!value.empty() && plain.fact(name).empty())
            plain.facts.emplace_back(name, std::move(value));
    };
    keep("views", clean(row.views));
    keep("published", clean(row.published));
    keep("channel_art_url", row.channel_art);
    keep("channel_id", row.channel_id);
    if (!row.badges.empty())
    {
        std::string badges;
        for (const std::string &badge : row.badges)
            badges += (badges.empty() ? "" : ",") + badge;
        keep("badges", std::move(badges));
    }
    if (row.live)
        keep("is_live", "1");
    if (row.upcoming)
        keep("is_upcoming", "1");
    if (row.is_short)
        keep("is_short", "1");
    if (row.duration_s > 0)
        keep("duration_sec", std::to_string(row.duration_s));
    if (row.resume_s > 0)
        keep("resume_sec", std::to_string(row.resume_s));
    page.rows.push_back(plain);
    const int at = static_cast<int>(page.rows.size()) - 1;

    if (starts_with(row.id, "channel:"))
    {
        model::Channel channel;
        channel.id = row.id.substr(8);
        channel.row = row.id;
        channel.name = plain.title;
        channel.subscribers = row.interface_text || plain.subtitle.empty() ? plain.fact("subscribers") : plain.subtitle;
        channel.avatar = row.art_url;
        channel.handle = plain.fact("handle");
        channel.banner = plain.fact("banner_url");
        channel.subscribed = plain.has("subscribed");
        channel.unseen = leading_number(plain.fact("unseen"));
        channel.live = plain.has("live") || row.live;
        channel.at = at;
        page.channels.push_back(std::move(channel));
        return;
    }
    if (row.folder)
    {
        if (starts_with(row.id, "section:") || starts_with(row.id, "search:"))
        {
            model::Topic topic;
            topic.id = row.id;
            topic.title = plain.title;
            topic.icon = topic_icon(row.id);
            page.topics.push_back(std::move(topic));
        }
        return;
    }
    if (row.interface_text)
        return; // a notice or an action, not a video

    model::Video video;
    video.id = row.id;
    video.video_id = is_video_id(row.id) ? row.id : std::string();
    video.title = plain.title;
    video.channel = plain.subtitle;
    video.channel_id = row.channel_id;
    video.views = clean(row.views);
    video.age = clean(row.published);
    video.overview = plain.overview;
    video.thumb = row.art_url;
    video.avatar = row.channel_art;
    video.duration_s = row.live ? 0 : static_cast<int>(std::clamp<long long>(row.duration_s, 0, 360000));
    if (row.resume_s > 0 && row.duration_s > 0)
        video.progress = std::clamp(static_cast<float>(row.resume_s) / static_cast<float>(row.duration_s), 0.0f, 1.0f);
    else if (row.watch_percent > 0.0f)
        video.progress = std::clamp(row.watch_percent / 100.0f, 0.0f, 1.0f);
    video.watched = row.played || row.watch_percent >= 100.0f;
    if (video.watched && video.progress <= 0.0f)
        video.progress = 1.0f;
    if (row.live)
        video.badges |= model::kBadgeLive;
    if (row.upcoming)
        video.badges |= model::kBadgeUpcoming;
    if (row.is_short)
        video.badges |= model::kBadgeShort;
    for (const std::string &badge : row.badges)
    {
        if (badge == "new")
            video.badges |= model::kBadgeNew;
        else if (badge == "4k")
            video.badges |= model::kBadge4K;
        else if (badge == "hdr")
            video.badges |= model::kBadgeHdr;
        else if (badge == "cc")
            video.badges |= model::kBadgeCaptions;
        else if (badge == "members")
            video.badges |= model::kBadgeMembers;
    }
    if (const std::string &days = plain.fact("published_days"); !days.empty() && days[0] >= '0' && days[0] <= '9')
        video.published_days = leading_number(days);
    video.at = at;
    page.videos.push_back(std::move(video));
}

bool page_from_json(const std::string &json, Page *page)
{
    *page = Page{};
    cJSON *root = cJSON_ParseWithLength(json.data(), json.size());
    if (!cJSON_IsObject(root))
    {
        cJSON_Delete(root);
        page->load = model::Load::failed;
        page->error = "The answer could not be read.";
        return false;
    }
    const std::string error = string_of(root, "error");
    if (!error.empty() || flag_of(root, "offline"))
    {
        page->load = flag_of(root, "offline") ? model::Load::offline : model::Load::failed;
        page->error = error;
        cJSON_Delete(root);
        return true;
    }
    const cJSON *item = nullptr;
    cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(root, "items"))
    {
        if (!cJSON_IsObject(item))
            continue;
        RawRow row;
        row.id = string_of(item, "id");
        row.channel_id = string_of(item, "channel_id");
        row.title = string_of(item, "title");
        row.subtitle = string_of(item, "subtitle");
        row.overview = string_of(item, "overview");
        row.art_url = string_of(item, "art_url");
        row.duration_s = static_cast<long long>(number_of(item, "duration_sec"));
        row.resume_s = static_cast<long long>(number_of(item, "resume_sec"));
        row.watch_percent = static_cast<float>(number_of(item, "watch_percent", -1.0));
        row.folder = flag_of(item, "is_folder");
        row.live = flag_of(item, "is_live");
        row.upcoming = flag_of(item, "is_upcoming");
        row.is_short = flag_of(item, "is_short");
        row.played = flag_of(item, "played");
        row.interface_text = flag_of(item, "interface");
        row.views = string_of(item, "views");
        row.published = string_of(item, "published");
        row.channel_art = string_of(item, "channel_art_url");
        const cJSON *badge = nullptr;
        cJSON_ArrayForEach(badge, cJSON_GetObjectItemCaseSensitive(item, "badges"))
        {
            if (cJSON_IsString(badge) && badge->valuestring != nullptr)
                row.badges.emplace_back(badge->valuestring);
        }
        static constexpr const char *kFixed[] = {"id", "channel_id", "title", "subtitle", "overview", "art_url", "duration_sec",
                                                 "resume_sec", "watch_percent", "is_folder", "is_live", "is_upcoming", "is_short",
                                                 "played", "interface", "views", "published", "channel_art_url", "badges"};
        const cJSON *member = nullptr;
        cJSON_ArrayForEach(member, item)
        {
            if (member->string == nullptr ||
                std::any_of(std::begin(kFixed), std::end(kFixed), [&](const char *name) { return std::strcmp(name, member->string) == 0; }))
                continue;
            if (cJSON_IsString(member) && member->valuestring != nullptr)
                row.facts.emplace_back(member->string, member->valuestring);
            else if (cJSON_IsBool(member))
                row.facts.emplace_back(member->string, cJSON_IsTrue(member) ? "1" : "0");
            else if (cJSON_IsNumber(member))
            {
                char number[32];
                std::snprintf(number, sizeof number, "%.10g", member->valuedouble);
                row.facts.emplace_back(member->string, number);
            }
        }
        if (!row.id.empty())
            add_row(*page, row);
    }
    page->more = flag_of(root, "has_more");
    page->listed = string_of(root, "listed");
    page->load = model::Load::ready;
    cJSON_Delete(root);
    return true;
}

bool profile_from_json(const std::string &json, model::Profile *profile)
{
    cJSON *root = cJSON_ParseWithLength(json.data(), json.size());
    if (!cJSON_IsObject(root))
    {
        cJSON_Delete(root);
        return false;
    }
    profile->id = string_of(root, "id");
    profile->name = string_of(root, "name");
    profile->guest = !cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(root, "guest"));
    profile->signed_in = flag_of(root, "signed_in");
    if (profile->id.empty())
        profile->id = "guest";
    cJSON_Delete(root);
    return true;
}

} // namespace afterglow
