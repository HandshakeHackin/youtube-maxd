// ProsperoTube Afterglow - the running platform and interface texts.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/base.hpp"
#include "afterglow/platform.hpp"

namespace afterglow
{

namespace
{
Platform *g_platform = nullptr;
} // namespace

Platform *platform()
{
    return g_platform;
}

void set_platform(Platform *platform)
{
    g_platform = platform;
}

const char *tr(const char *english)
{
    if (english == nullptr)
        return "";
    return g_platform != nullptr ? g_platform->translate(english) : english;
}

std::string fill(std::string_view pattern, std::string_view first, std::string_view second)
{
    std::string out;
    out.reserve(pattern.size() + first.size() + second.size());
    for (std::size_t i = 0; i < pattern.size(); ++i)
    {
        if (pattern[i] == '{' && i + 2 < pattern.size() && pattern[i + 2] == '}' && (pattern[i + 1] == '0' || pattern[i + 1] == '1'))
        {
            out += pattern[i + 1] == '0' ? first : second;
            i += 2;
        }
        else
        {
            out.push_back(pattern[i]);
        }
    }
    return out;
}

} // namespace afterglow
