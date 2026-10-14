#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <initializer_list>
#include <pthread.h>
#include <string>
using std::isfinite;

static void *play_fmt = reinterpret_cast<void *>(1);
static int video_stream_index = 0, audio_stream_index = 1;
static double media_duration_sec = 100;
static int player_paused, prospero_seek_pending, prospero_seek_restore_paused;
static double prospero_seek_target_seconds;
static long long controls_last_used_ms;
static pthread_mutex_t prospero_seek_mutex = PTHREAD_MUTEX_INITIALIZER;
static long long now_ms() { return 1000; }
static double resume_base_offset_seconds = 11.5;
static int g_pp_pb, begins, resumes, request_calls;
static bool hold, complete_before_return;
static bool live_bound, live_admit=true;
static int tube_live_context_matches(void*) {return live_bound;}
static int tube_live_seek_admit(void*,double target) {return !live_bound || (live_admit && target>=30 && target<120);}
static void pp_playback_notify_seek_begin(int *, int64_t) { ++begins; hold = true; }
static void pp_playback_resume(int *) { ++resumes; }
static void evo_boot_log(const char *, ...) {}
static int completion_seeks;
static void tube_completion_seek_started(double) { ++completion_seeks; }

#define prospero_request_inplace_seek shipping_request
#include "seek_request.inc"
#undef prospero_request_inplace_seek

static int prospero_request_inplace_seek(double target, int paused) {
    ++request_calls;
    int accepted = shipping_request(target, paused);
    if (accepted && complete_before_return) {
        // A demux seek may complete before the UI publishing call returns.
        pp_playback_notify_seek_begin(&g_pp_pb, static_cast<int64_t>(prospero_seek_target_seconds * 1000000));
        resume_base_offset_seconds = prospero_seek_target_seconds;
        hold = false; prospero_seek_pending = 0;
    }
    return accepted;
}

class PlaybackController {
public:
    struct { std::string provider="youtube"; } m_source;
    bool active = true;
    double m_resumeBaseOffset = 11.5;
    bool isActive() const { return active; }
    void seekTo(double);
    bool seekTo(double, bool);
};
#include "seek_ui.inc"

static void reset() {
    play_fmt = reinterpret_cast<void *>(1); video_stream_index = 0; audio_stream_index = 1;
    media_duration_sec = 100; player_paused = prospero_seek_pending = 0;
    controls_last_used_ms = 777;
    resume_base_offset_seconds = 11.5; begins = resumes = request_calls = 0;
    hold = complete_before_return = false;
    completion_seeks=0;live_bound=false;live_admit=true;
}

int main() {
    PlaybackController ui;
    reset(); play_fmt = nullptr;
    ui.seekTo(45.25);
    assert(request_calls == 1 && !prospero_seek_pending && resumes == 1);
    assert(!completion_seeks);
    assert(resume_base_offset_seconds == 11.5 && ui.m_resumeBaseOffset == 11.5 && !hold && begins == 0);

    reset(); ui.seekTo(45.25);
    assert(completion_seeks==1);
    assert(prospero_seek_pending && prospero_seek_target_seconds == 45.25 && player_paused);
    assert(resume_base_offset_seconds == 11.5 && ui.m_resumeBaseOffset == 11.5 && begins == 0);
    // A later rejected av_seek_frame leaves the previous base untouched.
    prospero_seek_pending = 0;
    assert(resume_base_offset_seconds == 11.5);

    reset(); complete_before_return = true; media_duration_sec = 10;
    ui.seekTo(10);
    assert(resume_base_offset_seconds == 9 && begins == 1 && !hold && !prospero_seek_pending);
    assert(ui.m_resumeBaseOffset == 11.5); // startup bookkeeping is not seek state

    reset();live_bound=true;ui.seekTo(110);
    assert(prospero_seek_pending && prospero_seek_target_seconds==110 && !player_paused); // no stale VOD clamp or preflight pause
    reset();live_bound=true;live_admit=false;ui.seekTo(50);
    assert(!prospero_seek_pending && !player_paused && resumes==1 && !begins);
    for (double target : {-1.0, std::numeric_limits<double>::quiet_NaN(),
                          std::numeric_limits<double>::infinity(), 1e300}) {
        reset(); ui.seekTo(target);
        assert(request_calls == 0 && !player_paused && !prospero_seek_pending && begins == 0);
        assert(resume_base_offset_seconds == 11.5);
    }
    for (double target : {std::numeric_limits<double>::quiet_NaN(),
                          std::numeric_limits<double>::infinity(), 1e300}) {
        reset(); assert(!shipping_request(target, 0));
        assert(!player_paused && !prospero_seek_pending && controls_last_used_ms == 777);
    }
    reset(); ui.active = false; ui.seekTo(20);
    assert(!request_calls && !prospero_seek_pending);
    return 0;
}
