/* Actual generated PlayerScreen paths; playback, pad and navigation are mocked.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>

static void tube_background_play_end_pause(int paused);
struct Playback {
    double position = 80, duration = 100, sought = -1;
    bool paused = false, scrubbing = false, live = false, seek_accepts = true;
    int stops = 0, seeks = 0, pause_changes = 0;
    double getPositionSeconds() { return position; }
    double getDurationSeconds() { return duration; }
    bool isPaused() { return paused; }
    bool isScrubbing() { return scrubbing; }
    bool isLiveSource() { return live; }
    void setPaused(bool value) { tube_background_play_end_pause(0); paused=value; ++pause_changes; }
    void stopPlayback() { tube_background_play_end_pause(0); ++stops; }
    void seekTo(double target) { sought = target; ++seeks; }
    bool seekTo(double target, bool keepPaused) {
        tube_background_play_end_pause(0);
        if (!seek_accepts) return false;
        paused = keepPaused; seekTo(target); return true;
    }
} state, *playback = &state;
struct ScreenManager { int returns = 0; void returnFromPlayback() { ++returns; } } manager;
struct Application {
    static Application& getInstance() { static Application app; return app; }
    ScreenManager* getScreenManager() { return &manager; }
};
namespace PadButtons { constexpr unsigned Cross = 1, Circle = 2, R3 = 4, Right = 8, Down = 16, Left = 32, Up = 64; }
static bool complete, seeking, pb_prebuffer_hold, engine_paused;
static struct { bool seek_discarding; } g_pp_pb;
static int skip_calls, confirm_calls, finish_calls, notices, highlight_calls, highlight_failures, idle_retries;
static bool finish_allowed, show_message, accepted;
static bool phone_finished, phone_owns, local_finished, background_play, background_end_pause;
static void tube_background_play_end_pause(int paused) { background_end_pause=background_play && paused; }
static int tube_background_play_take_end_pause() { bool resume=background_play && background_end_pause;background_end_pause=false;return resume; }
static int tube_background_play_active() { return background_play; }
static int local_finish_calls, phone_finish_calls;
static bool sponsor_completed;
static int tube_sponsor_completed() { return sponsor_completed; }
static void tube_completion_seek_observe(double,int) {}
struct tube_sponsor_skip_t { double from, to; unsigned rgb, age_ms; const char *category; };
static double next_target;
static bool afterglow, undo_ready;
static int skip_starts, undo_starts;
static int tube_afterglow_enabled() { return afterglow; }
static bool next_offer_wait, next_offer_ready;
static int next_offer_direction, next_offer_confirm, next_offer_cancel;
static int tube_next_offer_hold() { return next_offer_wait; }
static int tube_next_offer_input(int direction, int confirm, int cancel) {
    next_offer_direction=direction;next_offer_confirm=confirm;next_offer_cancel=cancel;
    return next_offer_ready;
}
static void tube_sponsor_skip_observe(double, int) {}
static void tube_sponsor_skip_started(double from, double to) { assert(from == state.position && to == next_target); ++skip_starts; }
static int tube_sponsor_last_skip(tube_sponsor_skip_t *out) {
    if (undo_ready) *out = {12, 20, 0x00ff00, 0, "Sponsor"};
    return undo_ready;
}
static void tube_sponsor_undo_started(double target) { assert(target == 12); ++undo_starts; undo_ready = false; }
enum { TUBE_COMPLETION_PAUSE=1, TUBE_COMPLETION_REPEAT=2 };
static int completion_action, completion_taken;
static int tube_take_completion_action() { ++completion_taken; int a=completion_action;completion_action=0;return a; }
static void tube_completion_playback_state(int,int) {}
static bool playlist_wait;
static bool manual_pending;
static int manual_status;
static int tube_manual_navigation_pending() { return manual_pending; }
static int tube_manual_navigation_status() {if(manual_status<0)manual_pending=false;return manual_status;}
static int playlist_cancels;
static int tube_playlist_eof_pending() { return playlist_wait; }
static void tube_playlist_eof_cancel() { manual_pending=false;playlist_wait=false; ++playlist_cancels; }
static const char* prompt;
static bool m_showStatsForNerds;
static void toast(const char*, const char*) { ++notices; }
// The shipping completion predicate suppresses drained EOF while paused.
static int evo_pb_is_complete() { return complete && !engine_paused && !state.paused && !state.scrubbing && !seeking; }
static int evo_pb_is_paused() { return engine_paused || state.paused; }
static int evo_demux_is_seeking() { return seeking; }
static int tube_phone_finished() { ++phone_finish_calls; return phone_finished; }
static int tube_phone_owns_media() { return phone_owns; }
static int tube_finish_drained_and_next(double) { ++local_finish_calls; return local_finished; }
static double evo_audio_rate() { return 1.75; }
static double tube_skip_target_at_rate(double, double rate) { assert(rate == 1.75); ++skip_calls; return next_target; }
static double tube_highlight_target(double, double) { ++highlight_calls; return next_target; }
static void tube_highlight_seek_failed(double target) { assert(target == next_target); ++highlight_failures; }
static void tube_highlight_retry_idle() { ++idle_retries; }
static const char* tube_sponsor_prompt(double) { return prompt; }
static double tube_sponsor_confirm(double, int accept) {
    ++confirm_calls; accepted = accept; prompt = nullptr;
    return accept ? next_target : -1;
}
static int tube_sponsor_show_message() { return show_message; }
static int tube_sponsor_finish(double target, double duration) {
    ++finish_calls;
    sponsor_completed=finish_allowed && target>=duration;
    return sponsor_completed;
}
static void tick() {
#include "tick.inc"
}
static bool input(unsigned pressed) {
#include "input.inc"
    return false;
}
static void reset() {
    next_offer_wait=next_offer_ready=false;next_offer_direction=next_offer_confirm=next_offer_cancel=0;
    afterglow = undo_ready = false; skip_starts = undo_starts = 0;
    state = Playback{}; manager.returns = 0;
    completion_action=completion_taken=0;
    sponsor_completed=false;
    playlist_wait=false; playlist_cancels=0;
    manual_pending=false;manual_status=0;
    complete = seeking = pb_prebuffer_hold = g_pp_pb.seek_discarding = engine_paused = false;
    skip_calls = confirm_calls = finish_calls = notices = highlight_calls = highlight_failures = idle_retries = 0;
    finish_allowed = show_message = accepted = false;
    phone_finished = phone_owns = local_finished = background_play = background_end_pause = false; local_finish_calls = phone_finish_calls = 0;
    next_target = -1; prompt = nullptr; m_showStatsForNerds = false;
}
static void test_background_completion() {
    for (bool sponsor : {false,true}) {
        reset(); background_play=true; complete=!sponsor; sponsor_completed=sponsor;
        local_finished=phone_finished=true; completion_action=TUBE_COMPLETION_REPEAT;
        tick();
        assert(state.paused && state.pause_changes==1 && !state.stops && !manager.returns);
        assert(!phone_finish_calls && !local_finish_calls && !completion_taken && !skip_calls);
        assert(completion_action==TUBE_COMPLETION_REPEAT);
        tick(); assert(state.pause_changes==1 && !completion_taken); /* hidden EOF is idempotent */
        assert(background_end_pause);
        if (tube_background_play_take_end_pause()) state.setPaused(false);
        background_play=local_finished=phone_finished=false; tick();
        assert(completion_taken==1 && state.seeks==1 && state.sought==0 && !state.paused);
    }
    for (int gate=0;gate<4;++gate) {
        reset(); background_play=complete=true;
        state.scrubbing=gate==0; seeking=gate==1; g_pp_pb.seek_discarding=gate==2; pb_prebuffer_hold=gate==3;
        tick(); assert(!state.paused && !state.stops && !manager.returns && !completion_taken);
        state.scrubbing=seeking=g_pp_pb.seek_discarding=pb_prebuffer_hold=false;
        tick(); assert(state.paused && !phone_finish_calls && !local_finish_calls);
    }
    reset(); background_play=true; next_target=90; tick();
    assert(state.seeks==1 && state.sought==90 && skip_starts==1 && !manager.returns);
    reset(); background_play=manual_pending=true; manual_status=1; tick();
    assert(state.stops==1 && !manager.returns); /* finishing a pending transition never steals the browser */
    reset();
}

