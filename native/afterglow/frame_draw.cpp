// ProsperoTube Afterglow - the frame's own drawing: page ground, rail, header, hints, snapshot.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/frame.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace afterglow
{

namespace
{

double now_ms()
{
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double, std::milli>(clock::now().time_since_epoch()).count();
}

void json_string(std::string &out, std::string_view value)
{
    out.push_back('"');
    for (const char c : value)
    {
        const unsigned char u = static_cast<unsigned char>(c);
        if (c == '"' || c == '\\')
        {
            out.push_back('\\');
            out.push_back(c);
        }
        else if (c == '\n')
            out += "\\n";
        else if (u < 0x20)
        {
            char buffer[8];
            std::snprintf(buffer, sizeof buffer, "\\u%04x", u);
            out += buffer;
        }
        else
            out.push_back(c);
    }
    out.push_back('"');
}

void json_field(std::string &out, const char *name, std::string_view value, bool comma = true)
{
    json_string(out, name);
    out.push_back(':');
    json_string(out, value);
    if (comma)
        out.push_back(',');
}

void json_number(std::string &out, const char *name, double value, bool comma = true)
{
    char buffer[48];
    std::snprintf(buffer, sizeof buffer, "\"%s\":%.6g%s", name, value, comma ? "," : "");
    out += buffer;
}

void json_focus(std::string &out, const FocusInfo &focus)
{
    out += "{";
    json_field(out, "id", focus.id);
    json_field(out, "text", focus.text);
    json_number(out, "index", focus.index);
    json_number(out, "total", focus.total);
    char buffer[96];
    std::snprintf(buffer, sizeof buffer, "\"rect\":[%.1f,%.1f,%.1f,%.1f]}", focus.rect.x, focus.rect.y, focus.rect.w,
                  focus.rect.h);
    out += buffer;
}

} // namespace

void Frame::draw_ground(gfx::DrawList &world)
{
    const Look &k = *look_;
    world.gradient_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0, k.base, k.base2);
    const float level = k.wash * ambient_level_.value;
    if (level > 0.01f)
    {
        // The wash drifts a little, so a still screen is never quite still.
        const bool reduced = options_.reduced_motion;
        const float dx = reduced ? 0.0f : std::sin(clock_ * 0.11f) * 44.0f;
        const float dy = reduced ? 0.0f : std::cos(clock_ * 0.09f) * 30.0f;
        const Rect r{-110.0f + dx, -70.0f + dy, 2140.0f, 1220.0f};
        const float t = ambient_fade_.running ? tween::smoothstep(ambient_fade_.progress()) : 1.0f;
        const auto wash = [&](const std::string &url, float amount)
        {
            if (url.empty() || amount <= 0.004f)
                return;
            const Picture picture = images_.get(url, PictureSize::ambient);
            if (picture.ready())
                world.image(picture.texture, r, gfx::kFullUv, Color{1.0f, 1.0f, 1.0f, level * amount});
        };
        wash(ambient_prev_, 1.0f - t);
        wash(ambient_now_, t);
    }
    // Weight at the top and the bottom, so the header and the hints always read.
    world.gradient_rect({0, 0, gfx::kVirtualWidth, 240}, 0, k.base.with_alpha(k.dark ? 0.55f : 0.35f), k.base.with_alpha(0.0f));
    world.gradient_rect({0, 780, gfx::kVirtualWidth, 300}, 0, k.base.with_alpha(0.0f), k.base.with_alpha(k.dark ? 0.8f : 0.5f));
}

void Frame::draw_header(G &g, const Entry &entry, float alpha)
{
    if (alpha <= 0.01f)
        return;
    const Look &k = *look_;
    g.l.push_opacity(alpha);
    std::string title = entry.screen->title();
    if (title.empty())
        title = entry.route.title;
    if (!title.empty())
        text(g, g.f.display, fit(g.f.display, title, kTypePage, 1200.0f), kLeft - 2.0f, 128.0f, kTypePage, k.ink);
    int hour = 0;
    int minute = 0;
    platform_.wall_clock(&hour, &minute);
    char clock[8];
    std::snprintf(clock, sizeof clock, "%02d:%02d", hour % 24, minute % 60);
    if (!k.dark)
        glass_panel(g, {kRight - 118.0f, 68.0f, 136.0f, 52.0f}, 26.0f); // a light page may have a dark picture up there
    text(g, g.f.display, clock, kRight, 104.0f, 28.0f, k.ink.with_alpha(0.9f), gfx::Align::right);
    g.l.pop_opacity();
}

