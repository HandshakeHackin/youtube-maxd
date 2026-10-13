// ProsperoTube - Text engine: glyphs as distance fields, and the atlas pages that keep them.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The distance field follows the family's other apps (ProsperoEden's gfx/system_fonts.cpp, same
// author, same licence): the outline is filled above the output size, the squared Euclidean
// distance transform of Felzenszwalb and Huttenlocher measures both sides of the edge, and the
// result is averaged down. Here FreeType fills the outline and partly covered pixels start the
// transform at their own sub-pixel distance.

#include "tube_text_atlas.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include <algorithm>
#include <cmath>
#include <cstring>

namespace tube::text
{

namespace
{

constexpr float kFar = 1e20f;

// One row or column: d[q] = min over p of (q - p)^2 + f[p].
void transform(const float *f, int count, float *d, int *v, float *z)
{
    const auto crossing = [f](int p, int q)
    {
        return ((f[q] + static_cast<float>(q) * static_cast<float>(q)) -
                (f[p] + static_cast<float>(p) * static_cast<float>(p))) /
               static_cast<float>(2 * (q - p));
    };
    int k = 0;
    v[0] = 0;
    z[0] = -2.0f * kFar;
    z[1] = kFar;
    for (int q = 1; q < count; ++q)
    {
        // Values lie in [0, kFar], so a crossing is above z[0]: the loop stops at k = 0 at the latest.
        float s = crossing(v[k], q);
        while (k > 0 && s <= z[k])
        {
            --k;
            s = crossing(v[k], q);
        }
        ++k;
        v[k] = q;
        z[k] = s;
        z[k + 1] = kFar;
    }
    k = 0;
    for (int q = 0; q < count; ++q)
    {
        while (z[k + 1] < static_cast<float>(q))
            ++k;
        const int p = v[k];
        d[q] = static_cast<float>((q - p) * (q - p)) + f[p];
    }
}

int floor_to(int value, int step)
{
    int q = value / step;
    if (value % step != 0 && value < 0)
        --q;
    return q * step;
}

int ceil_to(int value, int step)
{
    return -floor_to(-value, step);
}

std::uint8_t encode(float inside_distance, float spread)
{
    const float value = 128.0f + inside_distance * (128.0f / spread);
    return static_cast<std::uint8_t>(std::clamp(value + 0.5f, 0.0f, 255.0f));
}

// Distance to a box with rounded corners, negative inside.
float round_box(float px, float py, float half_x, float half_y, float radius)
{
    const float qx = std::fabs(px) - half_x + radius;
    const float qy = std::fabs(py) - half_y + radius;
    const float outside = std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f));
    return outside + std::min(std::max(qx, qy), 0.0f) - radius;
}

} // namespace

