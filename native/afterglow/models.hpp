// ProsperoTube Afterglow - view models: what the screens draw.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Plain values, filled by the data layer (data.hpp) and never by a screen. A
// text that comes from YouTube is kept as YouTube wrote it; the data layer has
// already removed what no face draws. docs/afterglow/ARCHITECTURE.md lists,
// field by field, which answer of the provider or the service fills each one.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace afterglow::model
{

// How far a request is.
enum class Load : std::uint8_t
{
    idle,    // not asked for yet
    loading, // asked, no answer yet
    ready,   // answered (possibly with nothing: see the value)
    failed,  // answered with an error: Result::error says why
    offline, // could not reach the service at all
};

enum Badge : unsigned
{
    kBadgeLive = 1u,
    kBadgeUpcoming = 2u,
    kBadgeNew = 4u,
    kBadge4K = 8u,
    kBadgeHdr = 16u,
    kBadgeCaptions = 32u,
    kBadgeShort = 64u,
    kBadgeMembers = 128u,
};

// One video as a card, a hero or a list row shows it.
struct Video
{
    std::string id;         // the row to open through the provider ("dQw4w9WgXcQ", "queue:...")
    std::string video_id;   // the 11-character YouTube id, "" for a row that is not a plain video
    std::string title;
    std::string channel;    // channel name
    std::string channel_id; // "UC..." or ""
    std::string views;      // as YouTube words it ("1.2M views"), "" when unknown
    std::string age;        // as YouTube words it ("3 weeks ago"), "" when unknown
    std::string overview;   // description snippet, "" when unknown
    std::string thumb;      // picture address, "" for none
    std::string avatar;     // the channel's picture address, "" for none
    int duration_s = 0;     // 0 for live and unknown
    float progress = 0.0f;  // 0..1 watched (local resume position, else YouTube's own mark)
    bool watched = false;
    unsigned badges = 0;    // Badge bits
};

struct Channel
{
    std::string id;   // "UC..."
    std::string row;  // the row to open through the provider ("channel:UC...")
    std::string name;
    std::string handle;      // "@name" or ""
    std::string subscribers; // as YouTube words it, "" when unknown
    std::string avatar;
    std::string banner;
    bool subscribed = false;
    int unseen = 0; // new videos since the profile last looked, 0 when unknown
};

// A topic tile of Guest Home and Discover.
struct Topic
{
    std::string id;    // the catalog to open ("section:gaming")
    std::string title; // an interface text, already in the profile's language
    std::string icon;  // a name of design.hpp's Icon ("gamepad"), "" for the default
};

enum class ShelfKind : std::uint8_t
{
    videos,
    topics,
    channels,
};

// One row of Home (and of any screen made of rows).
struct Shelf
{
    std::string id;      // stable within its screen: "continue", "recommended", "subscriptions", "topics", "trending"
    std::string title;   // an interface text, already in the profile's language
    std::string catalog; // where "see all" leads: a provider catalog id, "" for none
    ShelfKind kind = ShelfKind::videos;
    Load load = Load::idle;
    std::string error; // for Load::failed: the provider's message
    std::vector<Video> videos;
    std::vector<Topic> topics;
    std::vector<Channel> channels;
    bool more = false; // the catalog has further pages
};

struct Profile
{
    std::string id;   // "guest" or the account profile's id
    std::string name; // shown in the rail
    bool guest = true;
    bool signed_in = false; // a YouTube account is signed in on this profile
};

// Everything Home shows.
struct Home
{
    Profile profile;
    std::vector<Shelf> shelves;  // in display order; Guest: topics, trending. Signed in: continue, recommended, subscriptions
    std::vector<Channel> pinned; // the rail's channels
    bool invite_sign_in = false; // Guest: the invitation card is shown
    Load load = Load::idle;      // of the whole page: offline when the service cannot be reached
    std::string error;
};

} // namespace afterglow::model
