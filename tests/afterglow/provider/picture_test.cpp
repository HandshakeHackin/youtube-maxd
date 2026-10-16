// ProsperoTube Afterglow - the picture cache on what YouTube really sends.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The pictures of a search arrive as WebP under an address that ends in .jpg.
// The cache reads JPEG and PNG itself and hands the rest to the platform; the
// app's platform answers with decode_webp (native/afterglow/app). Run with a
// WebP and a JPEG file of the same 320 x 180 red picture. Covered: the WebP
// becomes a ready picture of its size and colour through the cache, exactly
// as the JPEG does; a platform without that decoder gives the picture up
// (what the first console session showed); a broken WebP and bytes that are
// no WebP are refused.

#include "afterglow/app/picture_decode.hpp"
#include "afterglow/images.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace
{

using namespace afterglow;

class TestPlatform final : public Platform
{
  public:
    bool webp = true; // the app's platform; false: a platform with the cache's own decoder only
    std::map<std::string, std::string> files;
    int textures = 0;
    int released = 0;

    std::uint32_t create_texture(int width, int height, const std::uint8_t *rgba) override
    {
        assert(width > 0 && height > 0 && rgba != nullptr);
        return static_cast<std::uint32_t>(++textures);
    }
    void release_texture(std::uint32_t) override
    {
        ++released;
    }
    unsigned fetch(const std::string &url, Fetched done) override
    {
        waiting_.emplace_back(url, std::move(done));
        return static_cast<unsigned>(++tickets_);
    }
    void cancel_fetch(unsigned) override {}
    bool decode_picture(const std::string &bytes, int *width, int *height, std::vector<std::uint8_t> *rgba) override
    {
        return webp && decode_webp(bytes, width, height, rgba);
    }
    bool read_asset(const std::string &, std::string *) override
    {
        return false;
    }
    void wall_clock(int *hour, int *minute) override
    {
        *hour = 12;
        *minute = 0;
    }
    const char *translate(const char *english) override
    {
        return english;
    }
    unsigned language_revision() override
    {
        return 0;
    }
    void log(const char *) override {}

    // Answers what was asked, as the app does between two frames.
    void deliver()
    {
        std::vector<std::pair<std::string, Fetched>> now;
        now.swap(waiting_);
        for (auto &entry : now)
        {
            const auto found = files.find(entry.first);
            entry.second(found != files.end(), found != files.end() ? std::string(found->second) : std::string());
        }
    }

  private:
    std::vector<std::pair<std::string, Fetched>> waiting_;
    int tickets_ = 0;
};

std::string read_file(const char *path)
{
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), {});
}

// The picture of an address after the frames a fetch and a decode take.
Picture settled(ImageCache &cache, TestPlatform &platform, const char *address, PictureSize size = PictureSize::card)
{
    Picture picture;
    for (int frame = 0; frame < 8; ++frame)
    {
        picture = cache.get(address, size);
        cache.pump(1.0f / 60.0f);
        platform.deliver();
    }
    return cache.get(address, size);
}

} // namespace

int main(int argc, char **argv)
{
    assert(argc == 4);
    const std::string webp = read_file(argv[1]);
    const std::string jpeg = read_file(argv[2]);
    {
        // A 151 x 270 portrait WebP: swscale converts whole blocks of 8 pixels, so a tight
        // 151-pixel row used to keep its last 7 pixels unconverted (and wider rows ran past it).
        int w = 0;
        int h = 0;
        std::vector<std::uint8_t> portrait;
        assert(decode_webp(read_file(argv[3]), &w, &h, &portrait) && w == 151 && h == 270 && portrait.size() == 151u * 270u * 4u);
        for (const unsigned row : {0u, 135u, 269u})
        {
            const std::uint8_t *right = &portrait[(row * 151u + 150u) * 4u];
            assert(right[0] > 200 && right[1] < 60 && right[2] < 60 && right[3] == 255);
        }
    }
    assert(is_webp(webp) && !is_webp(jpeg) && jpeg.size() > 100);
    // A search row's address, as the built-in service names it: .jpg and a signed query, WebP inside.
    const char *search = "https://i.ytimg.com/vi/aaaaaaaaaa1/hq720.jpg?sqp=-oaymwEnCNAFEJQDSFryq4qpAxkIARUAAIhCGAHYAQHiAQoIGBACGAY4AUAB&rs=AOn4CLfixture";
    const char *section = "https://i.ytimg.com/vi/aaaaaaaaaa2/hqdefault.jpg";

    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
    assert(decode_webp(webp, &width, &height, &pixels) && width == 320 && height == 180 && pixels.size() == 320u * 180u * 4u);
    const std::uint8_t *middle = &pixels[(90u * 320u + 160u) * 4u];
    assert(middle[0] > 200 && middle[1] < 60 && middle[2] < 60 && middle[3] == 255);
    assert(!decode_webp(jpeg, &width, &height, &pixels) && pixels.empty());
    assert(!decode_webp(webp.substr(0, 40), &width, &height, &pixels) && pixels.empty());
    assert(!decode_webp(std::string(), &width, &height, &pixels));

    {
        TestPlatform platform;
        platform.files[search] = webp;
        platform.files[section] = jpeg;
        ImageCache cache(platform);
        const Picture found = settled(cache, platform, search);
        assert(found.ready() && !found.failed && found.width == 320 && found.height == 180);
        assert(found.tone.r > 0.8f && found.tone.g < 0.25f && found.tone.b < 0.25f);
        const Picture plain = settled(cache, platform, section);
        assert(plain.ready() && plain.width == 320 && plain.height == 180 && plain.tone.r > 0.8f);
        // Scaled to its size class like any other, and the page's wash comes from the same bytes.
        const Picture small = settled(cache, platform, search, PictureSize::avatar);
        assert(small.ready() && small.width == 96 && small.height == 54);
        const Picture ground = settled(cache, platform, search, PictureSize::ambient);
        assert(ground.ready() && ground.width == 64 && ground.height == 36);
        assert(cache.failed() == 0 && cache.ready() == 4);
    }
    {
        // Without the platform's decoder the search's picture is given up and the section's is not:
        // Home with pictures, a topic's videos without, as the first console session showed.
        TestPlatform platform;
        platform.webp = false;
        platform.files[search] = webp;
        platform.files[section] = jpeg;
        ImageCache cache(platform);
        assert(settled(cache, platform, search).failed && settled(cache, platform, section).ready() && cache.failed() == 1);
    }
    {
        // A file that only begins as WebP does is given up, not drawn.
        TestPlatform platform;
        platform.files[search] = webp.substr(0, 64);
        ImageCache cache(platform);
        const Picture broken = settled(cache, platform, search);
        assert(broken.failed && !broken.ready() && platform.textures == 0);
    }
    std::puts("PASS: Afterglow pictures: a WebP under a .jpg address decodes through the platform to a ready picture in every size, "
              "a JPEG as before, and a broken or undecodable one is given up");
    return 0;
}