void Rasteriser::distances(std::vector<float> &grid, int width, int height)
{
    const std::size_t longest = static_cast<std::size_t>(std::max(width, height));
    f_.resize(longest);
    d_.resize(longest);
    v_.resize(longest);
    z_.resize(longest + 1);
    for (int x = 0; x < width; ++x)
    {
        for (int y = 0; y < height; ++y)
            f_[static_cast<std::size_t>(y)] = grid[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
        transform(f_.data(), height, d_.data(), v_.data(), z_.data());
        for (int y = 0; y < height; ++y)
            grid[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] = d_[static_cast<std::size_t>(y)];
    }
    for (int y = 0; y < height; ++y)
    {
        float *row = grid.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
        std::copy(row, row + width, f_.begin());
        transform(f_.data(), width, d_.data(), v_.data(), z_.data());
        std::copy(d_.begin(), d_.begin() + width, row);
    }
}

bool Rasteriser::glyph(FT_FaceRec_ *face, std::uint32_t glyph, int em, float spread, float bold, GlyphImage *out)
{
    out->w = 0;
    out->h = 0;
    out->left = 0.0f;
    out->top = 0.0f;
    out->pixels.clear();
    if (face == nullptr || em <= 0 || spread <= 0.0f)
        return false;
    // Small sizes are filled at twice the size: distances are then within a quarter pixel.
    const int super = em <= 64 ? 2 : 1;
    if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(em * super)) != 0)
        return false;
    if (FT_Load_Glyph(face, glyph, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) != 0)
        return false;
    FT_GlyphSlot slot = face->glyph;
    if (slot->format != FT_GLYPH_FORMAT_OUTLINE)
        return false;
    if (slot->outline.n_points == 0)
        return true;
    if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) != 0)
        return false;
    const FT_Bitmap &bitmap = slot->bitmap;
    if (bitmap.width == 0 || bitmap.rows == 0)
        return true;
    if (bitmap.pixel_mode != FT_PIXEL_MODE_GRAY || bitmap.buffer == nullptr)
        return false;
    const int limit = em * super * 4;
    if (static_cast<int>(bitmap.width) > limit || static_cast<int>(bitmap.rows) > limit)
        return false;
    const int bitmap_w = static_cast<int>(bitmap.width);
    const int bitmap_h = static_cast<int>(bitmap.rows);
    const int x0 = slot->bitmap_left;
    const int y0 = -slot->bitmap_top;
    const int pad = (static_cast<int>(std::ceil(spread + bold)) + 1) * super;
    // The field's pixels are whole output pixels from the pen.
    const int left = floor_to(x0 - pad, super);
    const int top = floor_to(y0 - pad, super);
    const int right = ceil_to(x0 + bitmap_w + pad, super);
    const int bottom = ceil_to(y0 + bitmap_h + pad, super);
    const int width = right - left;
    const int height = bottom - top;
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    outer_.assign(count, kFar); // from a pixel outside the glyph to the glyph
    inner_.assign(count, 0.0f); // from a pixel inside the glyph to its outside
    for (int row = 0; row < bitmap_h; ++row)
    {
        const unsigned char *source = bitmap.pitch >= 0
                                          ? bitmap.buffer + static_cast<std::ptrdiff_t>(row) * bitmap.pitch
                                          : bitmap.buffer - static_cast<std::ptrdiff_t>(bitmap_h - 1 - row) * bitmap.pitch;
        const std::size_t base = static_cast<std::size_t>(y0 - top + row) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x0 - left);
        for (int column = 0; column < bitmap_w; ++column)
        {
            const unsigned char a = source[column];
            if (a == 0)
                continue;
            const std::size_t i = base + static_cast<std::size_t>(column);
            if (a == 255)
            {
                outer_[i] = 0.0f;
                inner_[i] = kFar;
            }
            else
            {
                // A pixel the edge crosses: the edge is about (0.5 - coverage) from its centre.
                const float covered = static_cast<float>(a) / 255.0f;
                const float out_d = std::max(0.0f, 0.5f - covered);
                const float in_d = std::max(0.0f, covered - 0.5f);
                outer_[i] = out_d * out_d;
                inner_[i] = in_d * in_d;
            }
        }
    }
    distances(outer_, width, height);
    distances(inner_, width, height);

    out->w = width / super;
    out->h = height / super;
    out->left = static_cast<float>(left / super);
    out->top = static_cast<float>(top / super);
    out->pixels.resize(static_cast<std::size_t>(out->w) * static_cast<std::size_t>(out->h));
    const float scale = 1.0f / static_cast<float>(super * super * super); // the mean, in output pixels
    for (int y = 0; y < out->h; ++y)
    {
        for (int x = 0; x < out->w; ++x)
        {
            float sum = 0.0f;
            for (int dy = 0; dy < super; ++dy)
            {
                const std::size_t row = static_cast<std::size_t>(y * super + dy) * static_cast<std::size_t>(width);
                for (int dx = 0; dx < super; ++dx)
                {
                    const std::size_t i = row + static_cast<std::size_t>(x * super + dx);
                    sum += std::sqrt(inner_[i]) - std::sqrt(outer_[i]);
                }
            }
            out->pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(out->w) + static_cast<std::size_t>(x)] =
                encode(sum * scale + bold, spread);
        }
    }
    return true;
}

