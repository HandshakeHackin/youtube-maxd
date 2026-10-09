"""Checked PS5 EVO audio tail/drain overlay; generated trees are not sources."""


def apply_audio_drain_overlay(replace, player):
    source = player + "media/src/evo_audio_out.c"
    replace(source, '#include "evo_audio_out.h"', '#include "evo_audio_out.h"\n#include "tube_audio_drain.h"\n#include "evo_demux.h"\n#include "evo_playback.h"')
    replace(source, "static int16_t audio_accum[2048 * EVO_AUDIO_MAX_CH];", """static int16_t audio_accum[2048 * EVO_AUDIO_MAX_CH];
static tube_audio_drain tube_drain;
static _Atomic int tube_decode_parked = 1;
static int audio_queue_samples[AUDIO_QUEUE_BLOCKS];
void evo_audio_drain_reset(void) { tube_audio_reset(&tube_drain); }
int evo_audio_is_drained(void) { return tube_audio_drained(&tube_drain); }
int evo_audio_decode_is_parked(void) { return atomic_load_explicit(&tube_decode_parked, memory_order_acquire); }""")
    replace(source, "static void audio_queue_push(int16_t *buf) {", "static int audio_queue_push_counted(int16_t *buf, int samples) {")
    replace(source, "    if (audio_queue_count >= AUDIO_QUEUE_BLOCKS)\n        return;", """    if (audio_queue_count >= AUDIO_QUEUE_BLOCKS) {
        tube_audio_mark(&tube_drain, tube_audio_epoch(&tube_drain), TUBE_AUDIO_FAILED);
        return 0;
    }""")
    replace(source, "    audio_queue_write = (audio_queue_write + 1) % AUDIO_QUEUE_BLOCKS;", "    audio_queue_samples[audio_queue_write] = samples;\n    audio_queue_write = (audio_queue_write + 1) % AUDIO_QUEUE_BLOCKS;")
    replace(source, "    audio_queue_count++;\n}", """    audio_queue_count++;
    return 1;
}
static void audio_queue_push(int16_t *buf) {
    (void)audio_queue_push_counted(buf, AUDIO_BLOCK_SAMPLES);
}""")
    replace(source, "    int    video_stuck_iters = 0;", """    int    video_stuck_iters = 0;
    uint64_t drain_epoch = tube_audio_epoch(&tube_drain);
    unsigned drain_silence = 0;""")
    replace(source, "    while (audio_thread_running) {", """    while (audio_thread_running) {
        const uint64_t epoch = tube_audio_epoch(&tube_drain);
        if (epoch != drain_epoch) { drain_epoch = epoch; drain_silence = 0; }""")
    replace(source, "if ((g_pp_pb.active && g_pp_pb.seek_discarding) || pb_scrub_hold)", "if ((g_pp_pb.active && g_pp_pb.seek_discarding) || pb_scrub_hold || evo_demux_is_seeking())")
    replace(source, """                sceAudioOutOutput(audio_handle, audio_queue[audio_queue_read]);
                audio_samples_played += AUDIO_BLOCK_SAMPLES;""", """                drain_silence = 0;
                const int submitted = sceAudioOutOutput(audio_handle, audio_queue[audio_queue_read]);
                if (submitted < 0) tube_audio_mark(&tube_drain, drain_epoch, TUBE_AUDIO_FAILED);
                else audio_samples_played += audio_queue_samples[audio_queue_read];""")
    replace(source, "                sceAudioOutOutput(audio_handle, silence);", """                const int submitted = sceAudioOutOutput(audio_handle, silence);
                if (submitted < 0) {
                    tube_audio_mark(&tube_drain, drain_epoch, TUBE_AUDIO_FAILED);
                    drain_silence = 0;
                } else if (video_decode_done &&
                           (tube_audio_flags(&tube_drain, drain_epoch) & TUBE_AUDIO_DECODED)) {
                    /* AudioOut is double-buffered. Two successful silence
                     * submissions after the final PCM block retire that tail.
                     * Silence and padding never advance the media clock. */
                    if (drain_silence < 2) ++drain_silence;
                    if (drain_silence == 2)
                        tube_audio_mark(&tube_drain, drain_epoch, TUBE_AUDIO_OUTPUT);
                } else drain_silence = 0;""")
    # Share the same accumulator path with normal resampling and its EOF tail.
    # Keep the original mixing code intact; tail flushing uses bounded chunks
    # with identical sample accounting and counted final padding.
    replace(source, "void *audio_decode_thread_func(void *arg) {", """static int tube_audio_flush_tail(AVFrame *af, uint64_t epoch) {
    if (!audio_decode_thread_running || tube_audio_epoch(&tube_drain) != epoch || evo_demux_is_seeking()) return 2;
    /* libSceAudiodec's AU call is synchronous; its produced PCM was already
     * mixed. FFmpeg may retain decoded frames until a NULL packet is sent. */
    if (!(g_adec && evo_adec_active(g_adec) == EVO_ADEC_BACKEND_NATIVE) && audio_ctx) {
        unsigned pending = 0;
        for (;;) {
            int sent = avcodec_send_packet(audio_ctx, NULL);
            if (sent == AVERROR(EAGAIN)) {
                int received = avcodec_receive_frame(audio_ctx, af);
                if (received < 0 || ++pending > 256) return 0;
                if (!audio_decode_thread_running || tube_audio_epoch(&tube_drain) != epoch || evo_demux_is_seeking()) return 2;
                mix_audio_frame_to_queue(af); av_frame_unref(af);
                continue;
            }
            if (sent < 0 && sent != AVERROR_EOF) return 0;
            break;
        }
        unsigned frames = 0;
        for (;;) {
            int received = avcodec_receive_frame(audio_ctx, af);
            if (received == AVERROR_EOF) break;
            if (received < 0 || ++frames > 256) return 0;
            if (!audio_decode_thread_running || tube_audio_epoch(&tube_drain) != epoch || evo_demux_is_seeking()) return 2;
            mix_audio_frame_to_queue(af);
            av_frame_unref(af);
        }
    }
    SwrContext *resampler = prospero_audio_resampler_ctx();
    int16_t tail[AUDIO_BLOCK_SAMPLES * EVO_AUDIO_MAX_CH];
    const int ch = evo_audio_channels;
    if (ch <= 0 || ch > EVO_AUDIO_MAX_CH) return 0;
    unsigned iterations = 0;
    while (resampler) {
        if (!audio_decode_thread_running || tube_audio_epoch(&tube_drain) != epoch || evo_demux_is_seeking()) return 2;
        if (++iterations > 256) return 0;
        uint8_t *planes[] = {(uint8_t *)tail};
        int count = swr_convert(resampler, planes, AUDIO_BLOCK_SAMPLES, NULL, 0);
        if (count < 0) return 0;
        if (!count) break;
        int offset = 0;
        while (offset < count) {
            int run = AUDIO_BLOCK_SAMPLES - audio_accum_pos;
            if (run > count - offset) run = count - offset;
            memcpy(audio_accum + audio_accum_pos * ch, tail + offset * ch,
                   (size_t)run * ch * sizeof(int16_t));
            audio_accum_pos += run; audio_samples_decoded += run; offset += run;
            if (audio_accum_pos == AUDIO_BLOCK_SAMPLES) {
                if (!audio_queue_push_counted(audio_accum, AUDIO_BLOCK_SAMPLES)) return 0;
                audio_accum_pos = 0;
            }
        }
    }
    if (audio_accum_pos) {
        int samples = audio_accum_pos;
        memset(audio_accum + samples * ch, 0,
               (size_t)(AUDIO_BLOCK_SAMPLES - samples) * ch * sizeof(int16_t));
        if (!audio_queue_push_counted(audio_accum, samples)) return 0;
        audio_accum_pos = 0;
    }
    return !(tube_audio_flags(&tube_drain, epoch) & TUBE_AUDIO_FAILED);
}

void *audio_decode_thread_func(void *arg) {""")
    replace(source, """        if (!pkt) {
            /*
             * Do not terminate at EOF.""", """        if (!pkt) {
            uint64_t epoch = tube_audio_epoch(&tube_drain);
            unsigned flags = tube_audio_flags(&tube_drain, epoch);
            if (video_decode_done && !evo_demux_is_seeking() &&
                !(flags & (TUBE_AUDIO_DECODED | TUBE_AUDIO_FAILED))) {
                int drained = tube_audio_flush_tail(af, epoch);
                if (drained == 1)
                    tube_audio_mark(&tube_drain, epoch, TUBE_AUDIO_DECODED);
                else if (drained == 0 && tube_audio_epoch(&tube_drain) == epoch)
                    tube_audio_mark(&tube_drain, epoch, TUBE_AUDIO_FAILED);
            }
            /*
             * Do not terminate at EOF.""")
    replace(player + "media/include/evo_audio_out.h", "/* Threads (spawned by main.c via pthread_create). */", """/* Decoder/resampler/PCM tail and AudioOut submission drain. This does not
 * establish natural demux EOF or video completion. Reset before open/seek. */
void evo_audio_drain_reset(void);
int evo_audio_is_drained(void);
int evo_audio_decode_is_parked(void);

/* Threads (spawned by main.c via pthread_create). */""")
    replace(player + "core/src/services/PlaybackController.cpp", "    evo_pb_reset_decode_fatal();", "    evo_pb_reset_decode_fatal();\n    evo_audio_drain_reset();")
    replace(player + "core/src/services/PlaybackController.cpp", "prospero_audio_resampler_reset();", "prospero_audio_resampler_reset();\n                evo_audio_drain_reset();", count=2)
    replace(player + "media/src/evo_demux.c", "        prospero_audio_resampler_reset();", "        prospero_audio_resampler_reset();\n        evo_audio_drain_reset();")
    replace(source, "if (player_paused || pb_prebuffer_hold || pb_scrub_hold || screen != 2)", "if (player_paused || pb_prebuffer_hold || pb_scrub_hold || screen != 2 || evo_demux_is_seeking())")
    replace(source, "            /* Parked - see the note on the video decode thread. */", "            atomic_store_explicit(&tube_decode_parked, 1, memory_order_release);\n            /* Parked - see the note on the video decode thread. */")
    replace(source, "        while (audio_decode_thread_running && audio_queue_count > 10)", "        atomic_store_explicit(&tube_decode_parked, 0, memory_order_release);\n        while (audio_decode_thread_running && audio_queue_count > 10 && !evo_demux_is_seeking())")
    replace(source, "        AVPacket *pkt = packet_queue_pop(&audio_packet_queue);", "        if (evo_demux_is_seeking()) continue;\n        AVPacket *pkt = packet_queue_pop(&audio_packet_queue);")
    replace(source, "    av_frame_free(&af);\n    return NULL;", "    atomic_store_explicit(&tube_decode_parked, 1, memory_order_release);\n    av_frame_free(&af);\n    return NULL;")
    replace(source, """        !prospero_audio_resampler_configure(
            frame,
            evo_audio_channels,
            audio_ctx
        )
    ) {
        return;""", """        !prospero_audio_resampler_configure(
            frame,
            evo_audio_channels,
            audio_ctx
        )
    ) {
        tube_audio_mark(&tube_drain, tube_audio_epoch(&tube_drain), TUBE_AUDIO_FAILED);
        return;""")
    replace(source, "    if (output_capacity <= 0) {\n        return;", "    if (output_capacity <= 0) {\n        tube_audio_mark(&tube_drain, tube_audio_epoch(&tube_drain), TUBE_AUDIO_FAILED);\n        return;")
    replace(source, "        if (!grown) {\n            return;", "        if (!grown) {\n            tube_audio_mark(&tube_drain, tube_audio_epoch(&tube_drain), TUBE_AUDIO_FAILED);\n            return;")
    replace(source, "    if (converted > 0) {", "    if (converted < 0) tube_audio_mark(&tube_drain, tube_audio_epoch(&tube_drain), TUBE_AUDIO_FAILED);\n    if (converted > 0) {")
    replace(source, "                    audio_queue_count >=\n                        AUDIO_QUEUE_BLOCKS - 2", "                    !evo_demux_is_seeking() && audio_queue_count >=\n                        AUDIO_QUEUE_BLOCKS - 2")
    replace(source, "                if (!audio_decode_thread_running) {", "                if (!audio_decode_thread_running || evo_demux_is_seeking()) {")
    replace(player + "media/src/evo_demux.c", "static volatile int prospero_seek_in_progress = 0;", "static volatile int prospero_seek_in_progress = 0;\nint evo_demux_is_seeking(void) { return prospero_seek_pending || prospero_seek_in_progress; }")
    replace(player + "media/include/evo_demux.h", "void *demux_thread_func(void *arg);", "void *demux_thread_func(void *arg);\nint evo_demux_is_seeking(void);")
    replace(player + "media/src/evo_demux.c", "    video_decode_done = 0;\n    video_decode_ready = 1;", """    /* A NULL-packet audio drain and a seek must never share a codec/resampler.
     * Hold both consumers, and abort instead of flushing an in-flight call. */
    unsigned audio_wait_ms = 0;
    while (audio_decode_thread_running && !evo_audio_decode_is_parked() && audio_wait_ms < 2000) {
        usleep(1000); ++audio_wait_ms;
    }
    if ((video_thread_running && !video_decode_parked) ||
        (audio_decode_thread_running && !evo_audio_decode_is_parked())) {
        prospero_seek_in_progress = 0; video_decode_hold = 0;
        pp_playback_notify_seek_end(&g_pp_pb, 0, 0, 0);
        if (!restore_paused) pp_playback_resume(&g_pp_pb);
        player_paused = restore_paused ? 1 : 0;
        controls_last_used_ms = now_ms();
        toast("SEEK", "Decoder is busy; seek was not applied");
        return 0;
    }
    video_decode_done = 0;
    video_decode_ready = 1;""")
    demux = player + "media/src/evo_demux.c"
    replace(demux, '#include "evo_demux.h"', '#include "evo_demux.h"\n#include <stdatomic.h>')
    replace(demux, "static volatile int prospero_seek_in_progress = 0;", """static volatile int prospero_seek_in_progress = 0;
static _Atomic int tube_input_eof;
int evo_demux_natural_eof(void) { return atomic_load_explicit(&tube_input_eof, memory_order_acquire); }
void evo_demux_reset_eof(void) { atomic_store_explicit(&tube_input_eof, 0, memory_order_release); }""")
    replace(demux, "            video_decode_done = 1;\n            prebuffer_check", """            const int io_error = play_fmt->pb ? play_fmt->pb->error : 0;
            atomic_store_explicit(&tube_input_eof,
                read_result == AVERROR_EOF && (io_error == 0 || io_error == AVERROR_EOF),
                memory_order_release);
            video_decode_done = 1;
            prebuffer_check""")
    replace(demux, "        video_decode_done = 0;\n        prebuffer_check", "        evo_demux_reset_eof();\n        video_decode_done = 0;\n        prebuffer_check")
    replace(demux, "        evo_audio_drain_reset();", "        evo_audio_drain_reset();\n        evo_demux_reset_eof();")
    replace(player + "media/include/evo_demux.h", "int evo_demux_is_seeking(void);", "int evo_demux_is_seeking(void);\nint evo_demux_natural_eof(void);\nvoid evo_demux_reset_eof(void);")
    replace(player + "core/src/services/PlaybackController.cpp", "    evo_pb_reset_decode_fatal();", "    evo_pb_reset_decode_fatal();\n    evo_demux_reset_eof();")
    playback = player + "media/src/evo_playback.c"
    replace(playback, "int evo_pb_is_eof(void)            { return video_decode_done; }", """int evo_pb_is_eof(void)            { return video_decode_done; }
int evo_pb_input_complete(void) { return video_decode_done && evo_demux_natural_eof(); }
int evo_pb_audio_drained(void) { return audio_stream_index < 0 || evo_audio_is_drained(); }""")
    replace(player + "media/include/evo_playback.h", "int    evo_pb_is_eof(void);", "int    evo_pb_input_complete(void); /* natural EOF, not failed recovery */\nint    evo_pb_audio_drained(void); /* decoded tail and output submissions */\nint    evo_pb_is_eof(void);")
    replace(playback, "    if (audio_stream_index >= 0 && audio_handle >= 1 && audio_rel >= 0.05)", """    if (evo_audio_is_drained()) return fmax(audio_rel, video_rel);
    if (audio_stream_index >= 0 && audio_handle >= 1 && audio_rel >= 0.05)""")
    replace(playback, "    if (audio_stream_index < 0 || audio_handle < 1 || audio_clock_seconds < 0.05)", "    if (evo_audio_is_drained() || audio_stream_index < 0 || audio_handle < 1 || audio_clock_seconds < 0.05)")
    replace(player + "core/src/screens/PlayerScreen.cpp", "    const bool ended = (evo_pb_is_eof() != 0) &&", "    const bool ended = (evo_pb_input_complete() != 0) && evo_pb_audio_drained() &&")
