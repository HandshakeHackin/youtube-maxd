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

std::string with(const char *pattern, std::string_view first, std::string_view second)
{
    std::string out(pattern != nullptr ? pattern : "");
    const auto put = [&](const char *mark, std::string_view value)
    {
        const std::size_t at = out.find(mark);
        if (at != std::string::npos)
            out.replace(at, 3, value);
    };
    // The later mark first: a value may itself hold "{0}".
    put("{1}", second);
    put("{0}", first);
    return out;
}

const char *load_name(model::Load load)
{
    switch (load)
    {
    case model::Load::ready:
        return "ready";
    case model::Load::failed:
        return "failed";
    case model::Load::offline:
        return "offline";
    default:
        return "loading";
    }
}

std::string clock_text(float seconds)
{
    const int whole = std::max(0, static_cast<int>(std::ceil(seconds)));
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "%d:%02d", whole / 60, whole % 60);
    return buffer;
}

// ---- motion ----------------------------------------------------------------------

Travel::Travel(G &g, const Phase &phase) : l_(g.l)
{
    const float in = tween::clamp01(phase.in);
    const float out = tween::clamp01(phase.out);
    // What leaves is gone early; what arrives waits a moment for it.
    alpha_ = tween::clamp01(in * 1.4f - 0.2f) * (1.0f - tween::clamp01(out * 1.7f));
    if (in >= 1.0f && out <= 0.0f)
        alpha_ = 1.0f;
    float dx = 0.0f;
    float scale = 1.0f;
    if (!g.reduced)
    {
        const float arrives = 1.0f - tween::quint_out(in);
        const float leaves = tween::cubic_in(out);
        if (phase.move == Move::push)
            dx = 110.0f * arrives - 90.0f * leaves;
        else if (phase.move == Move::pop)
            dx = -110.0f * arrives + 90.0f * leaves;
        else if (phase.move == Move::fade)
            scale = 1.0f - 0.025f * leaves;
    }
    l_.push_opacity(alpha_);
    l_.push_transform(scale, gfx::kVirtualWidth * 0.5f, gfx::kVirtualHeight * 0.5f, dx, 0.0f);
}

Travel::~Travel()
{
    l_.pop_transform();
    l_.pop_opacity();
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

void spinner(G &g, float cx, float cy, float radius, Color color)
{
    const float thickness = std::max(3.0f, radius * 0.22f);
    g.l.ring(cx, cy, radius, thickness, g.k.raised);
    if (g.reduced)
    {
        // Still, and breathing: something is being waited for all the same.
        g.l.arc(cx, cy, radius, thickness, -1.5708f, 4.2f, color.with_alpha(0.55f + 0.45f * ui::breathe(g.time, 2.4f)), true);
        return;
    }
    const float turn = g.time * 5.2f;
    const float sweep = 2.2f + 1.5f * ui::breathe(g.time, 1.9f);
    g.l.arc(cx, cy, radius, thickness, turn, sweep, color, true);
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
    // Under the words, however many lines they take.
    const int lines = std::max(1, measure_block(fonts.regular, body, kTypeBody, 820.0f, 34.0f, 3).lines);
    const float width = measure(fonts.semibold, action, kTypeBody) + 96.0f;
    return {1000.0f - width * 0.5f, 502.0f + 34.0f * static_cast<float>(lines - 1) + 44.0f, width, 64.0f};
}

Rect message_page(G &g, Icon glyph, std::string_view title, std::string_view body, std::string_view action, float focus,
                  float amount, Color tone)
{
    const bool toned = tone.a > 0.0f;
    {
        Enter enter(g, amount, 26.0f);
        g.l.circle(1000.0f, 336.0f, 56.0f, toned ? tone.with_alpha(0.2f) : g.k.raised);
        icon(g, glyph, 1000.0f, 336.0f, 58.0f, toned ? tone : g.k.ink);
        text(g, g.f.display, fit(g.f.display, title, 36.0f, 1100.0f), 1000.0f, 452.0f, 36.0f, g.k.ink, gfx::Align::center);
        paragraph(g, g.f.regular, body, 1000.0f, 502.0f, kTypeBody, 820.0f, 34.0f, g.k.ink2, 3, gfx::Align::center);
    }
    const Rect button_rect = message_button_rect(g.f, body, action);
    if (!action.empty())
    {
        Enter enter(g, tween::clamp01(amount * 1.4f - 0.4f), 18.0f);
        button(g, button_rect, action, Icon::update, focus, true);
    }
    return button_rect;
}

// ---- the question with a safe answer ------------------------------------------------

void Question::ask(ui::StatusKind icon_kind, std::string title, std::string body, std::vector<std::string> answers, int danger,
                   int safe, bool stacked, Feedback &feedback)
{
    ui::DialogContent content;
    content.icon = icon_kind;
    content.title = std::move(title);
    content.body = std::move(body);
    for (std::size_t i = 0; i < answers.size(); ++i)
    {
        const bool warns = static_cast<int>(i) == danger;
        const bool leads = danger < 0 && i == 0;
        content.buttons.push_back({std::move(answers[i]), warns || leads ? ui::ButtonKind::primary : ui::ButtonKind::secondary, warns});
    }
    content.default_button = safe;
    dialog_.style.width = stacked ? 900.0f : 760.0f; // stacked answers are sentences: they get the room to be read whole
    dialog_.style.centered = true;
    dialog_.style.frosted = true;
    dialog_.style.buttons = stacked ? ui::DialogButtons::stacked : ui::DialogButtons::row;
    dialog_.set_bounds({0.0f, 0.0f, gfx::kVirtualWidth, gfx::kVirtualHeight});
    dialog_.open(std::move(content), feedback);
}

int Question::update(const InputFrame &input, float dt, Feedback &feedback, bool top)
{
    dialog_.update(dt);
    if (!dialog_.is_open() || !top)
        return -2;
    const ui::Event event = dialog_.handle(input, feedback);
    if (event == ui::Event::activated)
        return dialog_.choice();
    if (event == ui::Event::cancelled)
        return -1;
    return -2;
}

void Question::draw(const DrawContext &context) const
{
    if (!dialog_.visible())
        return;
    // REQUEST: UpdateContext::look, so a kit component is dressed where it is updated (requests/F.md, 3).
    if (worn_ != &context.look || dialog_.style.reduced_motion != context.reduced_motion)
    {
        worn_ = &context.look;
        dialog_.style.theme = context.look.theme;
        dialog_.style.sounds.set = context.look.theme.sounds;
        dialog_.style.reduced_motion = context.reduced_motion;
    }
    ui::Canvas canvas{context.ui, context.fonts, context.glass, context.time};
    dialog_.draw(canvas);
}

std::string Question::answer(int index) const
{
    const auto &buttons = dialog_.content().buttons;
    return index >= 0 && index < static_cast<int>(buttons.size()) ? buttons[static_cast<std::size_t>(index)].label : std::string();
}

} // namespace afterglow::acct
