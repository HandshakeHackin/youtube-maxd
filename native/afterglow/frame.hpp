// ProsperoTube Afterglow - the frame: everything around and between the screens.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One Frame is the whole interface. It owns the stack of screens and their
// hand-overs, the page ground with the colour of the focused picture, the rail,
// the header with the clock, the hint bar, toasts, the dialog, input routing
// and attract mode. Whatever runs it (mount.cpp in the app, the host harness
// in tests) calls update() and draw() once per frame and plays feedback().

#pragma once

#include "afterglow/screen.hpp"
#include "afterglow/sheet.hpp"

#include "ui/components.hpp"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace afterglow
{

constexpr float kAttractAfter = 45.0f; // seconds without input before attract mode
constexpr std::size_t kStackLimit = 24;

struct FrameOptions
{
    int scheme = 0;               // the Theme preference: an index into g_tube_schemes
    bool reduced_motion = false;
    bool opening = true;          // start with the opening moment (a cold start)
    float attract_after = kAttractAfter;
};

// What one frame cost, for the dev status file and the tests.
struct FrameStats
{
    std::size_t world_instances = 0;
    std::size_t ui_instances = 0;
    std::size_t runs = 0;       // draw calls a backend makes for the two lists
    float fill = 0.0f;          // drawn area / screen area: how many times the screen was covered
    float update_ms = 0.0f;     // the last update()
    float draw_ms = 0.0f;       // the last draw() (building the lists, not the GPU)
    float frame_ms = 0.0f;      // time between the last two update() calls, as the caller's dt
    float worst_frame_ms = 0.0f; // the longest of the last 120
    int pictures_ready = 0;
    int pictures_pending = 0;
    std::size_t picture_bytes = 0;
    int data_pending = 0;
};

class Frame final : private Nav
{
  public:
    Frame(Platform &platform, Source &source, const ui::Fonts &fonts, const FrameOptions &options);
    ~Frame() override;
    Frame(const Frame &) = delete;
    Frame &operator=(const Frame &) = delete;

    // One step: time, data, pictures, input, screens. dt is seconds since the
    // previous call.
    void update(const InputFrame &input, float dt);
    // Builds the frame's two layers, back to front: world, then (after the
    // renderer has made its blurred copy of world, where it can) ui. glass is
    // that copy's texture, 0 where there is none: panels are then solid.
    void draw(gfx::DrawList &world, gfx::DrawList &ui, std::uint32_t glass);
    // Sounds and rumble the last update asked for.
    const Feedback &feedback() const
    {
        return feedback_;
    }
    // The kit's sound set for the cues.
    audio::SoundSet sounds() const
    {
        return look_->theme.sounds;
    }
    // True while something moves: the caller must keep presenting frames.
    bool animating() const;

    // The Theme preference changed.
    void set_scheme(int scheme);
    void set_reduced_motion(bool reduced)
    {
        options_.reduced_motion = reduced;
    }
    const Look &look() const
    {
        return *look_;
    }

    // Replaces the stack: what the app opens the interface with, and tests.
    void open(const Route &route, Move move = Move::cut);
    // The app's player closed and the interface is on screen again.
    void resume();
    // The picture a video was handed to the player with (Nav::play), "" when
    // none travels: the player begins with the same picture.
    const std::string &launch_picture() const
    {
        return launch_.picture;
    }

    ScreenId screen() const;
    std::size_t depth() const
    {
        return stack_.size();
    }
    bool rail_open() const
    {
        return rail_open_;
    }
    bool attracting() const
    {
        return attract_;
    }
    // Circle on the last screen: the app decides (close, ask twice, nothing).
    // True once per such press; reading it clears it.
    bool take_exit_request();

    FocusInfo focus() const;
    // The interface's state as one JSON object: screen, stack, focus, what can
    // be focused, rail, toasts, dialog, the screen's own values, FrameStats.
    // The shape is the contract of tools/console and of the board checks.
    std::string snapshot_json() const;
    const FrameStats &stats() const
    {
        return stats_;
    }

  private:
    struct Entry
    {
        Route route;
        std::unique_ptr<Screen> screen;
        Move arrived = Move::cut; // how it was opened: Circle leaves the other way
        Rect carried{};
        std::string picture; // the picture that travelled with it, when the screen that pushed named it
    };

    // Nav, as the top screen sees it.
    void push(const Route &route, Move move, const Rect &carried) override;
    void push(const Route &route, Move move, const Rect &carried, std::string_view picture) override;
    void back() override;
    void root(const Route &route) override;
    void open_rail() override;
    void toast(std::string_view text, ToastKind kind) override;
    void toast(std::string_view text, std::string_view detail, ToastKind kind) override;
    void confirm(std::string_view title, std::string_view body, std::string_view confirm_label,
                 std::function<void(bool)> answered) override;
    void ambient(std::string_view picture_url) override;
    void ask(Question question, std::function<void(int answer)> answered) override;
    void ask_text(TextQuestion question, std::function<void(bool taken, std::string text)> answered) override;
    void actions(const std::string &row, std::string_view about, std::function<void()> changed) override;
    bool back_to(ScreenId screen) override;
    bool play(const model::Video &video, const Rect &carried, std::string_view picture) override;

    void apply_requests();
    void start_move(Move move, const Rect &carried, std::string picture = {});
    void put_sheets_away();
    void update_rail(const InputFrame &input);
    void rail_destinations();
    UpdateContext context(bool top);

    int active_destination() const;
    void draw_ground(gfx::DrawList &world);
    void draw_screen(const Entry &entry, DrawContext &dc, const Phase &phase);
    void draw_rail(G &g, float arrive);
    void draw_header(G &g, const Entry &entry, float alpha);
    void draw_hints(G &g, const Entry &entry, float alpha);
    void draw_launch(G &g);
    void measure(const gfx::DrawList &world, const gfx::DrawList &ui);

    Platform &platform_;
    const ui::Fonts &fonts_;
    FrameOptions options_;
    const Look *look_;
    Data data_;
    ImageCache images_;
    Services services_;
    Feedback feedback_;

    std::vector<std::unique_ptr<Entry>> stack_;
    Entry *outgoing_ = nullptr;      // the screen on its way out during a hand-over
    std::unique_ptr<Entry> removed_; // ... kept alive here when it has left the stack
    Move move_ = Move::cut;
    Rect carried_{};
    std::string carried_picture_;
    tween::Timer change_;
    bool outgoing_on_top_ = false; // back moves draw the leaving screen above the one it reveals

    // Requests of the current update, applied after it.
    enum class Request : std::uint8_t
    {
        none,
        push,
        back,
        root,
        back_to,
    };
    Request request_ = Request::none;
    Route request_route_;
    Move request_move_ = Move::push;
    Rect request_carried_{};
    std::string request_picture_;
    std::size_t request_depth_ = 0; // back_to: how many screens stay
    bool request_rail_ = false;
    bool exit_request_ = false;

    float clock_ = 0.0f;
    float idle_ = 0.0f;
    bool attract_ = false;

    // The page ground's colour.
    std::string ambient_now_;
    std::string ambient_prev_;
    tween::Timer ambient_fade_;
    tween::Spring ambient_level_{1.0f, 0.0f, 1.0f};

    // The rail.
    struct Destination
    {
        Icon icon;
        const char *label; // English; drawn through tr()
        ScreenId screen;
        float y;
    };
    std::vector<Destination> destinations_;
    bool rail_open_ = false;
    tween::Spring rail_amount_;
    int rail_focus_ = 0;
    tween::Spring rail_pos_;

    ui::ToastStack toasts_;
    ui::Dialog dialog_;
    std::function<void(bool)> dialog_answered_;
    std::function<void(int)> question_answered_; // the dialog is a Question of a screen's own

    // The sheets a screen asks for through Nav.
    ActionsSheet sheet_;
    std::function<void()> sheet_changed_;
    TextSheet text_sheet_;

    // A video handed to the player from its picture: the picture becomes the
    // whole screen, and returns to where it came from when the player closes.
    struct Launch
    {
        bool active = false;
        bool returning = false;
        tween::Timer move;
        float held = 0.0f; // seconds the whole-screen picture has stood since the move ended
        Rect from{};
        std::string picture;
    };
    Launch launch_;
    std::vector<std::string> toast_log_; // the last few, for snapshots

    FrameStats stats_;
    std::array<float, 120> frame_times_{};
    std::size_t frame_time_at_ = 0;
};

} // namespace afterglow
