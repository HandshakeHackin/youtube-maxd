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

} // namespace afterglow
