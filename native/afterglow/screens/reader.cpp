// ProsperoTube Afterglow - the reader: long text on one page.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A video's description, the credits, what changed in a version: one catalog
// whose rows are paragraphs. route.argument is the catalog ("description:<id>",
// "settings:about:credits", "settings:about:changelog"); a row's title is a
// heading (it may be empty), its overview the text under it.
//
// This file is the worked example of docs/afterglow/ARCHITECTURE.md: it shows
// everything a screen does and nothing else.

#include "afterglow/screen.hpp"
#include "tube_qr_page.h"

#include <algorithm>

namespace afterglow
{

namespace
{

constexpr float kColumn = 1180.0f; // the text column's width: about 75 characters of body text
constexpr float kPageTop = 196.0f;
constexpr float kPageBottom = 968.0f;
constexpr float kLine = 36.0f;
constexpr float kStep = 3.0f * kLine; // one press of the D-pad

class Reader final : public Screen
{
  public:
    ScreenId id() const override
    {
        return ScreenId::reader;
    }

    void enter(const Route &route, UpdateContext &context, bool restored) override
    {
        (void)context;
        if (restored)
            return; // back from a screen above: the text stays where it was
        route_ = route;
        paragraphs_ = static_cast<std::size_t>(-1);
        join_url_.clear();
        scroll_.snap(0.0f);
        age_ = 0.0f;
    }

    void update(const InputFrame &input, UpdateContext &context) override
    {
        age_ += context.dt;
        // Read every frame: the first call asks for the catalog, later ones return what has arrived.
        const Page &page = context.services.data.catalog(route_.argument);
        load_ = page.load;
        error_ = page.error;
        if (page.load == model::Load::ready && paragraphs_ != page.rows.size())
            lay_out(page, context.services.fonts);
        scroll_.update(context.dt, context.reduced_motion ? 60.0f : 18.0f);
        if (!context.top)
            return;

        const float limit = std::max(0.0f, height_ - (kPageBottom - kPageTop));
        float target = scroll_.target;
        if (input.nav == Direction::down)
            target += kStep;
        else if (input.nav == Direction::up)
            target -= kStep;
        else if (pressed(input, Action::jump_next))
            target += kPageBottom - kPageTop - kLine;
        else if (pressed(input, Action::jump_prev))
            target -= kPageBottom - kPageTop - kLine;
        else if (input.nav == Direction::left)
            context.nav.open_rail();
        else if (pressed(input, Action::back))
            context.nav.back();
        target = std::clamp(target, 0.0f, limit);
        if (target != scroll_.target)
        {
            scroll_.target = target;
            context.feedback.play(audio::Cue::tick);
        }
        else if ((input.nav == Direction::down || input.nav == Direction::up) && !input.nav_repeat)
        {
            context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f); // the end of the text
        }
    }

    void draw(const DrawContext &context, const Phase &phase) const override
    {
        G g = context.g();
        // A screen fades with its hand-over; the frame moves nothing for it.
        const float alpha = tween::clamp01(phase.in) * (1.0f - tween::clamp01(phase.out));
        Enter enter(g, alpha * tween::cubic_out(tween::clamp01(age_ / 0.35f)), 18.0f);
        if (load_ == model::Load::loading || load_ == model::Load::idle)
        {
            for (int i = 0; i < 6; ++i)
                g.l.rounded_rect({kLeft, kPageTop + 48.0f * static_cast<float>(i), kColumn * (i == 5 ? 0.55f : 1.0f), 22.0f}, 11.0f,
                                 g.k.raised.with_alpha(g.k.raised.a * (0.6f + 0.4f * ui::breathe(g.time + 0.12f * static_cast<float>(i), 1.6f))));
            return;
        }
        if (load_ != model::Load::ready)
        {
            icon(g, load_ == model::Load::offline ? Icon::offline : Icon::alert, kLeft + 22.0f, kPageTop + 20.0f, 44.0f, g.k.ink2);
            paragraph(g, g.f.regular, error_.empty() ? tr("This text could not be loaded.") : error_.c_str(), kLeft + 64.0f,
                      kPageTop + 30.0f, kTypeBody, kColumn - 64.0f, kLine, g.k.ink2, 4);
            return;
        }
        if (!join_url_.empty())
        {
            text(g, g.f.semibold, tr("Join from your phone"), kLeft, kPageTop, kTypeShelf, g.k.ink);
            qr_code(g, {kLeft, kPageTop + 54.0f, 416.0f, 416.0f}, join_url_, kBlack, kWhite);
            paragraph(g, g.f.regular, tr("Scan the code on the same local network. Guests suggest and vote; you approve videos on the PS5."),
                      kLeft + 468.0f, kPageTop + 88.0f, kTypeBody, 650.0f, kLine, g.k.ink2, 4);
            paragraph(g, g.f.regular, join_url_, kLeft, kPageTop + 534.0f, kTypeBody, kColumn, kLine, g.k.ink2, 3);
            return;
        }
        g.l.push_clip({kLeft - 8.0f, kPageTop - 34.0f, kColumn + 16.0f, kPageBottom - kPageTop + 44.0f});
        float y = kPageTop - scroll_.value;
        for (const Block &block : blocks_)
        {
            if (y + block.height > kPageTop - 60.0f && y < kPageBottom + 60.0f)
            {
                float line = y;
                if (!block.heading.empty())
                {
                    text(g, g.f.semibold, fit(g.f.semibold, block.heading, kTypeShelf, kColumn), kLeft, line, kTypeShelf, g.k.ink);
                    line += 44.0f;
                }
                paragraph(g, g.f.regular, block.body, kLeft, line, kTypeBody, kColumn, kLine, g.k.ink2);
            }
            y += block.height;
        }
        g.l.pop_clip();
        // How far the text has been read, where there is more than a page of it.
        const float page = kPageBottom - kPageTop;
        if (height_ > page)
        {
            const Rect track{kLeft + kColumn + 40.0f, kPageTop - 20.0f, 6.0f, page};
            g.l.rounded_rect(track, 3.0f, g.k.raised);
            const float thumb = std::max(48.0f, page * page / height_);
            g.l.rounded_rect({track.x, track.y + (page - thumb) * scroll_.value / (height_ - page), 6.0f, thumb}, 3.0f, g.k.ink3);
        }
    }

