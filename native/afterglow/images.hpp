// ProsperoTube Afterglow - pictures: thumbnails, avatars and banners as kit textures.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A screen asks for the picture of an address every frame it draws it. The
// cache fetches the bytes through the platform, decodes them (JPEG and PNG;
// stb_image), scales them down to the size class asked for, uploads a texture
// and works out the two colours the page ground and the focus light take from
// the picture. A picture nobody asked for during kIdleFrames frames gives its
// texture back; the total is bounded by kBudgetBytes, least recently used
// first. At most one picture is decoded per pump, so that a page of new
// thumbnails costs one decode per frame instead of one long frame.

#pragma once

#include "afterglow/base.hpp"
#include "afterglow/platform.hpp"

#include <map>
#include <memory>
#include <vector>

namespace afterglow
{

// How large the texture of a picture may be; the picture keeps its shape.
enum class PictureSize : std::uint8_t
{
    avatar,  // 96 pixels on the long side
    card,    // 512
    hero,    // 1280
    ambient, // 64 x 36, blurred: the page ground stretches it behind everything
};

// A picture as a screen draws it. texture is 0 until it is ready.
struct Picture
{
    std::uint32_t texture = 0;
    int width = 0;
    int height = 0;
    Color tone;        // the picture's average colour
    Color glow;        // its most vivid colour: focus light, the page's wash
    float age = 0.0f;  // seconds since it became ready: fade it in over the placeholder
    bool failed = false;

    bool ready() const
    {
        return texture != 0;
    }
    // The part of the texture that fills a box of this width:height without
    // distortion (centre crop), for DrawList::image.
    Rect cover(float aspect) const;
};

// Decoded pixels and the colours taken from them; no texture yet. Exposed for
// the tests of the decoder and the colour extraction.
struct Decoded
{
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
    Color tone;
    Color glow;
};
// False for bytes that are no JPEG or PNG, or larger than 4096 pixels a side.
bool decode_picture(const std::string &bytes, int longest_side, Decoded *out);

class ImageCache
{
  public:
    static constexpr std::size_t kBudgetBytes = 48u * 1024u * 1024u;
    static constexpr int kIdleFrames = 600;
    static constexpr int kMaxFetches = 4; // addresses asked of the platform at once

    explicit ImageCache(Platform &platform);
    ~ImageCache();
    ImageCache(const ImageCache &) = delete;
    ImageCache &operator=(const ImageCache &) = delete;

    // The picture of an address, asked for on first use. "" gives an empty
    // picture. Callable while drawing: it changes the cache, never a screen.
    Picture get(std::string_view url, PictureSize size = PictureSize::card);
    // Once per frame, before the screens update: decodes, uploads, retires.
    void pump(float dt);
    // Gives every texture back (the platform is going away, or memory is short).
    void clear();

    std::size_t bytes() const
    {
        return bytes_;
    }
    int pending() const;
    int ready() const;

  private:
    struct Entry;
    void retire(Entry &entry);

    Platform &platform_;
    std::map<std::string, std::unique_ptr<Entry>> entries_;
    std::vector<Entry *> queue_; // waiting to be fetched, oldest first
    std::size_t bytes_ = 0;
    int fetching_ = 0;
    unsigned frame_ = 0;
    std::shared_ptr<int> alive_; // lets a late fetch answer find the cache gone
};

} // namespace afterglow
