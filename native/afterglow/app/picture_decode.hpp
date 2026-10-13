// ProsperoTube Afterglow - the pictures the frame's own decoder does not read.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// YouTube answers many thumbnail addresses with WebP whatever their name ends
// in (the pictures of a search first of all; a section's are mostly JPEG).
// The picture cache reads JPEG and PNG itself; the app's platform hands the
// rest to the decoder the app already carries for the classic browser's
// artwork (FFmpeg's), here from memory and without a file.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace afterglow
{

// True for bytes that begin as a WebP file does.
bool is_webp(const std::string &bytes);
// The first picture of a WebP file as width x height RGBA pixels, straight
// alpha, top row first. False for anything else, for a broken file and for
// more than 4096 pixels a side.
bool decode_webp(const std::string &bytes, int *width, int *height, std::vector<std::uint8_t> *rgba);

} // namespace afterglow
