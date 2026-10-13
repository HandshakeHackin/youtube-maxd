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
// A small tag ("4K", "NEW"); returns its width. solid tags are filled.
float tag(G &g, float x, float cy, std::string_view label, Color fill, Color ink, float height = 28.0f);
float tag_width(G &g, std::string_view label, float height = 28.0f);
// A pill button. focus is 0..1; a primary button rests filled.
void button(G &g, const Rect &r, std::string_view label, Icon glyph, float focus, bool primary = false,
            bool selected = false);
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
