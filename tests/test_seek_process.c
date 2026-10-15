/* Shipping demux seek request/process, real FFmpeg seeking and pp hold/clock.
 * Decoder/output workers are parked; their flush boundaries are counted.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
#include "tube_seek_timeline.h"
#include "pp_playback.h"
static AVFormatContext *play_fmt;
static AVCodecContext *audio_ctx, *prospero_embedded_subtitle_ctx;
static int video_stream_index = -1, audio_stream_index = -1;
static int player_paused, prospero_seek_restore_paused;
static _Atomic int prospero_seek_pending, prospero_seek_in_progress;
static double prospero_seek_target_seconds, media_duration_sec, resume_base_offset_seconds;
static double resume_base_anchor_pending = -1, audio_seek_discard_until = -1;
static double audio_clock_seconds, audio_pts_seconds, video_clock_seconds;
static double first_audio_pts_seconds, first_video_pts_seconds;
static long long controls_last_used_ms, audio_samples_played, audio_samples_decoded;
static int video_decode_hold, video_thread_running, video_decode_parked = 1, audio_decode_thread_running;
static int audio_parked = 1;
static int video_decode_done, video_decode_ready, dbg_read_fail;
static int video_packet_queue, audio_packet_queue, audio_queue_count, audio_queue_read, audio_queue_write, audio_accum_pos;
static AVPacket *video_pending_pkt, *video_video_pending_pkt;
static void *g_vdec, *g_adec;
static pthread_t audio_decode_thread;
static pthread_mutex_t prospero_seek_mutex = PTHREAD_MUTEX_INITIALIZER;
static pp_playback g_pp_pb;
static int refuse_seek, seek_calls, flush_calls;
static int smooth_reset_calls, drain_reset_calls, smooth_clock_valid;
static int smooth_before_process, drain_before_process, flush_before_process;
static int live_bound, live_prepare_ok=1;
static int tube_live_context_matches(AVFormatContext *f) {(void)f;return live_bound;}
static int tube_live_seek_admit(AVFormatContext *f,double target) {(void)f;(void)target;return 1;}
static int tube_live_seek_prepare(AVFormatContext *f,double target) {(void)f;(void)target;return live_prepare_ok;}
static int check_busy_clock;
static int64_t seen_timestamp;
long long now_ms(void) {
    if (check_busy_clock) assert(prospero_seek_in_progress);
    return av_gettime_relative() / 1000;
}
void pp_stage_bc(const char *stage, const char *detail) { (void)stage; (void)detail; }
void pp_stage_bc_checkpoint(const char *stage, const char *detail) { (void)stage; (void)detail; }
void evo_boot_log(const char *format, ...) { (void)format; }
static void toast(const char *title, const char *message) { (void)title; (void)message; }
static void packet_queue_clear(int *queue) { (void)queue; }
static void evo_vdec_flush(void *decoder) { (void)decoder; ++flush_calls; }
static void evo_adec_flush(void *decoder) { (void)decoder; }
static int evo_audio_decode_is_parked(void) { return audio_parked; }
static void evo_audio_drain_reset(void) {
    assert(prospero_seek_in_progress && video_decode_hold && player_paused);
    ++drain_reset_calls;
}
static void evo_audio_clock_smooth_reset(void) {
    /* The real helper invalidates its interpolator and resets these two public
     * counters. Verify the demux calls it after drain/decoder reset while the
     * seek still owns the parked workers, never after their release. */
    assert(prospero_seek_in_progress && video_decode_hold && player_paused);
    assert(!video_thread_running || video_decode_parked);
    assert(!audio_decode_thread_running || audio_parked);
    assert(flush_calls == flush_before_process + 1);
    assert(drain_reset_calls == drain_before_process + 1);
    assert(smooth_reset_calls == smooth_before_process);
    assert(resume_base_offset_seconds == prospero_seek_target_seconds);
    assert(audio_seek_discard_until ==
        (audio_stream_index >= 0 ? prospero_seek_target_seconds : -1.0));
    ++smooth_reset_calls;
    smooth_clock_valid = 0;
    audio_samples_played = 0;
    audio_clock_seconds = 0.0;
}
static void evo_pb_reset_video_drain(void) {}
static void evo_demux_reset_eof(void) {}
static void prospero_audio_resampler_reset(void) {}
static void prospero_embedded_subtitle_reset(void) {}
static void *audio_decode_thread_func(void *arg) { (void)arg; return NULL; }
static int seek(AVFormatContext *format, int stream, int64_t timestamp, int flags) {
    ++seek_calls; seen_timestamp = timestamp;
    return refuse_seek ? AVERROR(EIO) : av_seek_frame(format, stream, timestamp, flags);
}
#define av_seek_frame seek
#include "seek_request.inc"
#include "seek_process.inc"
#undef av_seek_frame
static int process_seek_checked(void) {
    smooth_before_process = smooth_reset_calls;
    drain_before_process = drain_reset_calls;
    flush_before_process = flush_calls;
    smooth_clock_valid = 1;
    audio_samples_played = 123456;
    audio_clock_seconds = 2.572;
    check_busy_clock = 1;
    int result = prospero_process_seek_request();
    check_busy_clock = 0;
    assert(!prospero_seek_in_progress);
    if (result) {
        assert(smooth_reset_calls == smooth_before_process + 1);
        assert(!smooth_clock_valid && audio_samples_played == 0 && audio_clock_seconds == 0.0);
    } else {
        /* Refused seek, invalid timeline and busy-worker rejection must retain
         * the prior clock; no successful timeline change occurred. */
        assert(smooth_reset_calls == smooth_before_process);
        assert(smooth_clock_valid && audio_samples_played == 123456 && audio_clock_seconds == 2.572);
    }
    return result;
}

