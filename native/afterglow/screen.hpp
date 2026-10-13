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

#include "ui/components/overlay.hpp"

#include <functional>
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

// Move and Phase (how one screen hands over to the next, and where a screen is
// in a hand-over) are in design.hpp, beside the piece that turns them into
// motion (HandOver).

enum class ToastKind : std::uint8_t
{
    info,
    success,
    warning,
    error,
};

// A question of the screen's own with one to three answers (Nav::ask).
struct Question
{
    ui::StatusKind icon = ui::StatusKind::question;
    std::string title;
    std::string body;
    std::vector<std::string> answers; // one to three, already translated
    int danger = -1;                  // the answer that destroys something: drawn in the warning colour
    int safe = 0;                     // where the focus opens; never on `danger` while there is another answer
    bool stacked = false;             // answers that are sentences: one under the other, the panel wider
};

// What is typed on the frame's text sheet (Nav::ask_text).
struct TextQuestion
{
    std::string title;
    std::string detail;  // one line under the title: what the text is for
    std::string initial; // what the field starts with ("https://")
    std::string done;    // the word of the hint that takes the text ("Post"); "" for "Save"
    bool secret = false; // a password: shown as dots and never part of a snapshot
    bool may_be_empty = false; // an empty text is an answer too (a filter that is cleared)
};

// What a screen may ask of the frame. Requests take effect after the screen's
// update returns.
class Nav
{
  public:
    virtual ~Nav() = default;
    // Opens a screen above this one. carried is the picture's rectangle for Move::zoom.
    virtual void push(const Route &route, Move move = Move::push, const Rect &carried = {}) = 0;
    // The same, naming the picture that travels: the page that arrives shows
    // it from its first frame, before its own data has answered.
    virtual void push(const Route &route, Move move, const Rect &carried, std::string_view picture)
    {
        (void)picture;
        push(route, move, carried);
    }
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

    // ---- added after the first screens were built; each has a default so that
    // nothing that implements Nav for a test has to change ---------------------

    // A question with one to three answers of the screen's own: a destructive
    // one in the warning colour, the focus on the safe one. answered runs once
    // with the index of the answer, or -1 when the question was put away.
    virtual void ask(Question question, std::function<void(int answer)> answered)
    {
        (void)question;
        if (answered)
            answered(-1);
    }
    // A text typed on the frame's sheet (the kit's keyboard: Cross types,
    // Square deletes, Triangle is a space, Options takes the text, Circle puts
    // the sheet away). answered runs once: false when it was put away. Hand
    // the text to the provider with Data::act(row, text) and keep no copy of a
    // secret.
    virtual void ask_text(TextQuestion question, std::function<void(bool taken, std::string text)> answered)
    {
        (void)question;
        if (answered)
            answered(false, {});
    }
    // The actions of a row (a video, a channel, a playlist, a comment) as a
    // sheet over the page (board 14; Touchpad wherever such a thing has the
    // focus). The frame reads Data::menu(row, about), lists it, runs the
    // chosen row through Data::act() (asking first where something is taken
    // away), says what happened in a toast and opens the page a row names.
    // about is what the sheet is about, in words (the video's title). changed
    // runs once after an action went through: the lists of the screen may be
    // older than the account now (data.forget() what it shows).
    virtual void actions(const std::string &row, std::string_view about, std::function<void()> changed = {})
    {
        (void)row;
        (void)about;
        (void)changed;
    }
    // Leaves for the nearest screen of that kind below this one (Search again
    // on the results returns to the search screen under them). False when
    // there is none: nothing happens then and the screen decides.
    virtual bool back_to(ScreenId screen)
    {
        (void)screen;
        return false;
    }
    // Hands a video to the player from its picture's rectangle (the motion
    // table's "picture to player": the picture becomes the whole screen, and
    // returns to where it came from when the player closes). picture is the
    // address of that picture. False when the app refused to play it.
    virtual bool play(const model::Video &video, const Rect &carried, std::string_view picture = {})
    {
        (void)video;
        (void)carried;
        (void)picture;
        return false;
    }
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
    // In enter(): the hand-over this screen arrives by: its move, and for
    // Move::zoom the rectangle and the address of the picture that travels.
    // Null where nothing moves (a cut, the first screen).
    const Phase *arriving = nullptr;
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
    // A second layer, drawn after the frame's own furniture (rail, title,
    // clock, hints): a sheet or a panel of the screen that covers them, with
    // its scrim. The same rules as draw(). While over() is above 0 the
    // frame's hint row gives way to it.
    virtual void draw_over(const DrawContext &context, const Phase &phase) const
    {
        (void)context;
        (void)phase;
    }
    // How much of the page such a layer covers just now, 0..1.
    virtual float over() const
    {
        return 0.0f;
    }

    // What the frame draws at the head of the page.
    enum class Header : std::uint8_t
    {
        title,      // the page title (title(), else the route's), the line under it and the clock
        clock,      // the clock alone: the page has a head of its own
        on_picture, // ... and that head is a picture: the clock stands on a glass plate, 28 pixels in
    };
    virtual Header header() const
    {
        return Header::title;
    }

    // The header: the page title beside the mark; "" shows the route's title.
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

// ---- what every screen does the same way ------------------------------------------

// A direction with nowhere to go: the soft refusal of the design notes (the
// error cue at 60 %, a short rumble, the ring nudged; nudge is the screen's
// ui::Pulse, shaken with ui::shake() where the focus is drawn). Silent on a
// held direction. x pans the cue to where the focus is.
void refuse(UpdateContext &context, const InputFrame &input, ui::Pulse &nudge, float x = 960.0f);
// Cross on a video: the player takes over with the launch cue and its rumble,
// from the picture's rectangle when one is given (Nav::play), or a toast says
// why not. True when it plays.
bool play_video(UpdateContext &context, const model::Video &video, const Rect &carried = {}, float x = 960.0f);
// A provider row that is another page of the interface ("channel:UC...",
// "description:<video>", "library:playlist:<id>", ...): where it leads.
bool route_of_row(const model::Row &row, Route *route);
// The icon of a provider row in a list of actions, from its id.
Icon icon_of_row(std::string_view id);

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
