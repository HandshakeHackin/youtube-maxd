// ProsperoTube Afterglow - what the search screen and the results screen share.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The catalogs the two screens read, the search filters as the provider names
// them, and two small helpers. docs/afterglow/requests/A.md says what each
// catalog is expected to answer and what the data bridge has to add for it.

#pragma once

#include "afterglow/screen.hpp"

#include <string>
#include <string_view>

namespace afterglow::search
{

// The profile's recent searches: rows "search-history:run:<serial>" whose
// title is the query, a row "search-history:clear:<scope>" that forgets them
// all, and "search-history:forget:<serial>" that forgets one and answers with
// what is left.
inline constexpr const char *kHistory = "search-history";
inline constexpr std::string_view kHistoryRun = "search-history:run:";
inline constexpr std::string_view kHistoryForget = "search-history:forget:";
inline constexpr std::string_view kHistoryClear = "search-history:clear:";

// The text without the spaces around it.
inline std::string trimmed(std::string_view text)
{
    while (!text.empty() && text.front() == ' ')
        text.remove_prefix(1);
    while (!text.empty() && text.back() == ' ')
        text.remove_suffix(1);
    return std::string(text);
}

// Completions for the text typed so far: YouTube's own, through the service.
// A row's title is the completion; nothing is searched or kept by asking.
inline std::string suggest_catalog(std::string_view text)
{
    return "search-suggest:" + std::string(text);
}

// The best matches so far: the first videos of a search that is not kept in
// the profile's history. With nothing typed it is what is popular now.
inline std::string preview_catalog(std::string_view text)
{
    return text.empty() ? std::string("section:trending") : "search-preview:" + std::string(text);
}

// The filters of a search, each an index into the provider's own lists
// (g_search_dates, g_search_durations, g_search_types, g_search_orders) and
// three of its feature switches.
struct Filters
{
    int date = 0;     // any, today, week, month, year
    int duration = 0; // any, short, medium, long
    int type = 0;     // all, video, channel, playlist, movie, shorts
    int order = 0;    // relevance, view_count
    bool uhd = false;      // feature "4k"
    bool captions = false; // feature "subtitles"
    bool live = false;     // feature "live"

    bool any() const
    {
        return date != 0 || duration != 0 || type != 0 || order != 0 || uhd || captions || live;
    }
    // "date=month,duration=long,type=video,features=4k+subtitles+live,order=view_count";
    // only what is set, in this order; "" for a search without filters.
    std::string options() const
    {
        static constexpr const char *kDates[] = {"any", "today", "week", "month", "year"};
        static constexpr const char *kDurations[] = {"any", "short", "medium", "long"};
        static constexpr const char *kTypes[] = {"all", "video", "channel", "playlist", "movie", "shorts"};
        std::string out;
        const auto add = [&](const char *name, std::string_view value)
        {
            if (!out.empty())
                out += ',';
            out += name;
            out += '=';
            out += value;
        };
        if (date > 0 && date < 5)
            add("date", kDates[date]);
        if (duration > 0 && duration < 4)
            add("duration", kDurations[duration]);
        if (type > 0 && type < 6)
            add("type", kTypes[type]);
        if (uhd || captions || live)
        {
            std::string features;
            for (const char *feature : {uhd ? "4k" : "", captions ? "subtitles" : "", live ? "live" : ""})
            {
                if (*feature == '\0')
                    continue;
                if (!features.empty())
                    features += '+';
                features += feature;
            }
            add("features", features);
        }
        if (order == 1)
            add("order", "view_count");
        return out;
    }
};

// The submitted search: "search:<query>", or "search-with:<options>:<query>"
// when a filter is set. The options hold no colon, so the first one after the
// prefix ends them and the query may hold anything.
inline std::string results_catalog(std::string_view query, const Filters &filters)
{
    if (!filters.any())
        return "search:" + std::string(query);
    return "search-with:" + filters.options() + ":" + std::string(query);
}

} // namespace afterglow::search
