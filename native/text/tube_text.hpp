// ProsperoTube - Text engine: every script from the console's own fonts, as glyph quads.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// UTF-8 in, positioned glyph quads out. The engine reads font files (the PS5 system software's
// own, plus any the app carries), orders text with the Unicode bidirectional algorithm (GNU
// FriBidi), picks a font for every cluster, shapes with HarfBuzz, breaks and cuts lines, and
// keeps the glyphs it draws as single-channel distance fields in a few atlas pages.
//
// It knows nothing of the interface kit or of the player: an adapter turns Quad into the
// renderer's own glyph quad and AtlasUpdate into a texture upload. docs/afterglow/TEXT.md has the
// design, the limits and the numbers.
//
// One thread uses a Context (the interface's). No global state.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace tube::text
{

// ---- fonts ----

// What a face is for. A cluster asks the face of its script first, then every face in order.
enum class FaceRole : std::uint8_t
{
    latin,    // the interface face: Latin, Greek, Cyrillic; line metrics come from the first one
    japanese, // kana, and Han in the Japanese forms
    korean,   // Hangul, and Han in the Korean forms
    chinese,  // Han in the Chinese forms, Bopomofo
    thai,
    arabic,
    other, // any other script (Hebrew, Devanagari, symbols, ...): asked in the order given
};

// A font file the app names itself: one it carries, or (on a PC) a stand-in for a console font.
struct FaceSpec
{
    std::string path;
    int index = 0; // the face inside a collection (.ttc)
    FaceRole role = FaceRole::other;
    bool bold = false; // a bold cut: asked for bold text only, and before the regular faces
    // 0: for every text. 1 to 15: only for styles of that family, and before the others (a display
    // face, a fixed-width face); what such a face lacks comes from the faces of family 0.
    std::uint8_t family = 0;
};

// How font files are read.
enum class IoMode : std::uint8_t
{
    automatic, // mapped into memory; when that is refused, read on demand
    map,       // mapped only
    stream,    // tables and glyphs read from the file when first needed, never the whole file
};

// Emoji no font draws as an outline (there is no colour atlas).
enum class EmojiPolicy : std::uint8_t
{
    strip,       // the whole sequence is left out, with the space it leaves double
    placeholder, // one neutral dot per sequence
};

// The folders that hold the console's fonts, the best first: the folder TUBE_TEXT_FONTS names (a
// PC has no console; copies of its fonts can stand in), then /preinst/common/font.
std::vector<std::string> system_font_folders();

struct Config
{
    // Folders searched for the console's fonts by their known names, in order.
    std::vector<std::string> font_dirs;
    // Faces asked before the folders' (in this order).
    std::vector<FaceSpec> faces;
    // Also offer the other font files of font_dirs (after every known one, by name).
    bool discover = true;
    // The interface language, a tag such as "ja-JP", "zh-Hans", "ar": decides which face draws
    // the characters Japanese, Chinese and Korean share when the text itself does not tell, and
    // the language-specific forms a font has.
    std::string language;
    // Output pixels per layout unit (1 for a 1920x1080 surface laid out in 1080p units, 2 at
    // 3840x2160): glyphs are rasterised for the pixels they cover, positions stay in units.
    float pixel_scale = 1.0f;

    // The glyph atlas: max_pages pages of page_size x page_size single-channel pixels. One more
    // page-sized buffer exists while a full page is compacted. Hard bound of the pixel memory:
    // (max_pages + 1) * page_size * page_size bytes.
    int page_size = 2048;
    int max_pages = 2;
    // Glyphs are rasterised with the em at one of these many pixels (ascending, 0 ends the
    // list): the smallest that is at least 0.8 of the size drawn, in output pixels.
    int raster_sizes[4] = {28, 56, 112, 0};
    // The distance field spreads this share of the em (the interface kit's baked fonts: 8 at 56).
    float spread = 8.0f / 56.0f;
    // Synthetic bold widens every stroke by this share of the em on each side.
    float bold = 1.0f / 40.0f;

    // At most this many glyphs are rasterised between two advance_frame() calls, and none is
    // started once rasterising has taken raster_ms_per_frame of the frame (0: no time limit, the
    // count alone). The others are queued, keep their place in the line and are drawn on a later
    // frame. Glyphs that draw nothing (a space) do not count.
    int glyphs_per_frame = 32;
    double raster_ms_per_frame = 4.0;
    // Shaped texts kept (least recently used leaves first).
    int layout_cache_entries = 1024;
    std::size_t layout_cache_bytes = 4u << 20;
    // Longer texts are cut here (at a cluster) and reported as truncated.
    std::uint32_t max_codepoints = 16384;
    // A font file larger than this is refused.
    std::size_t max_font_bytes = 64u << 20;
    IoMode io = IoMode::automatic;
    EmojiPolicy emoji = EmojiPolicy::strip;
};

// ---- text ----

enum class Weight : std::uint8_t
{
    regular,
    bold, // a bold face when one is named, else the regular outline widened
};

enum class Direction : std::uint8_t
{
    automatic, // each paragraph: that of its first strong letter, left to right when it has none
    ltr,
    rtl,
};

struct Style
{
    float size = 28.0f; // the em, in layout units
    Weight weight = Weight::regular;
    float tracking = 0.0f;    // added between clusters, in layout units; never inside joined writing
    float line_height = 0.0f; // baseline to baseline; 0: the interface face's own
    bool tabular_digits = false; // every digit as wide as the widest (clocks, time codes)
    Direction direction = Direction::automatic;
    std::uint8_t family = 0; // the faces named with this family first (FaceSpec::family); 0: none in particular
};

enum class Align : std::uint8_t
{
    start, // left in a left-to-right paragraph, right in a right-to-left one
    end,
    left,
    right,
    center,
};

enum class Ellipsis : std::uint8_t
{
    none,
    end,   // the end of the text is cut: "Lorem ips…" (in a right-to-left paragraph the mark is at the left)
    start, // the start is cut: "…sum dolor"; one line only
};

struct Constraints
{
    float max_width = 0.0f; // 0: no limit (no wrapping, no cutting)
    // 1: one line (line feeds become spaces). n > 1: wrapped to at most n lines. 0: wrapped
    // without limit. Wrapping needs max_width.
    int max_lines = 1;
    Ellipsis ellipsis = Ellipsis::none; // what a text that does not fit ends (or starts) with
    Align align = Align::start;
};

// One glyph: a rectangle in layout units (x to the right, y down, from the top-left corner of the
// text block) and its rectangle in an atlas page (0..1).
struct Quad
{
    float x0, y0, x1, y1;
    float u0, v0, u1, v1;
    // The distance field's spread in layout units at the size drawn: the page holds 128 on the
    // outline and falls by 128 / spread per pixel, as the kit's baked fonts do.
    float range;
    std::uint16_t page;
    std::uint16_t line;
};

struct Line
{
    std::uint32_t first_quad = 0;
    std::uint32_t quad_count = 0;
    float x = 0.0f;        // left edge of the line's ink advance
    float baseline = 0.0f; // from the top of the block
    float width = 0.0f;
    // The bytes of the text this line shows: [text_begin, text_end).
    std::uint32_t text_begin = 0;
    std::uint32_t text_end = 0;
    bool rtl = false;      // its paragraph runs right to left
    bool ellipsis = false; // it ends (or starts) with the cut mark
};

struct Layout
{
    std::vector<Quad> quads;
    std::vector<Line> lines;
    float width = 0.0f;  // the widest line
    float height = 0.0f; // lines * line_height
    float ascent = 0.0f; // of the interface face at this size; the first baseline
    float descent = 0.0f;
    float line_height = 0.0f;
    std::uint32_t pending = 0; // glyphs not rasterised yet: lay the text out again next frame
    std::uint32_t missing = 0; // clusters no face has (drawn as a box)
    bool truncated = false;    // text was cut: an ellipsis, max_lines or max_codepoints
    bool rtl = false;          // the first paragraph runs right to left
};

struct Metrics
{
    float width = 0.0f;
    float height = 0.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
    float line_height = 0.0f;
    std::uint32_t lines = 0;
    std::uint32_t missing = 0;
    bool truncated = false;
    bool rtl = false;
};

// Pixels of an atlas page that changed since the last advance_frame(): upload them before
// drawing. pixels is the top-left one; the next row is stride bytes further.
struct AtlasUpdate
{
    std::uint16_t page;
    std::uint16_t x, y, w, h;
    const std::uint8_t *pixels;
    std::uint32_t stride;
    bool created; // the page is new: make its texture (the rectangle is the whole page)
};

struct Stats
{
    std::uint32_t frame = 0;
    std::uint32_t faces = 0;        // faces known
    std::uint32_t faces_open = 0;   // faces read so far
    std::uint64_t font_bytes_mapped = 0;
    std::uint64_t font_bytes_read = 0; // read from files on demand (stream mode)
    std::uint32_t pages = 0;
    std::uint64_t atlas_bytes = 0;  // pixel memory held now
    std::uint32_t glyphs = 0;       // glyphs in the atlas
    std::uint64_t rasterised = 0;   // glyphs drawn since the start
    std::uint64_t evicted = 0;      // glyphs dropped to make room
    std::uint64_t compactions = 0;  // full pages repacked
    std::uint64_t overflows = 0;    // glyphs without room right after room was made: the atlas is too small for what a frame draws
    std::uint32_t queued = 0;       // glyphs waiting for a frame's budget
    std::uint32_t layouts = 0;      // shaped texts kept
    std::uint64_t layout_bytes = 0;
    std::uint64_t layout_hits = 0;
    std::uint64_t layout_misses = 0;
    double raster_seconds = 0.0;
    double shape_seconds = 0.0;
    double font_open_seconds = 0.0;
};

class Context
{
  public:
    // Never fails for a missing font: a face that cannot be read is skipped and named in
    // messages(); with no face at all every cluster is a box. Null (and *error) only when the
    // configuration is unusable or FreeType does not start.
    static std::unique_ptr<Context> create(const Config &config, std::string *error = nullptr);
    ~Context();
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;

    // The size of the text block. Reads fonts, never touches the atlas.
    Metrics measure(std::string_view text, const Style &style, const Constraints &constraints = {});
    // Lines of quads into *out (its vectors are reused). Glyphs new to the atlas are rasterised
    // within the frame's budget; the rest are counted in out->pending and take their place later.
    // The quads are good until the next advance_frame(): no glyph moves in the atlas or leaves
    // it while a frame is laid out. Lay texts out again every frame.
    void layout(std::string_view text, const Style &style, const Constraints &constraints, Layout *out);
    // The same, before the first frame: no budget (a loading screen warms the texts it knows).
    void prewarm(std::string_view text, const Style &style);
    // True when every cluster of the text has a glyph (after the emoji policy).
    bool can_draw(std::string_view text);
    // The text as one line no wider than max_width: itself when it fits, else cut at a cluster with
    // the mark (U+2026, or three full stops when no face has it) at the cut end. For callers that
    // keep strings; the string measures within max_width when it is laid out again.
    std::string fit(std::string_view text, const Style &style, float max_width, Ellipsis ellipsis = Ellipsis::end);

    // What changed in the atlas since the last advance_frame(). Call it after the frame's last
    // layout() and upload before drawing.
    const std::vector<AtlasUpdate> &atlas_updates();
    // Once a frame, before its first layout(): makes room in the atlas when the last frame
    // found none (the least recently used glyphs leave, others may move), starts a new budget,
    // rasterises what the last frame queued, forgets the updates handed out.
    void advance_frame();

    int page_size() const;
    int page_count() const;
    const std::uint8_t *page_pixels(int page) const;

    // The interface face's metrics at a size, for a caller that places baselines itself.
    void font_metrics(const Style &style, float *ascent, float *descent, float *line_height);

    Stats stats() const;
    // What was found, read and refused, one line each (for the log).
    const std::vector<std::string> &messages() const;
    // "SST-Medium.otf (latin, mapped 1.2 MB), ...": the faces in the order they are asked.
    std::string describe() const;

    struct Impl;

  private:
    Context();
    std::unique_ptr<Impl> impl_;
};

} // namespace tube::text
