// ProsperoTube Afterglow - what the channel, channels and subscriptions screens share.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The three screens of this family are built from the same few parts: a
// catalog read page by page into one list, an action that is asked for and
// then confirmed or refused, the sheet of a row's actions, the grid cell,
// bones, the message of a page that has nothing to show, and the hand-over
// between screens.
//
// The parts marked REQUEST stand in for a piece that belongs to the frame
// owner's files; docs/afterglow/requests/C.md has the exact request for each.
// When the shared piece exists the stand-in goes.

#pragma once

#include "afterglow/screen.hpp"

#include "ui/components/tabs.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace afterglow::family
{

// REQUEST: theme.hpp has the shelf card but not the grid cell of the design
// notes (394 x 222, four columns from kLeft to kRight, rows 352 apart).
constexpr float kCellW = 394.0f;
constexpr float kCellH = 222.0f;
constexpr float kCellPitchX = kCellW + 24.0f;
constexpr float kCellPitchY = 352.0f;
constexpr int kColumns = 4;
// Nothing of a page is drawn below this line but the hint bar (ARCHITECTURE.md, section 8).
constexpr float kPageBottom = 968.0f;

inline bool starts_with(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

// "loading", "ready", ...: for snapshots.
const char *load_name(model::Load load);
// A translated pattern with its values in place: fill(tr("{0} new"), "3").
std::string fill(const char *pattern, std::string_view first, std::string_view second = {});
// The number a fact starts with ("3", "12 new"); 0 for none.
int number_of(std::string_view text);
// Parts joined by a middle dot, the empty ones left out.
std::string dotted(std::initializer_list<std::string_view> parts);

// The progress of the nth group of a screen that arrived `age` seconds ago.
inline float arrive(float age, int index, float step = 0.05f, float duration = 0.45f)
{
    return tween::stagger(age, index, step, duration);
}

// ---- a catalog, page by page --------------------------------------------------------

// REQUEST: add_row() fills a channel's name, picture and subscribers but not
// the facts ARCHITECTURE.md 5.4 lists for it (handle, banner, subscribed,
// unseen), and a video has no way back to its row. Until the view models carry
// them, the rows are kept beside the videos and channels they became.
struct Lists
{
    std::vector<model::Row> rows;
    std::vector<model::Video> videos;
    std::vector<model::Channel> channels;
    std::vector<int> video_row;   // rows[video_row[i]] is the row of videos[i]
    std::vector<int> channel_row; // ... of channels[i]

    const model::Row *row_of_video(std::size_t index) const
    {
        return index < video_row.size() ? &rows[static_cast<std::size_t>(video_row[index])] : nullptr;
    }
    const model::Row *row_of_channel(std::size_t index) const
    {
        return index < channel_row.size() ? &rows[static_cast<std::size_t>(channel_row[index])] : nullptr;
    }
};

// The pages of one catalog as one list. A screen calls read() every frame it is
// up: the first call asks for the first page, want_more() for the next one, and
// what has arrived is copied once. When the data layer forgets the catalog (a
// refresh, an action that changed it) what was read stays on screen until
// every page that was shown has arrived again; the focus is the screen's and
// stays where it was.
struct Feed : Lists
{
    std::string catalog;
    bool open = false;
    model::Load load = model::Load::idle; // of the first page; ready while a list that was read is read again
    std::string error;
    bool more = false;        // a further page exists
    bool more_failed = false; // ... and could not be read
    int wanted = 1;
    int taken = 0;
    unsigned version = 0; // changes whenever anything above does

    void start(std::string id);
    // Copies the pages again at the next read: the screen was covered and the answers may be others.
    void restart();
    void want_more()
    {
        if (more && !more_failed && !again_ && taken == wanted)
            ++wanted;
    }
    bool fetching_more() const
    {
        return !again_ && taken > 0 && taken < wanted && !more_failed;
    }
    // True when the list or its state changed.
    bool read(Data &data);

  private:
    Lists incoming_;     // the pages of a list that is read again, until all of them are there
    bool again_ = false; // what is on screen was read before the data layer forgot it
};

// An account or library action on its way (ARCHITECTURE.md 5.6): the acting
// row is listed once and, where the provider answers with the row that applies
// the change ("account:apply:<n>", its review step in the classic interface),
// that row is listed too. The screen has asked the person already where the
// change deserves a question.
struct Act
{
    unsigned ticket = 0;
    bool running = false;
    bool applying = false;

    void start(Data &data, const std::string &row)
    {
        ticket = data.act(row);
        running = true;
        applying = false;
    }
    // Load::loading while it runs; ready, failed or offline once, when it ends; idle otherwise.
    // answer is the page the provider listed, valid until the next call into the data layer.
    model::Load poll(Data &data, const Page **answer = nullptr);
};

// ---- motion -----------------------------------------------------------------------------

// REQUEST: the frame gives a screen its Phase and moves nothing for it, and
// design.hpp has no piece that turns a Phase into the hand-over of the motion
// table (deeper and back out: 90 and 110 pixels; a section change: the old
// screen sinks to 97.5 %; a card that becomes a video page: the furniture is
// gone in 150 ms). Every screen needs the same one.
struct Travel
{
    gfx::DrawList &l;
    float alpha = 1.0f;

    Travel(G &g, const Phase &phase);
    ~Travel();
    Travel(const Travel &) = delete;
    Travel &operator=(const Travel &) = delete;
};

// A direction with nowhere to go: the soft refusal of the design notes. Silent on a held direction.
void refuse(UpdateContext &context, const InputFrame &input, ui::Pulse &nudge, float x = 960.0f);
// Cross on a video: the player takes over, or says why not.
void play(const model::Video &video, float x, UpdateContext &context);

// ---- pieces -----------------------------------------------------------------------------

// REQUEST: a bone for loading states in design.hpp. A band of light crosses the
// page from left to right every 1.7 seconds; with less motion the bones rest.
void bone(G &g, const Rect &r, float radius);
// The bones of a video card: its picture and the lines under it.
void card_bone(G &g, const Rect &r, bool with_text = true);
// A shelf that only peeks over the foot shows pictures; this closes the last 76 pixels so the hints sit on clean ground.
void foot_scrim(G &g);
// A picture in a box, fading in over its placeholder when it arrives.
void picture_box(G &g, const Rect &r, float radius, const Picture &picture, Color tint = kWhite);

// How much of the focus a thing has: 1 under the gliding ring, falling to 0 a reach away.
inline float under_ring(const Rect &ring, const Rect &thing, float reach)
{
    return tween::clamp01(1.0f - std::hypot(ring.cx() - thing.cx(), ring.cy() - thing.cy()) / reach);
}

// REQUEST: button_width() and chip_width() of design.hpp need a G, which
// update() has not; a screen must know its rectangles there to answer
// focus(). The same sums, from the fonts update() does have.
float button_width(const ui::Fonts &fonts, std::string_view label, bool has_icon = true);
float chip_width(const ui::Fonts &fonts, std::string_view label, float height = 44.0f, bool menu = false);

// REQUEST: the page that has nothing to show (empty, signed out, failed,
// offline) as a piece of design.hpp: a mark, what is the matter, what to do,
// and one or two ways on. Every package draws this state.
struct Choice
{
    const char *label = ""; // already translated
    Icon icon = Icon::chevron_right;
};

struct Notice
{
    float cx = 0.0f;
    float top = 0.0f;
    int lines = 0;
    int count = 0;
    std::array<Rect, 2> buttons{};

    // Measures the message so that its buttons have a place before anything is drawn.
    void lay_out(const ui::Fonts &fonts, float centre, float y, std::string_view body, const Choice *choices, int choice_count);
    // focus_at is the button the focus is on (a spring between 0 and 1); focused says whether the message has the focus at all.
    void draw(G &g, Icon glyph, std::string_view title, std::string_view body, const Choice *choices, float focus_at, bool focused) const;
};

// What a page says when its catalog did not arrive.
const char *trouble_title(model::Load load);
Icon trouble_icon(model::Load load);

// ---- the actions of a row -----------------------------------------------------------------

// REQUEST: Touchpad opens the actions of whatever has the focus on every
// screen (board 14), so the sheet belongs to the frame: Nav::actions(row,
// title). Until it is there this is the sheet: the catalog Data::menu() names,
// its rows as a list, a row chosen through Data::act(), an answer that is a
// further list shown in place, a row that is another page handed back to the
// screen as a Route.
class Sheet
{
  public:
    Sheet();

    bool open() const
    {
        return open_;
    }
    // True while the sheet is on screen, also on its way out.
    bool visible() const
    {
        return amount_.value > 0.01f;
    }
    // Opens the actions of a row ("ptv...", "channel:UC..."); about is what the sheet is about, in words.
    void show(const std::string &row, std::string_view about, UpdateContext &context);
    // One frame of input while the sheet is open. True when the choice is another page: route says which.
    bool handle(const InputFrame &input, UpdateContext &context, Route *route);
    // Every frame: reads the menu, follows a running action, moves.
    void update(UpdateContext &context);
    void draw(G &g) const;
    // True once after an action went through: the lists of the screen may be older than the account now.
    bool take_changed();

    FocusInfo focus() const;
    std::span<const ui::Hint> hints() const;
    void snapshot(Snapshot &out) const;

  private:
    struct Level
    {
        std::string title;
        std::vector<model::Row> rows;
        int focus = 0;
    };
    void close(UpdateContext &context, bool sound);
    void begin(const model::Row &row, UpdateContext &context);
    Rect row_rect(int index) const;

    bool open_ = false;
    std::string menu_;
    std::string about_;
    std::vector<Level> levels_;
    model::Load load_ = model::Load::idle;
    std::string error_;
    Act act_;
    int acting_ = -1;
    std::string acting_title_;
    std::string confirmed_; // the row the person agreed to in the dialog
    bool changed_ = false;
    tween::Spring amount_;
    tween::Spring pos_;
    tween::Spring scroll_;
    ui::Pulse nudge_;
    std::shared_ptr<int> alive_;
};

} // namespace afterglow::family