void Rasteriser::box(int em, float spread, GlyphImage *out)
{
    // A hollow rounded rectangle standing on the baseline.
    const float e = static_cast<float>(em);
    const float centre_x = 0.35f * e;
    const float centre_y = -0.35f * e;
    const float half_x = 0.25f * e;
    const float half_y = 0.35f * e;
    const float radius = 0.07f * e;
    const float stroke = std::max(0.035f * e, 0.75f);
    const int pad = static_cast<int>(std::ceil(spread)) + 1;
    const int left = static_cast<int>(std::floor(centre_x - half_x - stroke)) - pad;
    const int top = static_cast<int>(std::floor(centre_y - half_y - stroke)) - pad;
    const int right = static_cast<int>(std::ceil(centre_x + half_x + stroke)) + pad;
    const int bottom = static_cast<int>(std::ceil(centre_y + half_y + stroke)) + pad;
    out->w = right - left;
    out->h = bottom - top;
    out->left = static_cast<float>(left);
    out->top = static_cast<float>(top);
    out->pixels.resize(static_cast<std::size_t>(out->w) * static_cast<std::size_t>(out->h));
    for (int y = 0; y < out->h; ++y)
        for (int x = 0; x < out->w; ++x)
        {
            const float px = static_cast<float>(left + x) + 0.5f - centre_x;
            const float py = static_cast<float>(top + y) + 0.5f - centre_y;
            const float inside = stroke - std::fabs(round_box(px, py, half_x, half_y, radius));
            out->pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(out->w) + static_cast<std::size_t>(x)] = encode(inside, spread);
        }
}

void Rasteriser::dot(int em, float spread, GlyphImage *out)
{
    const float e = static_cast<float>(em);
    const float centre_x = 0.36f * e;
    const float centre_y = -0.34f * e;
    const float radius = 0.17f * e;
    const int pad = static_cast<int>(std::ceil(spread)) + 1;
    const int left = static_cast<int>(std::floor(centre_x - radius)) - pad;
    const int top = static_cast<int>(std::floor(centre_y - radius)) - pad;
    const int right = static_cast<int>(std::ceil(centre_x + radius)) + pad;
    const int bottom = static_cast<int>(std::ceil(centre_y + radius)) + pad;
    out->w = right - left;
    out->h = bottom - top;
    out->left = static_cast<float>(left);
    out->top = static_cast<float>(top);
    out->pixels.resize(static_cast<std::size_t>(out->w) * static_cast<std::size_t>(out->h));
    for (int y = 0; y < out->h; ++y)
        for (int x = 0; x < out->w; ++x)
        {
            const float px = static_cast<float>(left + x) + 0.5f - centre_x;
            const float py = static_cast<float>(top + y) + 0.5f - centre_y;
            out->pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(out->w) + static_cast<std::size_t>(x)] =
                encode(radius - std::hypot(px, py), spread);
        }
}

// ---- atlas ----

Atlas::Atlas(int page_size, int max_pages) : size_(page_size), max_pages_(max_pages)
{
}

const std::uint8_t *Atlas::pixels(int page) const
{
    if (page < 0 || page >= page_count())
        return nullptr;
    return pages_[static_cast<std::size_t>(page)].pixels.data();
}

Placed *Atlas::find(std::uint64_t key)
{
    const auto found = glyphs_.find(key);
    return found == glyphs_.end() ? nullptr : &found->second;
}