int main() {
    test_background_completion();
    reset(); afterglow=true;complete=local_finished=next_offer_wait=true;tick();assert(!state.stops && !manager.returns);
    next_offer_wait=false;tick();assert(state.stops==1 && manager.returns==1);
    reset();afterglow=undo_ready=sponsor_completed=local_finished=true;state.paused=true;
    tick();assert(!state.stops && !local_finish_calls);
    complete=true;assert(input(PadButtons::Cross) && undo_starts==1 && state.sought==12 && state.paused);
    reset(); afterglow=next_offer_ready=true;playlist_wait=true;
    assert(input(PadButtons::Right) && next_offer_direction==1 && !playlist_cancels);
    assert(input(PadButtons::Cross) && next_offer_confirm && !playlist_cancels);
    assert(input(PadButtons::Cross|PadButtons::Circle) && next_offer_cancel && !playlist_cancels);
    reset();afterglow=next_offer_ready=phone_owns=true;assert(!input(PadButtons::Cross));
    reset(); next_target = 90; tick(); assert(skip_starts == 1);
    reset(); next_target = 90; state.seek_accepts = false; tick(); assert(skip_starts == 0 && notices == 1);
    reset(); next_target = 90; afterglow = show_message = true; tick(); assert(skip_starts == 1 && notices == 0);
    reset(); afterglow = undo_ready = true; state.paused = true;
    assert(input(PadButtons::Cross) && state.sought == 12 && state.paused && undo_starts == 1 && !undo_ready);
    reset(); afterglow = undo_ready = true; state.seek_accepts = false;
    assert(input(PadButtons::Cross) && !state.seeks && !undo_starts && undo_ready && notices == 1);
    reset(); undo_ready = true; assert(!input(PadButtons::Cross) && !state.seeks);
    reset(); afterglow = undo_ready = true; m_showStatsForNerds = true;
    assert(!input(PadButtons::Cross) && !state.seeks);
    reset(); afterglow = undo_ready = true;
    assert(!input(PadButtons::Cross | PadButtons::Circle) && !state.seeks);
    for (int gate = 0; gate < 6; ++gate) {
        reset(); afterglow = undo_ready = true;
        state.live = gate == 0; state.scrubbing = gate == 1; complete = gate == 2;
        seeking = gate == 3; g_pp_pb.seek_discarding = gate == 4; pb_prebuffer_hold = gate == 5;
        assert(input(PadButtons::Cross) && !state.seeks && !undo_starts && undo_ready);
    }
    reset();manual_pending=true;manual_status=1;tick();assert(state.stops==1 && manager.returns==1 && !local_finish_calls);
    reset();manual_pending=true;manual_status=1;seeking=true;tick();assert(!state.stops);
    seeking=false;tick();assert(state.stops==1);
    reset();manual_pending=true;phone_owns=true;tick();assert(!manual_pending && !state.stops);
    reset();manual_pending=true;manual_status=-1;tick();assert(notices==1 && !state.stops);tick();assert(notices==1);
    reset();manual_pending=true;input(PadButtons::Cross);assert(!manual_pending && playlist_cancels==1);
    reset();complete=true;completion_action=TUBE_COMPLETION_REPEAT;tick();
    assert(state.seeks==1 && state.sought==0 && !state.paused && !state.stops);
    tick();assert(state.seeks==1);
    reset();complete=true;completion_action=TUBE_COMPLETION_REPEAT;state.seek_accepts=false;tick();
    assert(state.paused && notices==1 && !state.stops);tick();assert(notices==1);
    reset();complete=true;completion_action=TUBE_COMPLETION_PAUSE;tick();assert(state.paused && !state.seeks);
    reset();complete=true;phone_owns=true;completion_action=TUBE_COMPLETION_REPEAT;tick();
    assert(!completion_taken && !state.seeks);
    for(int gate=0;gate<4;++gate) {
        reset();complete=true;completion_action=TUBE_COMPLETION_REPEAT;
        state.scrubbing=gate==0;seeking=gate==1;g_pp_pb.seek_discarding=gate==2;pb_prebuffer_hold=gate==3;
        tick();assert(!completion_taken && !state.seeks);
    }
    reset();playlist_wait=true;input(0);assert(playlist_wait && !playlist_cancels);
    input(PadButtons::Cross);assert(!playlist_wait && playlist_cancels==1);
    reset(); complete=true; state.scrubbing=true; phone_finished=true; local_finished=true; tick();
    assert(!state.stops && !phone_finish_calls && !local_finish_calls);
    for (bool discard:{false,true}) {
        reset(); complete=true; phone_finished=true; local_finished=true;
        seeking=!discard; g_pp_pb.seek_discarding=discard; tick();
        assert(!state.stops && !phone_finish_calls && !local_finish_calls);
    }
    // EOF is owned by the phone queue or local queue, before SponsorBlock runs.
    reset(); complete=true; phone_finished=true; local_finished=true; tick();
    assert(state.stops==1 && manager.returns==1 && !local_finish_calls && !skip_calls);
    reset(); complete=true; phone_owns=true; local_finished=true; tick();
    assert(!state.stops && !local_finish_calls && !skip_calls);
    reset(); complete=true; local_finished=true; tick();
    assert(state.stops==1 && manager.returns==1 && local_finish_calls==1 && !skip_calls);
    // Automatic skips cannot compete with pause, scrub, live or seek ownership.
    for (int gate = 0; gate < 8; ++gate) {
        reset(); next_target = 100; finish_allowed = true;
        switch (gate) {
            case 0: complete = true; break;
            case 1: state.paused = true; break;
            case 2: state.scrubbing = true; break;
            case 3: state.live = true; break;
            case 4: seeking = true; break;
            case 5: g_pp_pb.seek_discarding = true; break;
            case 6: pb_prebuffer_hold = true; break;
            case 7: engine_paused = true; break; // FSM can still report playing.
        }
        tick(); assert(!skip_calls && !finish_calls && !state.stops && !state.seeks);
    }
    reset(); next_target = 90; tick();
    assert(state.seeks == 1 && state.sought == 90 && !state.stops && !notices);
    reset(); next_target = 90; show_message = true; tick();
    assert(state.seeks == 1 && notices == 1 && !manager.returns);
    for (double end : {100.0, 105.0}) {
        reset(); next_target = end; finish_allowed = true; show_message = true; tick();
        assert(state.paused && !state.stops && !manager.returns && !state.seeks && notices == 1);
        reset(); next_target = end; finish_allowed = true; tick();
        assert(state.paused && !state.stops && !manager.returns && !state.seeks && !notices);
        local_finished=true;tick();assert(state.stops==1 && manager.returns==1);
    }
    reset();next_target=100;finish_allowed=true;tick();
    completion_action=TUBE_COMPLETION_REPEAT;tick();assert(state.seeks==1 && state.sought==0 && !state.stops);
    reset();sponsor_completed=true;phone_owns=true;phone_finished=true;tick();
    assert(state.stops==1 && manager.returns==1 && !local_finish_calls);
    reset();sponsor_completed=true;playlist_wait=true;tick();
    assert(!state.stops && local_finish_calls==1);
    local_finished=true;tick();assert(state.stops==1 && manager.returns==1);
    // A rejected/stale completion cannot fall through into a clamped end seek.
    reset(); next_target = 100; tick();
    assert(!state.stops && !state.seeks && !manager.returns);
    reset(); next_target = 100; finish_allowed = true; prompt = "Skip outro?";
    assert(input(PadButtons::Cross));
    assert(accepted && confirm_calls == 1 && state.paused && !state.stops && !manager.returns && !state.seeks);
    reset(); next_target = 90; prompt = "Skip sponsor?";
    assert(input(PadButtons::Cross));
    assert(accepted && state.seeks == 1 && state.sought == 90 && !state.stops);
    for (unsigned buttons : {PadButtons::Circle, PadButtons::Cross | PadButtons::Circle}) {
        reset(); next_target = 100; finish_allowed = true; prompt = "Skip outro?";
        assert(input(buttons));
        assert(!accepted && confirm_calls == 1 && !state.stops && !state.seeks && !manager.returns);
    }
    reset(); assert(!input(PadButtons::Circle) && !confirm_calls);
    // Explicit highlights keep paused playback and do not bypass seek ownership.
    reset(); state.paused = true; next_target = 90;
    assert(input(PadButtons::R3) && highlight_calls == 1 && state.seeks == 1 && state.paused && !notices);
    reset(); next_target = 90; show_message = true;
    assert(input(PadButtons::R3) && state.seeks == 1 && notices == 1);
    reset(); engine_paused = true; next_target = 90;
    assert(input(PadButtons::R3) && state.seeks == 1 && state.paused && idle_retries == 1);
    reset(); engine_paused = true; state.seek_accepts = false; next_target = 90;
    assert(input(PadButtons::R3) && !state.seeks && engine_paused && highlight_failures == 1 && notices == 1);
    reset(); engine_paused = true; state.seek_accepts = false; next_target = 90; prompt = "Jump to highlight?";
    assert(input(PadButtons::Cross) && !state.seeks && engine_paused && highlight_failures == 1);
    reset(); state.paused = true; next_target = 90;
    assert(input(PadButtons::R3) && state.seeks == 1 && idle_retries == 1);
    seeking = true;
    assert(input(PadButtons::R3) && state.seeks == 1 && idle_retries == 1);
    seeking = false; g_pp_pb.seek_discarding = true;
    assert(input(PadButtons::R3) && state.seeks == 1 && idle_retries == 1);
    g_pp_pb.seek_discarding = false; pb_prebuffer_hold = true;
    assert(input(PadButtons::R3) && state.seeks == 1 && idle_retries == 1);
    pb_prebuffer_hold = false; // Asynchronous failure settled at the paused position.
    assert(input(PadButtons::R3) && state.seeks == 2 && idle_retries == 2 && state.paused);
    for (int gate = 0; gate < 6; ++gate) {
        reset(); next_target = 90;
        switch (gate) {
            case 0: state.live = true; break;
            case 1: state.scrubbing = true; break;
            case 2: complete = true; break;
            case 3: seeking = true; break;
            case 4: g_pp_pb.seek_discarding = true; break;
            case 5: pb_prebuffer_hold = true; break;
        }
        assert(input(PadButtons::R3) && !highlight_calls && !state.seeks && !state.stops && !idle_retries);
        prompt = "Jump to highlight?";
        if (state.scrubbing) {
            assert(!input(PadButtons::Cross) && !input(PadButtons::Circle));
            assert(!confirm_calls && prompt);
            continue;
        }
        assert(input(PadButtons::Cross) && !confirm_calls && !state.seeks && prompt);
        assert(input(PadButtons::Circle) && confirm_calls == 1 && !state.seeks && !prompt);
    }
    reset(); assert(input(PadButtons::R3) && !state.seeks && notices == 1);
    reset(); m_showStatsForNerds = true; prompt = "Skip sponsor?";
    assert(!input(PadButtons::Cross) && !input(PadButtons::Circle) && !confirm_calls && prompt);
    reset(); next_target = 100; assert(input(PadButtons::R3) && !state.seeks && !state.stops);
    puts("PASS: shipping PS5 SponsorBlock tick/pad paths, skip/end decisions, navigation, silent/message actions and competing playback gates");
}