static void open_source(const char *path) {
    assert(avformat_open_input(&play_fmt, path, NULL, NULL) == 0);
    assert(avformat_find_stream_info(play_fmt, NULL) >= 0);
    video_stream_index = av_find_best_stream(play_fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    audio_stream_index = av_find_best_stream(play_fmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    media_duration_sec = (double)play_fmt->duration / AV_TIME_BASE;
    pp_playback_init(&g_pp_pb); assert(g_pp_pb.lock);
    pp_playback_on_file_open(&g_pp_pb);
    resume_base_offset_seconds = 1.75;
    seek_calls = flush_calls = refuse_seek = 0;
}
static void close_source(void) {
    pp_playback_shutdown(&g_pp_pb); avformat_close_input(&play_fmt);
}

static void audio_only(const char *path) {
    open_source(path); assert(audio_stream_index >= 0 && video_stream_index < 0);
    assert(prospero_request_inplace_seek(4.250001, 0));
    assert(process_seek_checked());
    assert(!g_pp_pb.seek_discarding && !pp_clock_is_paused(&g_pp_pb.clock));
    assert(g_pp_pb.stats.seek_requests == 0 && g_pp_pb.stats.seek_successes == 0);
    assert(audio_seek_discard_until == 4.250001 && resume_base_offset_seconds == 4.250001);
    assert(!player_paused && !prospero_seek_in_progress && !video_decode_hold);
    int64_t absolute, timestamp;
    assert(tube_seek_times(play_fmt, audio_stream_index, 4.250001, &absolute, &timestamp));
    assert(seen_timestamp == timestamp && seek_calls == 1 && flush_calls == 1);
    assert(prospero_request_inplace_seek(.001, 1) && process_seek_checked());
    assert(audio_seek_discard_until == .001 && player_paused && !g_pp_pb.seek_discarding);
    assert(prospero_request_inplace_seek(0, 0) && process_seek_checked());
    assert(audio_seek_discard_until == 0 && !player_paused && !g_pp_pb.seek_discarding);
    const double base = resume_base_offset_seconds;
    live_bound=1;live_prepare_ok=0;seek_calls=flush_calls=0;
    assert(prospero_request_inplace_seek(2,0));
    assert(!player_paused && !process_seek_checked());
    assert(!seek_calls && !flush_calls && !video_decode_hold && !g_pp_pb.seek_discarding);
    assert(!player_paused && resume_base_offset_seconds==base);
    live_bound=0;live_prepare_ok=1;
    refuse_seek = 1;
    assert(prospero_request_inplace_seek(2, 0) && !process_seek_checked());
    assert(resume_base_offset_seconds == base && !g_pp_pb.seek_discarding && !player_paused);
    pp_clock_start(&g_pp_pb.clock, 0);
    pp_playback_pause(&g_pp_pb);
    assert(pp_clock_is_paused(&g_pp_pb.clock));
    assert(prospero_request_inplace_seek(2, 1) && !process_seek_checked());
    assert(resume_base_offset_seconds == base && player_paused && pp_clock_is_paused(&g_pp_pb.clock));
    close_source();
}
static void video_hold(const char *path) {
    open_source(path); assert(video_stream_index >= 0 && audio_stream_index >= 0);
    assert(prospero_request_inplace_seek(4.25, 0) && process_seek_checked());
    int64_t absolute, timestamp;
    assert(tube_seek_times(play_fmt, video_stream_index, 4.25, &absolute, &timestamp));
    assert(g_pp_pb.seek_discarding && g_pp_pb.seek_target_us == absolute);
    assert(seen_timestamp == timestamp && audio_seek_discard_until == 4.25);
    pp_frame frame = {.pts_us = absolute - 1};
    assert(pp_playback_push_frame(&g_pp_pb, &frame) == 1 && g_pp_pb.seek_discarding);
    uint8_t y[4] = {0}, uv[2] = {128, 128};
    frame = (pp_frame){.format = PP_FRAME_NV12, .width = 2, .height = 2,
        .planes = {y, uv}, .strides = {2, 2}, .pts_us = absolute};
    assert(pp_playback_push_frame(&g_pp_pb, &frame) == 0 && !g_pp_pb.seek_discarding);
    const double base = resume_base_offset_seconds;
    refuse_seek = 1;
    assert(prospero_request_inplace_seek(1, 0) && !process_seek_checked());
    assert(resume_base_offset_seconds == base && !g_pp_pb.seek_discarding);
    assert(!pp_clock_is_paused(&g_pp_pb.clock) && !player_paused);
    pp_playback_pause(&g_pp_pb);
    assert(pp_clock_is_paused(&g_pp_pb.clock));
    assert(prospero_request_inplace_seek(1, 1) && !process_seek_checked());
    assert(resume_base_offset_seconds == base && player_paused && pp_clock_is_paused(&g_pp_pb.clock));
    // Invalid timeline conversion restores pause before releasing ownership.
    AVRational saved_time_base = play_fmt->streams[video_stream_index]->time_base;
    play_fmt->streams[video_stream_index]->time_base = (AVRational){0, 1};
    seek_calls = flush_calls = 0;
    assert(prospero_request_inplace_seek(1, 1) && !process_seek_checked());
    assert(!seek_calls && !flush_calls && player_paused && pp_clock_is_paused(&g_pp_pb.clock));
    play_fmt->streams[video_stream_index]->time_base = saved_time_base;
    // Busy-worker refusal touches no decoder; restored pause remains authoritative.
    audio_decode_thread_running = 1; audio_parked = 0;
    assert(prospero_request_inplace_seek(1, 1) && !process_seek_checked());
    assert(!seek_calls && !flush_calls && !video_decode_hold && player_paused && pp_clock_is_paused(&g_pp_pb.clock));
    audio_decode_thread_running = 0; audio_parked = 1;
    close_source();
}
int main(int argc, char **argv) {
    assert(argc == 4); audio_only(argv[1]); audio_only(argv[2]); video_hold(argv[3]);
    puts("PASS: shipping demux seeks, audio-only hold release, sub-50ms/zero gates, nonzero origins, real pp video discard, ordered smooth-clock reset and failed-seek clock retention");
}
