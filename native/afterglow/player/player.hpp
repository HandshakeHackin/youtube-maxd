// ProsperoTube Afterglow - the player's controls and everything that opens over the video.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A second way to draw the player EVO runs, not a second player: Player takes
// a PlayerView (view.hpp) every frame and draws it in the Afterglow design. It
// takes no input and decides nothing. What it owns is motion: how the controls
// arrive, how a sheet slides in, how a notice drains, and the sounds those
// moments ask for. Whoever runs it (app/mount.cpp inside EVO's kit application,
// the host harness in tests) calls update() and draw() once per frame.
//
// The boards of the proposal it implements are 16 to 24; the motion and sound
// rules are sections 8 and 9 of docs/afterglow/design-notes.md.

#pragma once

#include "afterglow/player/view.hpp"
#include "afterglow/screen.hpp"

#include "ui/components.hpp"

namespace afterglow::player
{

class Player
{
  public:
    // What draw() builds. The app draws the layers at the moments EVO draws
    // its own (the controls, then a dialog, then a toast); the harness draws
    // them all.
    enum Layer : unsigned
    {
        kControls = 1u, // shade, captions, title, timeline, marks, seek, chat, the sheets, dimming
        kNotice = 2u,   // the notice above the timeline (in the app also EVO's toasts while the player is up)
        kDialog = 4u,
        kToast = 8u,
        kAll = 15u,
    };

    // images may be null: pictures then stay placeholders.
    Player(const ui::Fonts &fonts, ImageCache *images);
    Player(const Player &) = delete;
    Player &operator=(const Player &) = delete;

    void set_look(const Look &look);
    const Look &look() const
    {
        return *look_;
    }

    // One step: takes this frame's view and advances everything that moves.
    void update(const PlayerView &view, float dt);
    // Builds the layers back to front, as the frame does: world is what a
    // frosted panel blurs (the shade over the video), ui everything on it. glass
    // is the renderer's blurred copy of world, 0 where there is none. The two
    // lists may be one: world is always written first.
    void draw(gfx::DrawList &world, gfx::DrawList &ui, std::uint32_t glass, unsigned layers = kAll) const;

    // Where the video belongs: the whole screen, or the frame it steps into
    // beside the live chat.
    void prepare_video(int chat_side,float dt);
    Rect video_rect() const;
    float video_radius() const;

    // Sounds and rumble the last update asked for.
    const Feedback &feedback() const
    {
        return feedback_;
    }
    // True while something still moves: the caller keeps presenting frames.
    bool animating() const;

    FocusInfo focus() const;
    // The player's state in the shape of the frame's snapshot: interface,
    // look, screen ("player"), dialog, focus, focusable, state.
    std::string snapshot_json() const;
    const PlayerView &view() const
    {
        return view_;
    }

  private:
    struct Motion; // how far each part has arrived, worked out once per draw

    G make_g(gfx::DrawList &list, std::uint32_t glass) const;
    Motion motion() const;
    int chapter_at(double seconds) const;
    float share(double seconds) const;
    Picture picture(const std::string &url, PictureSize size) const;
    Rect theatre_rect() const;
    Rect sheet_rect() const;
    Rect notice_rect(const Motion &m) const;
    Rect card_rect(int index, float focus) const;
    Rect lit_control() const;

    void wear_look();
    void build_sheet();
    void sounds(const PlayerView &before);

    // player_draw.cpp: what belongs to the video itself.
    void draw_ground(gfx::DrawList &world, const Motion &m) const;
    void draw_captions(G &g, const Motion &m) const;
    void draw_header(G &g, const Motion &m) const;
    void draw_timeline(G &g, const Rect &lane, float alpha, bool on_page) const;
    void draw_live_bar(G &g, const Rect &lane, float alpha, bool on_page) const;
    void draw_foot(G &g, const Motion &m) const;
    void draw_seek(G &g, const Motion &m) const;
    void draw_centre(G &g, const Motion &m) const;
    void draw_stats(G &g, const Motion &m) const;
    // player_panels.cpp: what opens over it.
    void draw_notice(G &g, const Motion &m) const;
    void draw_up_next(G &g, const Motion &m) const;
    void draw_sheet(G &g, const Motion &m) const;
    void draw_chat(G &g, const Motion &m) const;
    void draw_next(G &g, const Motion &m) const;

    const ui::Fonts &fonts_;
    ImageCache *images_;
    const Look *look_;
    PlayerView view_;
    Feedback feedback_;
    std::string last_cue_;
    float time_ = 0.0f;
    bool started_ = false;
    bool reduced_ = false;

    // ---- the controls ----
    tween::Spring chrome_;
    tween::Spring paused_;
    tween::Spring loading_;
    tween::Spring scrub_;
    tween::Spring progress_; // the played share as shown
    tween::Spring target_;   // the seek target's share as shown
    tween::Spring dim_;
    tween::Spring stats_;
    ui::Pulse pause_pop_;
    int scrub_chapter_ = -1;

    // ---- what opens over the video: how far in, and what it showed last ----
    tween::Spring sheet_amount_;
    Sheet sheet_;
    std::string sheet_key_;
    int sheet_focus_ = -1;
    ui::ListView list_;
    ui::TabBar tabs_;
    tween::Spring up_next_amount_;
    tween::Spring up_next_pos_;
    UpNext up_next_;
    tween::Spring chat_amount_;
    tween::Spring theatre_;
    bool video_prepared_ = false;
    int theatre_side_ = 2;
    Chat chat_;
    std::string chat_newest_;
    float chat_fresh_ = 10.0f; // seconds since the newest line arrived
    tween::Spring next_amount_;
    NextVideo next_;
    ui::Countdown ring_;
    Notice notice_;
    float notice_in_ = 0.0f;  // seconds since it appeared
    float notice_out_ = 10.0f; // seconds since it was taken away
    ui::Dialog dialog_;
    ui::ToastStack toasts_;
    int toast_id_ = 0;
    std::string toast_key_;
    mutable gfx::DrawList scratch_; // for measuring the dialog's panel
};

// A time of the video: "31:22", "1:02:07"; with negative a leading minus.
std::string clock_text(double seconds, bool negative = false);

} // namespace afterglow::player
