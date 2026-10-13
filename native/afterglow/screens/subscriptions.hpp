// ProsperoTube Afterglow - new videos from the channels a profile subscribes to: what its two source files share.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// subscriptions.cpp reads the lists, takes the input and moves things; subscriptions_draw.cpp draws.
// Nothing outside these two files includes this header.

#pragma once

#include "afterglow/screens/channel_family.hpp"

namespace afterglow::subscriptions_page
{

using namespace family;

constexpr float kStripY = 236.0f;
constexpr float kStripX = kLeft + 44.0f;
constexpr float kStripPitch = 138.0f;
constexpr int kStripShown = 12;
constexpr float kFeedY = 396.0f;       // the first row of cards
constexpr float kHeadAbove = 24.0f;    // a day's heading stands this far above its first row
constexpr float kSectionGap = 40.0f;   // more room before the next day than between two rows
constexpr float kClipTop = 176.0f;     // a scrolled feed passes under the header here
constexpr float kRowRest = 204.0f;     // where a focused row rests once the feed is scrolled
constexpr float kHeadRest = 236.0f;    // ... when it is the first of its day, so that the heading shows
constexpr int kMaxPages = 8;

enum class Mode : std::uint8_t
{
    loading,
    feed,
    kept,    // the account cannot be reached: the channels followed on this console, and why
    invite,  // no account
    empty,   // an account that subscribes to nothing
    trouble, // nothing arrived and nothing is kept
};

inline const char *mode_name(Mode mode)
{
    switch (mode)
    {
    case Mode::loading:
        return "loading";
    case Mode::feed:
        return "feed";
    case Mode::kept:
        return "kept";
    case Mode::invite:
        return "invite";
    case Mode::empty:
        return "empty";
    case Mode::trouble:
        return "trouble";
    }
    return "";
}

enum class Zone : std::uint8_t
{
    strip,
    feed,
    notice,
};

inline const char *filter_name(int filter)
{
    return filter == 1 ? "today" : filter == 2 ? "live" : filter == 3 ? "unwatched" : "all";
}

// The day a video belongs to: 0 today, 1 this week, 2 this month, 3 earlier.
inline int day_of(int days)
{
    return days <= 0 ? 0 : days < 7 ? 1 : days < 30 ? 2 : 3;
}

class Subscriptions final : public Screen
{
  public:
    Subscriptions();
    ScreenId id() const override;
    void enter(const Route &route, UpdateContext &context, bool restored) override;
    void update(const InputFrame &input, UpdateContext &context) override;
    void draw(const DrawContext &context, const Phase &phase) const override;
    std::string title() const override;
    std::span<const ui::Hint> hints() const override;
    FocusInfo focus() const override;
    std::vector<std::pair<std::string, std::string>> focusable() const override;
    void snapshot(Snapshot &out) const override;

  private:
    // One face of the strip: a channel, or the tile that stands for all of them (channel -1).
    struct Entry
    {
        int channel = -1; // an index into the strip's feed
        bool live = false;
        bool unseen = false;
    };
    // One row of cards in the feed.
    struct Line
    {
        int day = 0;
        int first = 0; // an index into order_
        int count = 0;
        float y = 0.0f;     // of its cards, before the page's scroll
        bool heads = false; // the first row of its day: the heading stands above it
    };

    bool has_all() const;
    // The centre of a face. Without the "All" tile the first name starts at the content's left edge.
    float strip_x(int index, float scroll) const;
    const Feed &strip_feed() const;
    // The strip entry the focus is on while it is in the strip: the picked one.
    int strip_focus() const;
    const model::Channel *strip_channel(int at) const;
    const model::Video *focused_video() const;
    const char *day_name(int day) const;
    int line_of(int index) const;
    Rect cell_rect(int index) const;

    // ---- data ---------------------------------------------------------------------------

    static bool waiting(const Feed &feed);
    static bool failed(const Feed &feed);
    void read(UpdateContext &context);
    void read_strip();

    // Works out which videos show (the filter, the picked channel) and where: by day, four to a row.
    void arrange();
    void ask_pictures(UpdateContext &context);

    // ---- input ------------------------------------------------------------------------------

    void press_choice(UpdateContext &context);
    void go(Zone zone, UpdateContext &context);
    void handle(const InputFrame &input, UpdateContext &context);
    void handle_strip(const InputFrame &input, UpdateContext &context);
    void handle_feed(const InputFrame &input, UpdateContext &context);
    void handle_notice(const InputFrame &input, UpdateContext &context);

    // ---- where things are, and how they move ---------------------------------------------------

    Icon notice_icon() const;
    const char *notice_title() const;
    std::string notice_body() const;
    void lay_out(const ui::Fonts &fonts);

    // The page's scroll: the strip shows while the focus is in it or on the first row.
    float wanted_scroll() const;
    void advance(UpdateContext &context);

    // ---- drawing ----------------------------------------------------------------------------

    void draw_bones(G &g) const;

    // Who the profile follows: a strip of faces, the one that is picked marked.
    void draw_strip(G &g, const DrawContext &context) const;
    void draw_feed(G &g, const DrawContext &context) const;

    // The filter or the picked channel leaves nothing of what has been read.
    void draw_nothing(G &g) const;

    Route route_;
    model::Profile profile_;
    Feed feed_;  // "library:subscriptions"
    Feed faces_; // "library:channels"
    Feed local_; // "subscriptions": followed on this console
    unsigned seen_ = 0;
    Mode mode_ = Mode::loading;
    Zone zone_ = Zone::feed;
    bool settled_ = false;
    bool dated_ = false; // the feed says how old its videos are
    ui::TabBar bar_;
    int filter_ = 0;
    int pick_ = 0; // the strip entry the feed is kept to; with an "All" tile that is entry 0
    int focus_ = 0;
    int choice_ = 0;
    std::vector<Entry> strip_;
    std::vector<int> order_; // the videos that show, as they stand: indices into the feed
    std::vector<Line> lines_;
    Choice choices_[2];
    const char *choice_names_[2] = {"", ""};
    Notice notice_;
    Sheet sheet_;

    float age_ = 0.0f;
    tween::Spring scroll_;
    tween::Spring strip_scroll_;
    tween::Spring feed_focus_;
    tween::Spring strip_focus_;
    tween::Spring pick_pos_;
    tween::Spring choice_pos_;
    ui::SpringRect ring_;
    bool ring_set_ = false;
    ui::Pulse nudge_;
};

} // namespace afterglow::subscriptions_page