void Frame::draw_hints(G &g, const Entry &entry, float alpha)
{
    if (rail_amount_.value > 0.5f)
    {
        const ui::Hint rail[] = {{ui::Button::cross, tr("Open")}, {ui::Button::circle, tr("Close")}};
        hints(g, rail, 2, alpha * tween::clamp01(rail_amount_.value * 2.0f - 1.0f));
        return;
    }
    const std::span<const ui::Hint> items = entry.screen->hints();
    if (!items.empty())
        hints(g, items.data(), static_cast<int>(items.size()), alpha * (1.0f - tween::clamp01(rail_amount_.value * 2.0f)));
}

void Frame::draw_rail(G &g, float arrive)
{
    const Look &k = *look_;
    const float open = tween::clamp01(rail_amount_.value);
    const float width = kRail + 272.0f * open;
    const int active = active_destination();
    const int count = static_cast<int>(destinations_.size());
    if (open > 0.01f)
        g.l.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0, kBlack.with_alpha((k.dark ? 0.5f : 0.25f) * open));
    Enter enter(g, arrive, 0.0f, -48.0f);
    if (open > 0.01f)
        g.l.shadow({width - 60.0f, 0.0f, 60.0f, gfx::kVirtualHeight}, 0.0f, 60.0f, kBlack.with_alpha(0.45f * open));
    if (g.glass != 0)
    {
        g.l.glass(g.glass, {0, 0, width, gfx::kVirtualHeight}, 0.0f, kWhite);
        g.l.rounded_rect({0, 0, width, gfx::kVirtualHeight}, 0.0f, k.panel.with_alpha(0.86f + 0.14f * open));
    }
    else
    {
        g.l.rounded_rect({0, 0, width, gfx::kVirtualHeight}, 0.0f, k.panel_solid);
    }
    g.l.rounded_rect({width - 1.5f, 0, 1.5f, gfx::kVirtualHeight}, 0.0f, k.hairline);

    mark(g, 60.0f, 86.0f, 46.0f, 1.0f, k.ink, k.accent);
    if (open > 0.01f)
    {
        g.l.push_opacity(tween::clamp01(open * 1.6f - 0.6f));
        const float w = text(g, g.f.display, "PROSPERO", 104.0f, 95.0f, 22.0f, k.ink2, gfx::Align::left, 5.0f);
        text(g, g.f.display, "TUBE", 104.0f + w + 5.0f, 95.0f, 22.0f, k.ink, gfx::Align::left, 5.0f);
        g.l.pop_opacity();
    }

    const auto y_at = [&](float index)
    {
        const int low = std::clamp(static_cast<int>(std::floor(index)), 0, count - 1);
        const int high = std::min(low + 1, count - 1);
        return tween::lerp(destinations_[static_cast<std::size_t>(low)].y, destinations_[static_cast<std::size_t>(high)].y,
                           index - static_cast<float>(low));
    };
    // The plate under the focused entry glides; the active entry keeps a mark.
    if (open > 0.01f)
    {
        const float y = y_at(rail_pos_.value);
        const Rect plate{24.0f, y - 36.0f, width - 48.0f, 72.0f};
        g.l.push_opacity(tween::clamp01(open * 1.4f));
        g.l.glow(plate, 24.0f, 20.0f, k.plate.with_alpha(0.22f));
        g.l.rounded_rect(plate, 24.0f, k.plate);
        g.l.pop_opacity();
    }
    const model::Profile profile = data_.profile();
    for (int i = 0; i < count; ++i)
    {
        const Destination &item = destinations_[static_cast<std::size_t>(i)];
        const bool is_active = i == active;
        const float focus = open > 0.01f ? tween::clamp01(1.0f - std::fabs(rail_pos_.value - static_cast<float>(i))) * tween::clamp01(open * 1.4f) : 0.0f;
        if (is_active && focus < 0.5f)
        {
            g.l.rounded_rect({24.0f, item.y - 36.0f, width - 48.0f, 72.0f}, 24.0f, k.raised);
            g.l.rounded_rect({9.0f, item.y - 15.0f, 5.0f, 30.0f}, 2.5f, k.accent);
        }
        const Color ink = gfx::mix(is_active ? k.ink : (open > 0.5f ? k.ink2 : k.ink3), k.on_plate, focus);
        const bool is_profile = item.screen == ScreenId::accounts;
        const std::string name = profile.guest || profile.name.empty() ? std::string(tr("Guest")) : profile.name;
        if (is_profile)
            avatar(g, 60.0f, item.y, 24.0f, name, profile.id, Picture{});
        else
            icon(g, item.icon, 60.0f, item.y, 30.0f, ink);
        if (open > 0.01f)
        {
            g.l.push_opacity(tween::clamp01(open * 1.6f - 0.5f));
            if (is_profile)
            {
                text(g, g.f.semibold, fit(g.f.semibold, name, 23.0f, width - 140.0f), 108.0f, item.y + 1.0f, 23.0f, ink);
                text(g, g.f.regular, fit(g.f.regular, tr(item.label), 18.0f, width - 140.0f), 108.0f, item.y + 25.0f, 18.0f,
                     gfx::mix(k.ink3, k.on_plate, focus));
            }
            else
            {
                text(g, g.f.semibold, fit(g.f.semibold, tr(item.label), kTypeBody, width - 140.0f), 108.0f, item.y + 8.5f,
                     kTypeBody, ink);
            }
            g.l.pop_opacity();
        }
    }
    if (open > 0.01f)
    {
        // Pinned channels live in the rail, as the classic main menu keeps them.
        const std::vector<model::Channel> &pinned = data_.home().pinned;
        if (!pinned.empty())
        {
            g.l.push_opacity(tween::clamp01(open * 1.6f - 0.6f));
            overline(g, tr("Pinned"), 40.0f, 732.0f, k.ink3);
            for (std::size_t i = 0; i < pinned.size() && i < 2; ++i)
            {
                const float y = 772.0f + 50.0f * static_cast<float>(i);
                avatar(g, 60.0f, y, 16.0f, pinned[i].name, pinned[i].id, images_.get(pinned[i].avatar, PictureSize::avatar));
                text(g, g.f.regular, fit(g.f.regular, pinned[i].name, 21.0f, width - 140.0f), 108.0f, y + 7.5f, 21.0f, k.ink2);
            }
            g.l.pop_opacity();
        }
    }
}

