// ProsperoTube Afterglow - the text seam on the kit's baked fonts.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The one implementation of text.hpp until the text engine arrives; see the
// header for what replaces this file.

#include "afterglow/text.hpp"

namespace afterglow
{

namespace
{

// Decodes one UTF-8 code point at *at and advances; invalid bytes read as U+FFFD.
std::uint32_t next_code_point(std::string_view text, std::size_t *at)
{
    const auto byte = [&](std::size_t i) { return static_cast<unsigned char>(text[i]); };
    const unsigned char lead = byte(*at);
    int extra = lead < 0x80 ? 0 : (lead >> 5) == 0x6 ? 1 : (lead >> 4) == 0xe ? 2 : (lead >> 3) == 0x1e ? 3 : -1;
    if (extra < 0 || static_cast<std::size_t>(extra) >= text.size() - *at)
    {
        ++*at;
        return 0xfffd;
    }
    std::uint32_t code = extra == 0 ? lead : lead & (0x3f >> extra);
    for (int i = 1; i <= extra; ++i)
    {
        if ((byte(*at + static_cast<std::size_t>(i)) & 0xc0) != 0x80)
        {
            ++*at;
            return 0xfffd;
        }
        code = code << 6 | (byte(*at + static_cast<std::size_t>(i)) & 0x3f);
    }
    *at += static_cast<std::size_t>(extra) + 1;
    return code;
}

// Code points that take no glyph of their own: joiners and variation selectors.
bool invisible(std::uint32_t code)
{
    return code == 0x200d || code == 0x200b || code == 0xfeff || (code >= 0xfe00 && code <= 0xfe0f) ||
           (code >= 0x1f3fb && code <= 0x1f3ff);
}

} // namespace

float measure(const Face &face, std::string_view value, float size, float tracking)
{
    return face.font != nullptr ? face.font->measure(value, size, tracking) : 0.0f;
}

std::string fit(const Face &face, std::string_view value, float size, float width)
{
    return face.font != nullptr ? face.font->fit(value, size, width) : std::string(value);
}

float draw_text(gfx::DrawList &list, const Face &face, std::string_view value, float x, float baseline, float size,
                Color color, gfx::Align align, float tracking)
{
    if (face.font == nullptr)
        return 0.0f;
    return list.text(*face.font, face.texture, value, x, baseline, size, color, align, tracking);
}

float draw_paragraph(gfx::DrawList &list, const Face &face, std::string_view value, float x, float y, float size,
                     float width, float line_height, Color color, int max_lines, gfx::Align align)
{
    if (face.font == nullptr)
        return y;
    return ui::paragraph(list, face, value, x, y, size, width, line_height, color, max_lines, align);
}

TextBlock measure_paragraph(const Face &face, std::string_view value, float size, float width, float line_height,
                            int max_lines)
{
    TextBlock block;
    if (face.font == nullptr)
        return block;
    const ui::ParagraphMetrics m = ui::measure_paragraph(face, value, size, width, line_height, max_lines);
    block.lines = m.lines;
    block.width = m.width;
    block.height = m.height;
    block.truncated = m.truncated;
    return block;
}

bool covers(const Face &face, std::string_view value)
{
    if (face.font == nullptr)
        return false;
    for (std::size_t at = 0; at < value.size();)
    {
        const std::uint32_t code = next_code_point(value, &at);
        if (code >= 0x20 && !invisible(code) && !face.font->has_glyph(code))
            return false;
    }
    return true;
}

std::string drawable(const Face &face, std::string_view value)
{
    std::string out;
    if (face.font == nullptr)
        return out;
    out.reserve(value.size());
    bool space = false; // one space where a run of undrawable code points was
    for (std::size_t at = 0; at < value.size();)
    {
        const std::size_t from = at;
        const std::uint32_t code = next_code_point(value, &at);
        if (code == '\n' || code == '\t')
        {
            out.push_back(' ');
            space = true;
            continue;
        }
        if (code < 0x20 || invisible(code))
            continue;
        if (!face.font->has_glyph(code))
        {
            if (!space && !out.empty())
                out.push_back(' ');
            space = true;
            continue;
        }
        if (code == ' ' && space)
            continue;
        space = code == ' ';
        out.append(value.substr(from, at - from));
    }
    while (!out.empty() && out.back() == ' ')
        out.pop_back();
    return out;
}

} // namespace afterglow
