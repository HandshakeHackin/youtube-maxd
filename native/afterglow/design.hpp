// ProsperoTube Afterglow - the design language: icons and the shared pieces.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Everything here is drawn with the kit's shapes and text; the standard
// controls (keyboard, form, dialog, tabs, toasts, lists) are the kit's own
// components wearing the look's theme (Look::theme). A screen builds its page
// from these pieces and adds none of its own colours, radii or type sizes.

#pragma once

#include "afterglow/images.hpp"
#include "afterglow/models.hpp"
#include "afterglow/text.hpp"
#include "afterglow/theme.hpp"

#include "ui/components/component.hpp"

#include <array>
#include <string>

namespace afterglow
{

// What every drawing helper needs.
struct G
{
    gfx::DrawList &l;
    const ui::Fonts &f;
    const Look &k;
    std::uint32_t glass; // the frame's blurred copy, or 0
    float time;          // free-running seconds
    bool reduced;        // the player asked for less motion
};

// A group that arrives: fades in while it travels the last few pixels.
struct Enter
{
    gfx::DrawList &l;
    Enter(G &g, float amount, float dy = 22.0f, float dx = 0.0f) : l(g.l)
    {
        const float a = tween::clamp01(amount);
        l.push_opacity(a);
        const float travel = g.reduced ? 0.0f : 1.0f - a;
        l.push_transform(1.0f, 0.0f, 0.0f, dx * travel, dy * travel);
    }
    ~Enter()
    {
        l.pop_transform();
        l.pop_opacity();
    }
    Enter(const Enter &) = delete;
    Enter &operator=(const Enter &) = delete;
};

// ---- hand-overs -------------------------------------------------------------------

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
    Rect carried{};      // zoom and shrink: the rectangle the picture travels from or to
    std::string picture; // ... and the address of that picture, when the screen that pushed knew it
};

// What a hand-over does to a whole page (design notes, section 8): a section
// change fades and sinks the old page to 97.5 %; deeper and back out slide the
// old page 90 pixels and bring the new one 110 from the other side; where a
// picture travels (a card that becomes a video page) the rest of both pages,
// the furniture, is gone in 150 ms and returns under the picture. With less
// motion nothing travels and the fades remain.
struct Motion
{
    float alpha = 1.0f;     // of everything the page draws
    float scale = 1.0f;     // about the middle of the screen
    float dx = 0.0f;
    float furniture = 1.0f; // of everything but a picture that travels
};
Motion hand_over(const Phase &phase, bool reduced);
// The same on a draw list for as long as this lives, as Enter is. A page that
// keeps a travelling picture on screen itself says so and fades its furniture
// by motion.furniture; any other page is faded as a whole.
struct HandOver
{
    Motion motion;
    float alpha = 1.0f; // what was pushed: draw nothing more when it is 0

    HandOver(G &g, const Phase &phase, bool keeps_picture = false);
    ~HandOver();
    HandOver(const HandOver &) = delete;
    HandOver &operator=(const HandOver &) = delete;

  private:
    gfx::DrawList &l_;
};
// How far the nth group of a page that arrived `age` seconds ago has come.
inline float arrive(float age, int index, float step = 0.05f, float duration = 0.45f)
{
    return tween::stagger(age, index, step, duration);
}

// The kit's components wear the look: its theme, its sounds, less motion.
void wear(ui::ComponentStyle &style, const Look &look, bool reduced);

// One line of text through the text seam (text.hpp).
inline float text(G &g, const Face &face, std::string_view value, float x, float baseline, float size, Color color,
                  gfx::Align align = gfx::Align::left, float tracking = 0.0f)
{
    return draw_text(g.l, face, value, x, baseline, size, color, align, tracking);
}
// A paragraph through the text seam.
inline float paragraph(G &g, const Face &face, std::string_view value, float x, float y, float size, float width,
                       float line_height, Color color, int max_lines = 99, gfx::Align align = gfx::Align::left)
{
    return draw_paragraph(g.l, face, value, x, y, size, width, line_height, color, max_lines, align);
}
// A small tracked label in capitals.
float overline(G &g, std::string_view value, float x, float baseline, Color color,
               gfx::Align align = gfx::Align::left);

