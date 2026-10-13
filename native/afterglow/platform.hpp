// ProsperoTube Afterglow - what the interface needs from whatever runs it.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Two implementations exist: the app on the console (mount.cpp, on EVO's AGC
// batch, network and i18n) and the host harness (tests/afterglow/host_main.cpp,
// on the kit's OpenGL renderer and files). Everything is called on the thread
// that runs the frame; nothing here may block.

#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace afterglow
{

class Platform
{
  public:
    virtual ~Platform() = default;

    // RGBA8, straight alpha, top row first. Returns the handle the draw lists
    // take, 0 on failure.
    virtual std::uint32_t create_texture(int width, int height, const std::uint8_t *rgba) = 0;
    virtual void release_texture(std::uint32_t handle) = 0;

    // A square one-channel texture for a page of the text engine's glyph
    // atlas (size x size coverage bytes, top row first), and the whole page
    // again after glyphs were added. 0 where the backend has no such texture:
    // the engine then keeps the page in an ordinary texture.
    virtual std::uint32_t create_glyph_page(int size, const std::uint8_t *coverage)
    {
        (void)size;
        (void)coverage;
        return 0;
    }
    virtual void update_glyph_page(std::uint32_t handle, int size, const std::uint8_t *coverage)
    {
        (void)handle;
        (void)size;
        (void)coverage;
    }
    virtual void release_glyph_page(std::uint32_t handle, bool coverage)
    {
        (void)coverage;
        release_texture(handle);
    }

    // The bytes behind a picture address. done runs once, on this thread,
    // during a later pump() of the frame; never from inside fetch(). Returns a
    // ticket, 0 when the request was refused (done is then never called).
    using Fetched = std::function<void(bool ok, std::string &&bytes)>;
    virtual unsigned fetch(const std::string &url, Fetched done) = 0;
    virtual void cancel_fetch(unsigned ticket) = 0;

    // A file packaged with the app, by its path under the assets
    // ("afterglow/..."). False when it is missing.
    virtual bool read_asset(const std::string &path, std::string *bytes) = 0;

    // The time of day for the header clock.
    virtual void wall_clock(int *hour, int *minute) = 0;

    // An interface text in the profile's language; english itself when the
    // catalog has no entry. The pointer stays valid until the language changes.
    virtual const char *translate(const char *english) = 0;
    // A counted text ("{0} reply", "{0} replies", 3) in the profile's language,
    // the number in place and the form the language's plural rule picks. The
    // default is the English text; a platform with catalogs overrides it.
    virtual std::string translate_count(const char *one, const char *other, long count);
    // Changes whenever translate() may answer differently.
    virtual unsigned language_revision() = 0;

    virtual void log(const char *line) = 0;
};

// The platform of the running frame, for tr() and for code that has no
// context at hand. Set by the frame for as long as it lives.
Platform *platform();
void set_platform(Platform *platform);

} // namespace afterglow
