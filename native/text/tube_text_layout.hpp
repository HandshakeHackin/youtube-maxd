// ProsperoTube - Text engine: from a text to lines of positioned glyphs.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "tube_text.hpp"
#include "tube_text_fonts.hpp"

#include <hb.h>

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tube::text
{

// A glyph of a shaped text. Positions are in layout units and do not depend on the atlas.
struct ShapedGlyph
{
    std::uint32_t glyph;  // the index in its face; for face -1, 1: the missing-glyph box, 2: the emoji dot
    std::int16_t face;    // -1: drawn by the engine
    std::uint8_t bold;    // the outline is widened
    std::uint8_t reserved;
    float x; // the glyph's origin from the line's left edge
    float y; // below the baseline
};

struct ShapedLine
{
    std::uint32_t first = 0;
    std::uint32_t count = 0;
    float x = 0.0f;
    float baseline = 0.0f;
    float width = 0.0f;
    std::uint32_t text_begin = 0;
    std::uint32_t text_end = 0;
    bool rtl = false;
    bool ellipsis = false;
};

struct Shaped
{
    std::vector<ShapedGlyph> glyphs;
    std::vector<ShapedLine> lines;
    float width = 0.0f;
    float height = 0.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
    float line_height = 0.0f;
    std::uint32_t missing = 0;
    bool truncated = false;
    bool rtl = false;

    std::size_t bytes() const
    {
        return sizeof(*this) + glyphs.capacity() * sizeof(ShapedGlyph) + lines.capacity() * sizeof(ShapedLine);
    }
};

class Shaper
{
  public:
    Shaper(const Config &config, Fonts *fonts);
    ~Shaper();
    Shaper(const Shaper &) = delete;
    Shaper &operator=(const Shaper &) = delete;

    void shape(std::string_view text, const Style &style, const Constraints &constraints, Shaped *out);

  private:
    // A glyph of a run, in the order it is drawn (left to right).
    struct RunGlyph
    {
        std::uint32_t glyph;
        std::uint32_t cluster; // the code point its cluster starts at
        float advance;
        float x;
        float y;
        float gap;          // letter-spacing included in the advance
        std::uint8_t kind;  // 0: of the run's face; 1: box; 2: dot
    };
    // Neighbours drawn by one face in one direction.
    struct Run
    {
        std::uint32_t begin;
        std::uint32_t end;
        std::uint32_t first_glyph;
        std::uint32_t glyph_count;
        hb_script_t script;
        std::int16_t face;
        std::uint8_t level;
        std::uint8_t kind; // 0: shaped; 1: boxes; 2: dots; 3: nothing to draw
        std::uint8_t bold;
    };
    struct Mark // the cut mark, shaped once per face
    {
        std::vector<RunGlyph> glyphs;
        float width = 0.0f;
        std::int16_t face = -1;
        std::uint8_t bold = 0;
    };
    struct Progress
    {
        int lines = 0;
        bool stop = false;
    };

    void paragraph(std::uint32_t begin, std::uint32_t end, bool more_after, const Style &style,
                   const Constraints &constraints, Progress *progress, Shaped *out);
    void filter_emoji(const Style &style);
    void resolve_scripts();
    void resolve_levels(Direction direction);
    void assign_faces(const Style &style, Shaped *out);
    void build_runs();
    void shape_run(Run &run, const Style &style, Shaped *out);
    const Mark &mark_for(int face_hint, const Style &style);
    std::uint32_t next_cluster(std::uint32_t at) const;
    bool hangs(std::uint32_t at) const;
    std::uint32_t find_break(std::uint32_t from, float max_width, float tracking) const;
    void emit_line(std::uint32_t from, std::uint32_t to, Ellipsis mark, const Style &style, Shaped *out);

    Config config_;
    Fonts *fonts_;
    hb_buffer_t *buffer_;
    hb_language_t language_;
    hb_language_t han_language_[3];

    // The whole text.
    std::vector<char32_t> cps_;
    std::vector<std::uint32_t> offsets_;
    // The paragraph in work, in logical order.
    std::vector<char32_t> p_;
    std::vector<std::uint32_t> p_begin_; // byte where each character starts
    std::vector<std::uint32_t> p_end_;   // ... and ends
    std::vector<std::uint8_t> starts_;   // a user-perceived character starts here
    std::vector<std::uint8_t> breaks_;
    std::vector<hb_script_t> own_script_;
    std::vector<hb_script_t> script_;
    std::vector<std::uint8_t> level_;
    std::vector<std::int16_t> face_;
    std::vector<std::uint8_t> kind_;
    std::vector<std::uint8_t> bold_;
    std::vector<std::uint8_t> cluster_; // a shaping cluster starts here: lines break and cut only there
    std::vector<float> advance_;        // of the cluster starting here
    std::vector<float> gap_;            // the letter-spacing in that advance
    std::vector<Run> runs_;
    std::vector<RunGlyph> run_glyphs_;
    std::unordered_map<int, Mark> marks_;
    std::vector<std::uint32_t> scratch_;
    int base_level_ = 0;
    HanForms han_ = HanForms::japanese;
};

} // namespace tube::text
