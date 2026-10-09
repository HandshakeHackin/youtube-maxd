/* Actual EVO decoder, decode loop, and locked presentation snapshot.
 * Hardware draw/flip retirement is injected at the pp acknowledgment boundary.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <assert.h>
#include <stdio.h>
#include "evo_playback.c"
#include "evo_dovi.h"

int player_paused, screen = 2, playback_profile = 2;
double media_duration_sec, resume_base_offset_seconds;
volatile int pb_prebuffer_hold, pb_scrub_hold;
struct SwsContext *play_sws;
AVFormatContext *play_fmt;
evo_vdec *g_vdec;
pp_playback g_pp_pb;
int g_first_frame_bc_done, dbg_video_frames, dbg_video_thread_alive, dbg_swaps, perf_decode_frames;
long long dbg_last_pts;
int audio_stream_index = -1, video_stream_index = -1, audio_handle = -1;
volatile double audio_clock_seconds;
volatile long long audio_samples_played;
volatile int audio_queue_count;
PacketQueue video_packet_queue;
static AVPacket *packets[1024];
static unsigned packet_count, packet_read;
static int natural_eof = 1, audio_drained = 1, seeking;

AVPacket *packet_queue_pop(PacketQueue *q) {
    assert(q == &video_packet_queue);
    return packet_read < packet_count ? packets[packet_read++] : NULL;
}
int evo_demux_natural_eof(void) { return natural_eof; }
int evo_demux_is_seeking(void) { return seeking; }
int evo_audio_is_drained(void) { return audio_drained; }
long long now_ms(void) { return 0; }
void toast(const char *title, const char *message) { (void)title; (void)message; }
pp_aspect_mode prospero_view_mode_to_aspect(void) { return PP_ASPECT_FIT; }
void evo_boot_log(const char *format, ...) { (void)format; }
void pp_stage_bc(const char *stage, const char *detail) { (void)stage; (void)detail; }
void pp_stage_bc_checkpoint(const char *stage, const char *detail) { (void)stage; (void)detail; }
/* HDR/Dolby Vision is outside this AVC/VP9 test; no fabricated metadata. */
int evo_dovi_stream_needs_reshape(const void *params) { (void)params; return 0; }
int evo_dovi_params_from_av(const void *metadata, evo_dovi_params *out)
{ (void)metadata; (void)out; assert(0); return -1; }
void *evo_direct_mem_alloc(size_t bytes) { (void)bytes; assert(0); return NULL; }
void evo_direct_mem_free(void *ptr) { (void)ptr; assert(0); }

