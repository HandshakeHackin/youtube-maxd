// ProsperoTube Afterglow - pictures: fetch, decode, scale, upload, retire.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/images.hpp"

// A private copy of the decoder, JPEG and PNG only: EVO's own copies are private to their files too.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "stb_image.h"
#pragma GCC diagnostic pop

#include <algorithm>
#include <cmath>
#include <cstring>

namespace afterglow
{

namespace
{

constexpr int kAmbientW = 64;
constexpr int kAmbientH = 36;
constexpr float kRetryAfter = 30.0f; // seconds before a failed address is asked again

int longest_side(PictureSize size)
{
    switch (size)
    {
    case PictureSize::avatar:
        return 96;
    case PictureSize::card:
        return 512;
    case PictureSize::hero:
        return 1280;
    case PictureSize::ambient:
        return kAmbientW;
    }
    return 512;
}

// Area-averaged copy of src at dw x dh.
std::vector<std::uint8_t> shrink(const std::uint8_t *src, int sw, int sh, int dw, int dh)
{
    std::vector<std::uint8_t> out(static_cast<std::size_t>(dw) * static_cast<std::size_t>(dh) * 4u);
    for (int y = 0; y < dh; ++y)
    {
        const int y0 = y * sh / dh;
        const int y1 = std::max(y0 + 1, (y + 1) * sh / dh);
        for (int x = 0; x < dw; ++x)
        {
            const int x0 = x * sw / dw;
            const int x1 = std::max(x0 + 1, (x + 1) * sw / dw);
            unsigned sum[4] = {0, 0, 0, 0};
            for (int yy = y0; yy < y1; ++yy)
            {
                const std::uint8_t *row = src + (static_cast<std::size_t>(yy) * static_cast<std::size_t>(sw) + static_cast<std::size_t>(x0)) * 4u;
                for (int xx = x0; xx < x1; ++xx, row += 4)
                {
                    sum[0] += row[0];
                    sum[1] += row[1];
                    sum[2] += row[2];
                    sum[3] += row[3];
                }
            }
            const unsigned count = static_cast<unsigned>((y1 - y0) * (x1 - x0));
            std::uint8_t *pixel = &out[(static_cast<std::size_t>(y) * static_cast<std::size_t>(dw) + static_cast<std::size_t>(x)) * 4u];
            for (int c = 0; c < 4; ++c)
                pixel[c] = static_cast<std::uint8_t>((sum[c] + count / 2) / count);
        }
    }
    return out;
}

// The small blurred copy the page ground stretches: 64 x 36, opaque.
std::vector<std::uint8_t> ambient_of(const std::uint8_t *rgba, int width, int height)
{
    std::vector<std::uint8_t> small = shrink(rgba, width, height, kAmbientW, kAmbientH);
    std::vector<std::uint8_t> other(small.size());
    for (int pass = 0; pass < 3; ++pass)
    {
        for (int y = 0; y < kAmbientH; ++y)
            for (int x = 0; x < kAmbientW; ++x)
            {
                unsigned sum[3] = {0, 0, 0};
                unsigned count = 0;
                for (int dy = -2; dy <= 2; ++dy)
                    for (int dx = -2; dx <= 2; ++dx)
                    {
                        const int xx = std::clamp(x + dx, 0, kAmbientW - 1);
                        const int yy = std::clamp(y + dy, 0, kAmbientH - 1);
                        const std::uint8_t *p = &small[static_cast<std::size_t>(yy * kAmbientW + xx) * 4u];
                        sum[0] += p[0];
                        sum[1] += p[1];
                        sum[2] += p[2];
                        ++count;
                    }
                std::uint8_t *q = &other[static_cast<std::size_t>(y * kAmbientW + x) * 4u];
                q[0] = static_cast<std::uint8_t>(sum[0] / count);
                q[1] = static_cast<std::uint8_t>(sum[1] / count);
                q[2] = static_cast<std::uint8_t>(sum[2] / count);
                q[3] = 255;
            }
        small.swap(other);
    }
    return small;
}

// The average colour and the most vivid one, from a coarse grid of the picture.
void colours_of(const std::uint8_t *rgba, int width, int height, Color *tone, Color *glow)
{
    constexpr int kCols = 16;
    constexpr int kRows = 9;
    const std::vector<std::uint8_t> grid = shrink(rgba, width, height, kCols, kRows);
    double sum[3] = {0, 0, 0};
    float best = -1.0f;
    Color vivid{0.5f, 0.5f, 0.5f, 1.0f};
    for (int i = 0; i < kCols * kRows; ++i)
    {
        const float r = grid[static_cast<std::size_t>(i) * 4u] / 255.0f;
        const float g = grid[static_cast<std::size_t>(i) * 4u + 1] / 255.0f;
        const float b = grid[static_cast<std::size_t>(i) * 4u + 2] / 255.0f;
        sum[0] += r;
        sum[1] += g;
        sum[2] += b;
        const float high = std::max(r, std::max(g, b));
        const float low = std::min(r, std::min(g, b));
        const float saturation = high > 0.0f ? (high - low) / high : 0.0f;
        const float score = saturation * (0.35f + 0.65f * high);
        if (score > best)
        {
            best = score;
            vivid = Color{r, g, b, 1.0f};
        }
    }
    const float n = static_cast<float>(kCols * kRows);
    *tone = Color{static_cast<float>(sum[0]) / n, static_cast<float>(sum[1]) / n, static_cast<float>(sum[2]) / n, 1.0f};
    // Light must be bright enough to read as light: lift a dark vivid colour.
    const float high = std::max(vivid.r, std::max(vivid.g, vivid.b));
    if (high > 0.0f && high < 0.75f)
    {
        const float lift = 0.75f / high;
        vivid = Color{std::min(vivid.r * lift, 1.0f), std::min(vivid.g * lift, 1.0f), std::min(vivid.b * lift, 1.0f), 1.0f};
    }
    *glow = vivid;
}

} // namespace

Rect Picture::cover(float aspect) const
{
    if (width <= 0 || height <= 0 || aspect <= 0.0f)
        return gfx::kFullUv;
    const float own = static_cast<float>(width) / static_cast<float>(height);
    if (own > aspect)
    {
        const float w = aspect / own;
        return {(1.0f - w) * 0.5f, 0.0f, w, 1.0f};
    }
    const float h = own / aspect;
    return {0.0f, (1.0f - h) * 0.5f, 1.0f, h};
}

bool decode_picture(const std::string &bytes, int longest, Decoded *out)
{
    int width = 0;
    int height = 0;
    int channels = 0;
    if (bytes.empty() || bytes.size() > 16u * 1024u * 1024u)
        return false;
    const auto *data = reinterpret_cast<const stbi_uc *>(bytes.data());
    if (!stbi_info_from_memory(data, static_cast<int>(bytes.size()), &width, &height, &channels) || width <= 0 ||
        height <= 0 || width > 4096 || height > 4096)
        return false;
    stbi_uc *pixels = stbi_load_from_memory(data, static_cast<int>(bytes.size()), &width, &height, &channels, 4);
    if (pixels == nullptr)
        return false;
    const int side = std::max(width, height);
    if (longest > 0 && side > longest)
    {
        out->width = std::max(1, width * longest / side);
        out->height = std::max(1, height * longest / side);
        out->rgba = shrink(pixels, width, height, out->width, out->height);
    }
    else
    {
        out->width = width;
        out->height = height;
        out->rgba.assign(pixels, pixels + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
    }
    stbi_image_free(pixels);
    colours_of(out->rgba.data(), out->width, out->height, &out->tone, &out->glow);
    return true;
}

struct ImageCache::Entry
{
    enum class State : std::uint8_t
    {
        queued,
        fetching,
        fetched,
        ready,
        failed,
    };
    std::string key;
    std::string url;
    PictureSize size = PictureSize::card;
    State state = State::queued;
    unsigned ticket = 0;
    std::string bytes;
    Picture picture;
    std::size_t texture_bytes = 0;
    std::vector<std::uint8_t> ambient; // 64 x 36 pixels, kept for the ambient entry of the same address
    unsigned touched = 0;
    float failed_for = 0.0f;
};

ImageCache::ImageCache(Platform &platform) : platform_(platform), alive_(std::make_shared<int>(0)) {}

ImageCache::~ImageCache()
{
    clear();
}

void ImageCache::retire(Entry &entry)
{
    if (entry.ticket != 0)
    {
        platform_.cancel_fetch(entry.ticket);
        entry.ticket = 0;
        if (entry.state == Entry::State::fetching)
            --fetching_;
    }
    if (entry.picture.texture != 0)
    {
        platform_.release_texture(entry.picture.texture);
        bytes_ -= std::min(bytes_, entry.texture_bytes);
        entry.picture.texture = 0;
    }
}

void ImageCache::clear()
{
    for (auto &pair : entries_)
        retire(*pair.second);
    entries_.clear();
    queue_.clear();
    bytes_ = 0;
    fetching_ = 0;
}

int ImageCache::pending() const
{
    int count = 0;
    for (const auto &pair : entries_)
        count += pair.second->state == Entry::State::queued || pair.second->state == Entry::State::fetching ||
                 pair.second->state == Entry::State::fetched;
    return count;
}

int ImageCache::ready() const
{
    int count = 0;
    for (const auto &pair : entries_)
        count += pair.second->state == Entry::State::ready;
    return count;
}

Picture ImageCache::get(std::string_view url, PictureSize size)
{
    if (url.empty())
        return {};
    std::string key(url);
    key.push_back('#');
    key.push_back(static_cast<char>('0' + static_cast<int>(size)));
    auto found = entries_.find(key);
    if (found == entries_.end())
    {
        auto entry = std::make_unique<Entry>();
        entry->key = key;
        entry->url = std::string(url);
        entry->size = size;
        Entry *raw = entry.get();
        found = entries_.emplace(key, std::move(entry)).first;
        queue_.push_back(raw);
    }
    Entry &entry = *found->second;
    entry.touched = frame_;
    if (entry.state == Entry::State::failed && entry.failed_for >= kRetryAfter)
    {
        entry.state = Entry::State::queued;
        entry.picture.failed = false;
        queue_.push_back(&entry);
    }
    return entry.picture;
}

void ImageCache::pump(float dt)
{
    ++frame_;
    // Retire what nobody asked for in a while, then what exceeds the budget.
    for (auto it = entries_.begin(); it != entries_.end();)
    {
        Entry &entry = *it->second;
        if (entry.state == Entry::State::ready)
            entry.picture.age += dt;
        if (entry.state == Entry::State::failed)
            entry.failed_for += dt;
        if (frame_ - entry.touched > static_cast<unsigned>(kIdleFrames))
        {
            retire(entry);
            queue_.erase(std::remove(queue_.begin(), queue_.end(), &entry), queue_.end());
            it = entries_.erase(it);
        }
        else
        {
            ++it;
        }
    }
    while (bytes_ > kBudgetBytes)
    {
        Entry *oldest = nullptr;
        for (auto &pair : entries_)
        {
            Entry &entry = *pair.second;
            if (entry.state == Entry::State::ready && entry.touched + 1 < frame_ &&
                (oldest == nullptr || entry.touched < oldest->touched))
                oldest = &entry;
        }
        if (oldest == nullptr)
            break; // everything is on screen: the budget gives way rather than the page
        retire(*oldest);
        const std::string key = oldest->key;
        entries_.erase(key);
    }

    // Ask the platform for the next addresses: the ones asked for most recently first.
    while (fetching_ < kMaxFetches && !queue_.empty())
    {
        auto next = std::max_element(queue_.begin(), queue_.end(),
                                     [](const Entry *a, const Entry *b) { return a->touched < b->touched; });
        Entry *entry = *next;
        queue_.erase(next);
        if (entry->state != Entry::State::queued)
            continue;
        // The ambient copy of an address that is already decoded needs no second fetch.
        if (entry->size == PictureSize::ambient)
        {
            for (const PictureSize other : {PictureSize::card, PictureSize::hero, PictureSize::avatar})
            {
                std::string key = entry->url;
                key.push_back('#');
                key.push_back(static_cast<char>('0' + static_cast<int>(other)));
                const auto sibling = entries_.find(key);
                if (sibling == entries_.end() || sibling->second->ambient.empty())
                    continue;
                entry->picture.texture = platform_.create_texture(kAmbientW, kAmbientH, sibling->second->ambient.data());
                entry->picture.width = kAmbientW;
                entry->picture.height = kAmbientH;
                entry->picture.tone = sibling->second->picture.tone;
                entry->picture.glow = sibling->second->picture.glow;
                entry->texture_bytes = static_cast<std::size_t>(kAmbientW * kAmbientH * 4);
                bytes_ += entry->texture_bytes;
                entry->state = entry->picture.texture != 0 ? Entry::State::ready : Entry::State::failed;
                break;
            }
            if (entry->state != Entry::State::queued)
                continue;
        }
        const std::weak_ptr<int> alive = alive_;
        const std::string key = entry->key;
        entry->state = Entry::State::fetching;
        ++fetching_;
        entry->ticket = platform_.fetch(entry->url, [this, alive, key](bool ok, std::string &&bytes)
        {
            if (alive.expired())
                return;
            const auto found = entries_.find(key);
            if (found == entries_.end() || found->second->state != Entry::State::fetching)
                return;
            Entry &target = *found->second;
            target.ticket = 0;
            --fetching_;
            if (ok && !bytes.empty())
            {
                target.bytes = std::move(bytes);
                target.state = Entry::State::fetched;
            }
            else
            {
                target.state = Entry::State::failed;
                target.picture.failed = true;
                target.failed_for = 0.0f;
            }
        });
        if (entry->ticket == 0 && entry->state == Entry::State::fetching)
        {
            --fetching_;
            entry->state = Entry::State::failed;
            entry->picture.failed = true;
            entry->failed_for = 0.0f;
        }
    }

    // One decode per frame.
    Entry *next = nullptr;
    for (auto &pair : entries_)
    {
        Entry &entry = *pair.second;
        if (entry.state == Entry::State::fetched && (next == nullptr || entry.touched > next->touched))
            next = &entry;
    }
    if (next != nullptr)
    {
        Decoded decoded;
        const bool ambient = next->size == PictureSize::ambient;
        if (decode_picture(next->bytes, ambient ? 256 : longest_side(next->size), &decoded))
        {
            next->ambient = ambient_of(decoded.rgba.data(), decoded.width, decoded.height);
            if (ambient)
            {
                decoded.rgba = next->ambient;
                decoded.width = kAmbientW;
                decoded.height = kAmbientH;
            }
            next->picture.texture = platform_.create_texture(decoded.width, decoded.height, decoded.rgba.data());
            next->picture.width = decoded.width;
            next->picture.height = decoded.height;
            next->picture.tone = decoded.tone;
            next->picture.glow = decoded.glow;
            next->picture.age = 0.0f;
            next->texture_bytes = decoded.rgba.size();
            bytes_ += next->texture_bytes;
        }
        next->bytes.clear();
        next->bytes.shrink_to_fit();
        if (next->picture.texture != 0)
        {
            next->state = Entry::State::ready;
        }
        else
        {
            next->state = Entry::State::failed;
            next->picture.failed = true;
            next->failed_for = 0.0f;
        }
    }
}

} // namespace afterglow
