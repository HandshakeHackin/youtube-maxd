// ProsperoTube Afterglow - placeholders for the screens that are not built yet.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Every factory of screen.hpp is defined here weakly. A file that implements a
// screen defines its factory normally, and the linker takes that one: nobody
// edits this file to add a screen, and the interface links at every stage.
// The placeholder is a real screen (it takes the focus, names itself, goes
// back on Circle) so that navigation and the tests of the frame work around
// screens that do not exist yet.

#include "afterglow/screen.hpp"

#define AFTERGLOW_STUB(name, id)                                                                                       \
    __attribute__((weak)) std::unique_ptr<Screen> make_##name()                                                        \
    {                                                                                                                  \
        return make_placeholder(ScreenId::id);                                                                         \
    }

namespace afterglow
{

namespace
{

class Placeholder final : public Screen
{
  public:
    explicit Placeholder(ScreenId id) : id_(id) {}

    ScreenId id() const override
    {
        return id_;
    }
    void enter(const Route &route, UpdateContext &context, bool restored) override
    {
        (void)context;
        if (!restored)
        {
            route_ = route;
            age_ = 0.0f;
        }
    }
    void update(const InputFrame &input, UpdateContext &context) override
    {
        age_ += context.dt;
        if (!context.top)
            return;
        if (pressed(input, Action::back))
            context.nav.back();
        else if (input.nav == Direction::left)
            context.nav.open_rail();
    }
    void draw(const DrawContext &context, const Phase &phase) const override
    {
        G g = context.g();
        const float alpha = tween::clamp01(phase.in) * (1.0f - tween::clamp01(phase.out));
        Enter enter(g, alpha * tween::cubic_out(tween::clamp01(age_ / 0.4f)), 18.0f);
        const Rect panel{kLeft, 240.0f, 900.0f, 280.0f};
        glass_panel(g, panel, kRadiusPanel);
        icon(g, Icon::info, panel.x + 64.0f, panel.y + 76.0f, 44.0f, g.k.ink2);
        text(g, g.f.semibold, tr("This page is not built yet"), panel.x + 112.0f, panel.y + 88.0f, kTypeTitle, g.k.ink);
        paragraph(g, g.f.regular, tr("The classic interface still has it: switch to it under Interface in the preferences."),
                  panel.x + 44.0f, panel.y + 156.0f, kTypeBody, panel.w - 88.0f, 34.0f, g.k.ink2, 3);
        if (!route_.argument.empty())
            text(g, g.f.mono, fit(g.f.mono, route_.argument, kTypeMeta, panel.w - 88.0f), panel.x + 44.0f, panel.y + 248.0f,
                 kTypeMeta, g.k.ink3);
    }
    std::string title() const override
    {
        return route_.title.empty() ? std::string(screen_name(id_)) : route_.title;
    }
    std::span<const ui::Hint> hints() const override
    {
        static ui::Hint items[1];
        items[0] = {ui::Button::circle, tr("Back")};
        return items;
    }
    FocusInfo focus() const override
    {
        FocusInfo info;
        info.id = "placeholder";
        info.text = screen_name(id_);
        info.rect = {kLeft, 240.0f, 900.0f, 280.0f};
        info.total = 1;
        return info;
    }
    std::vector<std::pair<std::string, std::string>> focusable() const override
    {
        return {{"placeholder", screen_name(id_)}};
    }
    void snapshot(Snapshot &out) const override
    {
        out.set("placeholder", 1);
        out.set("argument", route_.argument);
    }

  private:
    ScreenId id_;
    Route route_;
    float age_ = 0.0f;
};

} // namespace

std::unique_ptr<Screen> make_placeholder(ScreenId id)
{
    return std::make_unique<Placeholder>(id);
}

AFTERGLOW_STUB(opening, opening)
AFTERGLOW_STUB(home, home)
AFTERGLOW_STUB(search, search)
AFTERGLOW_STUB(results, results)
AFTERGLOW_STUB(grid, grid)
AFTERGLOW_STUB(channel, channel)
AFTERGLOW_STUB(video, video)
AFTERGLOW_STUB(library, library)
AFTERGLOW_STUB(subscriptions, subscriptions)
AFTERGLOW_STUB(notifications, notifications)
AFTERGLOW_STUB(channels, channels)
AFTERGLOW_STUB(comments, comments)
AFTERGLOW_STUB(settings, settings)
AFTERGLOW_STUB(accounts, accounts)
AFTERGLOW_STUB(sign_in, sign_in)
AFTERGLOW_STUB(link_phone, link_phone)
AFTERGLOW_STUB(reader, reader)

} // namespace afterglow
