// ProsperoTube Afterglow - the channel page: what its two source files share.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// channel.cpp reads the page, takes the input and moves things; channel_draw.cpp
// draws it. Nothing outside these two files includes this header.

#pragma once

#include "afterglow/screens/channel_family.hpp"

namespace afterglow::channel_page
{

using namespace family;

constexpr Rect kBanner{kLeft, kTop, kRight - kLeft, 224.0f};
constexpr float kAvatarX = kLeft + 92.0f;
constexpr float kAvatarY = 300.0f;
constexpr float kNameX = kLeft + 186.0f;
constexpr float kActionY = 310.0f;
constexpr float kActionH = 60.0f;
constexpr float kTabsY = 414.0f;       // the tab bar at rest
constexpr float kTabsPinnedY = 150.0f; // ... and while a grid is scrolled under it
constexpr float kTabsH = 56.0f;
constexpr float kContentY = 512.0f;
constexpr float kRowRestY = 240.0f; // where the focused row of a scrolled grid rests
constexpr Rect kFeatured{kLeft, kContentY, 624.0f, 351.0f};
constexpr float kFeaturedTextX = kLeft + 624.0f + 44.0f;
constexpr float kFeaturedTextW = 400.0f;
constexpr float kLatestX = 1292.0f;
constexpr float kLatestY = 566.0f;
constexpr float kLatestPitch = 104.0f;
constexpr int kLatestShown = 3;
constexpr float kSortRow = 56.0f;

// The sections, orders and notification levels are the provider's words; these are their names on screen ("" for one it does not know).
const char *section_label(std::string_view name);
const char *order_label(std::string_view name);
const char *level_label(std::string_view level);

enum class Zone : std::uint8_t
{
    actions, // Subscribe, the bell, search
    tabs,
    sort,    // the order of the section
    content, // the section's videos
    trouble, // the page did not arrive: one way on
};

enum class Button : std::uint8_t
{
    subscribe,
    sign_in, // a profile without an account: the invitation stands where Subscribe would
    notify,
    search,
};

// What the account is being asked to do.
enum class Doing : std::uint8_t
{
    nothing,
    subscribe,
    unsubscribe,
    level,
};

class Channel final : public Screen
{
  public:
    Channel();

    ScreenId id() const override
    {
        return ScreenId::channel;
    }
    void enter(const Route &route, UpdateContext &context, bool restored) override;
    void update(const InputFrame &input, UpdateContext &context) override;
    void draw(const DrawContext &context, const Phase &phase) const override;
    std::string title() const override
    {
        // REQUEST: the banner is this page's header, but the frame writes the
        // route's title when a screen has none of its own. A space keeps the
        // header empty until a screen can say that it has no title.
        return " ";
    }
    std::span<const ui::Hint> hints() const override;
    FocusInfo focus() const override;
    std::vector<std::pair<std::string, std::string>> focusable() const override;
    void snapshot(Snapshot &out) const override;

  private:
    // What the page knows about the channel itself.
    struct Head
    {
        bool known = false;
        std::string name;
        std::string handle;
        std::string subscribers;
        std::string videos; // "184 videos", as YouTube words it
        std::string avatar;
        std::string banner;
        std::string level; // the notification level: all, personalized, none
        std::string tab;   // the section the page's own catalog is
        bool subscribed = false;
        bool can_search = true;
    };

    // One thing of a section: a video, or a playlist that opens as a grid.
    struct Cell
    {
        bool video = true;
        model::Video v;
        std::string id; // a playlist: the catalog to open
        std::string title;
        std::string thumb;
        std::string count; // "24 videos"
        std::string note;  // "Updated last week"
    };

    struct Tab
    {
        std::string name;  // the provider's: home, videos, shorts, live, playlists
        std::string label; // in the profile's language
        Feed feed;
        unsigned seen = 0; // the feed's version the cells were made from
        std::vector<Cell> cells;
        std::vector<std::string> sorts; // the orders the provider offers for this section
        int sort = -1;                  // the one in force, an index into sorts
        bool chosen = false;            // the player chose it: the catalog names the order
        int focus = 0;                  // Home: 0 is the featured video, 1 and up the latest uploads
        int list_focus = 1;             // Home: where the focus was in the list
        tween::Spring list_scroll;      // Home: the latest uploads

        bool home() const
        {
            return name == "home";
        }
    };

    struct Slot
    {
        Button button;
        Rect rect{};
        std::string label; // "" for the round search button
    };

    bool has_tab() const
    {
        return active_ >= 0 && active_ < static_cast<int>(tabs_.size());
    }
    const Tab &tab() const
    {
        return tabs_[static_cast<std::size_t>(active_)];
    }
    Tab &tab()
    {
        return tabs_[static_cast<std::size_t>(active_)];
    }
    static bool tab_trouble(const Tab &tab)
    {
        return tab.feed.load == model::Load::failed || tab.feed.load == model::Load::offline;
    }
    // True when the section has something the focus can go to.
    static bool can_focus(const Tab &tab)
    {
        return !tab.cells.empty() || tab_trouble(tab);
    }
    const Cell *focused_cell() const;
    std::string catalog_of(const Tab &tab) const;

