// ProsperoTube Afterglow - the text seam: every text the interface measures or draws.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Screens and the design language never call a font directly: they call the
// functions below with a Face. Today a Face is one of the kit's baked fonts
// (ui::FontRef) and text_kit.cpp implements the functions with the kit's own
// shaping. The text engine under native/text/ (system fonts, HarfBuzz, bidi, a
// dynamic atlas) replaces text_kit.cpp with an implementation of the same
// functions; nothing else changes. The kit's standard components (keyboard,
// dialog, toasts, lists) draw through ui::Fonts themselves: the engine serves
// them by giving those FontRefs fallback faces (gfx::Font::add_fallback) whose
// atlases it fills, which is why the frame owns one ui::Fonts and passes it on.

#pragma once

#include "afterglow/base.hpp"

#include <vector>

namespace afterglow
{

using Face = ui::FontRef;

// What a paragraph takes, measured without drawing.
struct TextBlock
{
    int lines = 0;
    float width = 0.0f;  // widest line as drawn, the ellipsis included
    float height = 0.0f; // lines * line_height
    bool truncated = false;
};

// Width of one line.
float measure(const Face &face, std::string_view value, float size, float tracking = 0.0f);
// The text cut to width, ending in an ellipsis when something was cut.
std::string fit(const Face &face, std::string_view value, float size, float width);
// One line at a baseline. Returns its width.
float draw_text(gfx::DrawList &list, const Face &face, std::string_view value, float x, float baseline, float size,
                Color color, gfx::Align align = gfx::Align::left, float tracking = 0.0f);
// A word-wrapped paragraph of at most max_lines lines from the first baseline
// at y; the last line ends in an ellipsis when text remains. Returns the
// baseline after the last line drawn.
float draw_paragraph(gfx::DrawList &list, const Face &face, std::string_view value, float x, float y, float size,
                     float width, float line_height, Color color, int max_lines = 99,
                     gfx::Align align = gfx::Align::left);
// The same paragraph, measured.
TextBlock measure_block(const Face &face, std::string_view value, float size, float width, float line_height,
                            int max_lines = 99);
// True when the face (with its fallbacks) draws every code point of the text.
bool covers(const Face &face, std::string_view value);
// The text without the code points no face draws (today: emoji and scripts the
// baked sets lack), so that nothing is drawn as a question mark.
std::string drawable(const Face &face, std::string_view value);

// The face YouTube's texts are checked against when they enter the data layer;
// the frame names it for as long as it lives. clean() is drawable() with that
// face, and the text itself while there is none.
void set_reading_face(const Face *face);
std::string clean(std::string_view value);

} // namespace afterglow
