// ProsperoTube Afterglow - accounts: the screen's class.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// accounts.cpp has what the screen does (reading, input, what is being done),
// accounts_draw.cpp what it shows and reports. accounts.hpp is what it shares
// with the sign-in page and the phone page.

#pragma once

#include "afterglow/screens/accounts.hpp"

#include "ui/components/profile_picker.hpp"

#include <string>
#include <vector>

namespace afterglow::acct
{

inline constexpr Rect kTiles{kLeft, 208.0f, 1648.0f, 400.0f};
inline constexpr float kCentre = 1000.0f;
inline constexpr float kActionsTop = 716.0f;
inline constexpr float kActionHeight = 64.0f;
inline constexpr float kActionGap = 16.0f;
inline constexpr Rect kNotice{510.0f, 806.0f, 980.0f, 98.0f};
inline constexpr const char *kProfileRow = "library:profile:";

struct Tile
{
    std::string row; // "library:profile:<id>": listing it selects the profile
    std::string id;
    std::string name;
    std::string avatar;
    std::string subscriptions; // a count as text, "" when unknown
    bool guest = false;
    bool signed_in = false;
    bool selected = false;
    bool locked = false;
};

enum class Zone : std::uint8_t
{
    tiles,
    actions,
};

// What is being done, one listing after another.
struct Work
{
    enum class Kind : std::uint8_t
    {
        none,
        watch,       // select the profile, then Home
        remove,      // select it when it is not, then remove its account
        lock_set,    // a new password
        lock_change, // the current one, then a new one
        lock_remove, // the current one
    };
    enum class Step : std::uint8_t
    {
        select,   // the profile's row was listed
        password, // the profile's password was handed over
        removing, // "account:remove" was listed
        applying, // its "account:apply:<n>" was listed
        arming,   // "settings:lock:<verb>" was listed
        current,  // the current password was handed over
        fresh,    // the new password was handed over
    };
    Kind kind = Kind::none;
    Step step = Step::select;
    unsigned ticket = 0; // the action whose answer is awaited; 0 while the sheet has the word
    std::string row;
    std::string name;
    bool locked = false;
    float age = 0.0f;
};

class Accounts final : public Screen
{
  public:
    Accounts();

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
    enum class Asking : std::uint8_t
    {
        nothing,
        remove,
        password,
    };

    // ---- data ------------------------------------------------------------------

    void read(Data &data);

    // The picker's cards from the tiles, only when something changed: setting
    // them restarts its springs.
    void sync();

    void remember_focus();

    const Tile *tile_in_focus() const;

    static std::string shown_name(const Tile &tile);

    // Why there is no list, and what stays: whoever is watching keeps watching.
    std::string failure_words() const;

    // ---- what the focus can do ---------------------------------------------------

    // Guest has no account to remove; the tile that adds an account has no actions of its own.
    int action_count() const;

    static const char *action_label(int index);

    bool action_offered(int index) const;

    Rect action_rect(int index) const;

    void refuse(const InputFrame &input, UpdateContext &context);

    // What a build does not offer refuses; the notice under the actions says why, in the app's own words.
    void not_offered(UpdateContext &context);

    void update_tiles(const InputFrame &input, UpdateContext &context);

    void update_actions(const InputFrame &input, UpdateContext &context);

    // Triangle: the password of the profile that is watching.
    void password(UpdateContext &context);

    void answered(int choice, UpdateContext &context);

    // ---- what is being done --------------------------------------------------------

    void wait(Data &data, const std::string &row);

    // What an action may have changed is read again; the tiles stay until the new list is there.
    static void forget_lists(Data &data);

    void watch(const Tile &tile, Data &data);

    // The provider answers a protected profile's row with the notice that asks for its password.
    bool wants_password(const Page &page) const;

    void failed(const Page &page, UpdateContext &context);

    void advance(const Page &page, UpdateContext &context);

    void saved(const char *what, UpdateContext &context);

    // ---- drawing -------------------------------------------------------------------

    // REQUEST: UpdateContext::look, so a kit component is dressed where it is updated (requests/F.md, 3).
    void dress(const DrawContext &context) const;

    // A tile's face: the profile's picture or its initial, a check on whoever is watching, a lock on a protected one.
    void draw_face(ui::Canvas &canvas, const Rect &area, int index) const;

    void draw_actions(G &g) const;

    void draw_note(G &g) const;

    // The page before its list has arrived, in the list's own shape.
    void draw_bones(G &g) const;

    Route route_;
    const ui::Fonts *fonts_ = nullptr;
    float age_ = 0.0f;

    // What was read.
    model::Load load_ = model::Load::loading;
    std::string error_;
    std::vector<Tile> tiles_;
    bool have_tiles_ = false;
    std::string signature_;
    model::Profile profile_;
    int sign_in_ = -1; // 1: this build signs in to YouTube, 0: it does not, -1: not known yet
    int casting_ = -1; // the same for linking a phone

    // The focus.
    mutable ui::ProfilePicker picker_;
    mutable const Look *worn_ = nullptr;
    mutable const DrawContext *drawing_ = nullptr; // while draw() runs: what the picker's faces are drawn with
    Zone zone_ = Zone::tiles;
    int action_ = 0;
    tween::Spring action_focus_[3];
    tween::Spring actions_shown_;
    ui::Pulse nudge_;
    std::string focus_row_;      // the profile the focus is on, kept across lists
    std::string asked_at_start_; // a profile's row from the route

    // What is being done.
    Work work_;
    LocalQuestion question_;
    Asking asking_ = Asking::nothing;
    bool selected_before_ = false;
    PasswordSheet sheet_;
};

} // namespace afterglow::acct
