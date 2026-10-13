// ProsperoTube Afterglow - the screen interface.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A screen is one page of the interface: Home, Search, a channel, Settings.
// The frame (frame.hpp) owns the stack of screens, the page ground, the rail,
// the header clock, the hint bar, toasts, the dialog and every hand-over
// between screens; a screen owns what is inside its page and nothing else.
// One screen is one source file under screens/ that defines its make_<name>()
// factory. docs/afterglow/ARCHITECTURE.md has the rules and a worked example.

#pragma once

#include "afterglow/data.hpp"
#include "afterglow/design.hpp"
#include "afterglow/images.hpp"

#include <memory>
#include <span>
#include <string>
#include <vector>

namespace afterglow
{

enum class ScreenId : std::uint8_t
{
    opening,       // the opening moment of a cold start
    home,
    search,        // the search field, suggestions and history
    results,       // results of a search
    grid,          // any catalog of videos as a grid ("see all", a topic, a playlist)
    channel,
    video,         // the video page
    library,
    subscriptions,
    notifications,
    channels,      // subscribed and followed channels
    comments,
    settings,      // the preference tiles and their lists
    accounts,      // profiles and accounts
    sign_in,       // the device-code page
    link_phone,
    reader,        // long text: description, credits, changes
    count,
};
constexpr std::size_t kScreenCount = static_cast<std::size_t>(ScreenId::count);
// "home", "search", ...: logs, snapshots, picture names, board files.
const char *screen_name(ScreenId id);
bool screen_from_name(std::string_view name, ScreenId *id);

// Where to go and with what: the argument is the screen's own (a catalog id
// for grid, a channel id for channel, a video row id for video, a query for
// results, a settings page for settings, "" where there is nothing to say).
struct Route
{
    ScreenId screen = ScreenId::home;
    std::string argument;
    std::string title; // shown in the header where the screen has none of its own
};

// How one screen hands over to the next.
enum class Move : std::uint8_t
{
    cut,
    fade,   // the old screen sinks back, the new one assembles
    push,   // deeper: the new screen arrives from the right
    pop,    // back out: from the left
    zoom,   // a card's picture grows into the next screen's picture
    shrink, // ... and returns to its card
};

// Where a screen is in a hand-over: in runs 0..1 while it arrives, out runs
// 0..1 while it leaves. A screen that is simply up has in 1 and out 0.
struct Phase
{
    float in = 1.0f;
    float out = 0.0f;
    Move move = Move::cut;
    Rect carried{}; // zoom and shrink: the rectangle the picture travels from or to
};

enum class ToastKind : std::uint8_t
{
    info,
    success,
    warning,
    error,
};

// What a screen may ask of the frame. Requests take effect after the screen's
// update returns.
class Nav
{
  public:
    virtual ~Nav() = default;
    // Opens a screen above this one. carried is the picture's rectangle for Move::zoom.
    virtual void push(const Route &route, Move move = Move::push, const Rect &carried = {}) = 0;
    // Leaves this screen for the one below it; on the last screen this asks
    // the app what Circle does on the main menu.
    virtual void back() = 0;
    // Replaces the whole stack (the rail's destinations).
    virtual void root(const Route &route) = 0;
    // Left at the left edge of a screen: gives the focus to the rail.
    virtual void open_rail() = 0;
    virtual void toast(std::string_view text, ToastKind kind = ToastKind::info) = 0;
    // The same with a second, quieter line under the first (what it concerns).
    virtual void toast(std::string_view text, std::string_view detail, ToastKind kind) = 0;
    // A question with two answers; answered runs once, with true for confirm.
    virtual void confirm(std::string_view title, std::string_view body, std::string_view confirm_label,
                         std::function<void(bool)> answered) = 0;
    // The picture whose colours wash the page ground ("" for none). Call when
    // the focus moves to another picture; the frame cross-fades.
    virtual void ambient(std::string_view picture_url) = 0;
};

// Long-lived services, the same for every screen.
struct Services
{
    Data &data;
    ImageCache &images;
    Platform &platform;
    const ui::Fonts &fonts; // for measuring while updating; drawing has them in DrawContext
};

// What update() receives.
struct UpdateContext
{
    Services &services;
    Nav &nav;
    Feedback &feedback; // sounds and rumble for this frame
    float dt = 0.0f;
    float time = 0.0f;  // free-running seconds
    bool reduced_motion = false;
    bool top = true;    // false while the screen is leaving or covered: ignore input
    const Look *look = nullptr; // the look in use, for what depends on it while updating
};

// What draw() receives. world is the page ground's layer (ambient pictures,
// backdrops, anything a frosted panel should blur); ui is everything on it.
struct DrawContext
{
    gfx::DrawList &world;
    gfx::DrawList &ui;
    const ui::Fonts &fonts;
    const Look &look;
    ImageCache &images;
    std::uint32_t glass = 0; // the blurred world, 0 where the renderer has none
    float time = 0.0f;
    bool reduced_motion = false;

