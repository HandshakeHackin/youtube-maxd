// ProsperoTube Afterglow - screen names and the screen of an id.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/screen.hpp"

namespace afterglow
{

namespace
{

constexpr const char *kScreenNames[] = {
    "opening", "home", "search", "results", "grid", "channel", "video", "library", "subscriptions",
    "notifications", "channels", "comments", "settings", "accounts", "sign_in", "link_phone", "reader"};
static_assert(sizeof kScreenNames / sizeof kScreenNames[0] == kScreenCount);

} // namespace

const char *screen_name(ScreenId id)
{
    return id < ScreenId::count ? kScreenNames[static_cast<std::size_t>(id)] : "";
}

bool screen_from_name(std::string_view name, ScreenId *id)
{
    for (std::size_t i = 0; i < kScreenCount; ++i)
    {
        if (name == kScreenNames[i])
        {
            *id = static_cast<ScreenId>(i);
            return true;
        }
    }
    return false;
}

std::unique_ptr<Screen> make_screen(ScreenId id)
{
    switch (id)
    {
    case ScreenId::opening:
        return make_opening();
    case ScreenId::home:
        return make_home();
    case ScreenId::search:
        return make_search();
    case ScreenId::results:
        return make_results();
    case ScreenId::grid:
        return make_grid();
    case ScreenId::channel:
        return make_channel();
    case ScreenId::video:
        return make_video();
    case ScreenId::library:
        return make_library();
    case ScreenId::subscriptions:
        return make_subscriptions();
    case ScreenId::notifications:
        return make_notifications();
    case ScreenId::channels:
        return make_channels();
    case ScreenId::comments:
        return make_comments();
    case ScreenId::settings:
        return make_settings();
    case ScreenId::accounts:
        return make_accounts();
    case ScreenId::sign_in:
        return make_sign_in();
    case ScreenId::link_phone:
        return make_link_phone();
    case ScreenId::reader:
        return make_reader();
    case ScreenId::count:
        break;
    }
    return make_placeholder(ScreenId::home);
}

} // namespace afterglow