void Atlas::touch(int page, int x, int y, int w, int h)
{
    Page &p = pages_[static_cast<std::size_t>(page)];
    if (p.whole)
        return;
    const Rect rect{x, y, x + w, y + h};
    // Glyphs drawn one after the other sit side by side on a shelf: one rectangle grows.
    for (Rect &known : p.dirty)
    {
        const bool overlap_y = rect.y0 <= known.y1 && rect.y1 >= known.y0;
        const bool overlap_x = rect.x0 <= known.x1 + 2 && rect.x1 >= known.x0 - 2;
        if (overlap_x && overlap_y)
        {
            known.x0 = std::min(known.x0, rect.x0);
            known.y0 = std::min(known.y0, rect.y0);
            known.x1 = std::max(known.x1, rect.x1);
            known.y1 = std::max(known.y1, rect.y1);
            return;
        }
    }
    if (p.dirty.size() >= 8)
    {
        Rect all = rect;
        for (const Rect &known : p.dirty)
        {
            all.x0 = std::min(all.x0, known.x0);
            all.y0 = std::min(all.y0, known.y0);
            all.x1 = std::max(all.x1, known.x1);
            all.y1 = std::max(all.y1, known.y1);
        }
        p.dirty.clear();
        p.dirty.push_back(all);
        return;
    }
    p.dirty.push_back(rect);
}

void Atlas::copy(const std::uint8_t *source, int w, int h, int page, int x, int y)
{
    Page &p = pages_[static_cast<std::size_t>(page)];
    for (int row = 0; row < h; ++row)
        std::memcpy(&p.pixels[static_cast<std::size_t>(y + row) * static_cast<std::size_t>(size_) + static_cast<std::size_t>(x)],
                    source + static_cast<std::size_t>(row) * static_cast<std::size_t>(w), static_cast<std::size_t>(w));
    touch(page, x, y, w, h);
}

bool Atlas::place(int w, int h, int only_page, int *page, int *x, int *y)
{
    // One pixel of nothing right of and under every glyph (and along the page's edges).
    const int need_w = w + 1;
    const int need_h = h + 1;
    if (need_w + 1 > size_ || need_h + 1 > size_)
        return false;
    const int first = only_page >= 0 ? only_page : 0;
    const int last = only_page >= 0 ? only_page + 1 : page_count();
    // A shelf of about this height with room left, the closest fit first.
    int best_page = -1;
    int best_shelf = -1;
    int best_waste = 0;
    for (int p = first; p < last; ++p)
    {
        const Page &candidate = pages_[static_cast<std::size_t>(p)];
        for (std::size_t s = 0; s < candidate.shelves.size(); ++s)
        {
            const Shelf &shelf = candidate.shelves[s];
            const int waste = shelf.h - need_h;
            if (waste < 0 || waste > std::max(3, need_h / 8) || shelf.x + need_w > size_)
                continue;
            if (best_page < 0 || waste < best_waste)
            {
                best_page = p;
                best_shelf = static_cast<int>(s);
                best_waste = waste;
            }
        }
    }
    if (best_page >= 0)
    {
        Shelf &shelf = pages_[static_cast<std::size_t>(best_page)].shelves[static_cast<std::size_t>(best_shelf)];
        *page = best_page;
        *x = shelf.x;
        *y = shelf.y;
        shelf.x += need_w;
        return true;
    }
    for (int p = first; p < last; ++p)
    {
        Page &candidate = pages_[static_cast<std::size_t>(p)];
        if (candidate.next_y + need_h > size_)
            continue;
        const int shelf_h = std::min((need_h + 3) / 4 * 4, size_ - candidate.next_y);
        candidate.shelves.push_back({candidate.next_y, shelf_h, 1 + need_w});
        *page = p;
        *x = 1;
        *y = candidate.next_y;
        candidate.next_y += shelf_h;
        return true;
    }
    if (only_page < 0 && page_count() < max_pages_)
    {
        pages_.emplace_back();
        Page &fresh = pages_.back();
        fresh.pixels.assign(static_cast<std::size_t>(size_) * static_cast<std::size_t>(size_), 0);
        fresh.created = true;
        fresh.whole = true;
        return place(w, h, page_count() - 1, page, x, y);
    }
    return false;
}

