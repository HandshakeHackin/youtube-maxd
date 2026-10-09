"""Checked decoder EOF and render-retirement changes for the pinned PS5 engine."""


def apply_video_drain_overlay(replace, player):
    ff = player + "media/src/evo_vdec_ffmpeg.c"
    native = player + "media/src/evo_vdec_native.c"
    playback = player + "media/src/evo_playback.c"
    pp = player + "pp/src/pp_playback.c"
    replace(ff, "    int              frame_idx;", "    int              frame_idx;\n    int tube_drained, tube_failed;")
    replace(ff, "    if (ret == 0)\n        return 0;\n    if (ret == AVERROR(EAGAIN))", "    if (ret == 0 || (flush && ret == AVERROR_EOF))\n        return 0;\n    if (ret == AVERROR(EAGAIN))")
    replace(ff, "    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)\n        return 0;", "    if (ret == AVERROR_EOF) { v->tube_drained = 1; return 0; }\n    if (ret == AVERROR(EAGAIN))\n        return 0;")
    replace(ff, "    v->stats.send_calls++;", "    if (r < 0) v->tube_failed = 1;\n    v->stats.send_calls++;")
    replace(ff, "    } else if (r < 0) {\n        v->stats.fatal_errors++;", "    } else if (r < 0) {\n        v->tube_failed = 1;\n        v->stats.fatal_errors++;")
    replace(ff, "void evo_vdec_flush(evo_vdec *v)", """int evo_vdec_is_drained(const evo_vdec *v)
{
    if (!v || v->tube_failed) return 0;
    return v->backend == EVO_VDEC_BACKEND_NATIVE
        ? evo_vdec_native_is_drained(v->nat) : v->tube_drained;
}

void evo_vdec_flush(evo_vdec *v)""")
    replace(ff, "    v->pending_us = 0;   /* #8:", "    v->tube_drained = v->tube_failed = 0;\n    v->pending_us = 0;   /* #8:")
    replace(player + "media/include/evo_vdec.h", "void evo_vdec_flush(evo_vdec *v);", "int evo_vdec_is_drained(const evo_vdec *v); /* explicit EOF, never starvation/error */\nvoid evo_vdec_flush(evo_vdec *v);")
    replace(native, "    int      flushing;", "    int      flushing;\n    int tube_drain_finished;\n    unsigned tube_drain_calls;")
    replace(native, "static void drain_decoder(evo_vdec_native *n)", "static void drain_decoder(evo_vdec_native *n)")
    replace(native, "    for (int guard = 0; guard < 64; guard++) {", """    if (n->fatal || n->tube_drain_finished) return;
    for (int guard = 0; guard < 64; guard++) {
        if (n->ro_count >= RO_SLOTS - 1) return; /* resume after receive frees slots */
        if (++n->tube_drain_calls > 256) { n->fatal = 1; return; }""")
    replace(native, """        if (frc != 0)
            break;
        if (!(out.valid && out.picture_count))
            break;""", """        if (frc != 0) { n->fatal = 1; return; }
        if (!(out.valid && out.picture_count)) {
            n->tube_drain_finished = 1;
            return;
        }""")
    replace(native, """        drain_decoder(v);
        v->flushing = 1;
        return 0;""", """        v->flushing = 1;
        drain_decoder(v);
        return v->fatal ? -1 : 0;""")
    replace(native, "            (void)av_bsf_send_packet(v->bsf, NULL);", "            int sent = av_bsf_send_packet(v->bsf, NULL);\n            if (sent < 0 && sent != AVERROR_EOF) { v->fatal = 1; return -1; }")
    replace(native, "                if (dr < 0) break;", "                if (dr < 0) { v->fatal = 1; return -1; }")
    replace(native, """            while (av_bsf_receive_packet(v->bsf, v->filt_pkt) == 0) {
                int64_t fp""", """            int received;
            while ((received = av_bsf_receive_packet(v->bsf, v->filt_pkt)) == 0) {
                int64_t fp""")
    replace(native, """                if (dr < 0) { v->fatal = 1; return -1; }
            }
        }
        v->flushing""", """                if (dr < 0) { v->fatal = 1; return -1; }
            }
            if (received != AVERROR_EOF) { v->fatal = 1; return -1; }
        }
        v->flushing""")
    replace(native, "    int threshold = v->flushing ? 0 : EVO_VDEC_REORDER_DEPTH;", """    if (v->flushing && !v->tube_drain_finished && !v->ro_count) {
        drain_decoder(v);
        if (v->fatal) return -1;
    }
    int threshold = v->flushing ? 0 : EVO_VDEC_REORDER_DEPTH;""")
    replace(native, "void evo_vdec_native_flush(evo_vdec_native *v)   /* seek */", """int evo_vdec_native_is_drained(const evo_vdec_native *v)
{
    return v && v->flushing && v->tube_drain_finished && !v->fatal && !v->ro_count;
}

void evo_vdec_native_flush(evo_vdec_native *v)   /* seek */""")
    replace(native, "    v->flushing       = 0;", "    v->flushing       = 0;\n    v->tube_drain_finished = 0; v->tube_drain_calls = 0;")
    replace(native, "void evo_vdec_native_flush(evo_vdec_native *v) { (void)v; }", "int evo_vdec_native_is_drained(const evo_vdec_native *v) { (void)v; return 0; }\nvoid evo_vdec_native_flush(evo_vdec_native *v) { (void)v; }")
    replace(player + "media/src/evo_vdec_native.h", "void evo_vdec_native_flush(evo_vdec_native *v);", "int evo_vdec_native_is_drained(const evo_vdec_native *v);\nvoid evo_vdec_native_flush(evo_vdec_native *v);")
    replace(playback, '#include "evo_playback.h"', '#include "evo_playback.h"\n#include <stdatomic.h>')
    replace(playback, "static int s_vdec_fatal_streak", "static _Atomic int tube_video_decoded;\nstatic int tube_eof_sent;\nvoid evo_pb_reset_video_drain(void) { atomic_store(&tube_video_decoded, 0); tube_eof_sent = 0; }\n\nstatic int s_vdec_fatal_streak")
    replace(playback, "    g_pb_decode_fatal   = 0;", "    evo_pb_reset_video_drain();\n    g_pb_decode_fatal   = 0;")
    replace(playback, "        if (recv_ret >= 1) {", "        if (recv_ret >= 1) {\n            atomic_store(&tube_video_decoded, 0);")
    replace(playback, """            static int s_eof_drained = 0;
            if (!video_decode_done) {
                s_eof_drained = 0;""", """            if (!video_decode_done) {
                tube_eof_sent = 0;
                atomic_store(&tube_video_decoded, 0);""")
    replace(playback, """            if (!s_eof_drained) {
                s_eof_drained = 1;
                evo_vdec_send(g_vdec, NULL, 0, INT64_MIN);
                continue;
            }
            return 0;""", """            if (!tube_eof_sent) {
                int sent = evo_vdec_send(g_vdec, NULL, 0, INT64_MIN);
                if (sent < 0) { atomic_store(&tube_video_decoded, 0); note_vdec_result(1); return 0; }
                if (!sent) tube_eof_sent = 1; /* EAGAIN must retry after receive */
                continue;
            }
            atomic_store_explicit(&tube_video_decoded,
                recv_ret == 0 && evo_vdec_is_drained(g_vdec) && evo_pb_input_complete(),
                memory_order_release);
            return 0;""")
    replace(playback, "int evo_pb_audio_drained(void) { return audio_stream_index < 0 || evo_audio_is_drained(); }", """int evo_pb_audio_drained(void) { return audio_stream_index < 0 || evo_audio_is_drained(); }
int evo_pb_is_complete(void)
{
    if (player_paused || pb_scrub_hold || evo_demux_is_seeking() ||
        !evo_pb_input_complete() || !evo_pb_audio_drained() || g_pb_decode_fatal) return 0;
    if (video_stream_index < 0) return audio_stream_index >= 0;
    if (!atomic_load_explicit(&tube_video_decoded, memory_order_acquire)) return 0;
    int64_t dwell = video_fps >= 0.1 && video_fps <= 240.0
        ? (int64_t)(1000000.0 / video_fps) : 40000;
    return pp_playback_display_retired(&g_pp_pb, dwell);
}""")
    replace(player + "media/include/evo_playback.h", "int    evo_pb_input_complete(void);", "void evo_pb_reset_video_drain(void);\nint evo_pb_is_complete(void); /* natural input EOF + audio + final rendered video */\nint    evo_pb_input_complete(void);")
    replace(player + "media/src/evo_demux.c", "        evo_audio_drain_reset();", "        evo_audio_drain_reset();\n        evo_pb_reset_video_drain();")
    # Tokens travel in the same locked snapshot as the actual decoder planes.
    replace(player + "pp/include/pp_playback.h", "    void *lock;", "    uint64_t tube_epoch, tube_published, tube_retired, tube_retired_us;\n    void *lock;")
    replace(player + "pp/include/pp_playback.h", "typedef struct pp_video_frame {", "typedef struct pp_video_frame {\n    uint64_t tube_epoch, tube_serial;\n    int64_t pts_us;")
    replace(player + "pp/include/pp_playback.h", "int pp_playback_has_display(const pp_playback *pb);", """int pp_playback_has_display(const pp_playback *pb);
void pp_playback_note_retired(pp_playback *pb, uint64_t epoch, uint64_t serial);
int pp_playback_display_retired(pp_playback *pb, int64_t dwell_us);""")
    replace(pp, "#include <unistd.h>", "#include <unistd.h>\n#include <time.h>")
    replace(pp, "void pp_playback_on_file_open(pp_playback *pb)", """static uint64_t tube_monotonic_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull;
}
static void tube_display_reset(pp_playback *pb)
{
    if (pb->lock) pthread_mutex_lock(mtx(pb));
    ++pb->tube_epoch;
    pb->tube_published = pb->tube_retired = pb->tube_retired_us = 0;
    if (pb->lock) pthread_mutex_unlock(mtx(pb));
}
void pp_playback_note_retired(pp_playback *pb, uint64_t epoch, uint64_t serial)
{
    if (!pb) return;
    if (pb->lock) pthread_mutex_lock(mtx(pb));
    if (pb->active && epoch == pb->tube_epoch && serial &&
        serial <= pb->tube_published && serial > pb->tube_retired) {
        pb->tube_retired = serial;
        pb->tube_retired_us = tube_monotonic_us();
    }
    if (pb->lock) pthread_mutex_unlock(mtx(pb));
}
int pp_playback_display_retired(pp_playback *pb, int64_t dwell_us)
{
    if (!pb || dwell_us < 0) return 0;
    if (pb->lock) pthread_mutex_lock(mtx(pb));
    uint64_t now = tube_monotonic_us();
    int done = pb->active && !pb->seek_discarding && pb->tube_published &&
        pb->tube_retired == pb->tube_published && pb->tube_retired_us &&
        now >= pb->tube_retired_us && now - pb->tube_retired_us >= (uint64_t)dwell_us;
    if (pb->lock) pthread_mutex_unlock(mtx(pb));
    return done;
}
void pp_playback_on_file_open(pp_playback *pb)""")
    replace(pp, "    pb->active = 1;", "    tube_display_reset(pb);\n    pb->active = 1;")
    replace(pp, "    pb->active = 0;", "    tube_display_reset(pb);\n    pb->active = 0;")
    replace(pp, "    if (success)\n        pb->stats.seek_successes++;", "    if (success) {\n        tube_display_reset(pb);\n        pb->stats.seek_successes++;\n    }")
    replace(pp, "    pb->stats.frames_published++;", "    ++pb->tube_published;\n    pb->stats.frames_published++;")
    replace(pp, "        f->y        = pb->gl_src_y;", "        f->tube_epoch = pb->tube_epoch; f->tube_serial = pb->tube_published;\n        f->pts_us = pb->display_pts_us;\n        f->y        = pb->gl_src_y;")
    replace(pp, "        f->y        = pb->hold_buf;", "        f->pts_us = pb->display_pts_us;\n        f->y        = pb->hold_buf;")
    # Only a real successful draw + retired GPU fence and VideoOut flip may ack.
    agc = player + "media/src/evo_agc_runtime.c"
    replace(agc, "void evo_agc_runtime_frame_end(void)", "static int tube_present_retired;\nint evo_agc_runtime_last_present_retired(void) { return tube_present_retired; }\n\nvoid evo_agc_runtime_frame_end(void)")
    replace(agc, "void evo_agc_runtime_frame_end(void)\n{", "void evo_agc_runtime_frame_end(void)\n{\n    tube_present_retired = 0;")
    replace(agc, "                agc_mode_switch_presented();", "                tube_present_retired = fwaits < 120u && sp_rc == 0 &&\n                    *g_agc_dev.fences[slot] == marker;\n                agc_mode_switch_presented();")
    replace(player + "media/include/evo_agc_runtime.h", "void evo_agc_runtime_present(void);", "void evo_agc_runtime_present(void);\nint evo_agc_runtime_last_present_retired(void);")
    app = player + "core/src/Application.cpp"
    replace(app, "        bool swap = false;", "        bool swap = false;\n        uint64_t tube_frame_epoch = 0, tube_frame_serial = 0;\n        static uint64_t tube_last_epoch = 0, tube_last_serial = 0;")
    replace(app, "            s_last_pts = -1;", "            s_last_pts = -1;\n            tube_last_epoch = tube_last_serial = 0;")
    replace(app, "            int64_t current_pts = g_pp_pb.display_pts_us;\n            bool new_frame = (current_pts != s_last_pts);", "            int64_t current_pts = f.pts_us;\n            bool new_frame = have && (f.tube_epoch != tube_last_epoch || f.tube_serial != tube_last_serial);")
    replace(app, "                evo_agc_blit_yuv(f.y, f.y_pitch, f.uv, f.uv_pitch,", "                int tube_blit_result = evo_agc_blit_yuv(f.y, f.y_pitch, f.uv, f.uv_pitch,")
    replace(app, "                                 is_direct, current_pts, f.dovi);", """                                 is_direct, current_pts, f.dovi);
                if (tube_blit_result == 0 && !f.held) {
                    tube_frame_epoch = f.tube_epoch; tube_frame_serial = f.tube_serial;
                }""")
    replace(app, "            evo_agc_runtime_present();", """            evo_agc_runtime_present();
            if (tube_frame_serial && evo_agc_runtime_last_present_retired()) {
                pp_playback_note_retired(&g_pp_pb, tube_frame_epoch, tube_frame_serial);
                tube_last_epoch = tube_frame_epoch; tube_last_serial = tube_frame_serial;
            }""")
    replace(player + "core/src/screens/PlayerScreen.cpp", """    const bool ended = (evo_pb_input_complete() != 0) && evo_pb_audio_drained() &&
        tube_playback_at_end(playback->getPositionSeconds(), playback->getDurationSeconds());
    if (ended && tube_finish_and_next(playback->getPositionSeconds(), playback->getDurationSeconds()))""", """    const bool ended = evo_pb_is_complete() != 0;
    if (ended && tube_finish_drained_and_next(playback->getPositionSeconds()))""")
    remote = player + "src/evo_usb_remote.c"
    replace(remote, '"fatal=%d eof=%d active=%d parked=%d\\n",', '"fatal=%d eof=%d complete=%d active=%d parked=%d\\n",')
    replace(remote, "evo_pb_decode_fatal(), evo_pb_is_eof(), evo_pb_is_active(), parked);", "evo_pb_decode_fatal(), evo_pb_is_eof(), evo_pb_is_complete(), evo_pb_is_active(), parked);")
