// ProsperoTube Afterglow - Settings: the screen itself, for the files it is written in.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
//   settings.cpp          what the frame asks of the screen; reading and laying out a page
//   settings_changes.cpp  a change on its way to the provider, confirmed or refused
//   settings_input.cpp    the controller: tiles, rows, the move mode, typing
//   settings_view.cpp     what moves, the hints, the tiles and a page as drawn
// settings.hpp has what these share with the table (settings_model.cpp) and the
// pieces (settings_draw.cpp).

#pragma once

#include "afterglow/screens/settings.hpp"

#include "ui/components/keyboard.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <map>
#include <memory>

namespace afterglow::settings
{

constexpr float kView = kFormBottom - kFormTop;
constexpr std::string_view kTilePrefix = "tile:";
constexpr std::size_t kWaiting = 12; // changes that may wait for the provider at once
constexpr int kPagesAtMost = 16;

inline bool starts_with(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

inline std::uint64_t mixed(std::uint64_t hash, std::string_view text)
{
    for (const char c : text)
    {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ull;
    }
    hash ^= 0xffu;
    hash *= 1099511628211ull;
    return hash;
}

// A number that changes when anything a page shows of these rows changes.
inline std::uint64_t mark_of(const std::vector<model::Row> &rows, std::uint64_t hash = 1469598103934665603ull)
{
    for (const model::Row &row : rows)
    {
        hash = mixed(hash, row.id);
        hash = mixed(hash, row.title);
        hash = mixed(hash, row.subtitle);
        hash = mixed(hash, row.overview);
        for (const auto &fact : row.facts)
        {
            hash = mixed(hash, fact.first);
            hash = mixed(hash, fact.second);
        }
    }
    return hash;
}

inline void set_fact(model::Row &row, std::string_view name, std::string value)
{
    for (auto &fact : row.facts)
    {
        if (fact.first == name)
        {
            fact.second = std::move(value);
            return;
        }
    }
    row.facts.emplace_back(std::string(name), std::move(value));
}

// True when an answer is the list these rows are of, as it now stands.
inline bool same_list(const std::vector<model::Row> &rows, const std::vector<model::Row> &answer)
{
    if (rows.empty() || answer.empty())
        return false;
    std::size_t shared = 0;
    for (const model::Row &row : rows)
    {
        shared += std::any_of(answer.begin(), answer.end(), [&](const model::Row &other) { return other.id == row.id; }) ? 1u : 0u;
    }
    return shared * 10 >= rows.size() * 6 && shared * 10 >= answer.size() * 6;
}

inline std::string filled(const char *pattern, std::string_view value)
{
    std::string text = pattern;
    const std::size_t at = text.find("{0}");
    if (at != std::string::npos)
        text.replace(at, 3, value);
    return text;
}

inline std::vector<std::string> split(std::string_view text, char separator)
{
    std::vector<std::string> parts;
    while (!text.empty())
    {
        const std::size_t at = text.find(separator);
        if (at != 0)
            parts.emplace_back(text.substr(0, at));
        if (at == std::string_view::npos)
            break;
        text.remove_prefix(at + 1);
    }
    return parts;
}

inline void drop_last_character(std::string &text)
{
    while (!text.empty())
    {
        const unsigned char c = static_cast<unsigned char>(text.back());
        text.pop_back();
        if ((c & 0xC0u) != 0x80u)
            break;
    }
}

// Draws what follows at a share of its strength.
struct Faded
{
    gfx::DrawList &l;
    Faded(G &g, float amount) : l(g.l)
    {
        l.push_opacity(tween::clamp01(amount));
    }
    ~Faded()
    {
        l.pop_opacity();
    }
    Faded(const Faded &) = delete;
    Faded &operator=(const Faded &) = delete;
};

inline const char *load_name(model::Load load)
{
    switch (load)
    {
    case model::Load::ready:
        return "ready";
    case model::Load::failed:
        return "failed";
    case model::Load::offline:
        return "offline";
    default:
        return "loading";
    }
}

class Settings final : public Screen
{
  public:
    Settings() : alive_(std::make_shared<int>(0)) {}

    ScreenId id() const override;
    void enter(const Route &route, UpdateContext &context, bool restored) override;
    void update(const InputFrame &input, UpdateContext &context) override;
    void draw(const DrawContext &context, const Phase &phase) const override;
    std::string title() const override;
    Header header() const override { return home_ ? Header::title : Header::clock; }
    std::span<const ui::Hint> hints() const override;
    FocusInfo focus() const override;
    std::vector<std::pair<std::string, std::string>> focusable() const override;
    void snapshot(Snapshot &out) const override;

  private:
    // A catalog the page shows rows of.
    struct Source
    {
        std::string catalog;
        model::Load load = model::Load::idle;
        std::string error;
        std::vector<model::Row> rows; // as shown: the data layer's, or newer ones a change brought back
        std::uint64_t mark = 0;       // of the data layer's rows last taken over
        int pages = 1;                // how many pages are asked for
        bool more = false;            // a further page exists beyond those shown
    };

    struct Line
    {
        int item = -1; // -1: a heading
        std::string heading;
        float y = 0.0f; // from the top of the page's content
        float h = 0.0f;
    };

    // One change on its way to the provider.
    struct Change
    {
        std::string item;    // the row it is shown on ("" for a page's defaults)
        std::string catalog; // ... and the list that row is of
        std::string action;  // the row the provider is asked to carry out
        std::string text;    // typed answer, erased once the provider takes it
        bool typed = false;
        std::string value;   // what the row shows meanwhile
        std::string done;    // what to say when it worked, "" for nothing
        bool sets_value = false;
        bool on = false;
        bool sets_on = false;
        int index = -1;
        model::Row choice; // the choice that was taken: the previews read its facts
        bool has_choice = false;
        bool absolute = false; // names its result: a later one for the same row replaces it while it waits
        bool leave = false;    // a choice on a list page: the page closes once it is confirmed
        bool moved = false;    // the row changed its place
        std::vector<model::Row> before; // moved: the list as it was
        unsigned ticket = 0;
    };

    struct Shown
    {
        std::string value;
        bool on = false;
        int index = -1;
        bool waiting = false;
        const model::Row *choice = nullptr;
    };

    struct Motion
    {
        tween::Bounce on;
        tween::Spring fill;
        tween::Spring waiting;
        ui::Pulse applied;
        ui::Pulse refused;
        ui::Pulse nudge;
        float direction = 0.0f;
        bool placed = false;
    };

    struct Choices
    {
        bool ready = false;
        std::vector<Item> rows;
    };

    static constexpr Rect kNoticeButton{kLeft + 136.0f, 470.0f, 300.0f, 60.0f};

    // ---- reading --------------------------------------------------------------------

    void read(Data &data);
    void read_choices(Data &data);
    void add_source(std::string_view catalog);
    Source *source_of(std::string_view catalog);
    void rebuild();
    void place_focus();
    static std::string key_of(const Item &item);
    static std::string name_of(const Item &item);
    const Item *focused() const;
    model::Load page_load() const;
    const std::string &page_error() const;

    // ---- changes --------------------------------------------------------------------

    Shown shown(const Item &item) const;
    bool ask(Change change, UpdateContext &context);
    void ask_first(Change change, std::string_view title, std::string_view body, std::string_view yes, UpdateContext &context);
    void settle(UpdateContext &context);
    void confirmed(const Change &done, const std::vector<model::Row> &answer, UpdateContext &context);
    void patch(const Change &done, const std::vector<model::Row> &answer);
    void refused(const Change &done, const std::string &error, UpdateContext &context);

    // ---- input: the tiles -----------------------------------------------------------

    void input_home(const InputFrame &input, UpdateContext &context);
    void ask_reset(const Item &reset, std::string_view name, UpdateContext &context);
    void again(UpdateContext &context);
    void bump(const InputFrame &input, UpdateContext &context);

    // ---- input: a page --------------------------------------------------------------

    void input_page(const InputFrame &input, UpdateContext &context);
    void step_back(UpdateContext &context);
    void go(int direction, int rows, const InputFrame &input, UpdateContext &context);
    void reveal();
    std::string below(std::string_view name) const;
    void refuse(const Item &item, const InputFrame &input, UpdateContext &context);
    void flip(const Item &item, UpdateContext &context);
    void step(const Item &item, int direction, const InputFrame &input, UpdateContext &context);
    void activate(const Item &item, const InputFrame &input, UpdateContext &context);

    // ---- input: a row held in the move mode -----------------------------------------

    void input_move(const Item &item, const InputFrame &input, UpdateContext &context);
    static bool can_move(const Item &item, int direction);
    void move(const Item &item, int direction, const InputFrame &input, UpdateContext &context);
    void show_or_hide(const Item &item, const InputFrame &input, UpdateContext &context);

    // ---- what moves -----------------------------------------------------------------

    void advance(UpdateContext &context);
    void preview(bool reduced, float dt);
    void make_hints();

    // ---- drawing --------------------------------------------------------------------

    float arrive(int index, float step = 0.05f, float duration = 0.45f) const;
    float settled() const;
    float arrive_shown(int index, float step = 0.05f, float duration = 0.45f) const;
    void draw_home(G &g) const;
    void draw_page(G &g, const DrawContext &context) const;
    float shake_of(const Item &item, const G &g) const;


    Route route_;
    model::Profile profile_;
    bool home_ = true;
    int tile_ = -1;                 // a tile's page: its place among tiles()
    std::vector<std::string> path_; // the names of the pages from Settings down to this one
    std::vector<Source> sources_;   // the first is the main list on the tiles and on a tile's page
    std::vector<int> shown_;        // the tiles on the page, as places among tiles()
    std::vector<Item> items_;
    std::vector<Line> lines_;
    Item reset_; // the row that puts this page back to its defaults
    bool has_reset_ = false;
    float content_ = 0.0f;
    int unknown_ = 0;      // rows of the main list no rule knows: the "More" tile
    int focus_ = 0;        // the tiles: a place among shown_; a page: a place among lines_
    std::string focus_id_; // the row under the focus, so it is found again when the rows change
    bool placed_ = false;

    std::deque<Change> changes_;
    Change asking_; // the change the dialog is asking about
    std::map<std::string, Choices> choices_;
    std::map<std::string, Motion> motion_;
    int steps_ = 0;
    bool grabbed_ = false;
    bool typing_ = false;
    float leave_ = 0.0f;
    int applied_ = 0;
    int refused_ = 0;
    std::string last_applied_;
    std::string last_refused_;
    std::string said_row_; // the row whose change the provider refused ...
    std::string said_;     // ... and what it said

    float age_ = 0.0f;
    float shown_age_ = 0.0f; // seconds since the page had an answer to show
    ui::Scroller scroll_;
    tween::Spring tiles_scroll_;
    tween::Spring plate_; // the focus plate, down the page's content
    tween::Spring grab_;
    ui::SpringRect ring_;
    ui::Pulse edge_;
    PreviewKind sticky_ = PreviewKind::none;
    PreviewState preview_;
    bool preview_placed_ = false;
    tween::Spring look_scale_, look_position_, look_plate_, look_interface_, look_grid_;
    std::array<ui::Hint, 6> hints_{};
    std::size_t hint_count_ = 0;
    std::shared_ptr<int> alive_;
};

} // namespace afterglow::settings
