// ProsperoTube Afterglow tests - pictures painted from arithmetic.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Every picture is a function from a point to a colour, sampled four times per
// pixel. Nothing here is a photograph or anybody's artwork.

#include "art.hpp"


#include <algorithm>
#include <cmath>
#include <vector>

namespace afterglow_test
{

namespace
{

struct V3
{
    float r, g, b;
};
V3 operator+(V3 a, V3 b)
{
    return {a.r + b.r, a.g + b.g, a.b + b.b};
}
V3 operator*(V3 a, float s)
{
    return {a.r * s, a.g * s, a.b * s};
}
V3 operator*(V3 a, V3 b)
{
    return {a.r * b.r, a.g * b.g, a.b * b.b};
}
V3 mix(V3 a, V3 b, float t)
{
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}
float sat(float t)
{
    return std::clamp(t, 0.0f, 1.0f);
}
// 0 below a, 1 above b (a may be larger than b for a falling edge).
float sstep(float a, float b, float x)
{
    const float t = sat((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}
V3 rgb(std::uint32_t hex)
{
    return {static_cast<float>((hex >> 16) & 0xff) / 255.0f,
            static_cast<float>((hex >> 8) & 0xff) / 255.0f,
            static_cast<float>(hex & 0xff) / 255.0f};
}
float fract(float x)
{
    return x - std::floor(x);
}

std::uint32_t hash_u(std::uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}
float hash(std::uint32_t a, std::uint32_t b = 0, std::uint32_t c = 0)
{
    const std::uint32_t h = hash_u(a * 0x9e3779b1u ^ hash_u(b * 0x85ebca6bu ^ hash_u(c + 0x27d4eb2fu)));
    return static_cast<float>(h >> 8) / 16777216.0f;
}
float noise1(float x, std::uint32_t seed)
{
    const float f = std::floor(x);
    const float t = x - f;
    const float s = t * t * (3.0f - 2.0f * t);
    const auto i = static_cast<std::uint32_t>(static_cast<std::int32_t>(f));
    return hash(i, seed) * (1.0f - s) + hash(i + 1, seed) * s;
}
float noise2(float x, float y, std::uint32_t seed)
{
    const float fx = std::floor(x);
    const float fy = std::floor(y);
    const float tx = x - fx;
    const float ty = y - fy;
    const float sx = tx * tx * (3.0f - 2.0f * tx);
    const float sy = ty * ty * (3.0f - 2.0f * ty);
    const auto ix = static_cast<std::uint32_t>(static_cast<std::int32_t>(fx));
    const auto iy = static_cast<std::uint32_t>(static_cast<std::int32_t>(fy));
    const float a = hash(ix, iy, seed);
    const float b = hash(ix + 1, iy, seed);
    const float c = hash(ix, iy + 1, seed);
    const float d = hash(ix + 1, iy + 1, seed);
    return (a * (1.0f - sx) + b * sx) * (1.0f - sy) + (c * (1.0f - sx) + d * sx) * sy;
}
float fbm1(float x, int octaves, std::uint32_t seed)
{
    float sum = 0.0f;
    float amp = 0.5f;
    float norm = 0.0f;
    for (int i = 0; i < octaves; ++i)
    {
        sum += amp * noise1(x, seed + static_cast<std::uint32_t>(i) * 101u);
        norm += amp;
        x = x * 2.03f + 17.3f;
        amp *= 0.5f;
    }
    return sum / norm;
}
float fbm2(float x, float y, int octaves, std::uint32_t seed)
{
    float sum = 0.0f;
    float amp = 0.5f;
    float norm = 0.0f;
    for (int i = 0; i < octaves; ++i)
    {
        sum += amp * noise2(x, y, seed + static_cast<std::uint32_t>(i) * 131u);
        norm += amp;
        const float nx = x * 1.62f - y * 1.18f + 11.1f;
        y = x * 1.18f + y * 1.62f + 5.7f;
        x = nx;
        amp *= 0.5f;
    }
    return sum / norm;
}

struct Ctx
{
    std::uint32_t seed;
    int palette;
    float aspect; // x runs 0..aspect, y runs 0..1 downward
};

// ---- skies shared by the outdoor scenes -------------------------------------

struct Sky
{
    V3 top, mid, low, sun;
    bool night;
};
const Sky kSkies[] = {
    {rgb(0x1b1445), rgb(0x7a2f7a), rgb(0xff8a4c), rgb(0xffe2b0), false}, // dusk
    {rgb(0x071a3a), rgb(0x1f4f8f), rgb(0xf2a7b8), rgb(0xfff1d6), false}, // blue hour
    {rgb(0x0c2f3f), rgb(0x2a7f8f), rgb(0xffd27a), rgb(0xfff4c8), false}, // teal dawn
    {rgb(0x2a0d16), rgb(0x9c2f2f), rgb(0xffb347), rgb(0xfff0c0), false}, // ember
    {rgb(0x2f6fb5), rgb(0x8fd0e8), rgb(0xf6f3d0), rgb(0xffffff), false}, // day
    {rgb(0x05060f), rgb(0x0f1c3d), rgb(0x2b3f73), rgb(0xe8eeff), true},  // night
    {rgb(0x140a2e), rgb(0x4b2a8a), rgb(0xf08fc0), rgb(0xffe0f0), false}, // violet
    {rgb(0x0a2218), rgb(0x1f5f45), rgb(0xd8e08a), rgb(0xfff8c0), false}, // forest
};
constexpr int kSkyCount = 8;

V3 sky_at(const Sky &s, float t)
{
    t = sat(t);
    return t < 0.62f ? mix(s.top, s.mid, sstep(0.0f, 0.62f, t)) : mix(s.mid, s.low, sstep(0.62f, 1.0f, t));
}

float stars(float x, float y, std::uint32_t seed, float density)
{
    const float cell = 1.0f / 46.0f;
    const float gx = x / cell;
    const float gy = y / cell;
    const auto ix = static_cast<std::uint32_t>(static_cast<std::int32_t>(std::floor(gx)));
    const auto iy = static_cast<std::uint32_t>(static_cast<std::int32_t>(std::floor(gy)));
    if (hash(ix, iy, seed) > density)
        return 0.0f;
    const float px = 0.2f + 0.6f * hash(ix, iy, seed + 1);
    const float py = 0.2f + 0.6f * hash(ix, iy, seed + 2);
    const float d = std::hypot(fract(gx) - px, fract(gy) - py);
    const float bright = 0.25f + 0.75f * std::pow(hash(ix, iy, seed + 3), 3.0f);
    return bright * (1.0f - sstep(0.03f, 0.11f + 0.07f * bright, d));
}

// ---- the scenes -----------------------------------------------------------------

V3 ridges(const Ctx &c, float x, float y)
{
    const Sky &s = kSkies[c.palette % kSkyCount];
    const float horizon = 0.64f;
    V3 col = sky_at(s, y / horizon);
    if (s.night)
        col = col + rgb(0xdfe8ff) * (stars(x, y, c.seed + 5, 0.22f) * sstep(horizon, 0.1f, y));
    const float sx = c.aspect * (0.26f + 0.48f * hash(c.seed, 1));
    const float sy = horizon - 0.15f - 0.12f * hash(c.seed, 2);
    const float d = std::hypot(x - sx, y - sy);
    const float r = s.night ? 0.052f : 0.078f;
    col = col + s.sun * ((s.night ? 0.22f : 0.5f) * std::exp(-d * 4.2f));
    col = mix(col, s.sun, 1.0f - sstep(r - 0.003f, r + 0.003f, d));
    // Thin cloud bars that catch the low light.
    const float band = fbm2(x * 1.3f, y * 11.0f, 4, c.seed + 7);
    const float cloud = sstep(0.56f, 0.78f, band) * sstep(0.02f, 0.4f, y) * sstep(horizon, horizon - 0.2f, y);
    col = mix(col, mix(s.low, s.sun, 0.45f), cloud * 0.42f);

    const V3 haze = mix(s.low, s.mid, 0.45f);
    const V3 far = mix(haze, s.mid, 0.4f) * 0.86f;
    const V3 near = mix(s.top, rgb(0x04050a), 0.55f);
    constexpr int kLayers = 6;
    for (int i = 0; i < kLayers; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(kLayers - 1);
        const float base = horizon - 0.05f + t * 0.36f;
        const float amp = 0.06f + 0.085f * t;
        const float freq = 1.25f + 0.55f * static_cast<float>(i);
        float line = base - amp * 2.0f * (fbm1(x * freq + 13.7f * static_cast<float>(i), 5, c.seed + 31u * static_cast<std::uint32_t>(i)) - 0.5f);
        if (i == kLayers - 1) // a fringe of pines on the nearest slope
            line -= 0.014f * std::fabs(fract(x * 46.0f + hash(c.seed, 9)) - 0.5f) * (0.5f + noise1(x * 9.0f, c.seed + 3));
        if (y > line)
        {
            V3 layer = mix(far, near, std::pow(t, 0.85f));
            // Mist pools at the foot of each ridge and lights its crest.
            layer = mix(layer, haze, (1.0f - t) * 0.5f * sstep(0.0f, 0.22f, y - line));
            layer = mix(layer, s.low, 0.2f * (1.0f - t) * std::exp(-(y - line) * 38.0f));
            col = layer;
        }
    }
    return col;
}

V3 city(const Ctx &c, float x, float y)
{
    struct Pal
    {
        V3 top, mid, glow, window, body;
    };
    static const Pal kPal[] = {
        {rgb(0x070b1f), rgb(0x1a1f4f), rgb(0xff7a59), rgb(0xffd27a), rgb(0x0a0d1c)},
        {rgb(0x0b0620), rgb(0x3a1a6a), rgb(0xff4fa3), rgb(0x9fe8ff), rgb(0x0c0718)},
        {rgb(0x04121a), rgb(0x0f3a4a), rgb(0x39d0c0), rgb(0xfff0b0), rgb(0x061218)},
        {rgb(0x1a0a0a), rgb(0x5a1f1f), rgb(0xffa040), rgb(0xffe0a0), rgb(0x140a0a)},
    };
    const Pal &p = kPal[c.palette % 4];
    const float ground = 0.86f;
    V3 col = mix(p.top, p.mid, sstep(0.0f, ground, y));
    col = col + p.glow * (0.75f * std::exp(-(ground - y) * 5.5f) * (y < ground ? 1.0f : 0.0f));
    col = col + rgb(0xdfe8ff) * (stars(x, y, c.seed + 5, 0.14f) * sstep(0.5f, 0.05f, y));
    const float mx = c.aspect * (0.2f + 0.6f * hash(c.seed, 4));
    const float md = std::hypot(x - mx, y - 0.2f);
    col = col + rgb(0xdfe8ff) * (0.12f * std::exp(-md * 7.0f));
    col = mix(col, rgb(0xf4f1e6), 1.0f - sstep(0.04f, 0.044f, md));

    for (int layer = 0; layer < 3; ++layer)
    {
        const float t = static_cast<float>(layer) / 2.0f;
        const float w = 0.075f + 0.045f * t;
        const float gx = x / w + 3.7f * static_cast<float>(layer);
        const auto idx = static_cast<std::uint32_t>(static_cast<std::int32_t>(std::floor(gx)));
        const float tall = std::pow(hash(idx, static_cast<std::uint32_t>(layer), c.seed), 1.6f);
        const float top = ground - (0.12f + 0.2f * t) - tall * (0.42f - 0.14f * t);
        const float fx = fract(gx);
        const bool inside = y > top && y < ground && fx > 0.04f && fx < 0.96f;
        // A mast on the tallest towers.
        const bool mast = tall > 0.72f && std::fabs(fx - 0.5f) < 0.02f && y > top - 0.06f && y <= top;
        if (inside || mast)
        {
            V3 body = mix(mix(p.mid, p.glow, 0.25f) * 0.55f, p.body, t);
            body = mix(body, p.glow * 0.35f, 0.3f * std::exp(-(ground - y) * 6.0f));
            col = body;
            if (inside && layer > 0)
            {
                const float cw = 1.0f / (layer == 1 ? 4.0f : 5.0f);
                const float ch = 0.021f;
                const auto wx = static_cast<std::uint32_t>(fx / cw);
                const auto wy = static_cast<std::uint32_t>((y - top) / ch);
                const float ux = fract(fx / cw);
                const float uy = fract((y - top) / ch);
                const float lit = hash(idx * 7u + wx, wy, c.seed + static_cast<std::uint32_t>(layer));
                if (lit > 0.6f && ux > 0.28f && ux < 0.78f && uy > 0.3f && uy < 0.74f && (y - top) > 0.012f)
                    col = mix(col, lit > 0.93f ? p.glow : p.window, 0.55f + 0.4f * t);
            }
        }
    }
    if (y >= ground)
    {
        // Wet street: the glow smeared downward.
        const float streak = 0.5f + 0.5f * noise2(x * 26.0f, y * 3.0f, c.seed + 11);
        col = mix(p.body, p.glow * 0.5f, std::exp(-(y - ground) * 9.0f) * streak);
    }
    return col;
}

V3 synth(const Ctx &c, float x, float y)
{
    struct Pal
    {
        V3 top, low, sun_top, sun_low, grid, floor;
    };
    static const Pal kPal[] = {
        {rgb(0x0c0424), rgb(0x7b1fa2), rgb(0xffe066), rgb(0xff3d8b), rgb(0x4fe0ff), rgb(0x12052e)},
        {rgb(0x02101f), rgb(0x0f5c8a), rgb(0xfff0a0), rgb(0xff8a3d), rgb(0xff5cc8), rgb(0x04101c)},
        {rgb(0x150a1f), rgb(0xa02848), rgb(0xfff3b0), rgb(0xff5a3d), rgb(0xffb347), rgb(0x1a0816)},
    };
    const Pal &p = kPal[c.palette % 3];
    const float horizon = 0.6f;
    const float cx = c.aspect * 0.5f;
    V3 col = mix(p.top, p.low, std::pow(sat(y / horizon), 1.6f));
    col = col + rgb(0xffffff) * (stars(x, y, c.seed + 3, 0.16f) * sstep(0.45f, 0.05f, y));
    const float sy = horizon - 0.1f;
    const float d = std::hypot(x - cx, y - sy);
    col = col + p.sun_low * (0.4f * std::exp(-d * 3.4f));
    if (d < 0.25f && y < horizon)
    {
        const float t = sat((y - (sy - 0.25f)) / 0.5f);
        // Blinds that widen toward the bottom of the disc.
        const float bar = fract((y - sy) * 15.0f);
        const bool cut = y > sy - 0.02f && bar < 0.16f + 0.5f * sat((y - sy) / 0.25f);
        if (!cut)
            col = mix(p.sun_top, p.sun_low, t);
    }
    // A low range with a lit outline.
    const float range = horizon - 0.02f - 0.1f * fbm1(x * 2.2f, 4, c.seed + 2) * sstep(0.12f, 0.6f, std::fabs(x - cx));
    if (y > range && y <= horizon)
        col = mix(p.floor, p.grid, std::exp(-(y - range) * 160.0f) * 0.85f);
    if (y > horizon)
    {
        const float z = y - horizon + 0.012f;
        const float px = (x - cx) / z * 0.16f;
        const float pz = 0.22f / z + static_cast<float>(c.seed % 7) * 0.13f;
        const float wide = 0.02f + 0.03f / (z * 16.0f + 1.0f);
        const float gx = std::fabs(fract(px) - 0.5f);
        const float gz = std::fabs(fract(pz) - 0.5f);
        const float linev = 1.0f - sstep(0.0f, wide, 0.5f - gx);
        const float lineh = 1.0f - sstep(0.0f, wide * 1.8f, 0.5f - gz);
        const float fade = sstep(0.0f, 0.1f, z);
        col = mix(p.floor, p.low * 0.5f, std::exp(-z * 10.0f));
        col = col + p.grid * (std::max(linev, lineh) * (0.25f + 0.75f * fade));
        col = col + p.sun_low * (0.25f * std::exp(-z * 7.0f) * std::exp(-std::fabs(x - cx) * 2.0f));
    }
    return col;
}

V3 lit_sphere(V3 base_lit, V3 base_dark, V3 atmosphere, float nx, float ny, float nz, float bands,
              std::uint32_t seed)
{
    const float lx = -0.58f, ly = -0.48f, lz = 0.66f;
    const float diffuse = sat(nx * lx + ny * ly + nz * lz);
    const float b = fbm2(nx * 1.6f + 3.0f, ny * bands + fbm2(nx * 2.0f, ny * 2.0f, 3, seed) * 1.4f, 4, seed + 5);
    V3 col = mix(base_dark, base_lit, b) * (0.035f + 1.15f * diffuse);
    col = col + atmosphere * (std::pow(1.0f - nz, 3.0f) * (0.15f + 0.85f * diffuse) * 0.9f);
    return col;
}

V3 space(const Ctx &c, float x, float y)
{
    struct Pal
    {
        V3 neb_a, neb_b, lit, dark, air;
    };
    static const Pal kPal[] = {
        {rgb(0x3b2a8a), rgb(0xb0387a), rgb(0xf2b880), rgb(0x8a3d2a), rgb(0xffc890)},
        {rgb(0x123a6a), rgb(0x1f8f8f), rgb(0x9fd8ff), rgb(0x2a4a9a), rgb(0x8fe0ff)},
        {rgb(0x5a1f4a), rgb(0x8a3a1f), rgb(0xe8e0d0), rgb(0x7a6a5a), rgb(0xffe8d0)},
        {rgb(0x0f4a3a), rgb(0x3a2a7a), rgb(0xb8f0c0), rgb(0x2a6a5a), rgb(0xa0ffd8)},
    };
    const Pal &p = kPal[c.palette % 4];
    V3 col = mix(rgb(0x03040b), rgb(0x080a18), y);
    const float warp = fbm2(x * 1.3f, y * 1.3f, 4, c.seed + 1);
    const float n1 = fbm2(x * 1.4f + warp * 1.6f, y * 1.4f - warp, 5, c.seed + 2);
    const float n2 = fbm2(x * 1.1f - warp * 1.2f + 4.0f, y * 1.7f + warp, 5, c.seed + 3);
    col = col + p.neb_a * (sstep(0.42f, 0.86f, n1) * 0.75f);
    col = col + p.neb_b * (sstep(0.48f, 0.9f, n2) * 0.6f);
    col = col + rgb(0xffffff) * stars(x, y, c.seed + 9, 0.3f);
    col = col + rgb(0xcfe0ff) * (0.6f * stars(x * 2.3f, y * 2.3f, c.seed + 10, 0.18f));

    const bool right = hash(c.seed, 6) > 0.4f;
    const float px = c.aspect * (right ? 0.7f : 0.3f);
    const float py = 0.66f;
    const float pr = 0.44f;
    const float dx = (x - px) / pr;
    const float dy = (y - py) / pr;
    const float d2 = dx * dx + dy * dy;
    if (d2 < 1.0f)
    {
        col = lit_sphere(p.lit, p.dark, p.air, dx, dy, std::sqrt(1.0f - d2), 6.5f, c.seed);
    }
    else
    {
        const float d = std::sqrt(d2);
        const float side = sat(0.5f - (dx * 0.58f + dy * 0.48f) / d * 0.9f);
        col = col + p.air * (std::exp(-(d - 1.0f) * 22.0f) * 0.55f * side);
    }
    const float mx = c.aspect * (right ? 0.24f : 0.78f);
    const float my = 0.26f;
    const float mr = 0.07f;
    const float ex = (x - mx) / mr;
    const float ey = (y - my) / mr;
    const float e2 = ex * ex + ey * ey;
    if (e2 < 1.0f)
        col = lit_sphere(rgb(0xd8d4cc), rgb(0x8a857c), rgb(0x000000), ex, ey, std::sqrt(1.0f - e2), 2.0f, c.seed + 77);
    return col;
}

V3 ocean(const Ctx &c, float x, float y)
{
    const Sky &s = kSkies[c.palette % kSkyCount];
    const float horizon = 0.56f;
    const float sx = c.aspect * (0.3f + 0.4f * hash(c.seed, 1));
    const float sy = horizon - 0.035f;
    if (y < horizon)
    {
        V3 col = sky_at(s, y / horizon);
        const float d = std::hypot(x - sx, y - sy);
        col = col + s.sun * (0.6f * std::exp(-d * 3.6f));
        col = mix(col, s.sun, 1.0f - sstep(0.086f, 0.092f, d));
        const float band = fbm2(x * 0.9f, y * 8.0f, 5, c.seed + 7);
        const float cloud = sstep(0.5f, 0.74f, band) * sstep(0.0f, 0.3f, y);
        const V3 lit = mix(s.low, s.sun, sat(1.0f - d * 1.3f));
        col = mix(col, mix(s.mid * 0.7f, lit, sstep(0.2f, 0.56f, y)), cloud * 0.6f);
        return col;
    }
    const float t = (y - horizon) / (1.0f - horizon);
    V3 col = mix(mix(s.low, s.mid, 0.55f) * 0.62f, s.top * 0.55f, sstep(0.0f, 0.9f, t));
    // Ripples: rows that get thicker toward the viewer.
    const float row = std::log(t * 9.0f + 0.25f) * 9.0f;
    const float n = noise2(x * (9.0f - 6.0f * t), row * 2.2f, c.seed + 3);
    const float n2 = noise2(x * 3.0f + 9.0f, row * 0.9f, c.seed + 4);
    col = col * (0.82f + 0.36f * n2);
    const float path = std::exp(-std::pow((x - sx) / (0.045f + 0.3f * t), 2.0f));
    col = col + s.sun * (path * sstep(0.42f, 0.72f, n) * (1.0f - 0.45f * t));
    col = col + s.low * (0.22f * sstep(0.6f, 0.9f, n) * (1.0f - t));
    return col;
}

V3 aurora(const Ctx &c, float x, float y)
{
    struct Pal
    {
        V3 low, high;
    };
    static const Pal kPal[] = {
        {rgb(0x3dffa0), rgb(0x9a5cff)},
        {rgb(0x4fe0ff), rgb(0xff6fb5)},
        {rgb(0xb8ff5c), rgb(0x2ad0c0)},
    };
    const Pal &p = kPal[c.palette % 3];
    V3 col = mix(rgb(0x020712), rgb(0x0a2433), sstep(0.0f, 0.85f, y));
    col = col + rgb(0xe8f0ff) * (stars(x, y, c.seed + 5, 0.3f) * sstep(0.75f, 0.1f, y));
    V3 light{0.0f, 0.0f, 0.0f};
    for (int k = 0; k < 3; ++k)
    {
        const float fk = static_cast<float>(k);
        const float yc = 0.34f + 0.13f * fk + 0.085f * std::sin(x * (1.7f + 0.5f * fk) + fk * 1.9f + hash(c.seed, 20 + static_cast<std::uint32_t>(k)) * 6.0f) +
                         0.07f * (fbm1(x * 1.4f + fk * 7.0f, 3, c.seed + 40) - 0.5f);
        const float up = yc - y;
        const float intensity = up > 0.0f ? std::exp(-up * (4.2f + 1.5f * fk)) : std::exp(up * 46.0f);
        const float rays = 0.45f + 0.55f * fbm1(x * 15.0f + fk * 9.0f, 3, c.seed + 50u + static_cast<std::uint32_t>(k));
        light = light + mix(p.low, p.high, sstep(0.0f, 0.34f, up)) * (intensity * rays * (0.95f - 0.2f * fk));
    }
    col = col + light * 0.9f;
    const float ground = 0.83f + 0.03f * fbm1(x * 1.6f, 3, c.seed + 60);
    if (y > ground)
        col = mix(rgb(0x0b1c2a), p.low * 0.28f + rgb(0x1a2a3a), sstep(0.0f, 0.2f, y - ground) * 0.6f + 0.2f);
    for (int layer = 0; layer < 2; ++layer)
    {
        const float spacing = layer == 0 ? 0.045f : 0.075f;
        const float base = ground + (layer == 0 ? 0.0f : 0.07f);
        const float gx = x / spacing;
        for (int o = -1; o <= 1; ++o)
        {
            const auto idx = static_cast<std::uint32_t>(static_cast<std::int32_t>(std::floor(gx)) + o);
            const float tx = (static_cast<float>(static_cast<std::int32_t>(idx)) + 0.2f + 0.6f * hash(idx, c.seed + 70u + static_cast<std::uint32_t>(layer))) * spacing;
            const float tall = (layer == 0 ? 0.1f : 0.2f) * (0.55f + 0.9f * hash(idx, c.seed + 80u + static_cast<std::uint32_t>(layer)));
            const float wide = tall * 0.2f;
            const float rise = (base - y) / tall; // 0 at the foot, 1 at the tip
            if (rise > 0.0f && rise < 1.0f)
            {
                // Tiers of branches: a saw-tooth edge.
                const float edge = wide * (1.0f - rise) * (0.7f + 0.3f * fract(rise * 6.0f));
                if (std::fabs(x - tx) < edge)
                    col = layer == 0 ? rgb(0x071420) : rgb(0x02070c);
            }
        }
        if (layer == 1 && y > base)
            col = rgb(0x02070c);
    }
    return col;
}

V3 stage(const Ctx &c, float x, float y)
{
    struct Pal
    {
        V3 a, b;
    };
    static const Pal kPal[] = {
        {rgb(0xff3d8b), rgb(0x4f9dff)},
        {rgb(0xffb347), rgb(0xff4d3d)},
        {rgb(0x39e0c0), rgb(0x9a5cff)},
    };
    const Pal &p = kPal[c.palette % 3];
    const float haze = fbm2(x * 2.2f, y * 2.6f, 4, c.seed + 1);
    V3 col = mix(rgb(0x06040c), rgb(0x100a1c), y) * (0.8f + 0.5f * haze);
    constexpr int kBeams = 6;
    for (int i = 0; i < kBeams; ++i)
    {
        const float fi = static_cast<float>(i);
        const float ox = c.aspect * (0.08f + 0.84f * fi / static_cast<float>(kBeams - 1));
        const float tx = c.aspect * (0.5f + (hash(c.seed, 30u + static_cast<std::uint32_t>(i)) - 0.5f) * 1.1f);
        const float ax = tx - ox;
        const float ay = 1.05f;
        const float len = std::hypot(ax, ay);
        const float ux = ax / len;
        const float uy = ay / len;
        const float rx = x - ox;
        const float ry = y + 0.05f;
        const float along = rx * ux + ry * uy;
        if (along <= 0.0f)
            continue;
        const float perp = std::fabs(rx * uy - ry * ux);
        const float width = 0.012f + along * 0.085f;
        const float beam = std::exp(-(perp * perp) / (width * width)) * (1.0f - 0.55f * sat(along));
        const V3 tint = i % 3 == 2 ? rgb(0xfff4e0) : (i % 2 == 0 ? p.a : p.b);
        col = col + tint * (beam * (0.5f + 0.5f * haze) * 0.85f);
        const float lamp = std::hypot(x - ox, y - 0.0f);
        col = col + tint * (0.9f * std::exp(-lamp * 38.0f));
    }
    const float glow = std::exp(-std::hypot((x - c.aspect * 0.5f) * 0.7f, (y - 0.86f) * 1.8f) * 3.2f);
    col = col + mix(p.a, p.b, 0.5f) * (glow * 0.55f);
    // The crowd: a row of heads, a few raised hands.
    const float gx = x * 21.0f;
    const auto idx = static_cast<std::uint32_t>(static_cast<std::int32_t>(std::floor(gx)));
    const float head = std::sqrt(sat(1.0f - std::pow((fract(gx) - 0.5f) * 2.2f, 2.0f)));
    float line = 0.9f - 0.045f * head * (0.6f + 0.6f * hash(idx, c.seed + 9)) - 0.02f * noise1(x * 3.0f, c.seed + 4);
    if (hash(idx, c.seed + 12) > 0.78f && std::fabs(fract(gx) - 0.78f) < 0.07f)
        line -= 0.07f;
    if (y > line)
        col = mix(rgb(0x030208), mix(p.a, p.b, 0.5f) * 0.5f, std::exp(-(y - line) * 70.0f) * 0.7f);
    return col;
}

V3 dunes(const Ctx &c, float x, float y)
{
    struct Pal
    {
        V3 sky_top, sky_low, lit, shade, sun;
    };
    static const Pal kPal[] = {
        {rgb(0x2b4a7a), rgb(0xf4c58a), rgb(0xf0a85a), rgb(0x7a3f24), rgb(0xfff2d0)},
        {rgb(0x0b1030), rgb(0x3a4a8a), rgb(0x7a86c8), rgb(0x161a44), rgb(0xe8eeff)},
        {rgb(0x6a4a8a), rgb(0xffc0a0), rgb(0xffb07a), rgb(0x8a3f52), rgb(0xfff0e0)},
        {rgb(0x1f6f8f), rgb(0xfde9b8), rgb(0xf6cf8a), rgb(0xa8743a), rgb(0xffffff)},
    };
    const Pal &p = kPal[c.palette % 4];
    const float horizon = 0.5f;
    V3 col = mix(p.sky_top, p.sky_low, std::pow(sat(y / horizon), 1.3f));
    const float sx = c.aspect * (0.25f + 0.5f * hash(c.seed, 1));
    const float d = std::hypot(x - sx, y - 0.2f);
    col = col + p.sun * (0.35f * std::exp(-d * 4.0f));
    col = mix(col, p.sun, 1.0f - sstep(0.058f, 0.062f, d));
    constexpr int kLayers = 5;
    for (int i = 0; i < kLayers; ++i)
    {
        const float fi = static_cast<float>(i);
        const float t = fi / static_cast<float>(kLayers - 1);
        const float f = 1.6f + 0.5f * fi;
        const float ph = hash(c.seed, 10u + static_cast<std::uint32_t>(i)) * 6.28f;
        const auto crest = [&](float at)
        {
            return horizon - 0.03f + t * 0.4f +
                   (0.05f + 0.05f * t) * (0.62f * std::sin(at * f + ph) + 0.28f * std::sin(at * f * 2.3f + ph * 1.7f) +
                                          0.3f * (fbm1(at * 1.1f + fi * 5.0f, 3, c.seed + 5) - 0.5f));
        };
        const float line = crest(x);
        if (y > line)
        {
            const float slope = (crest(x + 0.01f) - line) / 0.01f;
            const float facing = sstep(-0.12f, 0.12f, slope * (sx > c.aspect * 0.5f ? 1.0f : -1.0f));
            V3 layer = mix(p.shade, p.lit, 0.25f + 0.75f * facing);
            layer = layer * (1.0f - 0.35f * sstep(0.0f, 0.3f, y - line));
            layer = mix(layer, p.sky_low, (1.0f - t) * 0.55f);
            // Wind ripples on the nearest dune.
            if (i == kLayers - 1)
                layer = layer * (0.94f + 0.08f * std::sin((y - line) * 240.0f + x * 30.0f + 3.0f * noise1(x * 4.0f, c.seed)));
            col = layer;
        }
    }
    return col;
}

V3 blobs(const Ctx &c, float x, float y)
{
    static const std::uint32_t kPal[][4] = {
        {0x0e0b2a, 0xff5c7a, 0x7a5cff, 0x2ad0c0},
        {0x101820, 0xffb347, 0xff4d6d, 0x5b8cff},
        {0x07121a, 0x39d98a, 0x2f80ed, 0xf2c94c},
        {0x1a0b12, 0xff8a5c, 0xff3d8b, 0xffd166},
        {0x0a0f24, 0x5ce1ff, 0x8a6cff, 0xff7ac8},
    };
    const std::uint32_t *pal = kPal[c.palette % 5];
    const float wx = x + 0.35f * (fbm2(x * 1.2f, y * 1.2f, 3, c.seed + 1) - 0.5f);
    const float wy = y + 0.35f * (fbm2(x * 1.2f + 7.0f, y * 1.2f, 3, c.seed + 2) - 0.5f);
    V3 col = rgb(pal[0]);
    for (int i = 0; i < 3; ++i)
    {
        const auto ui = static_cast<std::uint32_t>(i);
        const float bx = c.aspect * (0.15f + 0.7f * hash(c.seed, 10 + ui));
        const float by = 0.15f + 0.7f * hash(c.seed, 20 + ui);
        const float r = 0.45f + 0.35f * hash(c.seed, 30 + ui);
        const float d = std::hypot(wx - bx, wy - by);
        col = mix(col, rgb(pal[i + 1]), std::exp(-(d * d) / (r * r)) * 0.88f);
    }
    // A motif: rings, or a disc with its echo.
    const float mx = c.aspect * (0.35f + 0.3f * hash(c.seed, 41));
    const float my = 0.42f + 0.16f * hash(c.seed, 42);
    const float d = std::hypot(x - mx, y - my);
    if (hash(c.seed, 43) > 0.5f)
    {
        const float ring = std::fabs(fract(d * 6.5f) - 0.5f);
        col = col + rgb(0xffffff) * ((1.0f - sstep(0.0f, 0.035f, 0.5f - ring)) * 0.16f * sstep(0.62f, 0.3f, d));
    }
    else
    {
        col = mix(col, rgb(0xffffff), (1.0f - sstep(0.196f, 0.2f, d)) * 0.92f);
        col = col + rgb(0xffffff) * ((1.0f - sstep(0.0f, 0.004f, std::fabs(d - 0.27f))) * 0.5f);
        col = col + rgb(0xffffff) * ((1.0f - sstep(0.0f, 0.003f, std::fabs(d - 0.34f))) * 0.25f);
    }
    return col;
}

V3 globe(const Ctx &c, float x, float y)
{
    struct Pal
    {
        V3 sea, land, air, grid;
        float lights;
    };
    static const Pal kPal[] = {
        {rgb(0x0f3d7a), rgb(0x3f9f6a), rgb(0x7fd0ff), rgb(0xffffff), 0.0f},
        {rgb(0x040d1c), rgb(0x0c1f2c), rgb(0x3f8fff), rgb(0x7fb0ff), 1.0f},
        {rgb(0x151a38), rgb(0x5b6cff), rgb(0x9aa8ff), rgb(0xffffff), 0.0f},
        {rgb(0x2a1420), rgb(0xff7a59), rgb(0xffb8a0), rgb(0xffe8d8), 0.0f},
    };
    const Pal &p = kPal[c.palette % 4];
    V3 col = mix(rgb(0x02040c), rgb(0x0a1024), y);
    col = col + rgb(0xe0e8ff) * stars(x, y, c.seed + 5, 0.2f);
    const float cx = c.aspect * 0.5f;
    const float cy = 0.64f;
    const float radius = 0.5f;
    const float dx = (x - cx) / radius;
    const float dy = (y - cy) / radius;
    const float d2 = dx * dx + dy * dy;
    if (d2 >= 1.0f)
    {
        const float d = std::sqrt(d2);
        return col + p.air * (std::exp(-(d - 1.0f) * 16.0f) * 0.6f * sat(0.7f - dy * 0.6f));
    }
    const float nz = std::sqrt(1.0f - d2);
    const float turn = hash(c.seed, 3) * 6.28f;
    const float lon = std::atan2(dx, nz) + turn;
    const float lat = std::asin(std::clamp(-dy, -1.0f, 1.0f));
    const float continent = fbm2(lon * 1.5f + 5.0f, lat * 2.3f, 5, c.seed + 7);
    const bool land = continent > 0.53f;
    const float diffuse = sat(-dx * 0.5f - dy * 0.55f + nz * 0.67f);
    V3 surface = land ? p.land : p.sea;
    if (!land)
        surface = mix(surface, p.land, 0.25f * sstep(0.45f, 0.53f, continent)); // shallows
    V3 out = surface * (0.1f + 1.05f * diffuse);
    if (p.lights > 0.0f && land)
    {
        const float cell = 0.035f;
        const auto ix = static_cast<std::uint32_t>(static_cast<std::int32_t>(std::floor(lon / cell)));
        const auto iy = static_cast<std::uint32_t>(static_cast<std::int32_t>(std::floor(lat / cell)));
        if (hash(ix, iy, c.seed + 9) > 0.62f)
        {
            const float ld = std::hypot(fract(lon / cell) - 0.5f, fract(lat / cell) - 0.5f);
            out = out + rgb(0xffd27a) * ((1.0f - sstep(0.1f, 0.34f, ld)) * 0.9f);
        }
    }
    const float step = 3.14159265f / 9.0f;
    const float glon = std::fabs(fract(lon / step) - 0.5f);
    const float glat = std::fabs(fract(lat / step) - 0.5f);
    const float grid = std::max(1.0f - sstep(0.0f, 0.02f / std::max(0.2f, std::cos(lat)), 0.5f - glon),
                                1.0f - sstep(0.0f, 0.02f, 0.5f - glat));
    out = out + p.grid * (grid * 0.14f * (0.3f + diffuse));
    out = out + p.air * (std::pow(1.0f - nz, 2.6f) * (0.2f + 0.8f * diffuse) * 0.9f);
    return out;
}

float box(float x, float y, float cx, float cy, float hw, float hh, float r)
{
    const float qx = std::fabs(x - cx) - hw + r;
    const float qy = std::fabs(y - cy) - hh + r;
    return std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - r;
}

V3 desk(const Ctx &c, float x, float y)
{
    struct Pal
    {
        V3 a, b, wall;
    };
    static const Pal kPal[] = {
        {rgb(0x6a5cff), rgb(0xff5ca8), rgb(0x0e0f1c)},
        {rgb(0x2ad0c0), rgb(0x2f6fed), rgb(0x0a1216)},
        {rgb(0xffb347), rgb(0xff5a3d), rgb(0x15100c)},
    };
    const Pal &p = kPal[c.palette % 3];
    const float cx = c.aspect * 0.5f;
    const float top = 0.79f;
    V3 col = mix(p.wall, p.wall * 1.7f, sstep(0.0f, 0.8f, y));
    const float screen = box(x, y, cx, 0.4f, 0.5f, 0.27f, 0.012f);
    const V3 spill = mix(p.a, p.b, sat((x - cx + 0.5f)));
    col = col + spill * (0.5f * std::exp(-std::max(screen, 0.0f) * 4.5f));
    // A light bar above the screen.
    col = col + rgb(0xfff0d8) * (0.8f * std::exp(-std::max(box(x, y, cx, 0.105f, 0.3f, 0.004f, 0.004f), 0.0f) * 60.0f));
    if (screen < 0.0f)
    {
        col = rgb(0x05060a);
        if (screen < -0.012f)
        {
            const float gx = sat((x - (cx - 0.5f)) / 1.0f);
            const float gy = sat((y - 0.13f) / 0.54f);
            col = mix(p.a, p.b, sat(gx * 0.7f + gy * 0.5f)) * 0.9f;
            col = col + rgb(0xffffff) * (0.1f * fbm2(x * 3.0f, y * 3.0f, 3, c.seed));
            // Windows on the desktop.
            if (box(x, y, cx - 0.2f, 0.36f, 0.2f, 0.15f, 0.014f) < 0.0f)
                col = mix(col, rgb(0x0c0e18), 0.72f);
            if (box(x, y, cx + 0.24f, 0.44f, 0.16f, 0.17f, 0.014f) < 0.0f)
                col = mix(col, rgb(0xf4f6ff), 0.82f);
            for (int i = 0; i < 5; ++i)
            {
                const float ly = 0.27f + 0.04f * static_cast<float>(i);
                const float lw = 0.05f + 0.1f * hash(c.seed, 50u + static_cast<std::uint32_t>(i));
                if (box(x, y, cx - 0.36f + lw, ly, lw, 0.007f, 0.007f) < 0.0f)
                    col = mix(i % 2 == 0 ? p.a : p.b, rgb(0xffffff), 0.45f);
            }
        }
    }
    if (box(x, y, cx, 0.72f, 0.03f, 0.06f, 0.004f) < 0.0f || box(x, y, cx, top - 0.008f, 0.14f, 0.008f, 0.006f) < 0.0f)
        col = rgb(0x08090e);
    for (int side = -1; side <= 1; side += 2)
    {
        const float sxp = cx + static_cast<float>(side) * 0.72f;
        if (box(x, y, sxp, 0.62f, 0.085f, 0.17f, 0.012f) < 0.0f)
        {
            col = rgb(0x0b0c12) + spill * 0.06f;
            const float cone = std::hypot(x - sxp, y - 0.67f);
            if (cone < 0.06f)
                col = mix(rgb(0x14161f), rgb(0x05060a), sstep(0.02f, 0.06f, cone));
            if (std::hypot(x - sxp, y - 0.52f) < 0.022f)
                col = rgb(0x1a1d28);
        }
    }
    if (y > top)
    {
        const float depth = y - top;
        col = mix(rgb(0x1c130e), rgb(0x0b0806), sstep(0.0f, 0.21f, depth));
        col = col + spill * (0.42f * std::exp(-depth * 9.0f) * std::exp(-std::pow((x - cx) / 0.6f, 2.0f)));
        // Keys lit one hue per column.
        if (box(x, y, cx, top + 0.085f, 0.34f, 0.03f, 0.008f) < 0.0f)
        {
            const float kx = fract((x - cx) * 22.0f);
            const float ky = fract((y - top - 0.055f) * 50.0f);
            col = rgb(0x0a0b10);
            if (kx > 0.16f && kx < 0.84f && ky > 0.2f && ky < 0.8f)
                col = mix(p.a, p.b, sat((x - cx + 0.34f) / 0.68f)) * 0.75f;
        }
    }
    return col;
}

V3 plate(const Ctx &c, float x, float y)
{
    struct Pal
    {
        V3 table, table2, a, b, d;
    };
    static const Pal kPal[] = {
        {rgb(0x3a2416), rgb(0x2a180e), rgb(0xe8503a), rgb(0x6fbf4a), rgb(0xf2c94c)},
        {rgb(0x20262b), rgb(0x161b1f), rgb(0xff8a5c), rgb(0x9bd36a), rgb(0xfff0c8)},
        {rgb(0xd9c7a8), rgb(0xc9b592), rgb(0xc0392b), rgb(0x2e8b57), rgb(0xf4d35e)},
    };
    const Pal &p = kPal[c.palette % 3];
    const float grain = fbm1(y * 34.0f + 2.5f * fbm1(x * 2.0f, 3, c.seed), 4, c.seed + 1);
    V3 col = mix(p.table2, p.table, grain);
    if (fract(x * 1.9f + 0.3f) < 0.012f)
        col = col * 0.72f; // board joints
    const float cx = c.aspect * (0.46f + 0.1f * hash(c.seed, 2));
    const float cy = 0.52f;
    const float d = std::hypot(x - cx, y - cy);
    col = col * (1.0f - 0.55f * std::exp(-std::max(std::hypot(x - cx - 0.03f, y - cy - 0.04f) - 0.37f, 0.0f) * 14.0f));
    if (d < 0.38f)
    {
        col = mix(rgb(0xf6f3ec), rgb(0xd8d2c6), sstep(0.3f, 0.38f, d));
        if (d < 0.285f)
            col = mix(rgb(0xece7dc), rgb(0xfbf9f4), sstep(0.285f, 0.2f, d));
        for (int i = 0; i < 9; ++i)
        {
            const auto ui = static_cast<std::uint32_t>(i);
            const float ang = hash(c.seed, 10 + ui) * 6.28f;
            const float rad = 0.03f + 0.15f * std::sqrt(hash(c.seed, 20 + ui));
            const float fx = cx + std::cos(ang) * rad;
            const float fy = cy + std::sin(ang) * rad;
            const float fr = 0.03f + 0.055f * hash(c.seed, 30 + ui);
            const float fd = std::hypot(x - fx, y - fy);
            if (fd < fr)
            {
                const V3 food = i % 3 == 0 ? p.a : (i % 3 == 1 ? p.b : p.d);
                col = food * (0.72f + 0.4f * sstep(fr, 0.0f, std::hypot(x - fx + fr * 0.3f, y - fy + fr * 0.3f)));
            }
        }
    }
    // A bowl leaving the frame and a pair of sticks.
    const float bd = std::hypot(x - c.aspect * 0.93f, y - 0.14f);
    if (bd < 0.17f)
        col = bd < 0.135f ? mix(p.b * 0.8f, p.d, fbm2(x * 9.0f, y * 9.0f, 3, c.seed + 3)) : rgb(0x1f2a36);
    for (int i = 0; i < 2; ++i)
    {
        const float off = 0.03f * static_cast<float>(i);
        const float sd = std::fabs((y - 0.08f) - (x - (0.1f + off)) * 2.6f) / 2.8f;
        if (sd < 0.006f && y > 0.1f && y < 0.92f && x < cx - 0.3f)
            col = rgb(0x1a120c) * 1.4f;
    }
    return col;
}

V3 pitch(const Ctx &c, float x, float y)
{
    struct Pal
    {
        V3 a, b, out;
    };
    static const Pal kPal[] = {
        {rgb(0x2f8f4a), rgb(0x277c3f), rgb(0x1c5a2e)},
        {rgb(0xc7623a), rgb(0xb9552f), rgb(0x2f6f4f)},
        {rgb(0x2f6fb5), rgb(0x2861a3), rgb(0x1f8f6f)},
    };
    const Pal &p = kPal[c.palette % 3];
    const float cx = c.aspect * 0.5f;
    const float turf = 0.94f + 0.12f * fbm2(x * 40.0f, y * 40.0f, 2, c.seed);
    const bool inside = box(x, y, cx, 0.5f, c.aspect * 0.46f, 0.42f, 0.0f) < 0.0f;
    V3 col = inside ? ((static_cast<int>(std::floor(x * 5.0f)) & 1) != 0 ? p.a : p.b) : p.out;
    col = col * turf;
    float line = 0.0f;
    const auto stroke = [&](float d) { line = std::max(line, 1.0f - sstep(0.0035f, 0.0055f, std::fabs(d))); };
    stroke(box(x, y, cx, 0.5f, c.aspect * 0.46f, 0.42f, 0.0f));
    if (std::fabs(y - 0.5f) < 0.42f)
        stroke(x - cx);
    stroke(std::hypot(x - cx, y - 0.5f) - 0.15f);
    for (int side = -1; side <= 1; side += 2)
    {
        const float ex = cx + static_cast<float>(side) * c.aspect * 0.46f;
        stroke(box(x, y, ex, 0.5f, 0.26f, 0.24f, 0.0f));
        stroke(box(x, y, ex, 0.5f, 0.1f, 0.1f, 0.0f));
    }
    if (inside)
        col = mix(col, rgb(0xf4f6f0), line * 0.9f);
    if (std::hypot(x - cx, y - 0.5f) < 0.012f)
        col = rgb(0xf4f6f0);
    const float bx = cx + 0.32f + 0.2f * hash(c.seed, 3);
    const float by = 0.36f + 0.2f * hash(c.seed, 4);
    col = col * (1.0f - 0.5f * std::exp(-std::hypot(x - bx - 0.012f, y - by - 0.014f) * 60.0f));
    if (std::hypot(x - bx, y - by) < 0.018f)
        col = rgb(0xffffff);
    // Floodlights from two corners.
    col = col + rgb(0xfff6d8) * (0.22f * std::exp(-std::hypot(x, y) * 1.6f) + 0.22f * std::exp(-std::hypot(x - c.aspect, y) * 1.6f));
    return col;
}

V3 code(const Ctx &c, float x, float y)
{
    static const std::uint32_t kInk[] = {0x7aa2f7, 0xbb9af7, 0x9ece6a, 0xe0af68, 0xf7768e, 0x565f89};
    V3 col = rgb(0x0f1220);
    const float split = c.aspect * 0.56f;
    if (x < 0.1f)
        col = rgb(0x0b0d18);
    const float line_h = 0.058f;
    const auto row = static_cast<std::uint32_t>(y / line_h);
    const float fy = fract(y / line_h);
    if (x < split)
    {
        if (row == 6)
            col = col + rgb(0x1a2040);
        if (x < 0.1f && x > 0.045f && fy > 0.36f && fy < 0.64f && fract(x * 60.0f) < 0.6f)
            col = rgb(0x3a4166);
        float cursor = 0.14f + 0.05f * std::floor(hash(row, c.seed) * 4.0f);
        const int tokens = 1 + static_cast<int>(hash(row, c.seed + 1) * 4.99f);
        if (hash(row, c.seed + 2) > 0.14f)
        {
            for (int t = 0; t < tokens; ++t)
            {
                const auto ut = static_cast<std::uint32_t>(t);
                const float len = 0.05f + 0.16f * hash(row, ut, c.seed + 3);
                if (cursor + len > split - 0.05f)
                    break;
                if (box(x, y, cursor + len * 0.5f, (static_cast<float>(row) + 0.5f) * line_h, len * 0.5f, 0.011f, 0.011f) < 0.0f)
                    col = rgb(kInk[static_cast<std::size_t>(hash(row, ut, c.seed + 4) * 5.99f)]);
                cursor += len + 0.022f;
            }
        }
        return col;
    }
    // The other half: what the program draws, a ball on a checked floor.
    const float px = (x - split) / (c.aspect - split);
    col = mix(rgb(0x1a2550), rgb(0x0a0d1c), sstep(0.0f, 0.62f, y));
    if (y > 0.62f)
    {
        const float z = y - 0.62f + 0.02f;
        const bool check = (static_cast<int>(std::floor((px - 0.5f) / z * 0.5f)) + static_cast<int>(std::floor(0.3f / z))) & 1;
        col = mix(check ? rgb(0x2a3566) : rgb(0x141a38), rgb(0x0a0d1c), std::exp(-z * 9.0f));
    }
    const float bx = (px - 0.5f) * (c.aspect - split);
    const float by = y - 0.5f;
    const float radius = 0.21f;
    const float d2 = (bx * bx + by * by) / (radius * radius);
    if (d2 < 1.0f)
    {
        const float nz = std::sqrt(1.0f - d2);
        const float diffuse = sat(-bx / radius * 0.5f - by / radius * 0.6f + nz * 0.62f);
        col = mix(rgb(0xff5a47), rgb(0xffc857), c.palette % 2 == 0 ? 0.0f : 1.0f) * (0.08f + diffuse) +
              rgb(0xffffff) * std::pow(diffuse, 24.0f) * 0.7f;
    }
    if (x < split + 0.004f)
        col = rgb(0x05060c);
    return col;
}

V3 shade(Scene scene, const Ctx &c, float x, float y)
{
    switch (scene)
    {
    case Scene::ridges:
        return ridges(c, x, y);
    case Scene::city:
        return city(c, x, y);
    case Scene::synth:
        return synth(c, x, y);
    case Scene::space:
        return space(c, x, y);
    case Scene::ocean:
        return ocean(c, x, y);
    case Scene::aurora:
        return aurora(c, x, y);
    case Scene::stage:
        return stage(c, x, y);
    case Scene::dunes:
        return dunes(c, x, y);
    case Scene::blobs:
        return blobs(c, x, y);
    case Scene::globe:
        return globe(c, x, y);
    case Scene::desk:
        return desk(c, x, y);
    case Scene::plate:
        return plate(c, x, y);
    case Scene::pitch:
        return pitch(c, x, y);
    case Scene::code:
    case Scene::count:
        break;
    }
    return code(c, x, y);
}

} // namespace

Painted paint_art(Scene scene, std::uint32_t seed, int palette, int width, int height)
{
    Painted out;
    out.width = width;
    out.height = height;
    const Ctx ctx{seed, palette, static_cast<float>(width) / static_cast<float>(height)};

    std::vector<float> image(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3);
    static const float kOffsets[4][2] = {{0.375f, 0.125f}, {0.875f, 0.375f}, {0.125f, 0.625f}, {0.625f, 0.875f}};
    const float inv_h = 1.0f / static_cast<float>(height);
    for (int py = 0; py < height; ++py)
    {
        for (int px = 0; px < width; ++px)
        {
            V3 sum{0.0f, 0.0f, 0.0f};
            for (const auto &offset : kOffsets)
            {
                const float x = (static_cast<float>(px) + offset[0]) * inv_h;
                const float y = (static_cast<float>(py) + offset[1]) * inv_h;
                sum = sum + shade(scene, ctx, x, y);
            }
            V3 col = sum * 0.25f;
            // A lens: darker corners and a little grain.
            const float vx = (static_cast<float>(px) + 0.5f) / static_cast<float>(width) - 0.5f;
            const float vy = (static_cast<float>(py) + 0.5f) * inv_h - 0.5f;
            col = col * (1.0f - 0.5f * std::pow(vx * vx + vy * vy, 1.15f));
            const float grain = (hash(static_cast<std::uint32_t>(px), static_cast<std::uint32_t>(py), seed) - 0.5f) * 0.022f;
            const std::size_t at = (static_cast<std::size_t>(py) * static_cast<std::size_t>(width) + static_cast<std::size_t>(px)) * 3;
            image[at] = sat(col.r + grain);
            image[at + 1] = sat(col.g + grain);
            image[at + 2] = sat(col.b + grain);
        }
    }

    // The colours a screen takes from the picture.
    double mean[3] = {0.0, 0.0, 0.0};
    double vivid[3] = {0.0, 0.0, 0.0};
    double weight = 0.0;
    const std::size_t pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    for (std::size_t i = 0; i < pixels; ++i)
    {
        const float r = image[i * 3];
        const float g = image[i * 3 + 1];
        const float b = image[i * 3 + 2];
        mean[0] += r;
        mean[1] += g;
        mean[2] += b;
        const float high = std::max(r, std::max(g, b));
        const float low = std::min(r, std::min(g, b));
        const float chroma = high > 0.0f ? (high - low) / high : 0.0f;
        const double w = static_cast<double>(chroma * chroma * high * high) + 1e-6;
        vivid[0] += r * w;
        vivid[1] += g * w;
        vivid[2] += b * w;
        weight += w;
    }
    out.tone = {static_cast<float>(mean[0] / static_cast<double>(pixels)),
                static_cast<float>(mean[1] / static_cast<double>(pixels)),
                static_cast<float>(mean[2] / static_cast<double>(pixels)), 1.0f};
    gfx::Color glow{static_cast<float>(vivid[0] / weight), static_cast<float>(vivid[1] / weight),
                    static_cast<float>(vivid[2] / weight), 1.0f};
    // Lift it to a usable light: full value, whatever the picture's exposure.
    const float high = std::max(glow.r, std::max(glow.g, glow.b));
    if (high > 0.0f)
    {
        const float lift = 0.95f / high;
        glow.r *= lift;
        glow.g *= lift;
        glow.b *= lift;
    }
    out.glow = glow;

    out.rgba.resize(pixels * 4);
    for (std::size_t i = 0; i < pixels; ++i)
    {
        out.rgba[i * 4] = static_cast<std::uint8_t>(image[i * 3] * 255.0f + 0.5f);
        out.rgba[i * 4 + 1] = static_cast<std::uint8_t>(image[i * 3 + 1] * 255.0f + 0.5f);
        out.rgba[i * 4 + 2] = static_cast<std::uint8_t>(image[i * 3 + 2] * 255.0f + 0.5f);
        out.rgba[i * 4 + 3] = 255;
    }

    // The ambient copy: box-filtered to 64 x 36, then blurred until only the
    // big masses of colour are left.
    constexpr int kW = 64;
    constexpr int kH = 36;
    std::vector<float> small(static_cast<std::size_t>(kW * kH * 3), 0.0f);
    for (int sy = 0; sy < kH; ++sy)
    {
        for (int sx = 0; sx < kW; ++sx)
        {
            const int x0 = sx * width / kW;
            const int x1 = std::max(x0 + 1, (sx + 1) * width / kW);
            const int y0 = sy * height / kH;
            const int y1 = std::max(y0 + 1, (sy + 1) * height / kH);
            float acc[3] = {0.0f, 0.0f, 0.0f};
            for (int yy = y0; yy < y1; ++yy)
            {
                for (int xx = x0; xx < x1; ++xx)
                {
                    const std::size_t at = (static_cast<std::size_t>(yy) * static_cast<std::size_t>(width) + static_cast<std::size_t>(xx)) * 3;
                    acc[0] += image[at];
                    acc[1] += image[at + 1];
                    acc[2] += image[at + 2];
                }
            }
            const float n = static_cast<float>((x1 - x0) * (y1 - y0));
            const std::size_t to = static_cast<std::size_t>(sy * kW + sx) * 3;
            small[to] = acc[0] / n;
            small[to + 1] = acc[1] / n;
            small[to + 2] = acc[2] / n;
        }
    }
    std::vector<float> scratch(small.size());
    for (int pass = 0; pass < 6; ++pass)
    {
        for (int axis = 0; axis < 2; ++axis)
        {
            for (int sy = 0; sy < kH; ++sy)
            {
                for (int sx = 0; sx < kW; ++sx)
                {
                    float acc[3] = {0.0f, 0.0f, 0.0f};
                    for (int k = -2; k <= 2; ++k)
                    {
                        const int xx = std::clamp(sx + (axis == 0 ? k : 0), 0, kW - 1);
                        const int yy = std::clamp(sy + (axis == 1 ? k : 0), 0, kH - 1);
                        const std::size_t at = static_cast<std::size_t>(yy * kW + xx) * 3;
                        acc[0] += small[at];
                        acc[1] += small[at + 1];
                        acc[2] += small[at + 2];
                    }
                    const std::size_t to = static_cast<std::size_t>(sy * kW + sx) * 3;
                    scratch[to] = acc[0] / 5.0f;
                    scratch[to + 1] = acc[1] / 5.0f;
                    scratch[to + 2] = acc[2] / 5.0f;
                }
            }
            small.swap(scratch);
        }
    }
    out.ambient.resize(static_cast<std::size_t>(kW * kH * 4));
    for (std::size_t i = 0; i < static_cast<std::size_t>(kW * kH); ++i)
    {
        // Compress the highlights: a page lit by a bright picture must still
        // carry white text, so every ambient copy lands in the same band of
        // brightness and keeps only its colour.
        const float lum = 0.2126f * small[i * 3] + 0.7152f * small[i * 3 + 1] + 0.0722f * small[i * 3 + 2];
        const float scale = std::min(1.7f, 0.6f / (0.28f + lum));
        out.ambient[i * 4] = static_cast<std::uint8_t>(sat(small[i * 3] * scale) * 255.0f + 0.5f);
        out.ambient[i * 4 + 1] = static_cast<std::uint8_t>(sat(small[i * 3 + 1] * scale) * 255.0f + 0.5f);
        out.ambient[i * 4 + 2] = static_cast<std::uint8_t>(sat(small[i * 3 + 2] * scale) * 255.0f + 0.5f);
        out.ambient[i * 4 + 3] = 255;
    }
    return out;
}

std::vector<std::uint8_t> paint(std::string_view scene, std::uint32_t seed, int palette, int width, int height)
{
    static constexpr const char *kNames[] = {"ridges", "city", "synth", "space", "ocean", "aurora", "stage", "dunes", "blobs", "globe", "desk", "plate", "pitch", "code"};
    for (std::size_t i = 0; i < sizeof kNames / sizeof kNames[0]; ++i)
    {
        if (scene == kNames[i])
            return paint_art(static_cast<Scene>(i), seed, palette, width, height).rgba;
    }
    return {};
}

} // namespace afterglow_test
