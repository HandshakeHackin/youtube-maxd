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

// How many bytes the UTF-8 character at value[at] has; 0 when what stands there is not one whole,
// well-formed character (a title cut inside a character by a fixed-size buffer).
std::size_t utf8_length(std::string_view value, std::size_t at)
{
    const unsigned char lead = static_cast<unsigned char>(value[at]);
    const std::size_t length = lead < 0x80 ? 1 : lead >= 0xc2 && lead <= 0xdf ? 2 : lead >= 0xe0 && lead <= 0xef ? 3 : lead >= 0xf0 && lead <= 0xf4 ? 4 : 0;
    if (length == 0 || at + length > value.size())
        return 0;
    for (std::size_t i = 1; i < length; ++i)
    {
        if ((static_cast<unsigned char>(value[at + i]) & 0xc0u) != 0x80u)
            return 0;
    }
    const unsigned char next = length > 1 ? static_cast<unsigned char>(value[at + 1]) : 0;
    if ((lead == 0xe0 && next < 0xa0) || (lead == 0xed && next >= 0xa0) || (lead == 0xf0 && next < 0x90) || (lead == 0xf4 && next >= 0x90))
        return 0;
    return length;
}

// The snapshot is JSON whatever the texts are: what is not whole UTF-8 becomes U+FFFD.
void json_string(std::string &out, std::string_view value)
{
    out.push_back('"');
    for (std::size_t at = 0; at < value.size();)
    {
        const char c = value[at];
        const unsigned char u = static_cast<unsigned char>(c);
        if (c == '"' || c == '\\')
        {
            out.push_back('\\');
            out.push_back(c);
            ++at;
        }
        else if (c == '\n')
        {
            out += "\\n";
            ++at;
        }
        else if (u < 0x20 || u == 0x7f)
        {
            char buffer[8];
            std::snprintf(buffer, sizeof buffer, "\\u%04x", u);
            out += buffer;
            ++at;
        }
        else if (u < 0x80)
        {
            out.push_back(c);
            ++at;
        }
        else if (const std::size_t length = utf8_length(value, at); length != 0)
        {
            out.append(value.substr(at, length));
            at += length;
        }
        else
        {
            out += "\xEF\xBF\xBD";
            ++at;
        }
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
    // ... and whatever the numbers are: one that is none (a division by no frames) is written as 0.
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "\"%s\":%.6g%s", name, std::isfinite(value) ? value : 0.0, comma ? "," : "");
    out += buffer;
}

void json_focus(std::string &out, const FocusInfo &focus)
{
    out += "{";
    json_field(out, "id", focus.id);
    json_field(out, "text", focus.text);
    json_field(out, "kind", focus.kind);
    json_number(out, "live", focus.live ? 1 : 0);
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
    const Screen::Header header = entry.screen->header();
    g.l.push_opacity(alpha);
    if (header == Screen::Header::title)
    {
        std::string title = entry.screen->title();
        if (title.empty())
            title = entry.route.title;
        if (!title.empty())
            text(g, g.f.display, fit(g.f.display, title, kTypePage, 1200.0f), kLeft - 2.0f, 128.0f, kTypePage, k.ink);
        const std::string subtitle = entry.screen->subtitle();
        if (!subtitle.empty())
            text(g, g.f.regular, fit(g.f.regular, subtitle, kTypeSub, 1200.0f), kLeft, 166.0f, kTypeSub, k.ink2);
    }
    int hour = 0;
    int minute = 0;
    platform_.wall_clock(&hour, &minute);
    char clock[8];
    std::snprintf(clock, sizeof clock, "%02d:%02d", hour % 24, minute % 60);
    // On a picture the clock stands a little inside it, on glass: the picture may be any colour there.
    const bool on_picture = header == Screen::Header::on_picture;
    const float right = on_picture ? kRight - 28.0f : kRight;
    if (!k.dark || on_picture)
        glass_panel(g, {right - 118.0f, 68.0f, 136.0f, 52.0f}, 26.0f); // a light page may have a dark picture up there
    text(g, g.f.display, clock, right, 104.0f, 28.0f, k.ink.with_alpha(0.9f), gfx::Align::right);
    g.l.pop_opacity();
}

void Frame::draw_hints(G &g, const Entry &entry, float alpha)
{
    if (sheet_.visible() || text_sheet_.visible())
        return; // a sheet of the frame's has its own row
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
    // Notifications are an account's: for one, the bell counts the rows the provider marks unread
    // (fact `unread`). The list is the one the Notifications screen shows, asked once and kept.
    int unread = 0;
    if (profile.signed_in)
    {
        const Page &notifications = data_.catalog("library:notifications");
        if (notifications.load == model::Load::ready)
        {
            for (const model::Row &row : notifications.rows)
                unread += row.has("unread") ? 1 : 0;
        }
    }
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
        if (item.screen == ScreenId::notifications && unread > 0)
        {
            // What is new since the last look, as the boards have it: a count on the bell.
            char count[8];
            std::snprintf(count, sizeof count, unread > 9 ? "9+" : "%d", unread);
            g.l.circle(77.0f, item.y - 15.0f, 12.0f, k.accent);
            text(g, g.f.semibold, count, 77.0f, item.y - 9.5f, 15.0f, k.on_accent, gfx::Align::center);
        }
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
    const float in = tween::clamp01(phase.in);
    const float out = tween::clamp01(phase.out);
    const float furniture = tween::clamp01(entry.screen->furniture());
    if (entry.screen->shows_rail())
    {
        draw_header(g, entry, in * (1.0f - out) * furniture);
        // One hint row at a time: the leaving screen's is gone by the middle of a hand-over, the
        // arriving one's comes after it. A layer of the screen's own that covers the page has its own row.
        const float turn = tween::clamp01(in * 2.0f - 1.0f) * (1.0f - tween::clamp01(out * 2.0f));
        draw_hints(g, entry, turn * furniture * (1.0f - tween::clamp01(entry.screen->over())));
    }
}

