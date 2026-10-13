// ProsperoTube Afterglow - the player in the host harness: what a board of the player is held to.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/player/host/board.hpp"

#include "cJSON.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace afterglow::player::host
{

namespace
{

std::string text_of(const cJSON *object, const char *name, const char *fallback = "")
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) && item->valuestring != nullptr ? item->valuestring : fallback;
}

} // namespace

std::vector<std::string> standing_faults(const FocusInfo &focus, const gfx::DrawList &world, const gfx::DrawList &ui)
{
    std::vector<std::string> faults;
    if (focus.id.empty())
        faults.emplace_back("nothing has the focus");
    if ((focus.rect.w > 0.0f || focus.rect.h > 0.0f) &&
        (focus.rect.x < -0.5f || focus.rect.y < -0.5f || focus.rect.x + focus.rect.w > gfx::kVirtualWidth + 0.5f ||
         focus.rect.y + focus.rect.h > gfx::kVirtualHeight + 0.5f))
    {
        char line[200];
        std::snprintf(line, sizeof line, "the focus '%s' is off the screen: %.0f,%.0f %.0fx%.0f", focus.id.c_str(), focus.rect.x, focus.rect.y,
                      focus.rect.w, focus.rect.h);
        faults.emplace_back(line);
    }
    int stray = 0;
    char where[96] = "";
    for (const gfx::DrawList *list : {&world, &ui})
    {
        for (const gfx::Run &run : list->runs())
        {
            if (run.mesh)
                continue;
            for (std::uint32_t i = run.first; i < run.first + run.count; ++i)
            {
                const gfx::Instance &instance = list->instances()[i];
                if (static_cast<int>(instance.params[3]) != static_cast<int>(gfx::Shape::glyph) || instance.color_top[3] < 0.05f)
                    continue;
                float x0 = instance.rect[0];
                float y0 = instance.rect[1];
                float x1 = x0 + instance.rect[2];
                float y1 = y0 + instance.rect[3];
                if (run.clipped)
                {
                    // What a clip rectangle hides is cut on purpose (a row of cards, the chat).
                    x0 = std::max(x0, run.clip.x);
                    y0 = std::max(y0, run.clip.y);
                    x1 = std::min(x1, run.clip.x + run.clip.w);
                    y1 = std::min(y1, run.clip.y + run.clip.h);
                    if (x1 <= x0 || y1 <= y0)
                        continue;
                }
                // A glyph's quad carries a few pixels of distance field around the letter.
                if (x1 < 6.0f || y1 < 6.0f || x0 > gfx::kVirtualWidth - 6.0f || y0 > gfx::kVirtualHeight - 6.0f)
                    continue; // wholly outside: a panel on its way in or out
                if (x0 < -6.0f || y0 < -6.0f || x1 > gfx::kVirtualWidth + 6.0f || y1 > gfx::kVirtualHeight + 6.0f)
                {
                    if (stray++ == 0)
                        std::snprintf(where, sizeof where, " (the first at %.0f,%.0f, %.0f x %.0f)", x0, y0, x1 - x0, y1 - y0);
                }
            }
        }
    }
    if (stray > 0)
        faults.push_back(std::to_string(stray) + " glyphs cross the edge of the screen" + where);
    return faults;
}

std::vector<std::string> expect_faults(const std::string &snapshot, const cJSON *expect, const unsigned char *pixels, int width, int height)
{
    std::vector<std::string> faults;
    cJSON *state = cJSON_ParseWithLength(snapshot.data(), snapshot.size());
    if (state == nullptr)
    {
        faults.push_back("the snapshot is not JSON: " + snapshot.substr(0, 120));
        return faults;
    }
    const cJSON *wanted = nullptr;
    cJSON_ArrayForEach(wanted, expect)
    {
        const std::string key = wanted->string != nullptr ? wanted->string : "";
        if (key == "focus" && cJSON_IsString(wanted))
        {
            const std::string got = text_of(cJSON_GetObjectItemCaseSensitive(state, "focus"), "id");
            if (got != wanted->valuestring)
                faults.push_back("focus is '" + got + "', expected '" + wanted->valuestring + "'");
        }
        else if (key == "state")
        {
            const cJSON *value = nullptr;
            cJSON_ArrayForEach(value, wanted)
            {
                const std::string got = text_of(cJSON_GetObjectItemCaseSensitive(state, "state"), value->string, "(none)");
                if (!cJSON_IsString(value) || got != value->valuestring)
                    faults.push_back(std::string("state ") + value->string + " is '" + got + "', expected '" + (cJSON_IsString(value) ? value->valuestring : "?") + "'");
            }
        }
        else if (key == "pixels")
        {
            const cJSON *point = nullptr;
            cJSON_ArrayForEach(point, wanted)
            {
                int v[5] = {0, 0, 0, 0, 0};
                for (int n = 0; n < 5; ++n)
                    v[n] = static_cast<int>(cJSON_GetArrayItem(point, n) != nullptr ? cJSON_GetArrayItem(point, n)->valuedouble : 0);
                const int px = std::clamp(v[0] * width / 1920, 0, width - 1);
                const int py = std::clamp(v[1] * height / 1080, 0, height - 1);
                const unsigned char *got = &pixels[(static_cast<std::size_t>(py) * static_cast<std::size_t>(width) + static_cast<std::size_t>(px)) * 4u];
                if (std::abs(got[0] - v[2]) > 70 || std::abs(got[1] - v[3]) > 70 || std::abs(got[2] - v[4]) > 70)
                {
                    char line[160];
                    std::snprintf(line, sizeof line, "the picture at %d,%d is %d,%d,%d, expected about %d,%d,%d", v[0], v[1], got[0], got[1], got[2], v[2], v[3], v[4]);
                    faults.emplace_back(line);
                }
            }
        }
        else if (cJSON_IsString(wanted))
        {
            const std::string got = text_of(state, key.c_str(), "(none)");
            if (got != wanted->valuestring)
                faults.push_back(key + " is '" + got + "', expected '" + wanted->valuestring + "'");
        }
        else if (cJSON_IsNumber(wanted))
        {
            const cJSON *item = cJSON_GetObjectItemCaseSensitive(state, key.c_str());
            const double got = cJSON_IsNumber(item) ? item->valuedouble : -1.0;
            if (got != wanted->valuedouble)
                faults.push_back(key + " is " + std::to_string(got) + ", expected " + std::to_string(wanted->valuedouble));
        }
    }
    cJSON_Delete(state);
    return faults;
}

void apply(const cJSON *set, PlayerView *view)
{
    if (!cJSON_IsObject(set))
        return;
    char *text = cJSON_PrintUnformatted(set);
    if (text != nullptr)
    {
        read_view(text, view, nullptr);
        cJSON_free(text);
    }
}

} // namespace afterglow::player::host
