// ProsperoTube Afterglow - Home: what its three source files share.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// home.cpp takes over what Home is now, keeps the focus and moves things;
// home_input.cpp takes the controller; home_draw.cpp draws. Nothing outside
// these three files includes this header.

#pragma once

#include "afterglow/data_feed.hpp"
#include "afterglow/screen.hpp"

#include <map>
#include <string>
#include <vector>

namespace afterglow::home_page
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
    void enter(const Route &route, UpdateContext &context, bool restored) override;
    void update(const InputFrame &input, UpdateContext &context) override;
    void draw(const DrawContext &context, const Phase &phase) const override;
    std::span<const ui::Hint> hints() const override;
    float furniture() const override;
    bool attract(bool on) override;
    FocusInfo focus() const override;
    std::vector<std::pair<std::string, std::string>> focusable() const override;
    void snapshot(Snapshot &out) const override;

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

    // ---- home.cpp: what Home is now, and where things are ----
    void collect(const model::Home &home);
    float top_of(int index) const;
    Rect card_rect(int row, int col) const;
    const model::Video *hero_video() const;
    std::string hero_shelf_title() const;
    void follow_later(UpdateContext &context);
    // ---- home_input.cpp ----
    void refuse(UpdateContext &context, float direction);
    void handle(const InputFrame &input, UpdateContext &context);
    // ---- home_draw.cpp ----
    static std::string meta_of(const model::Video &video);
    void draw_state(G &g) const;
    void draw_hero(const DrawContext &context, G &g, const model::Video &video, const std::string &label, float alpha, float slide) const;
    void draw_shelf(const DrawContext &context, G &g, const Row &row, float y, float alpha, bool focused_row, float ring_cx, int stagger_base) const;

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

} // namespace afterglow::home_page
