/* Actual generated PlayerScreen paths; playback, pad and navigation are mocked.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>

struct Playback {
    double position = 80, duration = 100, sought = -1;
    bool paused = false, scrubbing = false, live = false;
    int stops = 0, seeks = 0;
    double getPositionSeconds() { return position; }
    double getDurationSeconds() { return duration; }
    bool isPaused() { return paused; }
    bool isScrubbing() { return scrubbing; }
    bool isLiveSource() { return live; }
    void stopPlayback() { ++stops; }
    void seekTo(double target) { sought = target; ++seeks; }
} state, *playback = &state;
struct ScreenManager { int returns = 0; void returnFromPlayback() { ++returns; } } manager;
struct Application {
    static Application& getInstance() { static Application app; return app; }
    ScreenManager* getScreenManager() { return &manager; }
};
namespace PadButtons { constexpr unsigned Cross = 1, Circle = 2; }
static bool complete, seeking, pb_prebuffer_hold;
static struct { bool seek_discarding; } g_pp_pb;
static int skip_calls, confirm_calls, finish_calls, notices;
static bool finish_allowed, show_message, accepted;
static double next_target;
static const char* prompt;
static uint64_t m_tubeSponsorPromptAt;
static uint64_t NowMs() { return 10000; }
static void toast(const char*, const char*) { ++notices; }
static int evo_pb_is_complete() { return complete; }
static int evo_demux_is_seeking() { return seeking; }
static int tube_finish_drained_and_next(double) { return 0; }
static double tube_skip_target(double) { ++skip_calls; return next_target; }
static const char* tube_sponsor_prompt(double) { return prompt; }
static double tube_sponsor_confirm(double, int accept) {
    ++confirm_calls; accepted = accept; prompt = nullptr;
    return accept ? next_target : -1;
}
static int tube_sponsor_show_message() { return show_message; }
static int tube_sponsor_finish(double target, double duration) {
    ++finish_calls;
    return finish_allowed && target >= duration;
}
static void tick() {
#include "tick.inc"
}
static bool input(unsigned pressed) {
#include "input.inc"
    return false;
}
static void reset() {
    state = Playback{}; manager.returns = 0;
    complete = seeking = pb_prebuffer_hold = g_pp_pb.seek_discarding = false;
    skip_calls = confirm_calls = finish_calls = notices = 0;
    finish_allowed = show_message = accepted = false;
    next_target = -1; prompt = nullptr; m_tubeSponsorPromptAt = 0;
}
int main() {
    // Automatic skips cannot compete with pause, scrub, live or seek ownership.
    for (int gate = 0; gate < 7; ++gate) {
        reset(); next_target = 100; finish_allowed = true;
        switch (gate) {
            case 0: complete = true; break;
            case 1: state.paused = true; break;
            case 2: state.scrubbing = true; break;
            case 3: state.live = true; break;
            case 4: seeking = true; break;
            case 5: g_pp_pb.seek_discarding = true; break;
            case 6: pb_prebuffer_hold = true; break;
        }
        tick(); assert(!skip_calls && !finish_calls && !state.stops && !state.seeks);
    }
    reset(); next_target = 90; tick();
    assert(state.seeks == 1 && state.sought == 90 && !state.stops && !notices);
    reset(); next_target = 90; show_message = true; tick();
    assert(state.seeks == 1 && notices == 1 && !manager.returns);
    for (double end : {100.0, 105.0}) {
        reset(); next_target = end; finish_allowed = true; show_message = true; tick();
        assert(state.stops == 1 && manager.returns == 1 && !state.seeks && notices == 1);
        reset(); next_target = end; finish_allowed = true; tick();
        assert(state.stops == 1 && manager.returns == 1 && !state.seeks && !notices);
    }
    // A rejected/stale completion cannot fall through into a clamped end seek.
    reset(); next_target = 100; tick();
    assert(!state.stops && !state.seeks && !manager.returns);
    reset(); next_target = 100; finish_allowed = true; prompt = "Skip outro?";
    assert(input(PadButtons::Cross));
    assert(accepted && confirm_calls == 1 && state.stops == 1 && manager.returns == 1 && !state.seeks);
    reset(); next_target = 90; prompt = "Skip sponsor?";
    assert(input(PadButtons::Cross));
    assert(accepted && state.seeks == 1 && state.sought == 90 && !state.stops);
    for (unsigned buttons : {PadButtons::Circle, PadButtons::Cross | PadButtons::Circle}) {
        reset(); next_target = 100; finish_allowed = true; prompt = "Skip outro?";
        assert(input(buttons));
        assert(!accepted && confirm_calls == 1 && !state.stops && !state.seeks && !manager.returns);
    }
    reset(); assert(!input(PadButtons::Circle) && !confirm_calls);
    puts("PASS: shipping PS5 SponsorBlock tick/pad paths, skip/end decisions, navigation, silent/message actions and competing playback gates");
}
