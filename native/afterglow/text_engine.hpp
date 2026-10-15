// ProsperoTube Afterglow - the text engine behind the kit's fonts.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// native/text shapes every script from font files into distance-field quads.
// The kit draws every text through gfx::Font; the builds compile a copy of
// that class with a door for a shaper (scripts/kit_text_patch.py), and this is
// the shaper. With it installed and the interface's four fonts bound, every
// text of every screen, of the design language and of the kit's own components
// (keyboard, dialogs, toasts) is measured, cut, wrapped and laid out by the
// engine; nothing above text.hpp changes. Without it (the engine could not
// start) the kit's baked fonts draw as before.
//
// Whoever runs the frame owns one TextEngine: begin_frame() before the frame's
// update, end_frame() after its draw and before the lists are rendered.

#pragma once

#include "afterglow/base.hpp"
#include "afterglow/platform.hpp"

#include "tube_text.hpp"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace afterglow
{

struct TextSetup
{
    // The faces the approved boards used, as font files: Latin, Greek and
    // Cyrillic are drawn with them.
    std::string regular;  // Inter Regular
    std::string semibold; // Inter SemiBold
    std::string display;  // Montserrat Medium
    std::string mono;     // DejaVu Sans Mono
    // Every other script: folders whose known fonts are found by name (the
    // console's /preinst/common/font) and faces named outright (the host's
    // stand-ins), asked in this order.
    std::vector<std::string> font_dirs;
    std::vector<tube::text::FaceSpec> faces;
    std::string language;     // the interface language ("ja-JP"): which Han forms are preferred
    float pixel_scale = 1.0f; // output pixels per virtual pixel: 1 at 1080p, 2 at 4K
    int page_size = 2048;     // one page of the glyph atlas, pixels a side
    int max_pages = 2;
};

class TextEngine final : public gfx::Font::Shaper
{
  public:
    // nullptr, with *error saying why, when the engine cannot start.
    static std::unique_ptr<TextEngine> create(Platform &platform, const TextSetup &setup, std::string *error);
    ~TextEngine() override;

    // The interface's fonts become the engine's styles and the shaper is
    // installed. fonts must outlive the engine.
    void bind(const ui::Fonts &fonts);
    void begin_frame();
    void end_frame();

    tube::text::Stats stats() const
    {
        return context_->stats();
    }
    std::string describe() const
    {
        return context_->describe();
    }
    // Clusters the last frames could not draw at all (no face had them).
    std::uint32_t missing() const
    {
        return missing_;
    }

    bool knows(const gfx::Font &font) const override;
    float measure(const gfx::Font &font, std::string_view text, float size, float tracking) override;
    std::string fit(const gfx::Font &font, std::string_view text, float size, float max_width, float tracking) override;
    float layout(const gfx::Font &font, std::string_view text, float x, float y, float size, gfx::Align align,
                 std::vector<gfx::GlyphQuad> &quads, float tracking) override;
    std::vector<std::string> wrap(const gfx::Font &font, std::string_view text, float size, float max_width) override;
    bool has_glyph(const gfx::Font &font, std::uint32_t codepoint) override;

  private:
    TextEngine(Platform &platform, std::unique_ptr<tube::text::Context> context);
    tube::text::Style style_of(const gfx::Font &font, float size, float tracking) const;
    void ensure_pages();
    void collect_updates();

    Platform &platform_;
    std::unique_ptr<tube::text::Context> context_;
    struct Bound
    {
        const gfx::Font *font = nullptr;
        tube::text::Style style;
    };
    std::array<Bound, 4> bound_{};
    struct PageTexture
    {
        std::uint32_t handle = 0;
        bool coverage = false; // a one-channel page of the platform; else RGBA made here
        bool dirty = true;
    };
    std::vector<PageTexture> pages_;
    std::vector<std::uint8_t> rgba_;
    tube::text::Layout scratch_;
    std::uint32_t missing_ = 0;
};

} // namespace afterglow