enum class Icon : std::uint8_t
{
    home,
    search,
    subs,
    library,
    bell,
    settings,
    user,
    users,
    play,
    pause,
    next,
    prev,
    queue,
    clock,
    heart,
    share,
    captions,
    quality,
    speed,
    audio,
    chat,
    block,
    hide,
    playlist_add,
    chevron_right,
    chevron_left,
    chevron_down,
    check,
    plus,
    close,
    info,
    phone,
    backup,
    update,
    globe,
    grid,
    skip,
    title,
    shorts,
    live,
    music,
    gamepad,
    news,
    sports,
    movie,
    flask,
    trending,
    more,
    bookmark,
    sort,
    qr,
    lock,
    mic,
    offline,
    alert,
    chapters,
    reply,
    pin,
    moon,
    count,
};

// Icons are drawn from strokes on a 24 unit grid, so they stay sharp at any
// size and take any colour. size is the box; (cx, cy) its centre.
void icon(G &g, Icon which, float cx, float cy, float size, Color color);
// The icon of a name ("gamepad"); Icon::grid for an unknown one.
Icon icon_by_name(std::string_view name);
const char *icon_name(Icon which);

// A frosted panel: the blurred picture behind, the look's tint, a light edge.
// Where the renderer has no blur (g.glass is 0) it is the look's solid panel.
void glass_panel(G &g, const Rect &r, float radius, float strength = 1.0f);
// A soft drop under something that floats.
void lift(G &g, const Rect &r, float radius, float amount = 1.0f);
// A channel's avatar: its picture, or two colours taken from its id and its initial.
void avatar(G &g, float cx, float cy, float radius, std::string_view name, std::string_view id,
            const Picture &picture);
// The two colours an avatar without a picture has, always the same for one id (or name).
void avatar_tint(std::string_view key, Color *top, Color *bottom);
// A small tag ("4K", "NEW"); returns its width. solid tags are filled.
float tag(G &g, float x, float cy, std::string_view label, Color fill, Color ink, float height = 28.0f);
float tag_width(G &g, std::string_view label, float height = 28.0f);
// A pill button. focus is 0..1; a primary button rests filled. busy: it was
// pressed and the answer is on its way (its icon turns, its words wait dimmed).
void button(G &g, const Rect &r, std::string_view label, Icon glyph, float focus, bool primary = false,
            bool selected = false, bool busy = false);
float button_width(G &g, std::string_view label, bool has_icon);
// A filter chip; returns its width.
// menu adds the chevron of a chip that opens a list.
float chip(G &g, float x, float cy, std::string_view label, bool selected, float focus, float height = 44.0f,
           bool menu = false);
float chip_width(G &g, std::string_view label, float height = 44.0f, bool menu = false);
// A thin progress line.
void progress(G &g, const Rect &r, float value, Color fill);
// The hint row, right-aligned at kRight (or left-aligned at x when left is set).
void hints(G &g, const ui::Hint *items, int count, float alpha = 1.0f, bool left = false, float x = kRight);
// The focus ring around r, with light in a colour of the content.
void focus_ring(G &g, const Rect &r, float radius, Color light, float amount = 1.0f);
// A resting picture box while its picture is on the way, or when it has none.
void picture_placeholder(G &g, const Rect &r, float radius, std::string_view initial = {});
// "48:12", "1:02:07"; "" for 0.
std::string duration_text(int seconds);
// One video card: picture, tags, length, progress and the text under it.
// focus is 0..1; alpha fades the whole card. text: 0 none, 1 title, channel
// and numbers on their own lines, 2 title and one line, 3 the title alone on
// one line.
void video_card(G &g, const Rect &r, const model::Video &video, const Picture &picture, float focus, float alpha,
                int text = 1);
// A topic's tile: two colours of its own, its icon and its name. focus is 0..1.
void topic_tile(G &g, const Rect &r, const model::Topic &topic, float focus, float alpha);
// A picture that dissolves into the page on its left and lower sides: fade_w
// pixels on the left, everything below solid_h. The hero of a page.
void faded_picture(gfx::DrawList &list, const Picture &picture, const Rect &r, float alpha, float fade_w, float solid_h);
// The bones of something that is loading, breathing a little. index offsets
// the breath so that neighbours do not pulse together.
void bone(G &g, const Rect &r, float radius, int index = 0);
// The bones of a video card: its picture and, with_text, the lines under it.
void card_bone(G &g, const Rect &r, bool with_text = true);
// A working arc: something is being waited for.
void spinner(G &g, float cx, float cy, float radius, Color color);
// A shelf or a grid row that only peeks over the foot of the page shows
// pictures; this closes the last 76 pixels so the hints sit on clean ground.
void foot_scrim(G &g);
// A picture in a box, fading in over its placeholder when it arrives.
void picture_box(G &g, const Rect &r, float radius, const Picture &picture, Color tint = kWhite);
// One controller glyph, as the hint row draws it (a button named inside a sentence or a field).
void glyph(G &g, ui::Button button, float cx, float cy, float size = 34.0f);