    std::string title() const override
    {
        return route_.title;
    }

    std::span<const ui::Hint> hints() const override
    {
        static ui::Hint items[3];
        items[0] = {ui::Button::dpad, tr("Scroll")};
        items[1] = {ui::Button::l2, tr("Page"), ui::Button::r2};
        items[2] = {ui::Button::circle, tr("Back")};
        return items;
    }

    FocusInfo focus() const override
    {
        FocusInfo info;
        info.id = "text";
        info.text = route_.title;
        info.rect = {kLeft, kPageTop - 34.0f, kColumn, kPageBottom - kPageTop + 34.0f};
        info.index = static_cast<int>(scroll_.target / kStep);
        info.total = static_cast<int>(std::max(0.0f, height_ - (kPageBottom - kPageTop)) / kStep) + 1;
        return info;
    }

    std::vector<std::pair<std::string, std::string>> focusable() const override
    {
        return {{"text", route_.title}};
    }

    void snapshot(Snapshot &out) const override
    {
        out.set("catalog", route_.argument);
        out.set("paragraphs", static_cast<int>(blocks_.size()));
        out.set("qr", !join_url_.empty() ? "yes" : "no");
        out.set("scroll", static_cast<int>(scroll_.target));
        out.set("load", load_ == model::Load::ready ? "ready" : load_ == model::Load::loading ? "loading" : load_ == model::Load::offline ? "offline" : "failed");
    }

  private:
    struct Block
    {
        std::string heading;
        std::string body;
        float height = 0.0f;
    };

    // Measures every paragraph once, when the text arrives: the scroll limit is known before anything is drawn.
    void lay_out(const Page &page, const ui::Fonts &fonts)
    {
        blocks_.clear();
        height_ = 0.0f;
        paragraphs_ = page.rows.size();
        join_url_.clear();
        if (route_.argument == "link:maxd-party" && !page.rows.empty() && tube_qr_link_valid(page.rows.front().overview.c_str()))
            join_url_ = page.rows.front().overview;
        for (const model::Row &row : page.rows)
        {
            Block block;
            block.heading = row.title;
            block.body = row.overview.empty() ? row.subtitle : row.overview;
            const TextBlock body = measure_block(fonts.regular, block.body, kTypeBody, kColumn, kLine);
            block.height = body.height + (block.heading.empty() ? 0.0f : 44.0f) + 28.0f;
            height_ += block.height;
            blocks_.push_back(std::move(block));
        }
    }

    Route route_;
    std::vector<Block> blocks_;
    std::size_t paragraphs_ = static_cast<std::size_t>(-1);
    model::Load load_ = model::Load::idle;
    std::string error_;
    std::string join_url_;
    float height_ = 0.0f;
    tween::Spring scroll_;
    float age_ = 0.0f;
};

} // namespace

std::unique_ptr<Screen> make_reader()
{
    return std::make_unique<Reader>();
}

} // namespace afterglow
