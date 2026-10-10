"""Decode seek preroll but present only the requested media position."""


def apply_accurate_resume_overlay(replace, player, work):
    controller = player + 'core/src/services/PlaybackController.cpp'
    replace(controller, '    if (wantResume) {', '''    int64_t tube_resume_target_us = 0;
    bool tube_resume_armed = false;
    if (wantResume) {''')
    replace(controller, '''        const bool converted = tube_seek_times(play_fmt, seekStream, seekPos,
                                               &absoluteTargetUs, &seekTs);''', '''        int64_t unused_timestamp;
        const bool converted = tube_seek_times(play_fmt, seekStream, seekPos,
                                               &absoluteTargetUs, &seekTs) &&
            tube_seek_times(play_fmt, seekStream, resumeOffset,
                            &tube_resume_target_us, &unused_timestamp);''')
    replace(controller, '            resume_base_anchor_pending = resumeOffset;', '''            resume_base_anchor_pending = resumeOffset;
            tube_resume_armed = true;''')
    replace(controller, '    pp_playback_on_file_open(&g_pp_pb);', '''    pp_playback_on_file_open(&g_pp_pb);
    if (tube_resume_armed) {
        /* on_file_open clears discard state. Arm after it, before starting
         * consumers. Audio-only playback has no video to release that hold. */
        if (video_stream_index >= 0) {
            pp_playback_notify_seek_begin(&g_pp_pb, tube_resume_target_us);
            pp_playback_notify_seek_end(&g_pp_pb, 1, 0, 0);
        }
        audio_seek_discard_until = audio_stream_index >= 0 ? resumeOffset : -1.0;
    }''')
    audio = player + 'media/src/evo_audio_out.c'
    demux = player + 'media/src/evo_demux.c'
    replace(demux, '    pp_playback_notify_seek_begin(&g_pp_pb, absolute_target_us);', '''    /* An audio-only source has no video frame to release a presentation hold. */
    if (video_stream_index >= 0)
        pp_playback_notify_seek_begin(&g_pp_pb, absolute_target_us);''')
    replace(demux, '        pp_playback_notify_seek_end(&g_pp_pb, 0, 0, 0);', '        if (video_stream_index >= 0) pp_playback_notify_seek_end(&g_pp_pb, 0, 0, 0);')
    replace(demux, '''    pp_playback_notify_seek_end(
        &g_pp_pb,
        result >= 0,
        0,
        0
    );''', '''    if (video_stream_index >= 0)
        pp_playback_notify_seek_end(&g_pp_pb, result >= 0, 0, 0);''')
    replace(demux, '''        if (target_seconds > 0.05)
            audio_seek_discard_until = target_seconds;
        else
            audio_seek_discard_until = -1.0;''', '''        /* Include zero and sub-50ms targets: negative codec priming samples
         * must not become audible after a seek back to the beginning. */
        audio_seek_discard_until = audio_stream_index >= 0 ? target_seconds : -1.0;''')
    replace(audio, '#include "tube_seek_timeline.h"', '#include "tube_seek_timeline.h"\n#include "tube_audio_seek.h"')
    replace(audio, 'void evo_audio_drain_reset(void) { tube_audio_reset(&tube_drain); }', '''static tube_audio_seek tube_seek_pcm;
void evo_audio_drain_reset(void) {
    tube_audio_reset(&tube_drain);
    memset(&tube_seek_pcm, 0, sizeof(tube_seek_pcm));
}
int evo_audio_has_failed(void) {
    return !!(tube_audio_flags(&tube_drain, tube_audio_epoch(&tube_drain)) & TUBE_AUDIO_FAILED);
}''')
    # The old packet gate must disappear entirely, including its give-up path.
    source = (work / audio).read_text()
    start = source.index('        if (pkt && audio_seek_discard_until >= 0.0) {')
    end = source.index('        if (!pkt) {', start)
    replace(audio, source[start:end], '')
    replace(audio, 'static int tube_audio_flush_tail(AVFrame *af, uint64_t epoch) {', '''static int tube_audio_accept_frame(AVFrame *af) {
    const int origin_stream = video_stream_index >= 0 ? video_stream_index : audio_stream_index;
    if (!play_fmt || audio_stream_index < 0 || origin_stream < 0 ||
        (unsigned)audio_stream_index >= play_fmt->nb_streams ||
        (unsigned)origin_stream >= play_fmt->nb_streams) {
        tube_audio_mark(&tube_drain, tube_audio_epoch(&tube_drain), TUBE_AUDIO_FAILED);
        return 0;
    }
    const int64_t timestamp = af->pts != AV_NOPTS_VALUE ? af->pts : af->best_effort_timestamp;
    double pts = timestamp == AV_NOPTS_VALUE ? NAN : timestamp *
        av_q2d(play_fmt->streams[audio_stream_index]->time_base);
    const double origin = (double)tube_media_origin_us(play_fmt, origin_stream) / AV_TIME_BASE;
    double target = audio_seek_discard_until;
    const int accepted = tube_audio_seek_frame(&tube_seek_pcm, af, &pts, origin, &target);
    if (accepted < 0) {
        tube_audio_mark(&tube_drain, tube_audio_epoch(&tube_drain), TUBE_AUDIO_FAILED);
        return 0;
    }
    audio_seek_discard_until = target;
    if (!accepted) return 1;
    if (isfinite(pts)) {
        audio_pts_seconds = pts;
        if (first_audio_pts_seconds < 0.0) {
            first_audio_pts_seconds = pts;
            anchor_resume_base();
        }
    }
    mix_audio_frame_to_queue(af);
    return !evo_audio_has_failed();
}

static int tube_audio_flush_tail(AVFrame *af, uint64_t epoch) {''')
    replace(audio, '                mix_audio_frame_to_queue(af); av_frame_unref(af);', '                if (!tube_audio_accept_frame(af)) { av_frame_unref(af); return 0; }\n                av_frame_unref(af);')
    replace(audio, '            mix_audio_frame_to_queue(af);\n            av_frame_unref(af);', '            if (!tube_audio_accept_frame(af)) { av_frame_unref(af); return 0; }\n            av_frame_unref(af);')
    source = (work / audio).read_text()
    start = source.index('                        if (pkt->pts != AV_NOPTS_VALUE')
    end = source.index('                        mix_audio_frame_to_queue(af);', start) + len('                        mix_audio_frame_to_queue(af);')
    replace(audio, source[start:end], '''                        af->pts = pkt->pts;
                        tube_audio_accept_frame(af);''')
    source = (work / audio).read_text()
    start = source.index('                if (af->pts != AV_NOPTS_VALUE')
    end = source.index('                mix_audio_frame_to_queue(af);', start) + len('                mix_audio_frame_to_queue(af);')
    replace(audio, source[start:end], '                tube_audio_accept_frame(af);')
    replace(audio, '    SwrContext *resampler = prospero_audio_resampler_ctx();', '''    /* Draining without reaching the requested position is not a successful
     * seek. Do not retire silent preroll as completed media. */
    if (audio_seek_discard_until >= 0.0) return 0;
    SwrContext *resampler = prospero_audio_resampler_ctx();''')
    replace(audio, '    if (!af) return NULL;', '''    if (!af) {
        tube_audio_mark(&tube_drain, tube_audio_epoch(&tube_drain), TUBE_AUDIO_FAILED);
        atomic_store_explicit(&tube_decode_parked, 1, memory_order_release);
        return NULL;
    }''')
    replace(audio, '                if (produced > 0 && ch > 0 && rate > 0) {', '''                if (produced < 0 || produced > (int)sizeof(g_adec_pcm) ||
                    ch <= 0 || ch > EVO_AUDIO_MAX_CH || rate <= 0 ||
                    produced % ((int)sizeof(int16_t) * ch)) {
                    tube_audio_mark(&tube_drain, tube_audio_epoch(&tube_drain), TUBE_AUDIO_FAILED);
                } else if (produced > 0) {''')
    replace(audio, '''                        tube_audio_accept_frame(af);
                    }
                    av_frame_unref(af);''', '''                        tube_audio_accept_frame(af);
                    } else {
                        tube_audio_mark(&tube_drain, tube_audio_epoch(&tube_drain), TUBE_AUDIO_FAILED);
                    }
                    av_frame_unref(af);''')
    replace(player + 'media/include/evo_audio_out.h', 'int evo_audio_is_drained(void);', 'int evo_audio_is_drained(void);\nint evo_audio_has_failed(void);')
    replace(player + 'media/src/evo_playback.c', 'int evo_pb_decode_fatal(void) { return g_pb_decode_fatal; }', 'int evo_pb_decode_fatal(void) { return g_pb_decode_fatal || evo_audio_has_failed(); }')
    replace(player + 'core/src/screens/PlayerScreen.cpp', '    if (evo_pb_decode_fatal()) {', '''    if (evo_pb_decode_fatal()) {
        if (evo_audio_has_failed())
            toast("AUDIO STOPPED", "Audio decoding or output failed. Select the video to retry.");''')
