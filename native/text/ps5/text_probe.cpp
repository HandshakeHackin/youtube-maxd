// ProsperoTube - Text engine: link proof. One call into every part of the engine: the console's
// font folder, measuring, laying out left-to-right, right-to-left and unspaced text, wrapping,
// cutting, the atlas updates and the statistics. Built for the PS5 (never run there by this
// repository's tools) and for the host, where scripts/test_text.sh runs it.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tube_text.hpp"

#include <cstdio>
#include <string>

// 0 on success; `report` then says what was found and drawn. Negative: the step that failed.
extern "C" int tube_text_probe(char *report, unsigned long capacity)
{
    using namespace tube::text;
    Config config;
    config.font_dirs = system_font_folders();
    config.language = "en-US";
    std::string error;
    std::unique_ptr<Context> context = Context::create(config, &error);
    if (!context)
    {
        std::snprintf(report, capacity, "create: %s", error.c_str());
        return -1;
    }
    static const char *const kTexts[] = {
        "Afterglow \xc3\xa9\xc3\xbc\xc3\x9f \xd0\x96 \xce\xa9",                             // Latin with marks, Cyrillic, Greek
        "\xe6\x9d\xb1\xe4\xba\xac\xe3\x81\xae\xe5\xa4\x9c \xec\x84\x9c\xec\x9a\xb8",         // Japanese, Korean
        "\xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7 abc 123 \xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d", // Arabic, Latin, Hebrew
        "\xe0\xb8\xaa\xe0\xb8\xa7\xe0\xb8\xb1\xe0\xb8\xaa\xe0\xb8\x94\xe0\xb8\xb5 \xe0\xa4\xa8\xe0\xa4\xae\xe0\xa4\xb8\xe0\xa5\x8d\xe0\xa4\xa4\xe0\xa5\x87", // Thai, Devanagari
        "1:23:45 \xf0\x9f\x94\xa5 \xe2\x9d\xa4\xef\xb8\x8f",                               // digits, emoji
    };
    Style style;
    style.size = 32.0f;
    style.tabular_digits = true;
    Constraints constraints;
    constraints.max_width = 240.0f;
    constraints.max_lines = 2;
    constraints.ellipsis = Ellipsis::end;
    Layout layout;
    unsigned quads = 0;
    unsigned missing = 0;
    unsigned updates = 0;
    float width = 0.0f;
    context->prewarm(kTexts[0], style);
    for (int frame = 0; frame < 4; ++frame)
    {
        context->advance_frame();
        quads = 0;
        missing = 0;
        for (const char *text : kTexts)
        {
            width += context->measure(text, style, constraints).width;
            context->layout(text, style, constraints, &layout);
            quads += static_cast<unsigned>(layout.quads.size());
            missing += layout.missing;
        }
        for (const AtlasUpdate &update : context->atlas_updates())
            updates += update.pixels != nullptr ? 1u : 0u;
    }
    if (!(width > 0.0f) || context->page_size() <= 0)
        return -2;
    if (quads != 0 && context->page_pixels(0) == nullptr)
        return -3;
    const Stats stats = context->stats();
    std::snprintf(report, capacity, "quads %u, clusters without a glyph %u, uploads %u, glyphs %u in %u page(s), faces %u read; %s",
                  quads, missing, updates, stats.glyphs, stats.pages, stats.faces_open, context->describe().c_str());
    return context->can_draw("abc") || missing != 0 ? 0 : -4;
}

#if !defined(TUBE_TEXT_PROBE_NO_MAIN)
int main()
{
    char report[2048];
    const int result = tube_text_probe(report, sizeof(report));
    std::printf("text probe: %d (%s)\n", result, result == 0 || result == -1 ? report : "");
    return result == 0 ? 0 : 1;
}
#endif