void Frame::draw_screen(const Entry &entry, DrawContext &dc, const Phase &phase)
{
    entry.screen->draw(dc, phase);
    G g = dc.g();
    const float alpha = tween::clamp01(phase.in) * (1.0f - tween::clamp01(phase.out)) * tween::clamp01(entry.screen->furniture());
    if (entry.screen->shows_rail())
    {
        draw_header(g, entry, alpha);
        draw_hints(g, entry, alpha);
    }
}

void Frame::draw(gfx::DrawList &world, gfx::DrawList &ui, std::uint32_t glass)
{
    const double started = now_ms();
    if (stack_.empty())
        return;
    draw_ground(world);
    // A dialog frosts the whole screen, so while one is up the screen is part
    // of what gets blurred; otherwise only the page is, and panels frost that.
    const bool flatten = dialog_.visible() && !change_.running;
    DrawContext dc{world, flatten ? world : ui, fonts_, *look_, images_, flatten ? 0u : glass, clock_, options_.reduced_motion};
    const Entry &top = *stack_.back();
    float rail_arrive = 1.0f;
    if (change_.running && outgoing_ != nullptr)
    {
        const float t = change_.progress();
        const Phase leaving{1.0f, std::max(t, 0.0001f), move_, carried_};
        const Phase arriving{t, 0.0f, move_, carried_};
        if (outgoing_on_top_)
        {
            draw_screen(top, dc, arriving);
            draw_screen(*outgoing_, dc, leaving);
        }
        else
        {
            draw_screen(*outgoing_, dc, leaving);
            draw_screen(top, dc, arriving);
        }
        if (!outgoing_->screen->shows_rail())
            rail_arrive = t;
        else if (!top.screen->shows_rail())
            rail_arrive = 1.0f - t;
    }
    else
    {
        draw_screen(top, dc, Phase{});
    }
    if (top.screen->shows_rail() || (change_.running && outgoing_ != nullptr && outgoing_->screen->shows_rail()))
    {
        G g = dc.g();
        const float shown = tween::clamp01(top.screen->furniture());
        if (shown > 0.01f)
        {
            g.l.push_opacity(shown);
            draw_rail(g, rail_arrive);
            g.l.pop_opacity();
        }
    }
    ui::Canvas canvas{ui, fonts_, glass, clock_};
    toasts_.draw(canvas);
    dialog_.draw(canvas);
    measure(world, ui);
    stats_.draw_ms = static_cast<float>(now_ms() - started);
}