static void load(const char *path) {
    assert(avformat_open_input(&play_fmt, path, NULL, NULL) == 0);
    assert(avformat_find_stream_info(play_fmt, NULL) >= 0);
    video_stream_index = av_find_best_stream(play_fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    assert(video_stream_index >= 0);
    AVCodecParameters *params = play_fmt->streams[video_stream_index]->codecpar;
    evo_vdec_open_params p = {.backend = EVO_VDEC_BACKEND_FFMPEG,
        .codec_id = params->codec_id, .width = params->width, .height = params->height,
        .avctx_params = params, .thread_count = 2, .thread_type = FF_THREAD_FRAME,
        .skip_loop_filter = EVO_VDEC_KEEP, .skip_frame = EVO_VDEC_KEEP, .skip_idct = EVO_VDEC_KEEP};
    g_vdec = evo_vdec_open(&p, NULL); assert(g_vdec);
    AVPacket *pkt = av_packet_alloc(); assert(pkt);
    while (av_read_frame(play_fmt, pkt) >= 0) {
        if (pkt->stream_index == video_stream_index) {
            assert(packet_count < 1024);
            packets[packet_count++] = av_packet_clone(pkt); assert(packets[packet_count - 1]);
        }
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    pp_playback_init(&g_pp_pb); assert(g_pp_pb.lock);
    pp_playback_on_file_open(&g_pp_pb);
    video_decode_ready = video_decode_done = 1;
    video_fps = av_q2d(play_fmt->streams[video_stream_index]->avg_frame_rate);
    evo_pb_reset_decode_fatal();
}

static void run(const char *path) {
    load(path);
    assert(!evo_vdec_is_drained(g_vdec) && !evo_pb_is_complete());
    /* Empty non-EOF input is starvation, never completion. */
    unsigned saved_count = packet_count; packet_count = 0;
    video_decode_done = 0;
    assert(decode_next_video_frame() == 1 && !evo_vdec_is_drained(g_vdec));
    packet_count = saved_count; video_decode_done = 1;
    pp_video_frame previous = {0}, last = {0};
    unsigned emitted = 0;
    for (unsigned guard = 0; guard < 2048; ++guard) {
        int more = decode_next_video_frame();
        pp_video_frame current;
        if (pp_playback_get_video_frame(&g_pp_pb, &current) && current.tube_serial != last.tube_serial) {
            if (previous.tube_serial)
                pp_playback_note_retired(&g_pp_pb, previous.tube_epoch, previous.tube_serial);
            previous = last; last = current; ++emitted;
        }
        if (!more) break;
        assert(guard < 2047);
    }
    assert(packet_read == packet_count && emitted > 2 && evo_vdec_is_drained(g_vdec));
    assert(atomic_load(&tube_video_decoded));
    assert(!evo_pb_is_complete()); // Last frame has not reached the injected renderer boundary.
    pp_playback_note_retired(&g_pp_pb, last.tube_epoch, last.tube_serial);
    assert(!evo_pb_is_complete()); // Retired final frame still needs its display interval.
    usleep((useconds_t)(1000000.0 / video_fps) + 10000);
    assert(evo_pb_is_complete()); // media_duration_sec remains 0: unknown duration works.
    natural_eof = 0; assert(!evo_pb_is_complete()); natural_eof = 1;
    audio_stream_index = 0; audio_drained = 0; assert(!evo_pb_is_complete());
    audio_drained = 1; assert(evo_pb_is_complete()); audio_stream_index = -1;
    player_paused = 1; assert(!evo_pb_is_complete()); player_paused = 0;
    pp_playback_pause(&g_pp_pb);
    pp_video_frame held;
    assert(pp_playback_get_video_frame(&g_pp_pb, &held) && held.held);
    assert(held.pts_us == last.pts_us && held.tube_serial == 0);
    pp_playback_resume(&g_pp_pb);
    seeking = 1; assert(!evo_pb_is_complete()); seeking = 0;
    g_pb_decode_fatal = 1; assert(!evo_pb_is_complete()); g_pb_decode_fatal = 0;
    pp_frame unpublishable = {.pts_us = last.pts_us, .format = PP_FRAME_YUV420P, .width = 2, .height = 2};
    present_pp_frame(&unpublishable); // Real pp path rejects missing pixel planes.
    assert(atomic_load(&tube_video_failed) && !evo_pb_is_complete());
    evo_pb_reset_video_drain();
    assert(decode_next_video_frame() == 0 && evo_pb_is_complete()); // Re-send NULL and collect EOF.
    const uint8_t invalid_after_eof[] = {0, 0, 0};
    assert(evo_vdec_send(g_vdec, invalid_after_eof, sizeof(invalid_after_eof), 0) < 0);
    assert(!evo_vdec_is_drained(g_vdec)); // Real AVERROR_EOF on an invalid post-EOF send is a failure.
    assert(decode_next_video_frame() == 0 && !evo_pb_is_complete());
    // A successful seek invalidates decoder EOF and all old renderer tokens.
    evo_vdec_flush(g_vdec); evo_pb_reset_video_drain();
    pp_playback_notify_seek_begin(&g_pp_pb, 0);
    pp_playback_notify_seek_end(&g_pp_pb, 1, 0, 0);
    pp_playback_note_retired(&g_pp_pb, last.tube_epoch, last.tube_serial);
    assert(!evo_vdec_is_drained(g_vdec) && !pp_playback_display_retired(&g_pp_pb, 0));
    assert(!evo_pb_is_complete());
    // Reopen also rejects stale acknowledgments and fabricated future serials.
    pp_playback_on_file_close(&g_pp_pb); pp_playback_on_file_open(&g_pp_pb);
    pp_playback_note_retired(&g_pp_pb, last.tube_epoch, last.tube_serial);
    pp_playback_note_retired(&g_pp_pb, g_pp_pb.tube_epoch, UINT64_MAX);
    assert(!pp_playback_display_retired(&g_pp_pb, 0));
    pp_playback_on_file_close(&g_pp_pb); pp_playback_shutdown(&g_pp_pb);
    evo_vdec_close(g_vdec); g_vdec = NULL;
    avformat_close_input(&play_fmt);
    packet_count = packet_read = 0;
    first_video_pts_seconds = -1; video_clock_seconds = 0; g_first_frame_bc_done = 0;
}

int main(int argc, char **argv) {
    assert(argc == 3);
    run(argv[1]); run(argv[2]);
    puts("PASS: actual AVC/VP9 decode loop, explicit EOF, final-frame retirement, unknown duration and stale seek/reopen tokens");
    return 0;
}