    // ---- channel.cpp: data ----
    void read(UpdateContext &context);
    void read_head();
    void read_cells(Tab &tab);
    // ---- the account ----
    void start(Doing doing, const std::string &row, UpdateContext &context);
    void finish_action(UpdateContext &context);
    void press_button(const Slot &slot, const InputFrame &input, UpdateContext &context);
    // ---- input ----
    void search(const InputFrame &input, UpdateContext &context);
    void retry(UpdateContext &context);
    void go(Zone zone, UpdateContext &context);
    void turn(int direction, const InputFrame &input, UpdateContext &context);
    void handle(const InputFrame &input, UpdateContext &context);
    void handle_actions(const InputFrame &input, UpdateContext &context);
    void handle_tabs(const InputFrame &input, UpdateContext &context);
    void handle_sort(const InputFrame &input, UpdateContext &context);
    void handle_sort_menu(const InputFrame &input, UpdateContext &context);
    void handle_retry(const InputFrame &input, UpdateContext &context);
    void handle_home(Tab &tab, const InputFrame &input, UpdateContext &context);
    void handle_grid(Tab &tab, const InputFrame &input, UpdateContext &context);
    void enter_content(Tab &tab, int column, UpdateContext &context);
    void move_focus(Tab &tab, int next, bool edge, const InputFrame &input, UpdateContext &context);
    void press_cell(Tab &tab, const InputFrame &input, UpdateContext &context);
    // ---- where things are ----
    float tabs_y(float scroll) const
    {
        return std::max(kTabsPinnedY, kTabsY - scroll);
    }
    float wanted_scroll() const;
    // A cell where the section is laid out, before the page's scroll. settled: the list at its target.
    Rect cell_rect(const Tab &tab, int index, bool settled) const;
    Rect on_screen(Rect r) const
    {
        r.y -= scroll_.target;
        return r;
    }
    Rect sort_menu() const;
    Rect sort_row(int index) const;
    static const char *sort_text(const Tab &tab);
    void lay_out(const ui::Fonts &fonts);
    void advance(UpdateContext &context);
    void ask_pictures(UpdateContext &context);

    // ---- channel_draw.cpp ----
    void draw_bones(G &g) const;
    void draw_home_bones(G &g) const;
    void draw_head(G &g, const DrawContext &context) const;
    void draw_buttons(G &g) const;
    void draw_tabs(G &g, const DrawContext &context) const;
    void draw_sort_menu(G &g) const;
    void draw_content(G &g, const DrawContext &context) const;
    void draw_nothing(G &g, const Tab &tab, float focus) const;
    void draw_home(G &g, const DrawContext &context, const Tab &tab, float zone_focus) const;
    void draw_grid(G &g, const DrawContext &context, const Tab &tab, float zone_focus) const;
    void draw_playlist(G &g, const Rect &r, const Cell &cell, const Picture &picture, float focus, bool whole) const;
    // The ring answers an edge: it nudges and settles.
    Rect nudged(const G &g, Rect r) const
    {
        r.x += ui::shake(nudge_.value, g.time, 14.0f, 8.0f);
        return r;
    }

    Route route_;
    std::string id_;
    model::Profile profile_;
    Feed base_; // "channel:<id>": the channel itself and the section it opens with
    Head head_;
    std::vector<Tab> tabs_;
    ui::TabBar bar_;
    int active_ = 0;
    Zone zone_ = Zone::tabs;
    int button_ = 0;
    std::vector<Slot> buttons_;
    Rect tab_rect_{};
    Rect sort_chip_{};
    Rect retry_{}; // where the section is laid out, before the page's scroll
    float name_width_ = 600.0f;
    Notice trouble_;
    Sheet sheet_;

    // Subscribe and the notification level: asked, then confirmed or refused.
    bool subscribed_ = false;
    std::string level_ = "personalized";
    std::string wanted_level_;
    bool override_ = false; // an answer of this visit says more than the page that was read before it
    Doing doing_ = Doing::nothing;
    Doing last_ = Doing::nothing;
    Act act_;
    std::string outcome_ = "idle";
    bool asked_unsubscribe_ = false;
    std::shared_ptr<int> alive_;

    bool sort_open_ = false;
    int sort_pick_ = 0;

    float age_ = 0.0f;
    tween::Spring scroll_;
    tween::Spring content_focus_;
    tween::Spring button_pos_;
    tween::Spring actions_focus_;
    tween::Spring sort_focus_;
    tween::Spring sort_amount_;
    tween::Spring sort_pos_;
    tween::Spring sub_amount_;
    tween::Spring follow_width_;
    ui::SpringRect ring_;
    bool ring_set_ = false;
    ui::Pulse pop_;
    ui::Pulse shake_;
    ui::Pulse nudge_;
};

} // namespace afterglow::channel_page
