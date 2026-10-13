// ProsperoTube Afterglow - the text engine behind the kit's fonts.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/text_engine.hpp"

#include "afterglow/text.hpp"

#include <algorithm>
#include <cstring>

namespace afterglow
{

namespace
{

// U+200F before a line of a right-to-left paragraph: drawn alone, the line is
// ordered as it was inside its paragraph.
constexpr const char *kRightToLeftMark = "\xE2\x80\x8F";

void utf8(std::uint32_t code, char out[5])
{
    if (code < 0x80)
    {
        out[0] = static_cast<char>(code);
        out[1] = 0;
    }
    else if (code < 0x800)
    {
        out[0] = static_cast<char>(0xc0 | (code >> 6));
        out[1] = static_cast<char>(0x80 | (code & 0x3f));
        out[2] = 0;
    }
    else if (code < 0x10000)
    {
        out[0] = static_cast<char>(0xe0 | (code >> 12));
        out[1] = static_cast<char>(0x80 | ((code >> 6) & 0x3f));
        out[2] = static_cast<char>(0x80 | (code & 0x3f));
        out[3] = 0;
    }
    else
    {
        out[0] = static_cast<char>(0xf0 | (code >> 18));
        out[1] = static_cast<char>(0x80 | ((code >> 12) & 0x3f));
        out[2] = static_cast<char>(0x80 | ((code >> 6) & 0x3f));
        out[3] = static_cast<char>(0x80 | (code & 0x3f));
        out[4] = 0;
    }
}

} // namespace

std::unique_ptr<TextEngine> TextEngine::create(Platform &platform, const TextSetup &setup, std::string *error)
{
    using tube::text::FaceRole;
    tube::text::Config config;
    // The interface faces first: line metrics come from the first Latin face.
    if (!setup.regular.empty())
        config.faces.push_back({setup.regular, 0, FaceRole::latin, false, 0});
    if (!setup.semibold.empty())
        config.faces.push_back({setup.semibold, 0, FaceRole::latin, true, 0});
    if (!setup.display.empty())
        config.faces.push_back({setup.display, 0, FaceRole::latin, false, 1});
    if (!setup.mono.empty())
        config.faces.push_back({setup.mono, 0, FaceRole::latin, false, 2});
    for (const tube::text::FaceSpec &face : setup.faces)
        config.faces.push_back(face);
    config.font_dirs = setup.font_dirs;
    config.discover = !setup.font_dirs.empty();
    config.language = setup.language;
    config.pixel_scale = setup.pixel_scale;
    config.emoji = tube::text::EmojiPolicy::strip;
    std::string why;
    std::unique_ptr<tube::text::Context> context = tube::text::Context::create(config, &why);
    if (!context)
    {
        if (error != nullptr)
            *error = why;
        return nullptr;
    }
    for (const std::string &line : context->messages())
        platform.log(("text: " + line).c_str());
    return std::unique_ptr<TextEngine>(new TextEngine(platform, std::move(context)));
}

TextEngine::TextEngine(Platform &platform, std::unique_ptr<tube::text::Context> context)
    : platform_(platform), context_(std::move(context))
{
}

TextEngine::~TextEngine()
{
    if (gfx::Font::shaper == this)
        gfx::Font::shaper = nullptr;
    set_reading_face(nullptr);
    for (PageTexture &page : pages_)
    {
        if (page.handle != 0)
            platform_.release_glyph_page(page.handle, page.coverage);
    }
}

void TextEngine::bind(const ui::Fonts &fonts)
{
    using tube::text::Weight;
    bound_[0].font = fonts.regular.font;
    bound_[0].style.family = 0;
    bound_[1].font = fonts.semibold.font;
    bound_[1].style.family = 0;
    bound_[1].style.weight = Weight::bold;
    bound_[2].font = fonts.display.font;
    bound_[2].style.family = 1;
    bound_[3].font = fonts.mono.font;
    bound_[3].style.family = 2;
    bound_[3].style.tabular_digits = true;
    gfx::Font::shaper = this;
    // The engine leaves out what no font draws: the data layer passes YouTube's texts through.
    set_reading_face(nullptr);
    ensure_pages();
}

bool TextEngine::knows(const gfx::Font &font) const
{
    return std::any_of(bound_.begin(), bound_.end(), [&](const Bound &bound) { return bound.font == &font; });
}

tube::text::Style TextEngine::style_of(const gfx::Font &font, float size, float tracking) const
{
    tube::text::Style style;
    for (const Bound &bound : bound_)
    {
        if (bound.font == &font)
            style = bound.style;
    }
    style.size = size;
    style.tracking = tracking;
    return style;
}

// Every page has its texture before a frame lays text out: a quad names its page's texture.
void TextEngine::ensure_pages()
{
    const int size = context_->page_size();
    while (static_cast<int>(pages_.size()) < context_->page_count())
    {
        const int index = static_cast<int>(pages_.size());
        PageTexture page;
        page.handle = platform_.create_glyph_page(size, context_->page_pixels(index));
        page.coverage = page.handle != 0;
        if (page.handle == 0)
        {
            // No one-channel pages here: the coverage is spread over the four channels of an ordinary texture.
            const std::uint8_t *pixels = context_->page_pixels(index);
            rgba_.resize(static_cast<std::size_t>(size) * static_cast<std::size_t>(size) * 4u);
            for (std::size_t i = 0; i < static_cast<std::size_t>(size) * static_cast<std::size_t>(size); ++i)
                std::memset(&rgba_[i * 4u], pixels[i], 4);
            page.handle = platform_.create_texture(size, size, rgba_.data());
        }
        pages_.push_back(page);
    }
}

void TextEngine::begin_frame()
{
    context_->advance_frame();
    ensure_pages();
    missing_ = 0;
}

void TextEngine::end_frame()
{
    const int size = context_->page_size();
    std::vector<bool> changed(static_cast<std::size_t>(context_->page_count()), false);
    for (const tube::text::AtlasUpdate &update : context_->atlas_updates())
    {
        if (update.page < changed.size())
            changed[update.page] = true;
    }
    ensure_pages(); // a page that was made while this frame was laid out
    for (std::size_t index = 0; index < changed.size() && index < pages_.size(); ++index)
    {
        if (!changed[index])
            continue;
        PageTexture &page = pages_[index];
        const std::uint8_t *pixels = context_->page_pixels(static_cast<int>(index));
        if (page.coverage)
        {
            platform_.update_glyph_page(page.handle, size, pixels);
            continue;
        }
        rgba_.resize(static_cast<std::size_t>(size) * static_cast<std::size_t>(size) * 4u);
        for (std::size_t i = 0; i < static_cast<std::size_t>(size) * static_cast<std::size_t>(size); ++i)
            std::memset(&rgba_[i * 4u], pixels[i], 4);
        if (page.handle != 0)
            platform_.release_texture(page.handle);
        page.handle = platform_.create_texture(size, size, rgba_.data());
    }
}

float TextEngine::measure(const gfx::Font &font, std::string_view text, float size, float tracking)
{
    return context_->measure(text, style_of(font, size, tracking)).width;
}

std::string TextEngine::fit(const gfx::Font &font, std::string_view text, float size, float max_width, float tracking)
{
    return context_->fit(text, style_of(font, size, tracking), max_width);
}

float TextEngine::layout(const gfx::Font &font, std::string_view text, float x, float y, float size, gfx::Align align,
                         std::vector<gfx::GlyphQuad> &quads, float tracking)
{
    context_->layout(text, style_of(font, size, tracking), {}, &scratch_);
    missing_ += scratch_.missing;
    const float left = align == gfx::Align::center ? x - scratch_.width * 0.5f : align == gfx::Align::right ? x - scratch_.width : x;
    const float top = y - scratch_.ascent; // y is the baseline
    for (const tube::text::Quad &quad : scratch_.quads)
    {
        const std::uint32_t texture = quad.page < pages_.size() ? pages_[quad.page].handle : 0;
        if (texture == 0)
            continue; // its page has no texture yet: the glyph appears next frame
        quads.push_back({left + quad.x0, top + quad.y0, left + quad.x1, top + quad.y1, quad.u0, quad.v0, quad.u1, quad.v1, texture, quad.range});
    }
    return scratch_.width;
}

std::vector<std::string> TextEngine::wrap(const gfx::Font &font, std::string_view text, float size, float max_width)
{
    tube::text::Constraints constraints;
    constraints.max_width = max_width;
    constraints.max_lines = 0;
    tube::text::Layout wrapped;
    // Measured, not rasterised: wrapping asks for lines, not for glyphs.
    context_->layout(text, style_of(font, size, 0.0f), constraints, &wrapped);
    std::vector<std::string> lines;
    for (const tube::text::Line &line : wrapped.lines)
    {
        std::string part = line.rtl ? std::string(kRightToLeftMark) : std::string();
        if (line.text_end > line.text_begin && line.text_end <= text.size())
            part.append(text.substr(line.text_begin, line.text_end - line.text_begin));
        while (!part.empty() && (part.back() == ' ' || part.back() == '\n'))
            part.pop_back();
        lines.push_back(std::move(part));
    }
    if (lines.empty())
        lines.emplace_back();
    return lines;
}

bool TextEngine::has_glyph(const gfx::Font &font, std::uint32_t codepoint)
{
    (void)font;
    char text[5];
    utf8(codepoint, text);
    return context_->can_draw(text);
}

} // namespace afterglow