void Frame::measure(const gfx::DrawList &world, const gfx::DrawList &ui)
{
    stats_.world_instances = world.instances().size();
    stats_.ui_instances = ui.instances().size();
    stats_.runs = world.runs().size() + ui.runs().size();
    double area = 0.0;
    for (const gfx::DrawList *list : {&world, &ui})
    {
        for (const gfx::Instance &instance : list->instances())
        {
            const float x0 = std::max(instance.rect[0], 0.0f);
            const float y0 = std::max(instance.rect[1], 0.0f);
            const float x1 = std::min(instance.rect[0] + instance.rect[2], gfx::kVirtualWidth);
            const float y1 = std::min(instance.rect[1] + instance.rect[3], gfx::kVirtualHeight);
            if (x1 > x0 && y1 > y0)
                area += static_cast<double>(x1 - x0) * static_cast<double>(y1 - y0);
        }
    }
    stats_.fill = static_cast<float>(area / (static_cast<double>(gfx::kVirtualWidth) * gfx::kVirtualHeight));
}

std::string Frame::snapshot_json() const
{
    std::string out = "{";
    json_field(out, "interface", "afterglow");
    json_field(out, "look", look_->id);
    json_field(out, "screen", stack_.empty() ? "" : screen_name(stack_.back()->screen->id()));
    out += "\"stack\":[";
    for (std::size_t i = 0; i < stack_.size(); ++i)
    {
        out += i == 0 ? "{" : ",{";
        json_field(out, "screen", screen_name(stack_[i]->screen->id()));
        json_field(out, "argument", stack_[i]->route.argument, false);
        out += "}";
    }
    out += "],";
    json_number(out, "moving", change_.running ? 1 : 0);
    json_number(out, "rail_open", rail_open_ ? 1 : 0);
    json_number(out, "attract", attract_ ? 1 : 0);
    json_number(out, "dialog", dialog_.is_open() ? 1 : 0);
    out += "\"focus\":";
    json_focus(out, focus());
    out += ",\"focusable\":[";
    if (!stack_.empty() && !rail_open_ && !dialog_.is_open())
    {
        bool first = true;
        for (const auto &item : stack_.back()->screen->focusable())
        {
            out += first ? "{" : ",{";
            first = false;
            json_field(out, "id", item.first);
            json_field(out, "text", item.second, false);
            out += "}";
        }
    }
    out += "],\"toasts\":[";
    for (std::size_t i = 0; i < toast_log_.size(); ++i)
    {
        if (i != 0)
            out.push_back(',');
        json_string(out, toast_log_[i]);
    }
    out += "],\"state\":{";
    Snapshot snapshot;
    if (!stack_.empty())
        stack_.back()->screen->snapshot(snapshot);
    for (std::size_t i = 0; i < snapshot.values.size(); ++i)
        json_field(out, snapshot.values[i].first.c_str(), snapshot.values[i].second, true);
    out += "\"lists\":{";
    for (std::size_t i = 0; i < snapshot.lists.size(); ++i)
    {
        if (i != 0)
            out.push_back(',');
        json_string(out, snapshot.lists[i].first);
        out += ":[";
        for (std::size_t k = 0; k < snapshot.lists[i].second.size(); ++k)
        {
            if (k != 0)
                out.push_back(',');
            json_string(out, snapshot.lists[i].second[k]);
        }
        out += "]";
    }
    out += "}},\"stats\":{";
    json_number(out, "frame_ms", stats_.frame_ms);
    json_number(out, "worst_frame_ms", stats_.worst_frame_ms);
    json_number(out, "update_ms", stats_.update_ms);
    json_number(out, "draw_ms", stats_.draw_ms);
    json_number(out, "instances", static_cast<double>(stats_.world_instances + stats_.ui_instances));
    json_number(out, "runs", static_cast<double>(stats_.runs));
    json_number(out, "fill", stats_.fill);
    json_number(out, "pictures_ready", stats_.pictures_ready);
    json_number(out, "pictures_pending", stats_.pictures_pending);
    json_number(out, "picture_bytes", static_cast<double>(stats_.picture_bytes));
    json_number(out, "data_pending", stats_.data_pending, false);
    out += "}}";
    return out;
}

} // namespace afterglow
