// ProsperoTube Afterglow - the channels a profile follows, as tiles: what its two source files share.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// channels.cpp reads the lists, takes the input and moves things; channels_draw.cpp draws.
// Nothing outside these two files includes this header.

#pragma once

#include "afterglow/screens/channel_family.hpp"

namespace afterglow::channels_page
{

using namespace family;

constexpr int kAcross = 6;
constexpr float kTileW = 258.0f;
constexpr float kTileH = 340.0f;
constexpr float kTilePitchX = 278.0f;
constexpr float kTilePitchY = 364.0f;
constexpr float kTilesY = 204.0f;
constexpr float kBannerY = 196.0f; // the notice above the tiles when the account cannot be reached
constexpr float kBannerH = 72.0f;
constexpr float kChipY = 150.0f;
constexpr float kChipsRight = kRight - 130.0f; // the clock keeps the corner

enum class Mode : std::uint8_t
{
    loading,
    account, // the channels the account subscribes to
    local,   // no account: the channels followed on this console
    kept,    // the account cannot be reached: the same, with the reason above them
    empty,   // an account that subscribes to nothing
    invite,  // no account and nothing followed
    trouble, // nothing arrived and nothing is kept
};

inline const char *mode_name(Mode mode)
{
    switch (mode)
    {
    case Mode::loading:
        return "loading";
    case Mode::account:
        return "account";
    case Mode::local:
        return "local";
    case Mode::kept:
        return "kept";
    case Mode::empty:
        return "empty";
    case Mode::invite:
        return "invite";
    case Mode::trouble:
        return "trouble";
    }
    return "";
}

enum class Zone : std::uint8_t
{
    order,
    tiles,
    notice,
};

// The provider's three orders ("settings:channel-sort:<n>"), in the words of the board.
inline const char *order_name(int number)
{
    return number == 2 ? "new" : number == 1 ? "name" : "account";
}
inline const char *order_label(int number)
{
    return number == 2 ? tr("New first") : number == 1 ? tr("A to Z") : tr("Account order");
}
inline const char *order_phrase(int number)
{
    return number == 2 ? tr("new uploads first") : number == 1 ? tr("A to Z") : tr("in your account's order");
}

class Channels final : public Screen
{
  public:
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
    struct Tile
    {
        model::Channel channel;
        std::string line; // subscribers, as YouTube words them
        int unseen = 0;
        bool live = false;
    };
    struct Chip
    {
        int number = 0; // the provider's: 0 account order, 1 by name, 2 new first
        std::string row;
        bool selected = false;
        Rect rect{};
    };

    bool uses_local() const;
    const Feed &list() const;
    Feed &list();
    int selected_order() const;
    int marks() const;
    Rect tile_rect(int index, float scroll) const;

    // ---- data ---------------------------------------------------------------------------

    static bool waiting(const Feed &feed);
    static bool failed(const Feed &feed);
    void read(UpdateContext &context);
    void read_orders(const std::vector<model::Row> &rows);
    void read_tiles();

    // ---- actions ----------------------------------------------------------------------------

    // doing_: 1 an order is being chosen, 2 the marks are being cleared.
    void choose_order(const Chip &chip, UpdateContext &context);
    void clear_marks(const InputFrame &input, UpdateContext &context);
    void finish(UpdateContext &context);

    // ---- input ------------------------------------------------------------------------------

    void press_choice(UpdateContext &context);
    void handle(const InputFrame &input, UpdateContext &context);
    void press_choice_retry(UpdateContext &context);
    void handle_order(const InputFrame &input, UpdateContext &context);
    void handle_tiles(const InputFrame &input, UpdateContext &context);

    // ---- where things are, and how they move ---------------------------------------------------

    Icon notice_icon() const;
    const char *notice_title() const;
    std::string notice_body() const;
    void lay_out(const ui::Fonts &fonts);
    std::string subtitle() const;
    void advance(UpdateContext &context);

    // ---- drawing ----------------------------------------------------------------------------

    void draw_head(G &g) const;
    void draw_bones(G &g) const;
    void draw_tiles(G &g, const DrawContext &context) const;

    Route route_;
    model::Profile profile_;
    Feed account_; // "library:channels"
    Feed local_;   // "subscriptions": followed on this console
    Mode mode_ = Mode::loading;
    std::vector<Tile> tiles_;
    std::vector<Chip> orders_; // as the provider lists them
    std::vector<Chip> chips_;  // as they stand in the header, left to right
    bool orders_known_ = false;
    bool orders_chosen_ = false;
    Zone zone_ = Zone::tiles;
    int focus_ = 0;
    int chip_ = 0;
    int choice_ = 0;
    Choice choices_[2];
    const char *choice_names_[2] = {"", ""};
    Notice notice_;
    std::string subtitle_;

    Act act_;
    int doing_ = 0;
    int wanted_ = -1;
    std::string outcome_ = "idle";
    bool cleared_ = false; // the new marks were cleared in this visit

    float age_ = 0.0f;
    tween::Spring scroll_;
    tween::Spring tiles_focus_;
    tween::Spring order_focus_;
    tween::Spring chip_pos_;
    tween::Spring choice_pos_;
    tween::Spring marks_{1.0f, 0.0f, 1.0f};
    ui::SpringRect ring_;
    bool ring_set_ = false;
    ui::Pulse nudge_;
};

} // namespace afterglow::channels_page
