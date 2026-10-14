// ProsperoTube Afterglow - the running platform and interface texts.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/base.hpp"
#include "afterglow/platform.hpp"

#include <algorithm>
#include <charconv>

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

std::string Platform::translate_count(const char *one, const char *other, long count)
{
    return fill(count == 1 ? one : other, std::to_string(count));
}

std::string tr_n(const char *one, const char *other, long count)
{
    if (one == nullptr || other == nullptr)
        return {};
    return g_platform != nullptr ? g_platform->translate_count(one, other, count) : fill(count == 1 ? one : other, std::to_string(count));
}

std::string tr_n(const char *one, const char *other, std::string_view count)
{
    if (count.empty() || count.front() < '0' || count.front() > '9')
        return std::string(count);
    long value = 0;
    const auto parsed = std::from_chars(count.data(), count.data() + count.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != count.data() + count.size())
        return std::string(count);
    return tr_n(one, other, value);
}

std::string dotted(std::initializer_list<std::string_view> parts)
{
    std::string out;
    for (const std::string_view part : parts)
    {
        if (part.empty())
            continue;
        if (!out.empty())
            out += " \xC2\xB7 ";
        out += part;
    }
    return out;
}

int leading_number(std::string_view text)
{
    int value = 0;
    for (const char c : text)
    {
        if (c < '0' || c > '9')
            break;
        value = std::min(value * 10 + (c - '0'), 99999999);
    }
    return value;
}

} // namespace afterglow