    G g() const
    {
        return G{ui, fonts, look, glass, time, reduced_motion};
    }
    G g_world() const
    {
        return G{world, fonts, look, 0, time, reduced_motion};
    }
};

// What has the focus, for tests and for the console session driver.
struct FocusInfo
{
    std::string id;   // stable within the screen: "continue/2", "rail/search", "action/play"
    std::string text; // what the focused thing says (a title, a label)
    Rect rect{};      // where it is drawn, virtual pixels
    int index = 0;    // 0-based position in its row or list
    int total = 0;    // how many that row or list has
};

// A screen's own state for snapshots: flat name/value pairs and named lists.
// The frame adds the screen's name, the focus, the rail and the stack.
struct Snapshot
{
    std::vector<std::pair<std::string, std::string>> values;            // "shelves" -> "3"
    std::vector<std::pair<std::string, std::vector<std::string>>> lists; // "shelf:continue" -> titles
    void set(std::string name, std::string value)
    {
        values.emplace_back(std::move(name), std::move(value));
    }
    void set(std::string name, int value)
    {
        values.emplace_back(std::move(name), std::to_string(value));
    }
    void list(std::string name, std::vector<std::string> items)
    {
        lists.emplace_back(std::move(name), std::move(items));
    }
};

class Screen
{
  public:
    virtual ~Screen() = default;

    virtual ScreenId id() const = 0;

    // The screen becomes the top of the stack. restored is false the first
    // time (route is new) and true when the screen above it left: keep the
    // focus and the scroll position, refresh what may have changed.
    virtual void enter(const Route &route, UpdateContext &context, bool restored) = 0;
    // The screen stops being the top (covered or removed).
    virtual void leave(UpdateContext &context)
    {
        (void)context;
    }

    // Once per frame while the screen is on the stack and visible. Input is
    // all zero unless context.top.
    virtual void update(const InputFrame &input, UpdateContext &context) = 0;
    // May run more than once per frame and must not change the screen.
    virtual void draw(const DrawContext &context, const Phase &phase) const = 0;

    // The header: the page title beside the mark; "" hides the header.
    virtual std::string title() const
    {
        return {};
    }
    // A second, quieter line under the title: where the page is ("Settings / Captions"), what it shows.
    virtual std::string subtitle() const
    {
        return {};
    }
    // The hint bar's entries for the current focus.
    virtual std::span<const ui::Hint> hints() const
    {
        return {};
    }
    // False for screens that take the whole page (opening, sign-in).
    virtual bool shows_rail() const
    {
        return true;
    }
    // How much of the frame's furniture (rail, title, clock, hints) shows over
    // this screen, 0..1: a screen that takes the whole picture for a while
    // (attract mode) fades it out.
    virtual float furniture() const
    {
        return 1.0f;
    }
    // Attract mode: the frame calls this with true after kAttractAfter seconds
    // without input and with false at the next input. A screen that has no
    // attract mode keeps the default.
    virtual bool attract(bool on)
    {
        (void)on;
        return false;
    }

    virtual FocusInfo focus() const = 0;
    // Every focusable thing currently reachable, as id and text.
    virtual std::vector<std::pair<std::string, std::string>> focusable() const
    {
        return {};
    }
    virtual void snapshot(Snapshot &out) const
    {
        (void)out;
    }
};

// ---- the screens ----------------------------------------------------------------
// One factory per screen. screens/stubs.cpp defines every one of them weakly as
// a placeholder page; the file that implements a screen defines its factory
// and so replaces the placeholder at link time. No list is edited to add one.
std::unique_ptr<Screen> make_opening();
std::unique_ptr<Screen> make_home();
std::unique_ptr<Screen> make_search();
std::unique_ptr<Screen> make_results();
std::unique_ptr<Screen> make_grid();
std::unique_ptr<Screen> make_channel();
std::unique_ptr<Screen> make_video();
std::unique_ptr<Screen> make_library();
std::unique_ptr<Screen> make_subscriptions();
std::unique_ptr<Screen> make_notifications();
std::unique_ptr<Screen> make_channels();
std::unique_ptr<Screen> make_comments();
std::unique_ptr<Screen> make_settings();
std::unique_ptr<Screen> make_accounts();
std::unique_ptr<Screen> make_sign_in();
std::unique_ptr<Screen> make_link_phone();
std::unique_ptr<Screen> make_reader();
// The placeholder itself, for the stubs.
std::unique_ptr<Screen> make_placeholder(ScreenId id);
// The screen of an id.
std::unique_ptr<Screen> make_screen(ScreenId id);

} // namespace afterglow
