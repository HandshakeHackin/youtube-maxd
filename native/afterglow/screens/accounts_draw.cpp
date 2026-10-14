// ProsperoTube Afterglow - accounts: what the screen shows and reports.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/screens/accounts_screen.hpp"

#include <algorithm>
#include <cmath>

namespace afterglow
{

namespace acct
{

void Accounts::draw(const DrawContext &context, const Phase &phase) const
{
    G g = context.g();
    drawing_ = &context;
    {
        HandOver travel(g, phase);
        {
            Enter enter(g, arrive(age_, 0, 0.06f, 0.5f), -14.0f);
            text(g, g.f.regular, fit(g.f.regular, tr("Each profile keeps its own history, queue and settings"), 22.0f, 1300.0f),
                 kLeft, 166.0f, 22.0f, g.k.ink2);
        }
        if (load_ == model::Load::ready)
        {
            {
                Enter enter(g, arrive(age_, 1, 0.06f, 0.5f), 24.0f);
                ui::Canvas canvas{context.ui, context.fonts, context.glass, context.time};
                picker_.draw(canvas);
            }
            draw_actions(g);
            draw_note(g);
        }
        else if (load_ == model::Load::loading || load_ == model::Load::idle)
        {
            draw_bones(g);
        }
        else
        {
            const bool offline = load_ == model::Load::offline;
            message_page(g, offline ? Icon::offline : Icon::alert,
                         offline ? tr("No connection to the YouTube service") : tr("The profiles could not be loaded"), failure_words(),
                         tr("Try again"), 1.0f, arrive(age_, 1, 0.06f, 0.5f), offline ? g.k.theme.warning : g.k.theme.danger);
        }
    }
    drawing_ = nullptr;
}

void Accounts::draw_over(const DrawContext &context, const Phase &) const
{
    sheet_.draw(context);
}

std::string Accounts::title() const
{
    return tr("Who is watching?");
}

std::span<const ui::Hint> Accounts::hints() const
{
    static ui::Hint items[3];
    int count = 0;
    if (sheet_.is_open())
        return sheet_.hints();
    if (asking_ != Asking::nothing)
    {
        items[count++] = {ui::Button::cross, tr("Choose")};
        items[count++] = {ui::Button::circle, tr("Cancel")};
    }
    else if (work_.kind != Work::Kind::none || load_ == model::Load::loading)
    {
        items[count++] = {ui::Button::circle, tr("Back")};
    }
    else if (load_ != model::Load::ready)
    {
        items[count++] = {ui::Button::cross, tr("Try again")};
        items[count++] = {ui::Button::circle, tr("Back")};
    }
    else
    {
        const Tile *tile = tile_in_focus();
        if (zone_ == Zone::actions)
            items[count++] = {ui::Button::cross, tr("Choose")};
        else if (tile != nullptr)
            items[count++] = {ui::Button::cross, tr("Watch as")};
        else if (sign_in_ != 0)
            items[count++] = {ui::Button::cross, tr("Add an account")};
        if (tile != nullptr && tile->selected && !tile->guest)
            items[count++] = {ui::Button::triangle, tile->locked ? tr("Change the password") : tr("Set a password")};
        items[count++] = {ui::Button::circle, tr("Back")};
    }
    return std::span<const ui::Hint>(items, static_cast<std::size_t>(count));
}

FocusInfo Accounts::focus() const
{
    FocusInfo info;
    if (sheet_.is_open())
        return sheet_.focus();
    if (load_ == model::Load::loading || load_ == model::Load::idle)
    {
        info.id = "profiles";
        info.rect = kTiles;
        return info;
    }
    if (load_ != model::Load::ready)
    {
        info.id = "action/retry";
        info.text = tr("Try again");
        info.rect = fonts_ != nullptr ? message_button_rect(*fonts_, failure_words(), tr("Try again")) : Rect{};
        info.total = 1;
        return info;
    }
    if (zone_ == Zone::actions)
    {
        static constexpr const char *kIds[] = {"action/sign-in", "action/link-phone", "action/remove"};
        info.id = kIds[std::clamp(action_, 0, 2)];
        info.text = action_label(action_);
        info.rect = action_rect(action_);
        info.index = action_;
        info.total = action_count();
        return info;
    }
    const int at = picker_.focus();
    info.id = at < static_cast<int>(tiles_.size()) ? "profile/" + std::to_string(at) : std::string("profile/add");
    info.text = at < static_cast<int>(tiles_.size()) ? shown_name(tiles_[static_cast<std::size_t>(at)]) : std::string(tr("Add an account"));
    info.rect = picker_.count() > 0 ? picker_.card_rect(at) : kTiles;
    info.index = at;
    info.total = picker_.count();
    return info;
}

std::vector<std::pair<std::string, std::string>> Accounts::focusable() const
{
    std::vector<std::pair<std::string, std::string>> out;
    if (load_ != model::Load::ready || (asking_ != Asking::nothing) || sheet_.is_open())
    {
        const FocusInfo only = focus();
        out.emplace_back(only.id, only.text);
        return out;
    }
    for (std::size_t i = 0; i < tiles_.size(); ++i)
        out.emplace_back("profile/" + std::to_string(i), shown_name(tiles_[i]));
    out.emplace_back("profile/add", tr("Add an account"));
    static constexpr const char *kIds[] = {"action/sign-in", "action/link-phone", "action/remove"};
    for (int i = 0; i < action_count(); ++i)
        out.emplace_back(kIds[i], action_label(i));
    return out;
}

void Accounts::snapshot(Snapshot &out) const
{
    out.set("load", load_name(load_));
    out.set("profiles", static_cast<int>(tiles_.size()));
    out.set("zone", zone_ == Zone::tiles ? "tiles" : "actions");
    out.set("sign_in", sign_in_ == 0 ? "unavailable" : sign_in_ == 1 ? "available" : "unknown");
    out.set("casting", casting_ == 0 ? "unavailable" : casting_ == 1 ? "available" : "unknown");
    static constexpr const char *kWork[] = {"", "watch", "remove", "lock_set", "lock_change", "lock_remove"};
    out.set("work", kWork[static_cast<std::size_t>(work_.kind)]);
    out.set("question", asking_ == Asking::nothing ? "" : asking_ == Asking::remove ? "remove" : "password");
    // Which password the sheet asks for; words of the interface are not state.
    out.set("sheet", !sheet_.is_open() ? "" : work_.step == Work::Step::fresh ? "new-password" : work_.step == Work::Step::current ? "current-password" : "password");
    std::vector<std::string> names;
    std::string selected;
    for (const Tile &tile : tiles_)
    {
        names.push_back(tile.guest ? std::string("guest") : tile.name);
        if (tile.selected)
            selected = names.back();
    }
    out.set("selected", selected);
    out.list("profiles", std::move(names));
}

std::string Accounts::shown_name(const Tile &tile)
{
    return tile.guest || tile.name.empty() ? std::string(tr("Guest")) : tile.name;
}

std::string Accounts::failure_words() const
{
    if (load_ != model::Load::offline)
        return error_.empty() ? std::string(tr("The list of profiles was not answered. Try again.")) : error_;
    const std::string name = profile_.guest || profile_.name.empty() ? std::string(tr("Guest")) : profile_.name;
    return fill(tr("The profiles cannot be read while the YouTube service is not answering. You keep watching as {0}, and what is saved on this PS5 stays available."), name);
}

const char *Accounts::action_label(int index)
{
    return index == 0 ? tr("Sign in to YouTube") : index == 1 ? tr("Link a phone") : tr("Remove this account");
}

Rect Accounts::action_rect(int index) const
{
    const int count = std::max(action_count(), 1);
    float widths[3] = {300.0f, 230.0f, 330.0f};
    float total = kActionGap * static_cast<float>(count - 1);
    for (int i = 0; i < count; ++i)
    {
        if (fonts_ != nullptr)
            widths[i] = measure(fonts_->semibold, action_label(i), kTypeBody) + 96.0f;
        total += widths[i];
    }
    float x = kCentre - total * 0.5f;
    for (int i = 0; i < std::min(index, count - 1); ++i)
        x += widths[i] + kActionGap;
    return {x, kActionsTop, widths[std::clamp(index, 0, 2)], kActionHeight};
}

void Accounts::dress(const UpdateContext &context)
{
    if (!context.look)
        return;
    wear(picker_.style, *context.look, context.reduced_motion);
    // The light around the focused tile is its picture's colour, or the look's own focus.
    const int count = static_cast<int>(picker_.profiles().size());
    for (int i = 0; i < count; ++i)
    {
        Color light = context.look->focus;
        if (i < static_cast<int>(tiles_.size()))
        {
            Color top, bottom;
            avatar_tint(tiles_[static_cast<std::size_t>(i)].id, &top, &bottom);
            light = top;
        }
        if (i < static_cast<int>(tiles_.size()) && !tiles_[static_cast<std::size_t>(i)].avatar.empty())
        {
            const Picture picture = context.services.images.get(tiles_[static_cast<std::size_t>(i)].avatar, PictureSize::avatar);
            if (picture.ready())
                light = picture.glow;
        }
        picker_.profile(i).accent = Color{light.r, light.g, light.b, 1.0f};
    }
}

void Accounts::draw_face(ui::Canvas &canvas, const Rect &area, int index) const
{
    if (drawing_ == nullptr)
        return;
    G g{canvas.list, canvas.fonts, drawing_->look, canvas.glass, canvas.time, drawing_->reduced_motion};
    const float radius = area.w * 0.5f;
    if (index < 0 || index >= static_cast<int>(tiles_.size()))
    {
        // The tile that would add an account, in a build that does not sign in.
        const float reach = area.w * 0.36f;
        g.l.ring(area.cx(), area.cy(), reach, 3.0f, g.k.ink3);
        icon(g, Icon::block, area.cx(), area.cy(), reach * 1.1f, g.k.ink3);
        return;
    }
    const Tile &tile = tiles_[static_cast<std::size_t>(index)];
    const Picture picture = tile.avatar.empty() ? Picture{} : drawing_->images.get(tile.avatar, PictureSize::avatar);
    avatar(g, area.cx(), area.cy(), radius, shown_name(tile), tile.id, picture);
    const float at = radius * 0.7071f;
    if (tile.selected)
    {
        g.l.circle(area.cx() + at, area.cy() + at, 26.0f, g.k.panel_solid.with_alpha(1.0f));
        g.l.circle(area.cx() + at, area.cy() + at, 22.0f, g.k.accent);
        icon(g, Icon::check, area.cx() + at, area.cy() + at, 32.0f, g.k.on_accent);
    }
    if (tile.locked)
    {
        g.l.circle(area.cx() + at, area.cy() - at, 21.0f, g.k.panel_solid.with_alpha(1.0f));
        g.l.ring(area.cx() + at, area.cy() - at, 21.0f, 1.5f, g.k.hairline);
        icon(g, Icon::lock, area.cx() + at, area.cy() - at, 24.0f, g.k.ink);
    }
}

void Accounts::draw_actions(G &g) const
{
    const float shown = tween::clamp01(actions_shown_.value);
    if (shown <= 0.01f)
        return;
    Enter enter(g, arrive(age_, 3, 0.07f, 0.5f) * shown, 22.0f);
    if (work_.kind != Work::Kind::none && work_.age > 0.25f && !sheet_.visible())
    {
        // What was asked for is on its way.
        const std::string line = work_.kind == Work::Kind::watch    ? fill(tr("Switching to {0}"), work_.name)
                                 : work_.kind == Work::Kind::remove ? fill(tr("Removing {0}"), work_.name)
                                                                    : std::string(tr("One moment"));
        const std::string fitted = fit(g.f.semibold, line, kTypeBody, 900.0f);
        const float width = measure(g.f.semibold, fitted, kTypeBody);
        spinner(g, kCentre - width * 0.5f - 30.0f, kActionsTop + 32.0f, 16.0f, g.k.accent);
        text(g, g.f.semibold, fitted, kCentre + 12.0f, kActionsTop + 40.5f, kTypeBody, g.k.ink, gfx::Align::center);
        return;
    }
    overline(g, tr("For the profile in focus"), kCentre, 688.0f, g.k.ink3, gfx::Align::center);
    static constexpr Icon kIcons[] = {Icon::user, Icon::phone, Icon::close};
    const float shift = g.reduced ? 0.0f : ui::shake(zone_ == Zone::actions ? nudge_.value : 0.0f, g.time);
    for (int i = 0; i < action_count(); ++i)
    {
        Rect r = action_rect(i);
        r.x += action_focus_[i].value > 0.5f ? shift : 0.0f;
        const float focus = action_focus_[i].value;
        if (action_offered(i))
        {
            button(g, r, action_label(i), kIcons[i], focus);
        }
        else
        {
            // Not part of this build: it rests dimmed and takes a ring, not the plate.
            g.l.push_opacity(0.45f);
            button(g, r, action_label(i), kIcons[i], 0.0f);
            g.l.pop_opacity();
            focus_ring(g, r, r.h * 0.5f, g.k.focus, focus);
        }
    }
}

void Accounts::draw_note(G &g) const
{
    Enter enter(g, arrive(age_, 4, 0.07f, 0.5f), 22.0f);
    const bool on_phone = zone_ == Zone::actions && action_ == 1 && action_count() > 1;
    if (on_phone && casting_ == 0)
    {
        notice(g, kNotice, Icon::phone, tr("Not available in this build"),
               tr("Linking a phone is not part of this built-in YouTube service."), g.k.theme.warning);
        return;
    }
    if (sign_in_ == 0)
    {
        notice(g, kNotice, Icon::info, tr("Sign-in to YouTube arrives in a later version"),
               tr("This version browses and plays as Guest, without an account"), g.k.theme.warning);
        return;
    }
    const char *words = tiles_.empty()
                            ? tr("Nobody has a profile on this PS5 yet. Add an account to bring your subscriptions and your history.")
                            : tr("Accounts are signed in on this console with a code you approve on another device. The sign-in stays in the app's own folder on this PS5.");
    paragraph(g, g.f.regular, words, kCentre, 846.0f, 22.0f, 980.0f, 32.0f, g.k.ink2, 2, gfx::Align::center);
}

void Accounts::draw_bones(G &g) const
{
    const float width = 4.0f * 300.0f + 3.0f * 32.0f;
    for (int i = 0; i < 4; ++i)
    {
        Enter enter(g, arrive(age_, 1 + i, 0.05f, 0.45f), 24.0f);
        const Rect card{kCentre - width * 0.5f + 332.0f * static_cast<float>(i), 248.5f, 300.0f, 330.0f};
        glass_panel(g, card, kRadiusPanel, 0.7f);
        bone(g, {card.cx() - 75.0f, card.y + 24.0f, 150.0f, 150.0f}, 75.0f, i);
        bone(g, {card.cx() - 80.0f, card.y + 200.0f, 160.0f, 24.0f}, 12.0f, i + 1);
        bone(g, {card.cx() - 104.0f, card.y + 240.0f, 208.0f, 16.0f}, 8.0f, i + 2);
    }
    Enter enter(g, arrive(age_, 5, 0.05f, 0.45f), 22.0f);
    const float widths[3] = {300.0f, 230.0f, 330.0f};
    float x = kCentre - (widths[0] + widths[1] + widths[2] + 2.0f * kActionGap) * 0.5f;
    for (int i = 0; i < 3; ++i)
    {
        bone(g, {x, kActionsTop, widths[i], kActionHeight}, kActionHeight * 0.5f, i + 4);
        x += widths[i] + kActionGap;
    }
    bone(g, {kCentre - 430.0f, 830.0f, 860.0f, 16.0f}, 8.0f, 7);
    bone(g, {kCentre - 250.0f, 862.0f, 500.0f, 16.0f}, 8.0f, 8);
}

} // namespace acct

} // namespace afterglow
