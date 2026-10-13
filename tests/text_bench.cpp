// ProsperoTube - Text engine: what it costs on this PC (the numbers of docs/afterglow/TEXT.md).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// usage: text_bench [stream]
// Times font opening, shaping per script, the cached path a frame takes, rasterising per size,
// and reports the memory held. Reads the fonts tests/text_host.hpp names.

#include "text_host.hpp"
#include "tube_text.hpp"
#include "tube_text_atlas.hpp"
#include "tube_text_fonts.hpp"

#include <malloc.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace tube::text;

namespace
{

double now()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::size_t heap()
{
    const struct mallinfo2 info = mallinfo2();
    return info.uordblks + info.hblkhd;
}

struct Script
{
    const char *name;
    const char *title; // an invented title, about forty characters
};

// How far a distance field filled at `fill` times the size is from one filled at eight times,
// in pixels at the em, over the pixels near an outline; for the choice of the filling factor.
void accuracy()
{
    std::vector<std::string> messages;
    Config config = text_host::stand_in(text_host::windows("segoeui.ttf"));
    Fonts fonts(config, &messages);
    Rasteriser rasteriser;
    struct Sample
    {
        int face;
        std::u32string characters;
    };
    const Sample samples[] = {
        {0, U"aegkwRS&@%"},
        {1, U"\U00006771\U00004EAC\U00009B31\U000097FF\U00007E41\U00003042\U000030AC\U00009928"}, // dense kanji, kana
        {3, U"\U0000D55C\U0000AE00\U0000BDC1\U0000C6B8"},                                         // Hangul
        {5, U"\U00000639\U00000634\U00000636\U00000647"},                                         // Arabic
    };
    std::printf("\ndistance field error against an eight-fold fill, pixels at the em (mean / 99th percentile / worst):\n");
    for (const int em : {28, 56, 112})
    {
        const float spread = static_cast<float>(em) * config.spread;
        for (const int fill : {1, 2, 4})
        {
            std::vector<float> errors;
            double seconds = 0.0;
            int glyphs = 0;
            for (const Sample &sample : samples)
            {
                FT_FaceRec_ *face = fonts.outlines(sample.face);
                for (const char32_t c : sample.characters)
                {
                    hb_codepoint_t id = 0;
                    if (face == nullptr || !hb_font_get_nominal_glyph(fonts.shaper(sample.face), c, &id))
                        continue;
                    GlyphImage reference;
                    GlyphImage image;
                    if (!rasteriser.glyph(face, id, em, spread, 0.0f, &reference, 8))
                        continue;
                    const double start = now();
                    for (int repeat = 0; repeat < 20; ++repeat)
                        rasteriser.glyph(face, id, em, spread, 0.0f, &image, fill);
                    seconds += (now() - start) / 20;
                    ++glyphs;
                    const int dx = static_cast<int>(image.left - reference.left);
                    const int dy = static_cast<int>(image.top - reference.top);
                    for (int y = 0; y < image.h; ++y)
                        for (int x = 0; x < image.w; ++x)
                        {
                            const int rx = x + dx;
                            const int ry = y + dy;
                            if (rx < 0 || ry < 0 || rx >= reference.w || ry >= reference.h)
                                continue;
                            const int truth = reference.pixels[static_cast<std::size_t>(ry) * reference.w + rx];
                            if (truth < 96 || truth > 160)
                                continue; // within a quarter of the spread of an outline: what draws the edge
                            const int value = image.pixels[static_cast<std::size_t>(y) * image.w + x];
                            errors.push_back(static_cast<float>(value > truth ? value - truth : truth - value) * spread / 128.0f);
                        }
                }
            }
            if (errors.empty())
                continue;
            std::sort(errors.begin(), errors.end());
            double sum = 0.0;
            for (const float e : errors)
                sum += e;
            std::printf("  em %3d filled %dx%s: %.3f / %.3f / %.3f   %.3f ms a glyph\n", em, fill,
                        fill == Rasteriser::supersampling(em) ? " (used)" : "       ", sum / static_cast<double>(errors.size()),
                        errors[errors.size() * 99 / 100], errors.back(), seconds * 1000.0 / glyphs);
        }
    }
}

const Script kScripts[] = {
    {"Latin", "How to bake real sourdough bread at home, step by step"},
    {"Latin with marks", "Größe zählt: Käsespätzle, crème brûlée và phở bò Hà Nội"},
    {"Cyrillic", "Как приготовить настоящий борщ: бабушкин рецепт"},
    {"Japanese", "【完全攻略】初心者でも分かる！東京ラーメン食べ歩きガイド"},
    {"Chinese", "十分钟学会做正宗麻婆豆腐，简单又下饭的家常菜教程"},
    {"Korean", "서울 야경 브이로그 | 한강에서 보내는 완벽한 주말 하루"},
    {"Arabic", "أفضل عشر وجهات سياحية في العالم العربي لعام ٢٠٢٦"},
    {"Hebrew", "המדריך המלא לטיול בצפון: עשרה מקומות שחייבים לבקר"},
    {"Thai", "เที่ยวเชียงใหม่สามวันสองคืน งบไม่เกินห้าพันบาท กินอะไรดี"},
    {"Devanagari", "दिल्ली की सबसे स्वादिष्ट स्ट्रीट फूड यात्रा – पूरी जानकारी"},
    {"Mixed directions", "مراجعة هاتف Nova X 17 Pro بعد 30 يوم من الاستخدام"},
};

} // namespace

