// ProsperoTube Afterglow - the design language: the icons.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Each icon is a handful of strokes on a 24 unit grid. Each has a name as
// well: a topic says by name which icon is its own.

#include "afterglow/design.hpp"

#include <algorithm>

namespace afterglow
{

namespace
{

float rad(float degrees)
{
    return degrees * 0.01745329252f;
}

} // namespace

void icon(G &g, Icon which, float cx, float cy, float size, Color color)
{
    const float u = size / 24.0f;
    const float w = std::max(2.0f, size * 0.085f);
    const Color none = clear();
    const auto X = [&](float x) { return cx + (x - 12.0f) * u; };
    const auto Y = [&](float y) { return cy + (y - 12.0f) * u; };
    const auto L = [&](float x1, float y1, float x2, float y2) { g.l.line(X(x1), Y(y1), X(x2), Y(y2), w, color); };
    const auto R = [&](float x, float y, float rw, float rh, float rr)
    { g.l.bordered_rect({X(x) - w * 0.5f, Y(y) - w * 0.5f, rw * u + w, rh * u + w}, rr * u + w * 0.5f, none, w, color); };
    const auto RF = [&](float x, float y, float rw, float rh, float rr) { g.l.rounded_rect({X(x), Y(y), rw * u, rh * u}, rr * u, color); };
    const auto C = [&](float x, float y, float r) { g.l.ring(X(x), Y(y), r * u + w * 0.5f, w, color); };
    const auto CF = [&](float x, float y, float r) { g.l.circle(X(x), Y(y), r * u, color); };
    const auto A = [&](float x, float y, float r, float start, float sweep)
    { g.l.arc(X(x), Y(y), r * u + w * 0.5f, w, rad(start), rad(sweep), color, true); };
    const auto TR = [&](float x, float y, float base, float height, float angle)
    { g.l.triangle({X(x) - base * u * 0.5f, Y(y) - height * u * 0.5f, base * u, height * u}, color, 0.0f, rad(angle)); };
    const auto word = [&](const char *letters)
    { g.l.text(*g.f.semibold.font, g.f.semibold.texture, letters, cx, cy + size * 0.13f, size * 0.36f, color, gfx::Align::center, 0.0f); };

    switch (which)
    {
    case Icon::count:
        break;
    case Icon::home:
        L(3.5f, 11.5f, 12, 4);
        L(12, 4, 20.5f, 11.5f);
        L(6, 10.5f, 6, 20);
        L(6, 20, 18, 20);
        L(18, 20, 18, 10.5f);
        L(10.5f, 20, 10.5f, 14.5f);
        L(10.5f, 14.5f, 13.5f, 14.5f);
        L(13.5f, 14.5f, 13.5f, 20);
        break;
    case Icon::search:
        C(10.5f, 10.5f, 6.2f);
        L(15.4f, 15.4f, 20.5f, 20.5f);
        break;
    case Icon::subs:
        L(7, 3.5f, 17, 3.5f);
        R(3.5f, 7.5f, 17, 13, 3);
        TR(12.4f, 14, 6.4f, 5.6f, 90);
        break;
    case Icon::library:
        L(5, 4.5f, 5, 19.5f);
        L(10, 4.5f, 10, 19.5f);
        L(14.5f, 5.5f, 19.3f, 19.5f);
        break;
    case Icon::bell:
        A(12, 11, 6.2f, -90, 180);
        L(5.8f, 11, 5.8f, 16);
        L(18.2f, 11, 18.2f, 16);
        L(4, 17.2f, 20, 17.2f);
        CF(12, 20.6f, 1.7f);
        break;
    case Icon::settings:
        L(4, 7, 20, 7);
        L(4, 12, 20, 12);
        L(4, 17, 20, 17);
        CF(9, 7, 2.6f);
        CF(15.5f, 12, 2.6f);
        CF(8, 17, 2.6f);
        break;
    case Icon::user:
        C(12, 8.6f, 3.9f);
        A(12, 21.5f, 7.6f, -72, 144);
        break;
    case Icon::users:
        C(9, 8.8f, 3.4f);
        A(9, 21, 6.6f, -72, 144);
        A(15.5f, 8.8f, 3.3f, -20, 190);
        A(16.5f, 21, 6.4f, 5, 62);
        break;
    case Icon::play:
        TR(12.8f, 12, 15, 13, 90);
        break;
    case Icon::pause:
        RF(6.5f, 4.5f, 4, 15, 1.4f);
        RF(13.5f, 4.5f, 4, 15, 1.4f);
        break;
    case Icon::next:
        TR(10.5f, 12, 13, 11, 90);
        RF(17.2f, 5.5f, 2.6f, 13, 1.2f);
        break;
    case Icon::prev:
        TR(13.5f, 12, 13, 11, -90);
        RF(4.2f, 5.5f, 2.6f, 13, 1.2f);
        break;
    case Icon::queue:
        L(4, 7, 15, 7);
        L(4, 12, 15, 12);
        L(4, 17, 10.5f, 17);
        TR(17.6f, 16.2f, 7, 6, 90);
        break;
    case Icon::clock:
        C(12, 12, 8.3f);
        L(12, 7.4f, 12, 12);
        L(12, 12, 15.4f, 14);
        break;
    case Icon::heart:
        CF(8.3f, 9.4f, 4.5f);
        CF(15.7f, 9.4f, 4.5f);
        g.l.rotated_rect({X(12) - 5.6f * u, Y(13.2f) - 5.6f * u, 11.2f * u, 11.2f * u}, 1.2f * u, rad(45), color);
        break;
    case Icon::share:
        CF(17.8f, 5.6f, 2.8f);
        CF(6.2f, 12, 2.8f);
        CF(17.8f, 18.4f, 2.8f);
        L(8.2f, 10.9f, 15.8f, 6.7f);
        L(8.2f, 13.1f, 15.8f, 17.3f);
        break;
    case Icon::captions:
        R(3, 5.5f, 18, 13, 3);
        word("CC");
        break;
    case Icon::quality:
        R(3, 5.5f, 18, 13, 3);
        word("HD");
        break;
    case Icon::speed:
        A(12, 14, 8.2f, -120, 240);
        L(12, 14, 16, 9);
        CF(12, 14, 1.6f);
        break;
    case Icon::audio:
        RF(3.5f, 9.5f, 4, 5, 0.8f);
        TR(8.6f, 12, 11, 6.5f, -90);
        A(12.5f, 12, 3.6f, 40, 100);
        A(12.5f, 12, 7.2f, 40, 100);
        break;
    case Icon::chat:
        R(3.5f, 4.5f, 17, 12.5f, 3.5f);
        TR(8.5f, 18.6f, 5, 3.6f, 180);
        CF(8, 10.8f, 1.2f);
        CF(12, 10.8f, 1.2f);
        CF(16, 10.8f, 1.2f);
        break;
    case Icon::block:
        C(12, 12, 8.3f);
        L(6.2f, 6.2f, 17.8f, 17.8f);
        break;
    case Icon::hide:
        C(12, 12, 8.3f);
        L(7.6f, 12, 16.4f, 12);
        break;
    case Icon::playlist_add:
        L(4, 7, 14, 7);
        L(4, 12, 14, 12);
        L(4, 17, 10, 17);
        L(18, 13, 18, 21);
        L(14, 17, 22, 17);
        break;
    case Icon::chevron_right:
        L(9.5f, 6, 15.5f, 12);
        L(15.5f, 12, 9.5f, 18);
        break;
    case Icon::chevron_left:
        L(14.5f, 6, 8.5f, 12);
        L(8.5f, 12, 14.5f, 18);
        break;
    case Icon::chevron_down:
        L(6, 9.5f, 12, 15.5f);
        L(12, 15.5f, 18, 9.5f);
        break;
    case Icon::check:
        L(5, 12.6f, 10, 17.6f);
        L(10, 17.6f, 19.4f, 7);
        break;
    case Icon::plus:
        L(12, 5, 12, 19);
        L(5, 12, 19, 12);
        break;
    case Icon::close:
        L(6, 6, 18, 18);
        L(18, 6, 6, 18);
        break;
    case Icon::info:
        C(12, 12, 8.3f);
        CF(12, 8, 1.3f);
        L(12, 11.6f, 12, 16.4f);
        break;
    case Icon::phone:
        R(7, 3, 10, 18, 2.6f);
        L(10.6f, 17.6f, 13.4f, 17.6f);
        break;
    case Icon::backup:
        L(4, 15, 4, 19.5f);
        L(4, 19.5f, 20, 19.5f);
        L(20, 19.5f, 20, 15);
        L(12, 4, 12, 14.5f);
        L(8, 10.5f, 12, 14.5f);
        L(16, 10.5f, 12, 14.5f);
        break;
    case Icon::update:
        A(12, 12, 7.8f, 50, 280);
        L(13.2f, 3.6f, 17.6f, 5.2f);
        L(17.6f, 5.2f, 15.6f, 9.4f);
        break;
    case Icon::globe:
        C(12, 12, 8.3f);
        L(3.8f, 12, 20.2f, 12);
        L(12, 3.8f, 12, 20.2f);
        L(5.6f, 7.6f, 18.4f, 7.6f);
        L(5.6f, 16.4f, 18.4f, 16.4f);
        break;
    case Icon::grid:
        R(4, 4, 6.4f, 6.4f, 1.8f);
        R(13.6f, 4, 6.4f, 6.4f, 1.8f);
        R(4, 13.6f, 6.4f, 6.4f, 1.8f);
        R(13.6f, 13.6f, 6.4f, 6.4f, 1.8f);
        break;
    case Icon::skip:
        TR(7.2f, 12, 12, 7.6f, 90);
        TR(14, 12, 12, 7.6f, 90);
        RF(18.6f, 6, 2.4f, 12, 1.1f);
        break;
    case Icon::title:
        L(4.5f, 6, 15.5f, 6);
        L(10, 6, 10, 19);
        g.l.star(X(18), Y(15.5f), 4.2f * u, color);
        break;
    case Icon::shorts:
        R(7, 3, 10, 18, 3);
        TR(12.4f, 12, 6, 5.2f, 90);
        break;
    case Icon::live:
        CF(12, 12, 2.7f);
        A(12, 12, 6.2f, 50, 80);
        A(12, 12, 6.2f, 230, 80);
        A(12, 12, 9.8f, 55, 70);
        A(12, 12, 9.8f, 235, 70);
        break;
    case Icon::music:
        CF(8, 17.6f, 3.1f);
        L(10.9f, 17.4f, 10.9f, 5);
        L(10.9f, 5, 18, 7);
        break;
    case Icon::gamepad:
        R(3, 7.5f, 18, 10, 5);
        L(6.9f, 12.5f, 10.5f, 12.5f);
        L(8.7f, 10.7f, 8.7f, 14.3f);
        CF(15, 11.4f, 1.3f);
        CF(17.3f, 13.7f, 1.3f);
        break;
    case Icon::news:
        R(4, 4, 16, 16, 2.6f);
        L(7.6f, 8.6f, 16.4f, 8.6f);
        L(7.6f, 12, 16.4f, 12);
        L(7.6f, 15.4f, 13, 15.4f);
        break;
    case Icon::sports:
        C(12, 12, 8.3f);
        A(12, 25, 10.4f, -34, 68);
        A(12, -1, 10.4f, 146, 68);
        break;
    case Icon::movie:
        R(3, 5, 18, 14, 2.6f);
        L(8, 5, 8, 19);
        L(16, 5, 16, 19);
        L(3, 12, 8, 12);
        L(16, 12, 21, 12);
        break;
    case Icon::flask:
        L(9.5f, 4, 14.5f, 4);
        L(10.2f, 4, 10.2f, 10);
        L(13.8f, 4, 13.8f, 10);
        L(10.2f, 10, 5, 19.6f);
        L(13.8f, 10, 19, 19.6f);
        L(5, 19.6f, 19, 19.6f);
        L(7.8f, 15.2f, 16.2f, 15.2f);
        break;
    case Icon::trending:
        L(3.5f, 17, 9.5f, 11);
        L(9.5f, 11, 13.5f, 14.5f);
        L(13.5f, 14.5f, 20.5f, 7);
        L(15.5f, 7, 20.5f, 7);
        L(20.5f, 7, 20.5f, 12);
        break;
    case Icon::more:
        CF(5.5f, 12, 1.9f);
        CF(12, 12, 1.9f);
        CF(18.5f, 12, 1.9f);
        break;
    case Icon::bookmark:
        L(7, 4, 17, 4);
        L(7, 4, 7, 20);
        L(17, 4, 17, 20);
        L(7, 20, 12, 15.6f);
        L(17, 20, 12, 15.6f);
        break;
    case Icon::sort:
        L(4, 7, 20, 7);
        L(6.6f, 12, 17.4f, 12);
        L(9.6f, 17, 14.4f, 17);
        break;
    case Icon::qr:
        R(4, 4, 6, 6, 1);
        R(14, 4, 6, 6, 1);
        R(4, 14, 6, 6, 1);
        RF(14, 14, 2.6f, 2.6f, 0.4f);
        RF(17.6f, 17.6f, 2.6f, 2.6f, 0.4f);
        RF(14, 18, 2.2f, 2.2f, 0.4f);
        RF(18, 14, 2.2f, 2.2f, 0.4f);
        break;
    case Icon::lock:
        R(6, 11, 12, 9, 2.2f);
        A(12, 11, 4.2f, -90, 180);
        break;
    case Icon::mic:
        R(9.4f, 3.4f, 5.2f, 10.4f, 2.6f);
        A(12, 11.2f, 6.4f, 90, 180);
        L(12, 17.8f, 12, 20.6f);
        break;
    case Icon::offline:
        A(12, 18, 5, -45, 90);
        A(12, 18, 9.6f, -45, 90);
        CF(12, 18, 1.7f);
        L(5, 4.5f, 19, 19.5f);
        break;
    case Icon::alert:
        g.l.triangle({X(3), Y(3.5f), 18 * u, 16.5f * u}, color, w, 0.0f);
        L(12, 9.6f, 12, 14);
        CF(12, 16.8f, 1.2f);
        break;
    case Icon::chapters:
        CF(5, 7, 1.5f);
        CF(5, 12, 1.5f);
        CF(5, 17, 1.5f);
        L(9, 7, 20, 7);
        L(9, 12, 20, 12);
        L(9, 17, 20, 17);
        break;
    case Icon::reply:
        L(9, 6, 4.5f, 10.5f);
        L(4.5f, 10.5f, 9, 15);
        L(4.5f, 10.5f, 14, 10.5f);
        A(14, 15.5f, 5, 0, 110);
        break;
    case Icon::pin:
        CF(12, 8, 4.2f);
        L(12, 12, 12, 20.5f);
        break;
    case Icon::moon:
        A(12, 12, 8, 20, 250);
        A(15.4f, 9, 6.4f, 215, 118);
        break;
    }
}

namespace
{
constexpr const char *kIconNames[] = {
    "home", "search", "subs", "library", "bell", "settings", "user", "users", "play", "pause", "next", "prev",
    "queue", "clock", "heart", "share", "captions", "quality", "speed", "audio", "chat", "block", "hide",
    "playlist_add", "chevron_right", "chevron_left", "chevron_down", "check", "plus", "close", "info", "phone",
    "backup", "update", "globe", "grid", "skip", "title", "shorts", "live", "music", "gamepad", "news", "sports",
    "movie", "flask", "trending", "more", "bookmark", "sort", "qr", "lock", "mic", "offline", "alert", "chapters",
    "reply", "pin", "moon"};
static_assert(sizeof kIconNames / sizeof kIconNames[0] == static_cast<std::size_t>(Icon::count));
} // namespace

const char *icon_name(Icon which)
{
    return which < Icon::count ? kIconNames[static_cast<std::size_t>(which)] : "grid";
}

Icon icon_by_name(std::string_view name)
{
    for (std::size_t i = 0; i < static_cast<std::size_t>(Icon::count); ++i)
    {
        if (name == kIconNames[i])
            return static_cast<Icon>(i);
    }
    return Icon::grid;
}

} // namespace afterglow
