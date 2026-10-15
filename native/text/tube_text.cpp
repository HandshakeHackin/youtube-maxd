// ProsperoTube - Text engine: the context that ties fonts, shaping and the glyph atlas together.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tube_text.hpp"

#include "tube_text_atlas.hpp"
#include "tube_text_fonts.hpp"
#include "tube_text_layout.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <list>
#include <unordered_map>
#include <unordered_set>

namespace tube::text
{

namespace
{

constexpr std::size_t kQueueLimit = 4096;

double seconds_since(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

// Everything besides the text that decides a shaped result.
struct Request
{
    float size;
    float tracking;
    float line_height;
    float max_width;
    std::int32_t max_lines;
    std::uint8_t weight;
    std::uint8_t tabular;
    std::uint8_t direction;
    std::uint8_t ellipsis;
    std::uint8_t align;
    std::uint8_t family;

    bool operator==(const Request &other) const
    {
        return size == other.size && tracking == other.tracking && line_height == other.line_height &&
               max_width == other.max_width && max_lines == other.max_lines && weight == other.weight &&
               tabular == other.tabular && direction == other.direction && ellipsis == other.ellipsis &&
               align == other.align && family == other.family;
    }
};

Request request_of(const Style &style, const Constraints &constraints)
{
    Request request{};
    request.size = style.size;
    request.tracking = style.tracking;
    request.line_height = style.line_height;
    request.max_width = constraints.max_width;
    request.max_lines = constraints.max_lines;
    request.weight = static_cast<std::uint8_t>(style.weight);
    request.tabular = style.tabular_digits ? 1 : 0;
    request.direction = static_cast<std::uint8_t>(style.direction);
    request.ellipsis = static_cast<std::uint8_t>(constraints.ellipsis);
    request.align = static_cast<std::uint8_t>(constraints.align);
    request.family = style.family & 15;
    return request;
}

std::uint64_t mix(std::uint64_t hash, const void *data, std::size_t size)
{
    const auto *bytes = static_cast<const unsigned char *>(data);
    for (std::size_t i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

std::uint64_t hash_of(std::string_view text, const Request &request)
{
    std::uint64_t hash = 14695981039346656037ull;
    hash = mix(hash, text.data(), text.size());
    hash = mix(hash, &request.size, sizeof(float));
    hash = mix(hash, &request.tracking, sizeof(float));
    hash = mix(hash, &request.line_height, sizeof(float));
    hash = mix(hash, &request.max_width, sizeof(float));
    hash = mix(hash, &request.max_lines, sizeof(std::int32_t));
    const std::uint8_t rest[6] = {request.weight, request.tabular, request.direction,
                                  request.ellipsis, request.align,   request.family};
    return mix(hash, rest, sizeof(rest));
}

} // namespace

struct Context::Impl
{
    struct Cached
    {
        std::uint64_t hash = 0;
        std::string text;
        Request request{};
        Shaped shaped;
        std::size_t bytes = 0;
    };
    struct Wanted // a glyph waiting for a frame with budget left
    {
        std::uint64_t key;
        std::uint32_t glyph;
        std::int16_t face;
        std::uint8_t bucket;
        std::uint8_t bold;
    };

    explicit Impl(const Config &c)
        : config(c), fonts(config, &messages), shaper(config, &fonts), atlas(config.page_size, config.max_pages)
    {
        bucket_count = 0;
        for (int size : config.raster_sizes)
        {
            if (size <= 0)
                break;
            ++bucket_count;
        }
        budget = config.glyphs_per_frame;
    }

    Config config;
    std::vector<std::string> messages;
    Fonts fonts;
    Shaper shaper;
    Atlas atlas;
    Rasteriser rasteriser;
    GlyphImage image;
    Layout scratch;
    int bucket_count = 0;
    std::uint32_t frame = 1;
    int budget = 0;
    double frame_seconds = 0.0; // spent rasterising in this frame

    std::list<Cached> cache; // most recently used first
    std::unordered_multimap<std::uint64_t, std::list<Cached>::iterator> index;
    std::size_t cache_bytes = 0;
    Shaped uncached;

    std::vector<Wanted> queue;
    std::unordered_set<std::uint64_t> queued;

    std::uint64_t rasterised = 0;
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    double raster_seconds = 0.0;
    double shape_seconds = 0.0;

    const Shaped &shaped(std::string_view text, const Style &style, const Constraints &constraints)
    {
        const Request request = request_of(style, constraints);
        const std::uint64_t hash = hash_of(text, request);
        const auto range = index.equal_range(hash);
        for (auto it = range.first; it != range.second; ++it)
        {
            if (it->second->request == request && it->second->text == text)
            {
                cache.splice(cache.begin(), cache, it->second);
                ++hits;
                return it->second->shaped;
            }
        }
        ++misses;
        const auto start = std::chrono::steady_clock::now();
        if (config.layout_cache_entries <= 0)
        {
            shaper.shape(text, style, constraints, &uncached);
            shape_seconds += seconds_since(start);
            return uncached;
        }
        cache.emplace_front();
        Cached &entry = cache.front();
        entry.hash = hash;
        entry.text.assign(text);
        entry.request = request;
        shaper.shape(text, style, constraints, &entry.shaped);
        entry.shaped.glyphs.shrink_to_fit();
        entry.shaped.lines.shrink_to_fit();
        entry.bytes = sizeof(Cached) + entry.text.capacity() + entry.shaped.bytes();
        cache_bytes += entry.bytes;
        index.emplace(hash, cache.begin());
        shape_seconds += seconds_since(start);
        // The newest entry always stays, however large.
        while (cache.size() > 1 && (cache.size() > static_cast<std::size_t>(config.layout_cache_entries) ||
                                    cache_bytes > config.layout_cache_bytes))
        {
            const auto last = std::prev(cache.end());
            const auto bucket = index.equal_range(last->hash);
            for (auto it = bucket.first; it != bucket.second; ++it)
            {
                if (it->second == last)
                {
                    index.erase(it);
                    break;
                }
            }
            cache_bytes -= last->bytes;
            cache.erase(last);
        }
        return cache.front().shaped;
    }

    int bucket_for(float size) const
    {
        const float pixels = size * config.pixel_scale;
        for (int b = 0; b < bucket_count; ++b)
            if (static_cast<float>(config.raster_sizes[b]) >= 0.8f * pixels)
                return b;
        return bucket_count - 1;
    }

    static std::uint64_t key_of(int face, std::uint32_t glyph, int bucket, int bold)
    {
        return (static_cast<std::uint64_t>(static_cast<std::uint16_t>(face)) << 48) |
               (static_cast<std::uint64_t>(bucket) << 40) | (static_cast<std::uint64_t>(bold) << 32) | glyph;
    }

    Placed *draw(std::uint64_t key, int face, std::uint32_t glyph, int bucket, int bold)
    {
        const auto start = std::chrono::steady_clock::now();
        const int em = config.raster_sizes[bucket];
        const float spread = config.spread * static_cast<float>(em);
        bool drawn = true;
        if (face < 0)
        {
            if (glyph == 2)
                rasteriser.dot(em, spread, &image);
            else
                rasteriser.box(em, spread, &image);
        }
        else
        {
            drawn = rasteriser.glyph(fonts.outlines(face), glyph, em, spread,
                                     bold != 0 ? config.bold * static_cast<float>(em) : 0.0f, &image);
        }
        if (!drawn)
        {
            image.w = 0;
            image.h = 0;
        }
        ++rasterised;
        Placed *placed = atlas.insert(key, image, frame);
        raster_seconds += seconds_since(start);
        return placed;
    }

    bool within_budget() const
    {
        return budget > 0 &&
               (config.raster_ms_per_frame <= 0.0 || frame_seconds * 1000.0 < config.raster_ms_per_frame);
    }

    // Draws against the frame's budget: a glyph that turns out to draw nothing costs none.
    Placed *draw_in_frame(std::uint64_t key, int face, std::uint32_t glyph, int bucket, int bold)
    {
        const double before = raster_seconds;
        Placed *placed = draw(key, face, glyph, bucket, bold);
        frame_seconds += raster_seconds - before;
        if (placed == nullptr || placed->w != 0)
            --budget;
        return placed;
    }

    // The glyph's place in the atlas; null while it waits for a frame with budget (or for room).
    Placed *glyph(int face, std::uint32_t glyph, int bucket, int bold, bool now)
    {
        const std::uint64_t key = key_of(face, glyph, bucket, bold);
        if (Placed *placed = atlas.find(key))
        {
            placed->used = frame;
            return placed;
        }
        // Once a glyph found no room, drawing more in this frame is work thrown away.
        if (atlas.wants_room())
            return nullptr;
        if (now)
            return draw(key, face, glyph, bucket, bold);
        if (within_budget())
            return draw_in_frame(key, face, glyph, bucket, bold);
        if (queue.size() < kQueueLimit && queued.insert(key).second)
            queue.push_back({key, glyph, static_cast<std::int16_t>(face), static_cast<std::uint8_t>(bucket),
                             static_cast<std::uint8_t>(bold)});
        return nullptr;
    }

    void emit(const Shaped &shaped, const Style &style, bool now, Layout *out)
    {
        out->quads.clear();
        out->lines.clear();
        out->width = shaped.width;
        out->height = shaped.height;
        out->ascent = shaped.ascent;
        out->descent = shaped.descent;
        out->line_height = shaped.line_height;
        out->pending = 0;
        out->missing = shaped.missing;
        out->truncated = shaped.truncated;
        out->rtl = shaped.rtl;
        if (bucket_count == 0)
            return;
        const float size = style.size > 0.0f && std::isfinite(style.size) ? style.size : 1.0f;
        const int bucket = bucket_for(size);
        const float inverse = 1.0f / static_cast<float>(atlas.page_size());
        out->lines.reserve(shaped.lines.size());
        out->quads.reserve(shaped.glyphs.size());
        for (std::size_t l = 0; l < shaped.lines.size(); ++l)
        {
            const ShapedLine &source = shaped.lines[l];
            Line line;
            line.first_quad = static_cast<std::uint32_t>(out->quads.size());
            line.x = source.x;
            line.baseline = source.baseline;
            line.width = source.width;
            line.text_begin = source.text_begin;
            line.text_end = source.text_end;
            line.rtl = source.rtl;
            line.ellipsis = source.ellipsis;
            for (std::uint32_t g = 0; g < source.count; ++g)
            {
                const ShapedGlyph &shaped_glyph = shaped.glyphs[source.first + g];
                int used_bucket = bucket;
                Placed *placed = glyph(shaped_glyph.face, shaped_glyph.glyph, bucket, shaped_glyph.bold, now);
                if (placed == nullptr)
                {
                    // Not drawn at this size yet: a copy at another size stands in, the larger first.
                    ++out->pending;
                    for (int other = bucket_count - 1; other >= 0 && placed == nullptr; --other)
                    {
                        if (other == bucket)
                            continue;
                        placed = atlas.find(key_of(shaped_glyph.face, shaped_glyph.glyph, other, shaped_glyph.bold));
                        used_bucket = other;
                    }
                    if (placed == nullptr)
                        continue;
                    placed->used = frame;
                }
                if (placed->w == 0)
                    continue;
                const float em = static_cast<float>(config.raster_sizes[used_bucket]);
                const float scale = size / em;
                Quad quad;
                quad.x0 = source.x + shaped_glyph.x + placed->left * scale;
                quad.y0 = source.baseline + shaped_glyph.y + placed->top * scale;
                quad.x1 = quad.x0 + static_cast<float>(placed->w) * scale;
                quad.y1 = quad.y0 + static_cast<float>(placed->h) * scale;
                quad.u0 = static_cast<float>(placed->x) * inverse;
                quad.v0 = static_cast<float>(placed->y) * inverse;
                quad.u1 = static_cast<float>(placed->x + placed->w) * inverse;
                quad.v1 = static_cast<float>(placed->y + placed->h) * inverse;
                quad.range = config.spread * em * scale;
                quad.page = placed->page;
                quad.line = static_cast<std::uint16_t>(std::min<std::size_t>(l, 0xffff));
                out->quads.push_back(quad);
            }
            line.quad_count = static_cast<std::uint32_t>(out->quads.size()) - line.first_quad;
            out->lines.push_back(line);
        }
    }
};

Context::Context() = default;
Context::~Context() = default;

std::unique_ptr<Context> Context::create(const Config &config_in, std::string *error)
{
    const auto fail = [error](const char *why) -> std::unique_ptr<Context>
    {
        if (error != nullptr)
            *error = why;
        return nullptr;
    };
    Config config = config_in;
    if (config.page_size < 256 || config.page_size > 8192)
        return fail("page_size must be between 256 and 8192");
    if (config.max_pages < 1 || config.max_pages > 16)
        return fail("max_pages must be between 1 and 16");
    if (!(config.pixel_scale > 0.0f) || !std::isfinite(config.pixel_scale))
        return fail("pixel_scale must be positive");
    if (!(config.spread > 0.01f) || config.spread > 0.5f)
        return fail("spread must be between 0.01 and 0.5 of the em");
    if (config.bold < 0.0f || config.bold > 0.2f)
        return fail("bold must be between 0 and 0.2 of the em");
    int previous = 0;
    int sizes = 0;
    for (int size : config.raster_sizes)
    {
        if (size <= 0)
            break;
        if (size <= previous || size < 8 || size > 512)
            return fail("raster_sizes must ascend, each between 8 and 512");
        // A glyph two em wide with its field around it must fit a page.
        if (size * 2 + static_cast<int>(config.spread * static_cast<float>(size)) * 2 + 8 > config.page_size)
            return fail("page_size is too small for the largest raster size");
        previous = size;
        ++sizes;
    }
    if (sizes == 0)
        return fail("raster_sizes is empty");
    config.glyphs_per_frame = std::max(config.glyphs_per_frame, 1);
    config.max_codepoints = std::max<std::uint32_t>(config.max_codepoints, 1);

    std::unique_ptr<Context> context(new Context());
    context->impl_ = std::make_unique<Impl>(config);
    if (!context->impl_->fonts.started())
        return fail("FreeType did not start");
    return context;
}

Metrics Context::measure(std::string_view text, const Style &style, const Constraints &constraints)
{
    const Shaped &shaped = impl_->shaped(text, style, constraints);
    Metrics metrics;
    metrics.width = shaped.width;
    metrics.height = shaped.height;
    metrics.ascent = shaped.ascent;
    metrics.descent = shaped.descent;
    metrics.line_height = shaped.line_height;
    metrics.lines = static_cast<std::uint32_t>(shaped.lines.size());
    metrics.missing = shaped.missing;
    metrics.truncated = shaped.truncated;
    metrics.rtl = shaped.rtl;
    return metrics;
}

void Context::layout(std::string_view text, const Style &style, const Constraints &constraints, Layout *out)
{
    impl_->emit(impl_->shaped(text, style, constraints), style, false, out);
}

void Context::prewarm(std::string_view text, const Style &style)
{
    Constraints constraints;
    constraints.max_lines = 0;
    impl_->emit(impl_->shaped(text, style, constraints), style, true, &impl_->scratch);
}

bool Context::can_draw(std::string_view text)
{
    Constraints constraints;
    constraints.max_lines = 0;
    return impl_->shaped(text, Style{}, constraints).missing == 0;
}

std::string Context::fit(std::string_view text, const Style &style, float max_width, Ellipsis ellipsis)
{
    if (ellipsis == Ellipsis::none || !(max_width > 0.0f))
        return std::string(text);
    const char *mark = can_draw("\xe2\x80\xa6") ? "\xe2\x80\xa6" : "...";
    Constraints constraints;
    constraints.ellipsis = ellipsis;
    float limit = max_width;
    // The cut text is laid out again by the caller as a text of its own, where the letter at the
    // cut may take another form than it had inside its word (an Arabic letter that no longer
    // joins what followed, and is wider for it): a string that then measures too wide gives up
    // one more cluster, and again.
    for (int round = 0; round < 16; ++round)
    {
        constraints.max_width = limit;
        const Shaped &shaped = impl_->shaped(text, style, constraints);
        if (shaped.lines.empty() || !shaped.lines.front().ellipsis)
            return std::string(text);
        const ShapedLine &line = shaped.lines.front();
        const float cut_width = line.width;
        const std::string kept(text.substr(line.text_begin, line.text_end - line.text_begin));
        if (kept.empty())
            break;
        std::string result = ellipsis == Ellipsis::start ? mark + kept : kept + mark;
        if (measure(result, style).width <= max_width + 0.01f)
            return result;
        limit = cut_width - 0.05f; // narrower than this cut: the next one is shorter
        if (!(limit > 0.0f))
            break;
    }
    return mark;
}

const std::vector<AtlasUpdate> &Context::atlas_updates()
{
    return impl_->atlas.updates();
}

void Context::acknowledge_atlas_updates()
{
    impl_->atlas.clear_updates();
}

void Context::advance_frame()
{
    Impl &impl = *impl_;
    impl.atlas.clear_updates();
    // The frame is over and its quads are drawn: now glyphs may move and leave.
    impl.atlas.make_room(impl.frame);
    ++impl.frame;
    impl.budget = impl.config.glyphs_per_frame;
    impl.frame_seconds = 0.0;
    // What the last frame could not draw comes first.
    std::size_t taken = 0;
    while (taken < impl.queue.size() && impl.within_budget() && !impl.atlas.wants_room())
    {
        const Impl::Wanted wanted = impl.queue[taken++];
        impl.queued.erase(wanted.key);
        if (impl.atlas.find(wanted.key) != nullptr)
            continue;
        impl.draw_in_frame(wanted.key, wanted.face, wanted.glyph, wanted.bucket, wanted.bold);
    }
    impl.queue.erase(impl.queue.begin(), impl.queue.begin() + static_cast<std::ptrdiff_t>(taken));
}

int Context::page_size() const
{
    return impl_->atlas.page_size();
}

int Context::page_count() const
{
    return impl_->atlas.page_count();
}

const std::uint8_t *Context::page_pixels(int page) const
{
    return impl_->atlas.pixels(page);
}

void Context::font_metrics(const Style &style, float *ascent, float *descent, float *line_height)
{
    float a = 0.0f;
    float d = 0.0f;
    float gap = 0.0f;
    impl_->fonts.metrics(impl_->fonts.primary(style.family), &a, &d, &gap);
    const float size = style.size > 0.0f ? style.size : 1.0f;
    if (ascent != nullptr)
        *ascent = a * size;
    if (descent != nullptr)
        *descent = d * size;
    if (line_height != nullptr)
        *line_height = style.line_height > 0.0f ? style.line_height : (a + d + gap) * size;
}

Stats Context::stats() const
{
    const Impl &impl = *impl_;
    Stats stats;
    stats.frame = impl.frame;
    stats.faces = static_cast<std::uint32_t>(impl.fonts.count());
    stats.faces_open = impl.fonts.open_count();
    stats.font_bytes_mapped = impl.fonts.bytes_mapped();
    stats.font_bytes_read = impl.fonts.bytes_read();
    stats.pages = static_cast<std::uint32_t>(impl.atlas.page_count());
    stats.atlas_bytes = impl.atlas.bytes();
    stats.glyphs = impl.atlas.glyph_count();
    stats.rasterised = impl.rasterised;
    stats.evicted = impl.atlas.evicted();
    stats.compactions = impl.atlas.compactions();
    stats.overflows = impl.atlas.overflows();
    stats.queued = static_cast<std::uint32_t>(impl.queue.size());
    stats.layouts = static_cast<std::uint32_t>(impl.cache.size());
    stats.layout_bytes = impl.cache_bytes;
    stats.layout_hits = impl.hits;
    stats.layout_misses = impl.misses;
    stats.raster_seconds = impl.raster_seconds;
    stats.shape_seconds = impl.shape_seconds;
    stats.font_open_seconds = impl.fonts.open_seconds();
    return stats;
}

const std::vector<std::string> &Context::messages() const
{
    return impl_->messages;
}

std::string Context::describe() const
{
    return impl_->fonts.describe();
}

} // namespace tube::text