int main(int argc, char **argv)
{
    const bool stream = argc > 1 && std::strcmp(argv[1], "stream") == 0;
    if (!text_host::have_windows())
    {
        std::printf("SKIP text_bench: the Windows fonts are not in %s\n", text_host::windows_folder().c_str());
        return 0;
    }
    Config config = text_host::stand_in(text_host::windows("segoeui.ttf"));
    config.io = stream ? IoMode::stream : IoMode::automatic;
    config.glyphs_per_frame = 100000;
    const std::size_t heap_start = heap();
    double start = now();
    std::string error;
    std::unique_ptr<Context> context = Context::create(config, &error);
    if (!context)
        return 1;
    std::printf("fonts %s\n", stream ? "read on demand" : "mapped");
    std::printf("create: %.2f ms\n", (now() - start) * 1000.0);

    Style style;
    style.size = 30.0f;
    Constraints card;
    card.max_width = 480.0f;
    card.max_lines = 2;
    card.ellipsis = Ellipsis::end;
    Layout layout;

    // The first text of each script: its font is opened, its tables read.
    std::printf("\n%-18s %10s %10s %10s %10s %8s\n", "script", "first ms", "shape us", "wrap us", "frame us", "glyphs");
    for (const Script &script : kScripts)
    {
        context->advance_frame();
        start = now();
        const Metrics first = context->measure(script.title, style);
        const double first_ms = (now() - start) * 1000.0;
        (void)first;
        // Shaping alone, on texts the cache has not seen (a digit changes each).
        const int rounds = 300;
        start = now();
        for (int i = 0; i < rounds; ++i)
            context->measure(std::string(script.title) + " " + std::to_string(i), style);
        const double shape_us = (now() - start) * 1e6 / rounds;
        start = now();
        for (int i = 0; i < rounds; ++i)
            context->measure(std::string(script.title) + " " + std::to_string(i), style, card);
        const double wrap_us = (now() - start) * 1e6 / rounds;
        // What a frame pays for a text it drew before: the cache, then one atlas look-up a glyph.
        context->layout(script.title, style, card, &layout);
        start = now();
        for (int i = 0; i < 2000; ++i)
            context->layout(script.title, style, card, &layout);
        const double frame_us = (now() - start) * 1e6 / 2000;
        std::printf("%-18s %10.2f %10.1f %10.1f %10.2f %8zu\n", script.name, first_ms, shape_us, wrap_us, frame_us,
                    layout.quads.size());
    }

    // A screen: 24 cards (a two-line title, a channel line, a line of numbers), every frame.
    std::vector<std::string> titles;
    for (int i = 0; i < 24; ++i)
        titles.push_back(std::string(kScripts[i % (sizeof(kScripts) / sizeof(kScripts[0]))].title) + " #" + std::to_string(i));
    Style small;
    small.size = 22.0f;
    Constraints row;
    row.max_width = 480.0f;
    row.ellipsis = Ellipsis::end;
    const auto screen = [&]()
    {
        std::size_t quads = 0;
        for (const std::string &title : titles)
        {
            context->layout(title, style, card, &layout);
            quads += layout.quads.size();
            context->layout("Invented Channel " + title.substr(title.size() - 2), small, row, &layout);
            quads += layout.quads.size();
            context->layout("1.2M views \xc2\xb7 3 days ago", small, row, &layout);
            quads += layout.quads.size();
        }
        return quads;
    };
    context->advance_frame();
    start = now();
    std::size_t quads = screen();
    std::printf("\na screen of 24 cards (72 texts): first frame %.2f ms (shaping and rasterising everything), ", (now() - start) * 1000.0);
    start = now();
    for (int frame = 0; frame < 200; ++frame)
    {
        context->advance_frame();
        quads = screen();
    }
    std::printf("then %.3f ms a frame for %zu quads\n", (now() - start) * 1000.0 / 200, quads);

    // Rasterising: glyphs new to the atlas, by raster size.
    std::printf("\n%-18s %12s %12s %12s\n", "rasterising, ms/glyph", "em 28", "em 56", "em 112");
    for (const char *name : {"Latin", "Japanese", "Chinese", "Korean", "Arabic", "Devanagari"})
    {
        const char *title = nullptr;
        for (const Script &script : kScripts)
            if (std::strcmp(script.name, name) == 0)
                title = script.title;
        std::printf("%-18s", name);
        for (const float size : {21.0f, 60.0f, 96.0f})
        {
            // Another weight is another set of glyphs: nothing is in the atlas yet.
            Style fresh;
            fresh.size = size;
            fresh.weight = Weight::bold;
            const Stats before = context->stats();
            context->advance_frame();
            context->layout(title, fresh, {}, &layout);
            const Stats after = context->stats();
            const double drawn = static_cast<double>(after.rasterised - before.rasterised);
            std::printf(" %12.3f", drawn > 0 ? (after.raster_seconds - before.raster_seconds) * 1000.0 / drawn : 0.0);
        }
        std::printf("\n");
    }

    const Stats stats = context->stats();
    std::printf("\nmemory: heap grew by %.2f MB in all; atlas %u page(s) = %.2f MB, %u glyphs; shaped texts kept %u = %.2f MB\n",
                static_cast<double>(heap() - heap_start) / 1048576.0, stats.pages, static_cast<double>(stats.atlas_bytes) / 1048576.0,
                stats.glyphs, stats.layouts, static_cast<double>(stats.layout_bytes) / 1048576.0);
    std::printf("fonts: %u opened in %.1f ms, %.2f MB mapped, %.2f MB read from the files\n", stats.faces_open,
                stats.font_open_seconds * 1000.0, static_cast<double>(stats.font_bytes_mapped) / 1048576.0,
                static_cast<double>(stats.font_bytes_read) / 1048576.0);
    std::printf("evicted %llu, repacked %llu, no room %llu\n", static_cast<unsigned long long>(stats.evicted),
                static_cast<unsigned long long>(stats.compactions), static_cast<unsigned long long>(stats.overflows));
    if (!stream)
        accuracy();
    return 0;
}
