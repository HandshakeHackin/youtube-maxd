// ProsperoTube Afterglow - catalog rows as they arrive, and what they become.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A RawRow is one row of a provider catalog: the fields of evo_provider_item_t
// plus the facts the provider keeps beside a row (native/tube_item_facts.h).
// ProviderSource fills it from the provider, FixtureSource from JSON; add_row()
// is the one place that decides what a row is. The JSON form uses the names of
// the service's own rows, so a fixture can be a recorded answer.

#pragma once

#include "afterglow/data.hpp"

namespace afterglow
{

struct RawRow
{
    std::string id;         // "id"
    std::string channel_id; // "channel_id"
    std::string title;      // "title"
    std::string subtitle;   // "subtitle": a video's channel name, a folder's detail
    std::string overview;   // "overview"
    std::string art_url;    // "art_url"
    long long duration_s = 0; // "duration_sec"
    long long resume_s = 0;   // "resume_sec": the profile's local resume position
    float watch_percent = -1.0f; // "watch_percent": YouTube's own mark, -1 unknown
    bool folder = false;      // "is_folder"
    bool live = false;        // "is_live"
    bool upcoming = false;    // "is_upcoming"
    bool is_short = false;    // "is_short"
    bool played = false;      // "played"
    bool interface_text = false; // "interface": the provider wrote the row
    // Facts, "" or empty where the provider has none.
    std::string views;       // "views": "1.2M views"
    std::string published;   // "published": "3 weeks ago"
    std::string channel_art; // "channel_art_url"
    std::vector<std::string> badges; // "badges": "new", "4k", "hdr", "cc", "members"
    // Any other named value of the row: in JSON, every further member that is
    // a string, a number or a boolean (true is "1", false is "0").
    std::vector<std::pair<std::string, std::string>> facts;
};

// Sorts a row into the page: a video, a channel, a topic or a plain row. Every
// row is also kept in page.rows.
void add_row(Page &page, const RawRow &row);

// {"items":[row...],"has_more":false} or {"error":"...","offline":true}.
// False when the text is no such document; the page then has Load::failed.
bool page_from_json(const std::string &json, Page *page);
// {"id":"guest","name":"","guest":true,"signed_in":false}
bool profile_from_json(const std::string &json, model::Profile *profile);

// The topic icon of a catalog id ("section:gaming" -> "gamepad").
const char *topic_icon(std::string_view catalog);
// True for an 11-character YouTube video id.
bool is_video_id(std::string_view id);

} // namespace afterglow
