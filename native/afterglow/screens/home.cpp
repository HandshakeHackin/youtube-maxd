// ProsperoTube Afterglow - Home.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The hero follows the focus: the focused video's picture dissolves into the
// page, its title and its play line stand on the left. Under it the shelves:
// signed in, Continue watching, Recommended and From your subscriptions; as
// Guest, the topics, Trending and an invitation to sign in. Left of the first
// card is the rail. Alone for a while, the page shows off what is on.

#include "afterglow/data_feed.hpp"
#include "afterglow/screen.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace afterglow
{

namespace
{

constexpr float kShelfTop = 588.0f;      // top of the first shelf's cards under the hero
constexpr float kShelfTopTopics = 508.0f; // ... when the first shelf is the topic tiles
constexpr float kTileW = 248.0f;
constexpr float kTileH = 136.0f;
constexpr float kTilePitch = 272.0f;
constexpr float kTopicPitch = 232.0f; // from the topic tiles to the next shelf
constexpr Rect kHeroImage{696.0f, 0.0f, 1224.0f, 688.0f};
constexpr float kAttractStep = 6.0f; // seconds a video stays up in attract mode
constexpr int kBones = 5;            // cards a loading shelf shows

class Home final : public Screen
{
  public:
    ScreenId id() const override
    {
        return ScreenId::home;
    }

    void enter(const Route &route, UpdateContext &context, bool restored) override
    {
        (void)route;
        if (restored)
        {
            // Back from a video or a page: what was watched may have moved shelves.
            context.services.data.forget("history");
            return;
        }
        age_ = 0.0f;
        row_ = 0;
        wanted_.clear();
        shown_.clear();
        row_pos_.snap(0.0f);
        ring_ready_ = false;
        attract_ = false;
        attract_amount_.snap(0.0f);
    }

    void update(const InputFrame &input, UpdateContext &context) override
    {
        age_ += context.dt;
        const bool reduced = context.reduced_motion;
        collect(context.services.data.home());
        const int count = static_cast<int>(rows_.size());
        row_ = std::clamp(row_, 0, std::max(0, count - 1));

        if (attract_ && count > 0)
        {
            attract_step_ += context.dt;
            Row &row = rows_[static_cast<std::size_t>(row_)];
            if (attract_step_ > kAttractStep && row.count() > 0)
            {
                attract_step_ = 0.0f;
                state(row).col = (state(row).col + 1) % row.count();
            }
        }
        else if (context.top && count > 0)
        {
            handle(input, context);
        }
        else if (context.top)
        {
            // Nothing to focus: the page is loading, empty or offline.
            if (pressed(input, Action::confirm) && load_ != model::Load::loading)
            {
                context.services.data.refresh();
                context.feedback.play(audio::Cue::select);
            }
            else if (pressed(input, Action::west))
                context.nav.push(Route{ScreenId::search, {}, {}});
            else if (input.nav == Direction::left)
                context.nav.open_rail();
            else if (pressed(input, Action::back))
                context.nav.back();
        }

        follow_later(context);

        // ---- motion ----
        row_pos_.target = static_cast<float>(row_);
        row_pos_.update(context.dt, reduced ? 60.0f : 11.0f);
        for (Row &row : rows_)
        {
            RowState &s = state(row);
            const float pitch = row.topics() ? kTilePitch : kCardPitch;
            const float width = row.topics() ? kTileW : kCardW;
            const float start = static_cast<float>(s.col) * pitch;
            s.scroll.reveal(start, start + width, kRight - kLeft, pitch * 0.5f);
            s.scroll.update(context.dt, reduced ? 60.0f : 12.0f);
        }
        if (count > 0)
        {
            const Rect target = card_rect(row_, state(rows_[static_cast<std::size_t>(row_)]).col);
            if (!ring_ready_)
            {
                ring_.snap(target);
                ring_ready_ = true;
            }
            ring_.target(target);
            ring_.update(context.dt, reduced ? 60.0f : 20.0f);
        }
        nudge_.update(context.dt, 9.0f);
        attract_amount_.target = attract_ ? 1.0f : 0.0f;
        attract_amount_.update(context.dt, reduced ? 40.0f : 3.2f);

        // The hero and the page's colour follow the focus.
        const model::Video *focused = hero_video();
        const std::string now = focused != nullptr ? focused->id : std::string();
        if (now != hero_now_.id)
        {
            hero_prev_ = hero_now_;
            hero_now_ = focused != nullptr ? *focused : model::Video{};
            hero_label_prev_ = hero_label_;
            hero_fade_.start(reduced ? 0.12f : 0.5f);
            context.nav.ambient(hero_now_.thumb);
        }
        hero_label_ = hero_shelf_title();
        hero_fade_.update(context.dt);
    }

    void draw(const DrawContext &context, const Phase &phase) const override
    {
        G w = context.g_world();
        G g = context.g();
        const Look &k = g.k;
        const float fade = tween::clamp01(phase.in) * (1.0f - tween::clamp01(phase.out));
        const float attract = attract_amount_.value;

        // ---- the page: the picture in focus, dissolving into the wash ----
        const float backdrop_in = arrive(0, 0.0f, 0.9f) * fade;
        const float cross = hero_fade_.running ? hero_fade_.progress() : 1.0f;
        const auto backdrop = [&](const model::Video &video, float alpha)
        {
            if (alpha <= 0.004f || video.thumb.empty())
                return;
            const Picture picture = context.images.get(video.thumb, PictureSize::hero);
            // Alone with the screen, the picture takes all of it and drifts.
            const float a = tween::smoothstep(attract);
            const Rect r = lerp_rect(kHeroImage, {0.0f, 0.0f, gfx::kVirtualWidth, gfx::kVirtualHeight}, a);
            const float drift = g.reduced ? 0.0f : attract_step_ / kAttractStep;
            const float arrived = g.reduced ? 1.0f : tween::clamp01(picture.age / 0.35f);
            w.l.push_transform(1.0f + 0.05f * a * drift, 960.0f, 540.0f, 0.0f, 0.0f);
            faded_picture(w.l, picture, r, alpha * arrived * (k.dark ? 0.94f : 1.0f), tween::lerp(480.0f, 0.0f, a),
                          std::floor(tween::lerp(348.0f, 1080.0f, a) / 12.0f) * 12.0f);
            w.l.pop_transform();
        };
        if (cross < 1.0f)
            backdrop(hero_prev_, backdrop_in * (1.0f - tween::smoothstep(cross * 1.4f)));
        backdrop(hero_now_, backdrop_in * tween::smoothstep(cross));
        // Under the title the page is evened out, so text never depends on the picture.
        w.l.shadow({kRail - 160.0f, 190.0f, 760.0f, 250.0f}, 120.0f, 260.0f, k.base.with_alpha((k.dark ? 0.74f : 0.34f) * (1.0f - attract)));
        if (attract > 0.01f)
        {
            w.l.gradient_rect({0.0f, 600.0f, gfx::kVirtualWidth, 480.0f}, 0.0f, kBlack.with_alpha(0.0f), kBlack.with_alpha(0.8f * attract));
            w.l.gradient_rect({0.0f, 0.0f, gfx::kVirtualWidth, 220.0f}, 0.0f, kBlack.with_alpha(0.5f * attract), kBlack.with_alpha(0.0f));
        }

        g.l.push_opacity(fade * furniture());

        // ---- the wordmark, where other pages have their title ----
        {
            Enter enter(g, arrive(1, 0.05f, 0.5f), -14.0f);
            const float ww = text(g, g.f.display, "PROSPERO", kLeft, 102.0f, 23.0f, k.ink2, gfx::Align::left, 6.5f);
            text(g, g.f.display, "TUBE", kLeft + ww + 6.5f, 102.0f, 23.0f, k.ink, gfx::Align::left, 6.5f);
        }

        if (rows_.empty())
        {
            draw_state(g);
            g.l.pop_opacity();
            return;
        }

        // ---- the hero: the old title leaves quickly, the new one lands a beat later ----
        {
            const float in = arrive(2, 0.07f, 0.6f);
            g.l.push_opacity(in);
            if (hero_fade_.running)
            {
                draw_hero(context, g, hero_prev_, hero_label_prev_, 1.0f - tween::smoothstep(cross * 2.4f),
                          g.reduced ? 0.0f : -30.0f * tween::cubic_in(tween::clamp01(cross * 2.4f)));
                const float land = tween::clamp01((cross - 0.28f) / 0.72f);
                draw_hero(context, g, hero_now_, hero_label_, tween::smoothstep(land), g.reduced ? 0.0f : 38.0f * (1.0f - tween::quint_out(land)));
            }
            else
            {
                draw_hero(context, g, hero_now_, hero_label_, 1.0f, g.reduced ? 0.0f : 28.0f * (1.0f - in));
            }
            g.l.pop_opacity();
        }

        // ---- Guest: an invitation, not a wall ----
        if (invite_)
        {
            const float in = arrive(4, 0.07f, 0.6f);
            Enter enter(g, in, 0.0f, 40.0f);
            const Rect card{1304.0f, 150.0f, 520.0f, 290.0f};
            lift(g, card, kRadiusPanel, 0.8f);
            glass_panel(g, card, kRadiusPanel);
            overline(g, tr("You are watching as Guest"), card.x + 36.0f, card.y + 52.0f, k.accent);
            text(g, g.f.display, fit(g.f.display, tr("Make it yours"), 38.0f, card.w - 68.0f), card.x + 34.0f, card.y + 106.0f, 38.0f, k.ink);
            paragraph(g, g.f.regular,
                      tr("Sign in to YouTube for your subscriptions, your history and recommendations that know you. Everything here works without it."),
                      card.x + 36.0f, card.y + 148.0f, 21.0f, card.w - 72.0f, 30.0f, k.ink2, 3);
            const ui::GlyphStyle glyphs = k.dark ? ui::GlyphStyle::dark() : ui::GlyphStyle::light();
            ui::draw_button(g.l, g.f, glyphs, ui::Button::options, card.x + 36.0f, card.y + 252.0f, 30.0f);
            text(g, g.f.semibold, fit(g.f.semibold, tr("Sign in with a code"), 22.0f, card.w - 116.0f), card.x + 80.0f, card.y + 260.0f, 22.0f, k.ink);
        }

        // ---- the shelves ----
        Rect ring = ring_.value();
        ring.x += ui::shake(nudge_.value, g.time, 14.0f, 8.0f) * nudge_dir_;
        const float ring_cx = ring_.value().cx();
        for (int r = 0; r < static_cast<int>(rows_.size()); ++r)
        {
            const float distance = static_cast<float>(r) - row_pos_.value;
            // Shelves above the focused one are gone; the one below waits, dimmed.
            const float visible = tween::clamp01(1.0f + distance * 2.6f) * (distance > 0.0f ? 1.0f - 0.4f * tween::clamp01(distance) : 1.0f);
            if (visible <= 0.01f || distance > 2.2f)
                continue;
            draw_shelf(context, g, rows_[static_cast<std::size_t>(r)], card_rect(r, 0).y, visible, r == row_, ring_cx, 4 + r * 3);
        }
        {
            const Row &row = rows_[static_cast<std::size_t>(row_)];
            if (row.count() > 0)
            {
                const float in = arrive(5, 0.06f, 0.5f);
                const Picture picture = hero_now_.thumb.empty() ? Picture{} : context.images.get(hero_now_.thumb, PictureSize::card);
                focus_ring(g, ring, row.topics() ? 20.0f : kRadiusThumb, picture.ready() ? picture.glow : k.accent, in);
            }
        }

        // ---- the foot of the screen: the hint bar reads over whatever scrolls under it ----
        g.l.gradient_rect({kRail, 944.0f, gfx::kVirtualWidth - kRail, 60.0f}, 0.0f, k.base.with_alpha(0.0f), k.base.with_alpha(0.95f));
        g.l.rounded_rect({kRail, 1004.0f, gfx::kVirtualWidth - kRail, 76.0f}, 0.0f, k.base.with_alpha(0.95f));
        g.l.pop_opacity();

        // ---- attract: the picture, its name, and how to get back ----
        if (attract > 0.01f && !hero_now_.id.empty())
        {
            const float a = tween::smoothstep((attract - 0.3f) / 0.7f) * fade;
            g.l.push_opacity(a);
            mark(g, 120.0f, 92.0f, 46.0f, 1.0f, kWhite, k.accent);
            const float ww = text(g, g.f.display, "PROSPERO", 166.0f, 101.0f, 23.0f, kWhite.with_alpha(0.72f), gfx::Align::left, 6.5f);
            text(g, g.f.display, "TUBE", 166.0f + ww + 6.5f, 101.0f, 23.0f, kWhite, gfx::Align::left, 6.5f);
            const float swap = hero_fade_.running ? tween::smoothstep(cross) : 1.0f;
            g.l.push_opacity(swap);
            overline(g, hero_label_, 98.0f, 838.0f, k.accent);
            text(g, g.f.display, fit(g.f.display, hero_now_.title, 46.0f, 1700.0f), 96.0f, 902.0f, 46.0f, kWhite);
            avatar(g, 114.0f, 946.0f, 18.0f, hero_now_.channel, hero_now_.channel_id, context.images.get(hero_now_.avatar, PictureSize::avatar));
            const float cw = text(g, g.f.semibold, fit(g.f.semibold, hero_now_.channel, kTypeBody, 700.0f), 144.0f, 954.0f, kTypeBody, kWhite);
            text(g, g.f.regular, fit(g.f.regular, meta_of(hero_now_), kTypeBody, 700.0f), 144.0f + cw + 18.0f, 954.0f, kTypeBody, kWhite.with_alpha(0.72f));
            g.l.pop_opacity();
            // Time until the next one.
            g.l.rounded_rect({96.0f, 1004.0f, 1728.0f, 3.0f}, 1.5f, kWhite.with_alpha(0.16f));
            g.l.rounded_rect({96.0f, 1004.0f, 1728.0f * tween::clamp01(attract_step_ / kAttractStep), 3.0f}, 1.5f, k.accent);
            const ui::Hint items[] = {{ui::Button::dpad, tr("Back to Home")}};
            ui::HintLayout layout;
            layout.size = 34.0f;
            layout.text_size = 22.0f;
            layout.cy = 946.0f;
            layout.item_gap = 36.0f;
            ui::draw_hints(g.l, g.f, ui::GlyphStyle::dark(), items, 1, kRight, true, layout);
            g.l.pop_opacity();
        }
    }

    std::span<const ui::Hint> hints() const override
    {
        static ui::Hint items[4];
        if (rows_.empty())
        {
            if (load_ == model::Load::loading)
                return {};
            items[0] = {ui::Button::cross, tr("Try again")};
            items[1] = {ui::Button::square, tr("Search")};
            return {items, 2};
        }
        const Row &row = rows_[static_cast<std::size_t>(row_)];
        if (row.topics())
        {
            items[0] = {ui::Button::cross, tr("Open topic")};
            items[1] = {ui::Button::square, tr("Search")};
            items[2] = {ui::Button::options, tr("Sign in")};
            return {items, static_cast<std::size_t>(invite_ ? 3 : 2)};
        }
        items[0] = {ui::Button::cross, tr("Play")};
        items[1] = {ui::Button::triangle, tr("Details")};
        // Watch later is a list of the YouTube account: a Guest's Square searches instead.
        items[2] = {ui::Button::square, signed_in_ ? tr("Watch later") : tr("Search")};
        items[3] = {ui::Button::touchpad, tr("Actions")};
        return items;
    }

    float furniture() const override
    {
        return 1.0f - tween::smoothstep(tween::clamp01(attract_amount_.value * 1.6f));
    }

    bool attract(bool on) override
    {
        if (on && (rows_.empty() || rows_[static_cast<std::size_t>(row_)].topics() || rows_[static_cast<std::size_t>(row_)].count() == 0))
            return false; // nothing to show off
        attract_ = on;
        attract_step_ = 0.0f;
        return on;
    }

    FocusInfo focus() const override
    {
        FocusInfo info;
        if (rows_.empty())
        {
            info.id = load_ == model::Load::loading ? "loading" : load_ == model::Load::offline ? "offline" : "empty";
            info.rect = {kLeft, 360.0f, 900.0f, 200.0f};
            info.total = 1;
            return info;
        }
        const Row &row = rows_[static_cast<std::size_t>(row_)];
        const int col = state_of(row).col;
        info.id = row.shelf.id + "/" + std::to_string(col);
        info.text = row.count() == 0 ? row.shelf.title : row.topics() ? row.shelf.topics[static_cast<std::size_t>(col)].title
                                                                       : row.shelf.videos[static_cast<std::size_t>(col)].title;
        info.rect = card_rect(row_, col);
        info.index = col;
        info.total = row.count();
        if (row.count() > 0)
        {
            info.kind = row.topics() ? "topic" : "video";
            info.live = !row.topics() && (row.shelf.videos[static_cast<std::size_t>(col)].badges & model::kBadgeLive) != 0;
        }
        return info;
    }

    std::vector<std::pair<std::string, std::string>> focusable() const override
    {
        std::vector<std::pair<std::string, std::string>> all;
        for (const Row &row : rows_)
        {
            for (int i = 0; i < row.count(); ++i)
                all.emplace_back(row.shelf.id + "/" + std::to_string(i), row.topics() ? row.shelf.topics[static_cast<std::size_t>(i)].title
                                                                                      : row.shelf.videos[static_cast<std::size_t>(i)].title);
        }
        return all;
    }

    void snapshot(Snapshot &out) const override
    {
        out.set("load", load_ == model::Load::ready ? "ready" : load_ == model::Load::loading ? "loading" : load_ == model::Load::offline ? "offline" : "failed");
        out.set("profile", guest_ ? "guest" : "account");
        out.set("shelves", static_cast<int>(rows_.size()));
        out.set("row", rows_.empty() ? std::string() : rows_[static_cast<std::size_t>(row_)].shelf.id);
        out.set("hero", hero_now_.title);
        out.set("invite", invite_ ? 1 : 0);
        for (const Row &row : rows_)
        {
            std::vector<std::string> titles;
            for (int i = 0; i < row.count(); ++i)
                titles.push_back(row.topics() ? row.shelf.topics[static_cast<std::size_t>(i)].title : row.shelf.videos[static_cast<std::size_t>(i)].title);
            out.list("shelf:" + row.shelf.id, std::move(titles));
        }
    }

  private:
    // A shelf that is on the page: loading, or ready with something in it.
    struct Row
    {
        model::Shelf shelf;
        bool topics() const
        {
            return shelf.kind == model::ShelfKind::topics;
        }
        int count() const
        {
            return static_cast<int>(topics() ? shelf.topics.size() : shelf.videos.size());
        }
    };
    // What a shelf keeps while its content comes and goes.
    struct RowState
    {
        int col = 0;
        ui::Scroller scroll;
    };

    RowState &state(const Row &row)
    {
        return states_[row.shelf.id];
    }
    const RowState &state_of(const Row &row) const
    {
        static const RowState kNone;
        const auto found = states_.find(row.shelf.id);
        return found != states_.end() ? found->second : kNone;
    }

    float arrive(int index, float step = 0.05f, float duration = 0.45f) const
    {
        return tween::stagger(age_, index, step, duration);
    }

    // Takes over what Home is now: the shelves to show, in order, keeping the focus where it was.
    void collect(const model::Home &home)
    {
        const std::string focused = rows_.empty() ? std::string() : rows_[static_cast<std::size_t>(std::clamp(row_, 0, static_cast<int>(rows_.size()) - 1))].shelf.id;
        load_ = home.load;
        error_ = home.error;
        guest_ = home.profile.guest;
        invite_ = home.invite_sign_in;
        signed_in_ = home.profile.signed_in;
        rows_.clear();
        bool any_ready = false;
        for (const model::Shelf &shelf : home.shelves)
        {
            const bool ready = shelf.load == model::Load::ready;
            const bool filled = !shelf.videos.empty() || !shelf.topics.empty();
            any_ready = any_ready || ready;
            if (shelf.load == model::Load::failed && error_.empty())
                error_ = shelf.error;
            if (ready)
            {
                if (filled)
                    shown_[shelf.id] = shelf;
                else
                    shown_.erase(shelf.id);
            }
            // A shelf that is being read again keeps what it showed until the answer is in.
            const auto kept = shown_.find(shelf.id);
            if (kept != shown_.end() && (ready || shelf.load == model::Load::loading))
            {
                rows_.push_back(Row{kept->second});
                any_ready = true;
            }
        }
        if (!any_ready)
            rows_.clear(); // the whole page waits together; shelves do not pop in around an empty middle
        if (rows_.empty() && load_ == model::Load::ready)
            load_ = error_.empty() ? model::Load::ready : model::Load::failed;
        (void)focused;
        if (wanted_.empty() && !rows_.empty())
            wanted_ = rows_.front().shelf.id;
        for (std::size_t i = 0; i < rows_.size(); ++i)
        {
            RowState &s = state(rows_[i]);
            s.col = std::clamp(s.col, 0, std::max(0, rows_[i].count() - 1));
            if (rows_[i].shelf.id == wanted_)
                row_ = static_cast<int>(i);
        }
    }

    float top_of(int index) const
    {
        float y = 0.0f;
        for (int i = 0; i < index && i < static_cast<int>(rows_.size()); ++i)
            y += rows_[static_cast<std::size_t>(i)].topics() ? kTopicPitch : kShelfPitch;
        return y;
    }

    Rect card_rect(int row, int col) const
    {
        if (rows_.empty())
            return {kLeft, kShelfTop, kCardW, kCardH};
        const int last = static_cast<int>(rows_.size()) - 1;
        const int low = std::clamp(static_cast<int>(std::floor(row_pos_.value)), 0, last);
        const int high = std::min(low + 1, last);
        const float scroll = tween::lerp(top_of(low), top_of(high), row_pos_.value - static_cast<float>(low));
        const Row &r = rows_[static_cast<std::size_t>(std::clamp(row, 0, last))];
        const float first = rows_.front().topics() ? kShelfTopTopics : kShelfTop;
        const float y = first + top_of(row) - scroll;
        const float offset = state_of(r).scroll.offset();
        if (r.topics())
            return {kLeft + static_cast<float>(col) * kTilePitch - offset, y, kTileW, kTileH};
        return {kLeft + static_cast<float>(col) * kCardPitch - offset, y, kCardW, kCardH};
    }

    // The video the hero shows: the focused one; on the topic tiles, the first of the first shelf that has videos.
    const model::Video *hero_video() const
    {
        if (rows_.empty())
            return nullptr;
        const Row &row = rows_[static_cast<std::size_t>(row_)];
        if (!row.topics() && row.count() > 0)
            return &row.shelf.videos[static_cast<std::size_t>(state_of(row).col)];
        for (const Row &other : rows_)
        {
            if (!other.topics() && other.count() > 0)
                return &other.shelf.videos[0];
        }
        return nullptr;
    }
    std::string hero_shelf_title() const
    {
        if (rows_.empty())
            return {};
        const Row &row = rows_[static_cast<std::size_t>(row_)];
        if (!row.topics())
            return row.shelf.title;
        for (const Row &other : rows_)
        {
            if (!other.topics() && other.count() > 0)
                return other.shelf.title;
        }
        return {};
    }

    static std::string meta_of(const model::Video &video)
    {
        std::string meta = video.views;
        if (!video.age.empty())
            meta += (meta.empty() ? "" : " \xC2\xB7 ") + video.age;
        return meta;
    }

    void refuse(UpdateContext &context, float direction)
    {
        context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        context.feedback.rumble(0.25f, 0.05f);
        nudge_dir_ = direction;
        nudge_.trigger();
    }

    void handle(const InputFrame &input, UpdateContext &context)
    {
        Row &row = rows_[static_cast<std::size_t>(row_)];
        RowState &s = state(row);
        const int count = row.count();
        switch (input.nav)
        {
        case Direction::left:
        case Direction::right:
        {
            const int next = s.col + (input.nav == Direction::right ? 1 : -1);
            if (next >= 0 && next < count)
            {
                s.col = next;
                context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(card_rect(row_, next).cx()));
            }
            else if (next < 0)
            {
                context.nav.open_rail(); // past the first card is the rail
            }
            else if (!input.nav_repeat)
            {
                refuse(context, 1.0f);
            }
            break;
        }
        case Direction::up:
        case Direction::down:
        {
            const int next = row_ + (input.nav == Direction::down ? 1 : -1);
            if (next >= 0 && next < static_cast<int>(rows_.size()))
            {
                row_ = next;
                wanted_ = rows_[static_cast<std::size_t>(row_)].shelf.id;
                // Rows sound like steps: lower shelves, lower pitch.
                context.feedback.play(audio::Cue::tab, 1.1f - 0.07f * static_cast<float>(row_));
            }
            else if (!input.nav_repeat)
            {
                refuse(context, 0.0f);
            }
            break;
        }
        case Direction::none:
            break;
        }
        const Row &now = rows_[static_cast<std::size_t>(row_)];
        const int col = state(now).col;
        if (pressed(input, Action::back))
        {
            context.nav.back();
        }
        else if (pressed(input, Action::west) && (!signed_in_ || now.topics() || now.count() == 0))
        {
            context.nav.push(Route{ScreenId::search, {}, {}});
        }
        else if (pressed(input, Action::menu) && invite_)
        {
            context.nav.push(Route{ScreenId::sign_in, {}, {}}, Move::fade);
        }
        else if (now.count() == 0)
        {
            return;
        }
        else if (now.topics())
        {
            if (pressed(input, Action::confirm))
            {
                const model::Topic &topic = now.shelf.topics[static_cast<std::size_t>(col)];
                context.nav.push(Route{ScreenId::grid, topic.id, topic.title});
            }
        }
        else
        {
            const model::Video &video = now.shelf.videos[static_cast<std::size_t>(col)];
            if (pressed(input, Action::confirm))
            {
                // Picture to player: the card's picture becomes the whole screen.
                play_video(context, video, card_rect(row_, col), card_rect(row_, col).cx());
            }
            else if (pressed(input, Action::north))
            {
                context.nav.push(Route{ScreenId::video, video.id, video.title}, Move::zoom, ring_.value(), video.thumb);
            }
            else if (pressed(input, Action::west))
            {
                // Watch later: the provider names the change (a confirm row), then makes it (its apply row).
                if (later_.running)
                {
                    refuse(context, 0.0f); // one at a time: the answer of the last one is on its way
                }
                else
                {
                    later_.start(context.services.data, "account:confirm:watch_later_add:" + video.id);
                    later_title_ = video.title;
                    context.feedback.play(audio::Cue::select, 1.0f, ui::pan_for_x(card_rect(row_, col).cx()));
                }
            }
            else if (pressed(input, Action::touch))
            {
                // The actions of the video in focus, as the frame's sheet. What they change, Home reads again.
                Data *data = &context.services.data;
                context.nav.actions(video.id, video.title, [data]
                {
                    for (const char *catalog : {"history", "library:home", "library:subscriptions", "section:trending", "library:watch_later"})
                        data->forget(catalog);
                });
            }
        }
    }

    // Watch later on its way: said in a toast when YouTube has answered.
    void follow_later(UpdateContext &context)
    {
        const Page *answer = nullptr;
        const model::Load result = later_.poll(context.services.data, &answer);
        if (result == model::Load::ready)
        {
            const bool said = answer != nullptr && !answer->rows.empty() && !answer->rows.front().title.empty();
            context.nav.toast(said ? answer->rows.front().title.c_str() : tr("Added to Watch later"), later_title_, ToastKind::success);
            context.feedback.rumble(0.3f, 0.06f);
            context.services.data.forget("library:watch_later");
        }
        else if (result == model::Load::failed || result == model::Load::offline)
        {
            const bool said = answer != nullptr && !answer->error.empty();
            context.nav.toast(said ? answer->error.c_str() : tr("YouTube did not confirm the change. Nothing was changed."), later_title_, ToastKind::error);
            context.feedback.play(audio::Cue::error);
            context.feedback.rumble(0.25f, 0.05f);
        }
    }

    void draw_state(G &g) const
    {
        const Look &k = g.k;
        if (load_ == model::Load::loading || load_ == model::Load::idle)
        {
            // Bones in the shape of the page: a title, a play line, a shelf.
            bone(g, {kLeft, 250.0f, 720.0f, 46.0f}, 12.0f, 0);
            bone(g, {kLeft, 312.0f, 520.0f, 46.0f}, 12.0f, 1);
            bone(g, {kLeft, 412.0f, 330.0f, 60.0f}, 30.0f, 2);
            bone(g, {kLeft, kShelfTop - 46.0f, 260.0f, 26.0f}, 8.0f, 3);
            for (int i = 0; i < kBones; ++i)
                bone(g, {kLeft + static_cast<float>(i) * kCardPitch, kShelfTop, kCardW, kCardH}, kRadiusThumb, 4 + i);
            return;
        }
        const bool offline = load_ == model::Load::offline;
        const bool failed = load_ == model::Load::failed;
        Enter enter(g, arrive(2, 0.06f, 0.5f), 18.0f);
        const Rect panel{kLeft, 300.0f, 980.0f, 300.0f};
        glass_panel(g, panel, kRadiusPanel);
        icon(g, offline ? Icon::offline : failed ? Icon::alert : Icon::home, panel.x + 70.0f, panel.y + 84.0f, 52.0f, offline || failed ? k.accent : k.ink2);
        const char *title = offline ? tr("No connection to YouTube") : failed ? tr("Home could not be loaded") : tr("Nothing here yet");
        text(g, g.f.display, fit(g.f.display, title, kTypeTitle + 4.0f, panel.w - 170.0f), panel.x + 124.0f, panel.y + 98.0f, kTypeTitle + 4.0f, k.ink);
        const std::string body = offline  ? std::string(tr("The built-in service cannot reach YouTube. Check the network of this PS5, then try again."))
                                 : failed ? (error_.empty() ? std::string(tr("Try again in a moment.")) : error_)
                                          : std::string(tr("Search for something, or open a topic from the rail."));
        paragraph(g, g.f.regular, body, panel.x + 46.0f, panel.y + 168.0f, kTypeBody, panel.w - 92.0f, 36.0f, k.ink2, 3);
    }

    void draw_hero(const DrawContext &context, G &g, const model::Video &video, const std::string &label, float alpha, float slide) const
    {
        if (alpha <= 0.01f || video.id.empty())
            return;
        const Look &k = g.k;
        const float x = kLeft + slide;
        const float play_y = rows_.front().topics() ? 372.0f : 412.0f;
        g.l.push_opacity(tween::clamp01(alpha));

        // The block is anchored at its foot: a longer title grows upward.
        const float title_w = invite_ ? 1060.0f : 900.0f;
        const TextBlock block = measure_block(g.f.display, video.title, 52.0f, title_w, 60.0f, 2);
        const int lines = std::max(1, block.lines);
        const float last = play_y - 88.0f;
        const float first = last - 60.0f * static_cast<float>(lines - 1);
        if (!label.empty())
            overline(g, label, x + 2.0f, first - 64.0f, k.accent);
        paragraph(g, g.f.display, video.title, x - 3.0f, first, 52.0f, title_w, 60.0f, k.ink, 2);

        // Channel, numbers, what the video offers.
        const float my = last + 46.0f;
        avatar(g, x + 18.0f, my - 8.0f, 18.0f, video.channel, video.channel_id, context.images.get(video.avatar, PictureSize::avatar));
        float cursor = x + 48.0f;
        cursor += text(g, g.f.semibold, fit(g.f.semibold, video.channel, kTypeBody, 340.0f), cursor, my, kTypeBody, k.ink) + 14.0f;
        const std::string meta = meta_of(video);
        if (!meta.empty())
        {
            g.l.circle(cursor, my - 8.0f, 2.5f, k.ink3);
            cursor += 14.0f;
            cursor += text(g, g.f.regular, fit(g.f.regular, meta, kTypeBody, std::max(120.0f, x + 800.0f - cursor)), cursor, my, kTypeBody, k.ink2) + 18.0f;
        }
        // The line ends before the picture begins: tags only where they still stand on the page.
        if ((video.badges & model::kBadge4K) != 0 && cursor < x + 820.0f)
            cursor += tag(g, cursor, my - 8.0f, "4K", k.raised, k.ink) + 8.0f;
        if ((video.badges & model::kBadgeCaptions) != 0 && cursor < x + 820.0f)
            cursor += tag(g, cursor, my - 8.0f, "CC", k.raised, k.ink) + 8.0f;

        // The play line: what Cross does, and how much is left.
        const bool live = (video.badges & model::kBadgeLive) != 0;
        const bool resume = video.progress > 0.02f && video.progress < 0.97f && video.duration_s > 0;
        const char *verb = live ? tr("Watch live") : resume ? tr("Resume") : tr("Play");
        std::string right;
        if (live)
            right = video.views;
        else if (resume)
            right = tr_n("{0} min left", "{0} min left", std::max(1, static_cast<int>((1.0f - video.progress) * static_cast<float>(video.duration_s) / 60.0f + 0.5f)));
        else
            right = duration_text(video.duration_s);
        const float verb_w = measure(g.f.semibold, verb, kTypeBody);
        const float right_w = measure(g.f.regular, right, 22.0f);
        const float bar_w = resume ? 190.0f : 0.0f;
        const float width = 66.0f + verb_w + (resume ? 24.0f + bar_w : 0.0f) + (right.empty() ? 0.0f : 22.0f + right_w) + 30.0f + (live ? 22.0f : 0.0f);
        const Rect line{x, play_y, width, 60.0f};
        lift(g, line, 30.0f, 0.5f);
        glass_panel(g, line, 30.0f);
        const ui::GlyphStyle glyphs = k.dark ? ui::GlyphStyle::dark() : ui::GlyphStyle::light();
        ui::draw_button(g.l, g.f, glyphs, ui::Button::cross, line.x + 14.0f, line.cy(), 36.0f);
        float px = line.x + 62.0f;
        px += text(g, g.f.semibold, verb, px, line.cy() + 8.5f, kTypeBody, k.ink) + 24.0f;
        if (resume)
        {
            g.l.rounded_rect({px, line.cy() - 3.0f, bar_w, 6.0f}, 3.0f, k.ink.with_alpha(0.2f));
            g.l.rounded_rect({px, line.cy() - 3.0f, bar_w * video.progress, 6.0f}, 3.0f, k.accent);
            px += bar_w + 22.0f;
        }
        if (live)
        {
            const float pulse = g.reduced ? 0.5f : ui::breathe(g.time, 1.6f);
            g.l.circle(px + 6.0f, line.cy(), 6.0f + 3.0f * pulse, k.accent.with_alpha(0.3f));
            g.l.circle(px + 6.0f, line.cy(), 5.0f, k.accent);
            px += 22.0f;
        }
        text(g, g.f.regular, right, px, line.cy() + 7.5f, 22.0f, k.ink2);
        g.l.pop_opacity();
    }

    void draw_shelf(const DrawContext &context, G &g, const Row &row, float y, float alpha, bool focused_row, float ring_cx, int stagger_base) const
    {
        // A shelf that only peeks over the foot of the screen shows its pictures,
        // not text that the edge would cut.
        const int with_text = y + kCardH + 110.0f < 960.0f ? 1 : (y + kCardH + 44.0f < 992.0f ? 3 : 0);
        if (alpha <= 0.01f)
            return;
        const Look &k = g.k;
        const RowState &s = state_of(row);
        g.l.push_opacity(alpha);
        {
            Enter enter(g, arrive(stagger_base, 0.06f, 0.5f), 18.0f);
            text(g, g.f.semibold, fit(g.f.semibold, row.shelf.title, kTypeShelf, 900.0f), kLeft, y - 24.0f, kTypeShelf, focused_row ? k.ink : k.ink2);
        }
        const float pitch = row.topics() ? kTilePitch : kCardPitch;
        const float w = row.topics() ? kTileW : kCardW;
        const float h = row.topics() ? kTileH : kCardH;
        int focused = -1;
        int shown = 0;
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int c = 0; c < row.count(); ++c)
            {
                Rect r{kLeft + static_cast<float>(c) * pitch - s.scroll.offset(), y, w, h};
                if (r.x > gfx::kVirtualWidth + 40.0f || r.x + r.w < kRail - 40.0f)
                    continue;
                const float focus = focused_row ? tween::clamp01(1.0f - std::fabs(ring_cx - r.cx()) / (pitch * 0.7f)) : 0.0f;
                const bool is_focused = focused_row && c == s.col;
                if (pass == 0 && is_focused)
                {
                    focused = c;
                    continue; // drawn last, on top of its neighbours
                }
                if (pass == 1 && c != focused)
                    continue;
                const float in = arrive(stagger_base + 1 + (pass == 0 ? shown++ : 0), 0.055f, 0.5f);
                Enter enter(g, in, 30.0f);
                // The focused card grows about its centre and lifts.
                const float grow = 0.06f * focus;
                r = {r.x - r.w * grow * 0.5f, r.y - r.h * grow * 0.5f - 6.0f * focus, r.w * (1.0f + grow), r.h * (1.0f + grow)};
                // Cards slide under the rail's edge rather than over it.
                const float edge = tween::clamp01((r.x + r.w - kRail) / 120.0f);
                // What the edge of the screen cuts is a picture leaving the page: it is clipped there
                // with whatever is on it, and it carries no words underneath.
                const bool cut = r.x + r.w > gfx::kVirtualWidth - 4.0f || r.y + r.h > gfx::kVirtualHeight - 4.0f;
                if (cut)
                    g.l.push_clip({0.0f, 0.0f, gfx::kVirtualWidth, gfx::kVirtualHeight});
                if (row.topics())
                {
                    topic_tile(g, r, row.shelf.topics[static_cast<std::size_t>(c)], focus, edge);
                }
                else
                {
                    const model::Video &video = row.shelf.videos[static_cast<std::size_t>(c)];
                    video_card(g, r, video, context.images.get(video.thumb, PictureSize::card), focus, edge, cut ? 0 : with_text);
                }
                if (cut)
                    g.l.pop_clip();
            }
        }
        g.l.pop_opacity();
    }

    std::vector<Row> rows_;
    std::map<std::string, RowState> states_;
    std::map<std::string, model::Shelf> shown_; // what each shelf last had, by id
    std::string wanted_;                        // the shelf the focus belongs to
    model::Load load_ = model::Load::loading;
    std::string error_;
    bool guest_ = true;
    bool invite_ = false;
    bool signed_in_ = false;
    Act later_;               // Square on a video: Watch later, until YouTube has answered
    std::string later_title_; // ... and the video it was asked for
    int row_ = 0;
    tween::Spring row_pos_;
    ui::SpringRect ring_;
    bool ring_ready_ = false;
    ui::Pulse nudge_;
    float nudge_dir_ = 0.0f;
    model::Video hero_now_;
    model::Video hero_prev_;
    std::string hero_label_;
    std::string hero_label_prev_;
    tween::Timer hero_fade_;
    bool attract_ = false;
    tween::Spring attract_amount_;
    float attract_step_ = 0.0f;
    float age_ = 0.0f;
};

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

std::unique_ptr<Screen> make_home()
{
    return std::make_unique<Home>();
}

std::unique_ptr<Screen> make_opening()
{
    return std::make_unique<Opening>();
}

} // namespace afterglow