bool Atlas::make_room(int w, int h, std::uint32_t frame)
{
    const std::uint64_t page_area = static_cast<std::uint64_t>(size_) * static_cast<std::uint64_t>(size_);
    const std::uint64_t wanted = static_cast<std::uint64_t>(w + 1) * static_cast<std::uint64_t>(h + 1);
    const auto area = [](const Placed &g) { return static_cast<std::uint64_t>(g.w + 1) * static_cast<std::uint64_t>(g.h + 1); };
    const auto in_use = [frame](const Placed &g) { return g.used + 1 >= frame; };
    std::vector<std::uint64_t> held(pages_.size(), 0);
    for (const auto &entry : glyphs_)
        if (entry.second.page != 0xffff && in_use(entry.second))
            held[entry.second.page] += area(entry.second);
    std::vector<int> order(pages_.size());
    for (std::size_t i = 0; i < order.size(); ++i)
        order[i] = static_cast<int>(i);
    std::sort(order.begin(), order.end(), [&](int a, int b)
              { return held[static_cast<std::size_t>(a)] != held[static_cast<std::size_t>(b)] ? held[static_cast<std::size_t>(a)] < held[static_cast<std::size_t>(b)] : a < b; });

    for (const int victim : order)
    {
        // Shelves waste some of a page: what must stay may take at most 85% of it.
        if ((held[static_cast<std::size_t>(victim)] + wanted) * 100 > page_area * 85)
            continue;
        struct Kept
        {
            std::uint64_t key;
            Placed *glyph;
            std::size_t offset; // of its pixels in the copy
        };
        std::vector<Kept> on_page;
        std::uint64_t idle_area = 0; // of the glyphs no recent frame drew
        for (auto &entry : glyphs_)
        {
            if (entry.second.page != victim)
                continue;
            on_page.push_back({entry.first, &entry.second, 0});
            if (!in_use(entry.second))
                idle_area += area(entry.second);
        }
        if (idle_area == 0)
            continue; // nothing may leave this page
        std::sort(on_page.begin(), on_page.end(), [](const Kept &a, const Kept &b)
                  { return a.glyph->used != b.glyph->used ? a.glyph->used > b.glyph->used : a.key < b.key; });
        // Of the idle glyphs the more recently used half stays (by area), and no more than brings
        // the page to half full: a page is seldom repacked, and what was just seen is still there.
        std::uint64_t kept_area = 0;
        std::uint64_t kept_idle = 0;
        std::size_t kept_count = 0;
        for (; kept_count < on_page.size(); ++kept_count)
        {
            const Placed &g = *on_page[kept_count].glyph;
            if (!in_use(g))
            {
                if ((kept_idle + area(g)) * 2 > idle_area || (kept_area + area(g)) * 2 > page_area)
                    break;
                kept_idle += area(g);
            }
            kept_area += area(g);
        }
        // The pixels of what stays, then an empty page, then those glyphs again, tallest first.
        Page &page = pages_[static_cast<std::size_t>(victim)];
        std::vector<std::uint8_t> saved;
        saved.reserve(static_cast<std::size_t>(kept_area));
        for (std::size_t i = 0; i < kept_count; ++i)
        {
            Kept &kept = on_page[i];
            kept.offset = saved.size();
            for (int row = 0; row < kept.glyph->h; ++row)
            {
                const std::uint8_t *source = &page.pixels[static_cast<std::size_t>(kept.glyph->y + row) * static_cast<std::size_t>(size_) + kept.glyph->x];
                saved.insert(saved.end(), source, source + kept.glyph->w);
            }
        }
        for (std::size_t i = kept_count; i < on_page.size(); ++i)
        {
            glyphs_.erase(on_page[i].key);
            ++evicted_;
            --placed_count_;
        }
        on_page.resize(kept_count);
        std::fill(page.pixels.begin(), page.pixels.end(), std::uint8_t{0});
        page.shelves.clear();
        page.next_y = 1;
        page.dirty.clear();
        page.whole = true;
        std::stable_sort(on_page.begin(), on_page.end(), [](const Kept &a, const Kept &b) { return a.glyph->h > b.glyph->h; });
        for (const Kept &kept : on_page)
        {
            int at_page = 0;
            int x = 0;
            int y = 0;
            if (!place(kept.glyph->w, kept.glyph->h, victim, &at_page, &x, &y))
            {
                glyphs_.erase(kept.key);
                ++evicted_;
                --placed_count_;
                continue;
            }
            kept.glyph->x = static_cast<std::uint16_t>(x);
            kept.glyph->y = static_cast<std::uint16_t>(y);
            copy(saved.data() + kept.offset, kept.glyph->w, kept.glyph->h, victim, x, y);
        }
        ++compactions_;
        return true;
    }
    return false;
}

