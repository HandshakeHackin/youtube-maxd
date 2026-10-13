// ProsperoTube Afterglow - Opening.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/screen.hpp"

#include <cmath>

namespace afterglow
{

namespace
{

// The opening moment of a cold start: the mark is drawn, the wordmark sets, and
// the page leaves for Home as soon as Home has something to show.
class Opening final : public Screen
{
  public:
    ScreenId id() const override
    {
        return ScreenId::opening;
    }
    void enter(const Route &route, UpdateContext &context, bool restored) override
    {
        (void)route;
        (void)context;
        if (!restored)
            age_ = 0.0f;
        left_ = false;
    }
    void update(const InputFrame &input, UpdateContext &context) override
    {
        age_ += context.dt;
        // Home's data is asked for while the mark is drawn.
        const model::Home &home = context.services.data.home();
        if (!context.top || left_)
            return;
        const bool any = input.pressed != 0 || input.nav != Direction::none;
        const bool ready = home.load != model::Load::loading;
        if ((age_ > 1.7f && (ready || any)) || age_ > 3.3f)
        {
            left_ = true;
            context.nav.root(Route{ScreenId::home, {}, {}});
        }
    }
    void draw(const DrawContext &context, const Phase &phase) const override
    {
        G g = context.g();
        const Look &k = g.k;
        const float t = age_;
        // Leaving, the mark travels to its place at the head of the rail.
        const float go_home = phase.out > 0.0f ? tween::cubic_in_out(tween::clamp01(phase.out)) : 0.0f;
        const float cx = tween::lerp(960.0f, 60.0f, go_home);
        const float cy = tween::lerp(452.0f, 86.0f, go_home);
        const float size = tween::lerp(208.0f, 46.0f, go_home);
        const float rest = 1.0f - tween::clamp01(phase.out * 2.2f);

        // A room that warms up: light gathers behind the mark before it is drawn.
        const float warm = tween::smoothstep(t / 1.4f) * rest;
        context.world.glow({660.0f, 250.0f, 600.0f, 400.0f}, 200.0f, 420.0f, k.accent.with_alpha(0.16f * warm));
        context.world.glow({860.0f, 380.0f, 200.0f, 150.0f}, 80.0f, 260.0f, Color::rgb(0x7a5cff, 0.12f * warm));

        mark(g, cx, cy, size, tween::clamp01((t - 0.15f) / 1.15f), k.ink, k.accent);

        // The wordmark sets letter by letter, from the middle outward.
        const char *word = "PROSPEROTUBE";
        constexpr int kLetters = 12;
        constexpr float kSize = 50.0f;
        constexpr float kTracking = 15.0f;
        float widths[kLetters];
        float total = 0.0f;
        for (int i = 0; i < kLetters; ++i)
        {
            const char letter[2] = {word[i], '\0'};
            widths[i] = measure(g.f.display, letter, kSize);
            total += widths[i] + (i + 1 < kLetters ? kTracking : 0.0f);
        }
        float x = 960.0f - total * 0.5f;
        for (int i = 0; i < kLetters; ++i)
        {
            const char letter[2] = {word[i], '\0'};
            const float order = std::fabs(static_cast<float>(i) - 5.5f);
            const float in = tween::cubic_out((t - 1.0f - order * 0.05f) / 0.5f) * rest;
            if (in > 0.01f)
            {
                const float rise = g.reduced ? 0.0f : 16.0f * (1.0f - in);
                text(g, g.f.display, letter, x, 668.0f + rise, kSize, (i < 8 ? k.ink.with_alpha(0.72f) : k.ink).with_alpha(in));
            }
            x += widths[i] + kTracking;
        }
        // What the app is really doing meanwhile: its service is starting.
        const float line = tween::cubic_in_out(tween::clamp01((t - 1.5f) / 1.5f)) * rest;
        if (line > 0.01f)
        {
            g.l.rounded_rect({960.0f - 150.0f, 728.0f, 300.0f, 3.0f}, 1.5f, k.ink.with_alpha(0.12f * rest));
            g.l.rounded_rect({960.0f - 150.0f, 728.0f, 300.0f * line, 3.0f}, 1.5f, k.accent.with_alpha(rest));
        }
    }
    bool shows_rail() const override
    {
        return false;
    }
    FocusInfo focus() const override
    {
        FocusInfo info;
        info.id = "opening";
        info.text = "ProsperoTube";
        info.rect = {856.0f, 348.0f, 208.0f, 208.0f};
        info.total = 1;
        return info;
    }
    void snapshot(Snapshot &out) const override
    {
        out.set("left", left_ ? 1 : 0);
    }

  private:
    float age_ = 0.0f;
    bool left_ = false;
};

} // namespace

std::unique_ptr<Screen> make_opening()
{
    return std::make_unique<Opening>();
}

} // namespace afterglow