void Frame::draw_launch(G &g)
{
    if (!launch_.active)
        return;
    // The player's screen while the video is on its way: black, the video's picture dimmed on it, the
    // working mark in the middle and the title at the foot. From a card the picture becomes the whole
    // screen (corners to 0) and returns to where it came from when the player closes; from anywhere
    // else the screen fades in over the page, with the picture the card already has.
    const float t = launch_.move.running ? launch_.move.progress() : 1.0f;
    const float grown = launch_.returning ? 1.0f - tween::cubic_in_out(t) : tween::quint_out(t);
    const Rect whole{0.0f, 0.0f, gfx::kVirtualWidth, gfx::kVirtualHeight};
    const float dim = tween::lerp(1.0f, 0.4f, grown);
    const Color tint{dim, dim, dim, 1.0f};
    if (launch_.grows)
    {
        g.l.rounded_rect(whole, 0.0f, kBlack.with_alpha(tween::clamp01(grown * 1.5f)));
        picture_box(g, lerp_rect(launch_.from, whole, grown), tween::lerp(kRadiusThumb, 0.0f, grown), images_.get(launch_.picture, PictureSize::hero), tint);
    }
    else
    {
        g.l.rounded_rect(whole, 0.0f, kBlack.with_alpha(grown));
        const Picture picture = images_.get(launch_.picture, PictureSize::card);
        if (picture.ready())
            g.l.image(picture.texture, whole, picture.cover(whole.w / whole.h), tint.with_alpha(grown * (g.reduced ? 1.0f : tween::clamp01(picture.age / 0.25f))), 0.0f);
    }
    if (launch_.returning)
        return;
    const float shown = tween::clamp01(grown * 2.0f - 1.0f);
    spinner(g, gfx::kVirtualWidth * 0.5f, gfx::kVirtualHeight * 0.5f, 30.0f, kWhite.with_alpha(0.9f * shown));
    if (!launch_.title.empty())
    {
        text(g, g.f.semibold, fit(g.f.semibold, launch_.title, 34.0f, 1500.0f), 96.0f, 944.0f, 34.0f, kWhite.with_alpha(shown));
        if (!launch_.channel.empty())
            text(g, g.f.regular, fit(g.f.regular, launch_.channel, 22.0f, 1500.0f), 96.0f, 986.0f, 22.0f, kWhite.with_alpha(0.75f * shown));
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
        const Phase leaving{1.0f, std::max(t, 0.0001f), move_, carried_, carried_picture_};
        const Phase arriving{t, 0.0f, move_, carried_, carried_picture_};
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
    {
        // What covers the page: a layer of the screen's own, then the frame's sheets, then a
        // picture on its way to the player.
        top.screen->draw_over(dc, Phase{});
        G g = dc.g();
        sheet_.draw(g);
        text_sheet_.draw(g);
        draw_launch(g);
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
    json_field(out, "argument", stack_.empty() ? "" : stack_.back()->route.argument);
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
    json_number(out, "sheet", sheet_.open() ? 1 : 0);
    json_number(out, "typing", text_sheet_.open() ? 1 : 0);
    json_number(out, "launching", launch_.active ? 1 : 0);
    json_number(out, "starting", launch_.active && launch_.started ? 1 : 0); // the app is starting the video: the pad is the player's screen's
    json_field(out, "launch_title", launch_.active ? launch_.title : std::string());
    out += "\"focus\":";
    json_focus(out, focus());
    out += ",\"focusable\":[";
    if (!stack_.empty() && !rail_open_ && !dialog_.is_open() && !sheet_.open() && !text_sheet_.open())
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
    if (!stack_.empty() && rail_open_ && !dialog_.is_open())
    {
        // While the rail has the focus, its destinations are what can be focused.
        for (std::size_t i = 0; i < destinations_.size(); ++i)
        {
            out += i == 0 ? "{" : ",{";
            json_field(out, "id", std::string("rail/") + screen_name(destinations_[i].screen));
            json_field(out, "text", destinations_[i].label, false);
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
    out += "],\"toast_details\":[";
    for (std::size_t i = 0; i < toast_detail_log_.size(); ++i)
    {
        if (i != 0)
            out.push_back(',');
        json_string(out, toast_detail_log_[i]);
    }
    out += "],\"state\":{";
    Snapshot snapshot;
    if (!stack_.empty())
        stack_.back()->screen->snapshot(snapshot);
    // The frame's sheets report beside the screen they were opened from.
    if (sheet_.open())
        sheet_.snapshot(snapshot);
    if (text_sheet_.open())
        text_sheet_.snapshot(snapshot);
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
    json_number(out, "pictures_failed", stats_.pictures_failed);
    json_number(out, "frames", static_cast<double>(stats_.frames));
    json_number(out, "slow_frames", static_cast<double>(stats_.slow_frames));
    json_number(out, "output_w", stats_.output_w);
    json_number(out, "output_h", stats_.output_h);
    json_number(out, "text_engine", stats_.text_engine ? 1 : 0);
    json_number(out, "glyph_pages", stats_.glyph_pages);
    json_number(out, "glyphs", stats_.glyphs);
    json_number(out, "fonts", stats_.fonts);
    json_number(out, "missing_glyphs", stats_.missing_glyphs);
    json_number(out, "picture_bytes", static_cast<double>(stats_.picture_bytes));
    json_number(out, "data_pending", stats_.data_pending, false);
    out += "}}";
    return out;
}

} // namespace afterglow
