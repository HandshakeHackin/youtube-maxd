// ProsperoTube Afterglow - what the account screens share: data helpers, motion, pieces.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/screens/accounts.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace afterglow::acct
{

// ---- data ------------------------------------------------------------------------

const model::Row *find_row(const Page &page, std::string_view id)
{
    for (const model::Row &row : page.rows)
    {
        if (row.id == id)
            return &row;
    }
    return nullptr;
}

const model::Row *find_prefix(const Page &page, std::string_view prefix)
{
    for (const model::Row &row : page.rows)
    {
        if (row.id.size() >= prefix.size() && row.id.compare(0, prefix.size(), prefix) == 0)
            return &row;
    }
    return nullptr;
}

const std::string &fact_of(const Page &page, std::string_view name)
{
    static const std::string kNone;
    for (const model::Row &row : page.rows)
    {
        const std::string &value = row.fact(name);
        if (!value.empty())
            return value;
    }
    return kNone;
}

std::string clock_text(float seconds)
{
    const int whole = std::max(0, static_cast<int>(std::ceil(seconds)));
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "%d:%02d", whole / 60, whole % 60);
    return buffer;
}

// ---- pieces ----------------------------------------------------------------------

void page_head(G &g, std::string_view family, std::string_view title, float amount)
{
    Enter enter(g, amount, -14.0f);
    overline(g, fit(g.f.semibold, family, kTypeOver, 620.0f), kLeft + 2.0f, 96.0f, g.k.accent);
    text(g, g.f.display, fit(g.f.display, title, kTypePage, 1440.0f), kLeft - 2.0f, 156.0f, kTypePage, g.k.ink);
}

void step_disc(G &g, float cx, float cy, int number)
{
    g.l.circle(cx, cy, 24.0f, g.k.raised);
    g.l.ring(cx, cy, 24.0f, 1.5f, g.k.hairline);
    const char digit[2] = {static_cast<char>('0' + number % 10), '\0'};
    text(g, g.f.semibold, digit, cx, cy + 9.0f, 24.0f, g.k.ink, gfx::Align::center);
}

void code_box(G &g, const Rect &r, std::string_view value, float size, float in, float radius, float tracking)
{
    const float shown = tween::clamp01(in);
    if (shown <= 0.005f)
        return;
    const float pop = tween::back_out(shown);
    const Rect box = grown(r, g.reduced ? 0.0f : (pop - 1.0f) * 0.3f);
    g.l.push_opacity(tween::clamp01(shown * 2.5f));
    glass_panel(g, box, radius, 1.1f);
    text(g, g.f.display, value, box.cx() + tracking * 0.5f, box.cy() + size * 0.365f, size, g.k.ink, gfx::Align::center, tracking);
    g.l.pop_opacity();
}

void code_bone(G &g, const Rect &r, float radius, int index)
{
    bone(g, r, radius, index);
    g.l.bordered_rect(r, radius, clear(), 1.5f, g.k.hairline);
}

void notice(G &g, const Rect &r, Icon glyph, std::string_view title, std::string_view body, Color tone)
{
    glass_panel(g, r, 24.0f);
    const float cx = r.x + 52.0f;
    const bool toned = tone.a > 0.0f;
    g.l.circle(cx, r.cy(), 26.0f, toned ? tone.with_alpha(0.2f) : g.k.raised);
    icon(g, glyph, cx, r.cy(), 28.0f, toned ? tone : g.k.ink);
    const float x = r.x + 98.0f;
    const float room = r.w - 98.0f - 30.0f;
    if (body.empty())
    {
        text(g, g.f.semibold, fit(g.f.semibold, title, kTypeBody, room), x, r.cy() + 8.5f, kTypeBody, g.k.ink);
        return;
    }
    text(g, g.f.semibold, fit(g.f.semibold, title, kTypeBody, room), x, r.cy() - 5.0f, kTypeBody, g.k.ink);
    text(g, g.f.regular, fit(g.f.regular, body, kTypeMeta, room), x, r.cy() + 24.0f, kTypeMeta, g.k.ink2);
}

Rect message_button_rect(const ui::Fonts &fonts, std::string_view body, std::string_view action)
{
    if (action.empty())
        return {};
    const std::string label(action);
    const Choice choice{label.c_str(), Icon::update};
    Notice notice;
    notice.lay_out(fonts, 1000.0f, 280.0f, body, &choice, 1);
    return notice.buttons[0];
}

Rect message_page(G &g, Icon glyph, std::string_view title, std::string_view body, std::string_view action, float focus,
                  float amount, Color tone)
{
    const std::string label(action);
    const Choice choice{label.c_str(), Icon::update};
    Notice notice;
    notice.lay_out(g.f, 1000.0f, 280.0f, body, &choice, action.empty() ? 0 : 1);
    Enter enter(g, amount, 26.0f);
    notice.draw(g, glyph, title, body, &choice, 0.0f, focus > 0.5f, tone);
    return action.empty() ? Rect{} : notice.buttons[0];
}

} // namespace afterglow::acct
