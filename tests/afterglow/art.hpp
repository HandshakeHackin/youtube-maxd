// ProsperoTube Afterglow tests - pictures painted from arithmetic.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The host harness may use nobody's pictures, so the fixtures name scenes
// ("art://ridges/0/11") and this paints them: layered ridges, a skyline, a
// planet, a stage. Carried over from the interface proposal. Host only.

#pragma once

#include "gfx/draw_list.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace afterglow_test
{

namespace gfx = hui::gfx;

enum class Scene : std::uint8_t
{
    ridges, // layered mountains under a low sun
    city,   // a skyline at night with lit windows
    synth,  // a striped sun over a perspective grid
    space,  // a planet and its moon in a nebula
    ocean,  // the sun's path on water
    aurora, // curtains of light over a pine forest
    stage,  // spotlights over a crowd
    dunes,  // lit and shaded sand
    blobs,  // an abstract colour field with rings
    globe,  // a world with a grid of meridians
    desk,   // a screen glowing in a dark room
    plate,  // a table seen from above
    pitch,  // a playing field seen from above
    code,   // an editor with coloured lines
    count,
};

// A painted picture before it becomes textures.
struct Painted
{
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;    // width x height
    std::vector<std::uint8_t> ambient; // 64 x 36
    gfx::Color tone;
    gfx::Color glow;
};

// Paints a scene. Touches no OpenGL state: safe on a worker thread.
Painted paint_art(Scene scene, std::uint32_t seed, int palette, int width, int height);

// The picture of a scene by its name ("ridges", "city", ...), RGBA rows top
// first; empty for an unknown name.
std::vector<std::uint8_t> paint(std::string_view scene, std::uint32_t seed, int palette, int width, int height);

} // namespace afterglow_test
