// ProsperoTube Afterglow - Settings: the tiles, the grouping table, a row as a form row.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/screens/settings.hpp"

#include <cstdlib>
#include <cstring>

namespace afterglow::settings
{

namespace
{

constexpr ScreenId kOwn = ScreenId::settings;

// The tiles of the approved board, in its order. "more" is shown only while
// the main list holds a row no rule below knows.
constexpr Tile kTiles[] = {
    {"player", "Player", Icon::play, kOwn, "", "", {"settings:extra:player", nullptr},
     {"settings:quality", "settings:frame-rate", "settings:codec"}, "value", ""},
    {"captions", "Captions", Icon::captions, kOwn, "", "", {"settings:extra:captions", nullptr},
     {"settings:languages", "settings:extra:captions", nullptr}, "value", ""},
    {"audio", "Audio", Icon::audio, kOwn, "", "", {"settings:extra:audio", nullptr}, {"settings:extra:audio", nullptr, nullptr}, "value", ""},
    {"content", "Content & language", Icon::globe, kOwn, "", "", {nullptr, nullptr},
     {"settings:content-language", "settings:content-country", nullptr}, "value", ""},
    {"interface", "Interface", Icon::grid, kOwn, "", "", {"settings:ui", nullptr}, {"settings:ui", nullptr, nullptr}, "value", ""},
    {"sponsorblock", "SponsorBlock", Icon::skip, kOwn, "", "", {"settings:sb", nullptr}, {"settings:sb", nullptr, nullptr}, "value", ""},
    {"dearrow", "DeArrow", Icon::title, kOwn, "", "", {"settings:dearrow", nullptr}, {"settings:dearrow", nullptr, nullptr}, "value", ""},
    {"accounts", "Accounts", Icon::users, ScreenId::accounts, "Profiles and accounts", "Who is watching, signing in to YouTube and the accounts on this PS5", {"settings:accounts", nullptr},
     {"settings:accounts", nullptr, nullptr}, "value", ""},
    {"phone", "Phone & casting", Icon::phone, ScreenId::link_phone, "Link a phone", "Play from the YouTube app on a phone to this PS5", {nullptr, nullptr},
     {"settings:remote", nullptr, nullptr}, "value", ""},
    {"backup", "Backup", Icon::backup, kOwn, "", "", {"settings:backup", nullptr}, {"settings:backup", nullptr, nullptr}, "value", ""},
    {"updates", "Updates", Icon::update, kOwn, "", "", {"settings:about", nullptr}, {"settings:about", nullptr, nullptr}, "update",
     "settings:about"},
    {"about", "About", Icon::info, kOwn, "", "", {"settings:about", nullptr}, {"settings:about", nullptr, nullptr}, "value", ""},
    {"more", "More", Icon::more, kOwn, "", "", {nullptr, nullptr}, {nullptr, nullptr, nullptr}, "value", ""},
};
constexpr int kTileCount = static_cast<int>(sizeof kTiles / sizeof kTiles[0]);

// Rows of the main list that are another screen's (package F reads them): the tile links there.
constexpr const char *kLinked[] = {"settings:remote"};

// Where a row is shown: the start of its id names the tile and the heading.
// A prefix that ends in ':' or '-' is the start of an id; any other is a whole
// id or what stands before its next ':'. The longest prefix wins; the order of
// the lines is the order of the rows on the page. `catalog` holds a rule to the
// rows of one catalog, for ids that several lists use.
struct Rule
{
    const char *prefix;
    const char *tile;
    const char *section; // English; shown through tr()
    const char *catalog = nullptr;
};
constexpr Rule kRules[] = {
    // ---- Player ----
    {"settings:quality", "player", "Picture"},
    {"settings:frame-rate", "player", "Picture"},
    {"settings:codec", "player", "Picture"},
    {"settings:remember-quality", "player", "Picture"},
    {"settings:ask-stream", "player", "Picture"},
    {"settings:video-buffer", "player", "Picture"},
    {"settings:error-fixer", "player", "Picture"},
    {"settings:playback-mode", "player", "Playback"},
    {"settings:autoplay", "player", "Playback"},
    {"settings:queue-respects-mode", "player", "Playback"},
    {"settings:use-section-playlist", "player", "Playback"},
    {"settings:loop-shorts", "player", "Playback"},
    {"settings:remember-short-position", "player", "Playback"},
    {"settings:player-suggestions", "player", "Playback"},
    {"settings:chapter-notification", "player", "Playback"},
    {"settings:sleep-timer", "player", "Playback"},
    {"settings:speed", "player", "Speed"},
    {"settings:speed-mode", "player", "Speed"},
    {"settings:speed-list", "player", "Speed"},
    {"settings:player-ok", "player", "Controls"},
    {"settings:player-hide", "player", "Controls"},
    {"settings:seek-interval", "player", "Controls"},
    {"settings:player-seek", "player", "Controls"},
    {"settings:seek-preview", "player", "Controls"},
    {"settings:keys-left-right", "player", "Controls"},
    {"settings:keys-up-down", "player", "Controls"},
    {"settings:player-exit", "player", "Controls"},
    {"settings:extra:buttons", "player", "Controls"},
    {"settings:button-", "player", "Controls"},
    {"settings:player-clock", "player", "On the screen"},
    {"settings:time-format", "player", "On the screen"},
    {"settings:time-display", "player", "On the screen"},
    {"settings:quality-info", "player", "On the screen"},
    {"settings:quality-bitrate", "player", "On the screen"},
    {"settings:player-chat", "player", "On the screen"},
    {"settings:dislikes", "player", "On the screen"},
    {"settings:extra:dimming", "player", "On the screen"},
    {"settings:dim-", "player", "On the screen"},
    {"settings:screen-off-", "player", "On the screen"},
    {"settings:video-rotate", "player", "Shape of the picture"},
    {"settings:video-mirror", "player", "Shape of the picture"},
    {"settings:video-aspect", "player", "Shape of the picture"},
    {"settings:pixel-ratio", "player", "Shape of the picture"},
    {"settings:video-zoom", "player", "Shape of the picture"},
    // ---- Captions ----
    {"settings:captions", "captions", "Captions"},
    {"settings:languages", "captions", "Captions"},
    {"settings:automatic", "captions", "Captions"},
    {"settings:more-subtitles", "captions", "Captions"},
    {"settings:caption-memory", "captions", "Captions"},
    {"settings:subtitle-size", "captions", "Appearance"},
    {"settings:subtitle-style", "captions", "Appearance"},
    {"settings:subtitle-position", "captions", "Appearance"},
    {"settings:subtitle-", "captions", "Appearance"},
    // ---- Audio ----
    {"settings:player-volume", "audio", "Sound"},
    {"settings:auto-volume", "audio", "Sound"},
    {"settings:player-mute", "audio", "Sound"},
    {"settings:audio-languages", "audio", "Sound"},
    {"settings:audio-delay", "audio", "Timing"},
    {"settings:audio-delay-enabled", "audio", "Timing"},
    // ---- Content & language ----
    {"settings:interface-language", "content", "Language and region"},
    {"settings:content-language", "content", "Language and region"},
    {"settings:content-country", "content", "Language and region"},
    {"settings:hide", "content", "What is shown"},
    {"settings:thumbnails", "content", "What is shown"},
    {"settings:channel-sort", "content", "What is shown"},
    {"settings:search", "content", "What is shown"},
    {"settings:history", "content", "History"},
    {"settings:search-history", "content", "History"},
    {"settings:clear-history", "content", "History"},
    {"settings:clear-queue", "content", "History"},
    // ---- Interface ----
    {"settings:interface", "interface", "Look"},
    {"settings:ui:scheme", "interface", "Look"},
    {"settings:ui:scale", "interface", "Look"},
    {"settings:ui:grid", "interface", "Cards"},
    {"settings:ui:title-lines", "interface", "Cards"},
    {"settings:ui:title-scroll", "interface", "Cards"},
    {"settings:ui:scroll-speed", "interface", "Cards"},
    {"settings:ui:subtitle-lines", "interface", "Cards"},
    {"settings:ui:rounded", "interface", "Cards"},
    {"settings:interface:sounds", "interface", "Sounds and vibration"},
    {"settings:interface:volume", "interface", "Sounds and vibration"},
    {"settings:interface:playback-sounds", "interface", "Sounds and vibration"},
    {"settings:interface:quiet-hours", "interface", "Sounds and vibration"},
    {"settings:interface:vibration", "interface", "Sounds and vibration"},
    {"settings:interface:reduce-motion", "interface", "Motion"},
    {"settings:interface:attract", "interface", "Motion"},
    {"settings:sections", "interface", "Menus"},
    {"settings:menu", "interface", "Menus"},
    {"settings:boot-section", "interface", "Menus"},
    {"settings:ui:exit", "interface", "Menus"},
    {"settings:ui:search-exit", "interface", "Menus"},
    // ---- SponsorBlock ----
    {"settings:sponsor", "sponsorblock", "SponsorBlock"},
    {"settings:sb:action:", "sponsorblock", "Categories"},
    {"settings:sb:minimum", "sponsorblock", "Skipping"},
    {"settings:sb:repeat", "sponsorblock", "Skipping"},
    {"settings:sb:exclusions", "sponsorblock", "Skipping"},
    {"settings:sb:colors", "sponsorblock", "Skipping"},
    {"settings:sb:paid", "sponsorblock", "Skipping"},
    {"settings:sb:alt", "sponsorblock", "Server"},
    {"settings:sb:server", "sponsorblock", "Server"},
    {"settings:sb:about", "sponsorblock", "About"},
    {"settings:sb:status", "sponsorblock", "About"},
    // ---- DeArrow ----
    {"settings:dearrow:titles", "dearrow", "DeArrow"},
    {"settings:dearrow:thumbnails", "dearrow", "DeArrow"},
    {"settings:dearrow", "dearrow", "About", "settings:dearrow"},
    {"settings:dearrow:about", "dearrow", "About"},
    {"settings:dearrow:status", "dearrow", "About"},
    // ---- Accounts ----
    {"settings:account-on-boot", "accounts", "On this PS5"},
    {"settings:separate-settings", "accounts", "On this PS5"},
    {"settings:lock:", "accounts", "Password of this profile"},
    {"account:notice", "accounts", "Password of this profile", "settings:accounts"},
    // ---- Backup ----
    {"account:notice:backup", "backup", "Backup", "settings:backup"},
    {"settings:backup:create", "backup", "Backup"},
    {"settings:backup:list", "backup", "Backup"},
    {"settings:backup:auto", "backup", "Backup"},
    {"settings:backup:import", "backup", "Backup"},
    {"account:notice:backup-accounts", "backup", "Backup", "settings:backup"},
    // ---- Updates ----
    {"settings:update:", "updates", "Updates"},
    {"description:update", "updates", "Updates"},
    // ---- About ----
    {"account:notice:version", "about", "About"},
    {"account:notice:author", "about", "About"},
    {"link:about", "about", "About"},
    {"link:feedback", "about", "About"},
    {"settings:about:credits", "about", "About"},
    {"settings:about:changelog", "about", "About"},
    {"settings:experimental", "about", "About"},
};

bool starts_with(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

bool matches(std::string_view id, std::string_view prefix)
{
    if (!starts_with(id, prefix))
        return false;
    const char last = prefix.empty() ? ':' : prefix.back();
    return last == ':' || last == '-' || id.size() == prefix.size() || id[prefix.size()] == ':';
}

Kind kind_of(const model::Row &row)
{
    static constexpr struct
    {
        const char *name;
        Kind kind;
    } kKinds[] = {{"switch", Kind::toggle}, {"list", Kind::list},     {"slider", Kind::slider}, {"cycle", Kind::cycle},
                  {"choice", Kind::choice}, {"action", Kind::action}, {"text", Kind::text},     {"page", Kind::page},
                  {"link", Kind::link},     {"info", Kind::info},     {"order", Kind::order},   {"reset", Kind::reset}};
    const std::string &name = row.fact("kind");
    for (const auto &entry : kKinds)
    {
        if (name == entry.name)
            return entry.kind;
    }
    // A row that does not say what it is (a provider older than the facts): a
    // notice says something, a marked row is a choice, a text to read leaves for
    // the reader, and anything else opens as a page, as the classic browser opens it.
    if (starts_with(row.id, "account:notice"))
        return Kind::info;
    if (row.has("selected"))
        return Kind::choice;
    if (starts_with(row.id, "link:") || starts_with(row.id, "description:") || starts_with(row.id, "settings:about:"))
        return Kind::link;
    return Kind::page;
}

const model::Row *find(const std::vector<model::Row> &rows, std::string_view id)
{
    for (const model::Row &row : rows)
    {
        if (row.id == id)
            return &row;
    }
    return nullptr;
}

} // namespace

std::span<const Tile> tiles()
{
    return kTiles;
}

int tile_index(std::string_view key)
{
    for (int i = 0; i < kTileCount; ++i)
    {
        if (key == kTiles[i].key)
            return i;
    }
    return -1;
}

bool is_container(std::string_view id)
{
    for (const Tile &tile : kTiles)
    {
        for (const char *catalog : tile.reads)
        {
            if (catalog != nullptr && id == catalog)
                return true;
        }
    }
    for (const char *linked : kLinked)
    {
        if (id == linked)
            return true;
    }
    return false;
}

Place place_of(std::string_view catalog, std::string_view id)
{
    Place place;
    // "settings:reset:<tile>" puts that tile back to its defaults.
    constexpr std::string_view kReset = "settings:reset:";
    if (starts_with(id, kReset))
    {
        place.tile = tile_index(id.substr(kReset.size()));
        place.rank = 0;
        return place;
    }
    std::size_t best = 0;
    bool held = false;
    for (std::size_t i = 0; i < sizeof kRules / sizeof kRules[0]; ++i)
    {
        const Rule &rule = kRules[i];
        if (rule.catalog != nullptr && catalog != rule.catalog)
            continue;
        const std::size_t length = std::strlen(rule.prefix);
        if (!matches(id, rule.prefix))
            continue;
        if (place.tile >= 0 && (length < best || (length == best && (held || rule.catalog == nullptr))))
            continue;
        best = length;
        held = rule.catalog != nullptr;
        place.tile = tile_index(rule.tile);
        place.section = rule.section;
        place.rank = static_cast<int>(i) + 1;
    }
    return place;
}

int fallback_tile(std::string_view catalog)
{
    if (catalog == kMain)
        return kTileCount - 1;
    int found = -1;
    for (int i = 0; i < kTileCount; ++i)
    {
        for (const char *read : kTiles[i].reads)
        {
            if (read != nullptr && catalog == read)
                found = i; // of two tiles that read one catalog, the later one keeps what neither knows
        }
    }
    return found;
}

Item item_of(const model::Row &row, std::string_view catalog)
{
    Item item;
    item.row = row;
    item.catalog = std::string(catalog);
    item.kind = kind_of(row);
    item.value = row.fact("value");
    item.label = row.fact("label");
    if (item.label.empty())
    {
        // Without a name of its own the row's title is "Name: value", as the classic browser shows it.
        item.label = row.title;
        const std::size_t colon = item.label.find(": ");
        if (!item.value.empty() && colon != std::string::npos && colon > 0)
            item.label.resize(colon);
    }
    item.detail = row.subtitle;
    item.section = row.fact("section");
    item.on = row.has("on") || (item.kind == Kind::choice && row.has("selected"));
    item.danger = row.has("danger");
    item.attention = row.has("attention");
    item.disabled = row.has("disabled");
    item.index = static_cast<int>(number_of(row.fact("index"), -1.0f));
    item.count = static_cast<int>(number_of(row.fact("count"), 0.0f));
    return item;
}

std::string tile_summary(const Tile &tile, const std::vector<model::Row> &main)
{
    std::string line;
    for (const char *id : tile.summary)
    {
        if (id == nullptr)
            continue;
        const model::Row *row = find(main, id);
        if (row == nullptr)
            continue;
        const std::string &part = row->fact(tile.fact);
        if (part.empty())
            continue;
        if (!line.empty())
            line += " \xC2\xB7 ";
        line += part;
    }
    return line;
}

bool tile_attention(const Tile &tile, const std::vector<model::Row> &main)
{
    const model::Row *row = tile.attention[0] != 0 ? find(main, tile.attention) : nullptr;
    return row != nullptr && row->has("attention");
}

bool tile_has_page(const Tile &tile, const std::vector<model::Row> &main)
{
    if (tile.link == kOwn || tile.reads[0] != nullptr)
        return true;
    const int index = tile_index(tile.key);
    for (const model::Row &row : main)
    {
        if (!is_container(row.id) && place_of(kMain, row.id).tile == index && row.fact("kind") != "reset")
            return true;
    }
    return false;
}

const model::Row *tile_reset(const Tile &tile, const std::vector<model::Row> &main)
{
    const model::Row *row = find(main, std::string("settings:reset:") + tile.key);
    return row != nullptr && row->fact("kind") == "reset" ? row : nullptr;
}

float number_of(const std::string &text, float fallback)
{
    if (text.empty())
        return fallback;
    char *end = nullptr;
    const float value = std::strtof(text.c_str(), &end);
    return end == text.c_str() ? fallback : value;
}

Color colour_of(const std::string &hex, Color fallback)
{
    const char *text = hex.c_str();
    if (*text == '#')
        ++text;
    if (std::strlen(text) != 6)
        return fallback;
    char *end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 16);
    return *end != 0 ? fallback : Color::rgb(static_cast<std::uint32_t>(value));
}

} // namespace afterglow::settings