// ---- the cards beside the video card ------------------------------------------------

// What a playlist, a section or a channel among videos says.
struct CardText
{
    std::string_view title;
    std::string_view line;  // the quieter line: an owner, a count of followers, what is inside
    std::string_view count; // a playlist: how many videos, as it is to be shown ("24")
    std::string_view id;    // a channel: its id, for the colours of an avatar without a picture
    Icon icon = Icon::grid; // a section
    bool marked = false;    // a channel: subscribed
};
// A playlist: its first picture with the others stacked behind it. r is the picture's box (a grid cell).
void playlist_card(G &g, const Rect &r, const CardText &card, const Picture &picture, float focus, float alpha);
// A section inside a section: a tile that opens, with its picture when it has one.
void folder_card(G &g, const Rect &r, const CardText &card, const Picture &picture, float focus, float alpha);
// A channel among the videos: its face, its name and how many follow it. r is the whole tile.
void channel_card(G &g, const Rect &r, const CardText &card, const Picture &picture, float focus, float alpha);

// ---- the page that has nothing to show ----------------------------------------------

// One way on from such a page.
struct Choice
{
    const char *label = ""; // already translated
    Icon icon = Icon::chevron_right;
    ui::Button button = ui::Button::cross;
};
// The message of a page that is empty, signed out, failed or offline (boards
// 37 and 38): a mark on a disc, what is the matter, up to three lines of what
// to do, and one or two ways on. One way on is drawn as the boards have it,
// the Cross glyph and its words; two are buttons, so the focus can be seen
// moving between them. lay_out() runs while updating (the buttons' places are
// what focus() answers); draw() draws what was laid out.
struct Notice
{
    float cx = 0.0f;
    float top = 0.0f;
    int lines = 0;
    int count = 0;
    std::array<Rect, 2> buttons{};

    void lay_out(const ui::Fonts &fonts, float centre, float y, std::string_view body, const Choice *choices, int choice_count);
    // focus_at is the way on the focus is at (a spring between 0 and 1);
    // focused says whether the message has the focus at all.
    void draw(G &g, Icon glyph, std::string_view title, std::string_view body, const Choice *choices, float focus_at, bool focused, Color tone = clear()) const;
    // The height the message takes, mark to the foot of its ways on.
    float height() const;
};
// What a page says when its catalog did not arrive, and the mark beside it.
const char *trouble_title(model::Load load);
Icon trouble_icon(model::Load load);

// ---- measuring while updating -------------------------------------------------------

// The widths button(), chip() and tag() draw, from the fonts alone: a screen
// knows its rectangles while updating, where there is no draw list.
float button_width(const ui::Fonts &fonts, std::string_view label, bool has_icon = true);
float chip_width(const ui::Fonts &fonts, std::string_view label, float height = 44.0f, bool menu = false);
float tag_width(const ui::Fonts &fonts, std::string_view label, float height = 28.0f);

// A QR code for text (version 2 to 6, byte mode, level M), drawn from squares.
void qr_code(G &g, const Rect &r, std::string_view text, Color dark, Color light);
// The app mark: the tube seen end on, with the afterglow in it. amount draws it in.
void mark(G &g, float cx, float cy, float size, float amount, Color ink, Color glow);

inline Rect lerp_rect(const Rect &a, const Rect &b, float t)
{
    return {tween::lerp(a.x, b.x, t), tween::lerp(a.y, b.y, t), tween::lerp(a.w, b.w, t), tween::lerp(a.h, b.h, t)};
}
// A rectangle grown about its centre by a share of its size.
inline Rect grown(const Rect &r, float share, float lift_y = 0.0f)
{
    return {r.x - r.w * share * 0.5f, r.y - r.h * share * 0.5f - lift_y, r.w * (1.0f + share), r.h * (1.0f + share)};
}

} // namespace afterglow