Placed *Atlas::insert(std::uint64_t key, const GlyphImage &image, std::uint32_t frame)
{
    if (image.w <= 0 || image.h <= 0)
    {
        // Glyphs that draw nothing take no room; they are few (spaces, joiners).
        if (glyphs_.size() > static_cast<std::size_t>(placed_count_) + 16384)
        {
            for (auto it = glyphs_.begin(); it != glyphs_.end();)
                it = it->second.page == 0xffff ? glyphs_.erase(it) : std::next(it);
        }
        Placed empty;
        empty.used = frame;
        return &glyphs_.insert_or_assign(key, empty).first->second;
    }
    int page = 0;
    int x = 0;
    int y = 0;
    if (!place(image.w, image.h, -1, &page, &x, &y))
    {
        if (!make_room(image.w, image.h, frame) || !place(image.w, image.h, -1, &page, &x, &y))
        {
            ++overflows_;
            return nullptr;
        }
    }
    copy(image.pixels.data(), image.w, image.h, page, x, y);
    Placed placed;
    placed.page = static_cast<std::uint16_t>(page);
    placed.x = static_cast<std::uint16_t>(x);
    placed.y = static_cast<std::uint16_t>(y);
    placed.w = static_cast<std::uint16_t>(image.w);
    placed.h = static_cast<std::uint16_t>(image.h);
    placed.left = image.left;
    placed.top = image.top;
    placed.used = frame;
    const auto result = glyphs_.insert_or_assign(key, placed);
    if (result.second)
        ++placed_count_;
    return &result.first->second;
}

const std::vector<AtlasUpdate> &Atlas::updates()
{
    updates_.clear();
    for (std::size_t p = 0; p < pages_.size(); ++p)
    {
        const Page &page = pages_[p];
        const auto add = [&](int x0, int y0, int x1, int y1, bool created)
        {
            AtlasUpdate update;
            update.page = static_cast<std::uint16_t>(p);
            update.x = static_cast<std::uint16_t>(x0);
            update.y = static_cast<std::uint16_t>(y0);
            update.w = static_cast<std::uint16_t>(x1 - x0);
            update.h = static_cast<std::uint16_t>(y1 - y0);
            update.pixels = page.pixels.data() + static_cast<std::size_t>(y0) * static_cast<std::size_t>(size_) + static_cast<std::size_t>(x0);
            update.stride = static_cast<std::uint32_t>(size_);
            update.created = created;
            updates_.push_back(update);
        };
        if (page.whole)
            add(0, 0, size_, size_, page.created);
        else
            for (const Rect &rect : page.dirty)
                add(rect.x0, rect.y0, rect.x1, rect.y1, false);
    }
    return updates_;
}

void Atlas::clear_updates()
{
    for (Page &page : pages_)
    {
        page.dirty.clear();
        page.whole = false;
        page.created = false;
    }
    updates_.clear();
}

} // namespace tube::text
