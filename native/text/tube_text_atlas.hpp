// ProsperoTube - Text engine: glyphs as distance fields, and the atlas pages that keep them.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "tube_text.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

struct FT_FaceRec_;

namespace tube::text
{

// A glyph as a distance field: 128 on the outline, falling by 128 / spread per pixel outward.
struct GlyphImage
{
    int w = 0; // 0: nothing to draw (a space)
    int h = 0;
    float left = 0.0f; // the top-left pixel from the pen on the baseline, y down
    float top = 0.0f;
    std::vector<std::uint8_t> pixels;
};

// The glyphs the engine draws itself (no font has them).
constexpr float kBoxAdvance = 0.70f; // a cluster no face has, in em
constexpr float kDotAdvance = 0.72f; // an emoji sequence, with EmojiPolicy::placeholder

class Rasteriser
{
  public:
    // The glyph's outline with the em at `em` pixels, as a distance field spreading `spread`
    // pixels; `bold` pixels are added to every stroke on each side. False when FreeType cannot
    // draw it (the caller keeps it as an empty glyph). The outline is filled at `fill` times the
    // size and averaged down; 0 picks the factor by size (supersampling()).
    bool glyph(FT_FaceRec_ *face, std::uint32_t glyph, int em, float spread, float bold, GlyphImage *out, int fill = 0);
    static int supersampling(int em);
    void box(int em, float spread, GlyphImage *out);
    void dot(int em, float spread, GlyphImage *out);

  private:
    bool distances(int width, int height, int x0, int y0, int x1, int y1);

    // Kept between glyphs so that drawing one allocates nothing once the largest was seen.
    std::vector<std::uint8_t> coverage_; // how much of each pixel the glyph covers
    std::vector<float> grid_;            // squared distances, then the signed distance
    std::vector<float> offset_;          // of an edge pixel: how far inside its centre is
    std::vector<int> nearest_row_;       // of a pixel: the row of the nearest edge pixel in its column
    std::vector<float> f_;
    std::vector<float> d_;
    std::vector<float> z_;
    std::vector<int> from_;
    std::vector<int> v_;
};

// Where a glyph is in the atlas.
struct Placed
{
    std::uint16_t page = 0xffff; // 0xffff: it draws nothing
    std::uint16_t x = 0;
    std::uint16_t y = 0;
    std::uint16_t w = 0;
    std::uint16_t h = 0;
    float left = 0.0f;
    float top = 0.0f;
    std::uint32_t used = 0; // the last frame that drew it
};

class Atlas
{
  public:
    Atlas(int page_size, int max_pages);

    Placed *find(std::uint64_t key);
    // Copies the image into a page. Null when every page is full: nothing moves while a frame is
    // laid out, room is made by make_room() once it is over.
    Placed *insert(std::uint64_t key, const GlyphImage &image, std::uint32_t frame);
    // True when insert() found no room since the last make_room().
    bool wants_room() const
    {
        return wants_room_;
    }
    // Between two frames, when room was wanted: the page that `frame` (the one just over) used
    // least is repacked. What that frame drew stays; of the rest the more recently used half
    // stays (and no more than leaves the page half full), the others are dropped. Glyphs that
    // stay may move, so quads handed out before are void. True when a page was repacked.
    bool make_room(std::uint32_t frame);

    const std::vector<AtlasUpdate> &updates();
    void clear_updates();

    int page_size() const
    {
        return size_;
    }
    int page_count() const
    {
        return static_cast<int>(pages_.size());
    }
    const std::uint8_t *pixels(int page) const;
    std::uint32_t glyph_count() const
    {
        return placed_count_;
    }
    std::uint64_t evicted() const
    {
        return evicted_;
    }
    std::uint64_t compactions() const
    {
        return compactions_;
    }
    std::uint64_t overflows() const
    {
        return overflows_;
    }
    std::uint64_t bytes() const
    {
        return static_cast<std::uint64_t>(pages_.size()) * static_cast<std::uint64_t>(size_) * static_cast<std::uint64_t>(size_);
    }

  private:
    struct Rect
    {
        int x0, y0, x1, y1;
    };
    struct Shelf
    {
        int y, h, x;
    };
    struct Page
    {
        std::vector<std::uint8_t> pixels;
        std::vector<Shelf> shelves;
        int next_y = 1;
        std::vector<Rect> dirty;
        bool whole = false;   // every pixel changed
        bool created = false; // since the updates were last cleared
    };

    bool place(int w, int h, int only_page, int *page, int *x, int *y);
    void copy(const std::uint8_t *source, int w, int h, int page, int x, int y);
    void touch(int page, int x, int y, int w, int h);

    int size_;
    int max_pages_;
    std::vector<Page> pages_;
    std::unordered_map<std::uint64_t, Placed> glyphs_;
    std::vector<AtlasUpdate> updates_;
    std::uint32_t placed_count_ = 0;
    bool wants_room_ = false;
    bool repacked_ = false; // by the last make_room()
    bool stuck_ = false;    // the last make_room() found nothing that may leave
    int wanted_w_ = 0;      // the largest glyph that found no room
    int wanted_h_ = 0;
    std::uint64_t evicted_ = 0;
    std::uint64_t compactions_ = 0;
    std::uint64_t overflows_ = 0;
};

} // namespace tube::text
