"""Keep seek clock commits and begin/end ownership on the demux thread."""


def apply_seek_overlay(replace, player):
    controller = player + 'core/src/services/PlaybackController.cpp'
    replace(controller, '#include <cmath>', '#include <cmath>\n#include "tube_seek_timeline.h"')
    replace(controller, '''        int64_t seekTs = static_cast<int64_t>(
            seekPos / av_q2d(play_fmt->streams[seekStream]->time_base));

        if (av_seek_frame(play_fmt, seekStream, seekTs, AVSEEK_FLAG_BACKWARD) >= 0) {''', '''        int64_t absoluteTargetUs, seekTs;
        const bool converted = tube_seek_times(play_fmt, seekStream, seekPos,
                                               &absoluteTargetUs, &seekTs);
        if (converted && av_seek_frame(play_fmt, seekStream, seekTs, AVSEEK_FLAG_BACKWARD) >= 0) {''')
    replace(controller, '''    if (!isActive() || targetSeconds < 0.0) return;

    m_resumeBaseOffset = targetSeconds;
    resume_base_offset_seconds = targetSeconds;

    int64_t targetUs = static_cast<int64_t>(targetSeconds * 1000000.0);''', '''    if (!isActive() || !std::isfinite(targetSeconds) || targetSeconds < 0.0 ||
        targetSeconds > 1000000.0) return;

    /* The request is not a successful seek. The demux thread commits its
     * clamped position only after av_seek_frame succeeds; refusals and failed
     * seeks must retain the old clock base. */''')
    replace(controller, '    pp_playback_notify_seek_begin(&g_pp_pb, targetUs);', '''    /* The demux thread owns seek begin/end. Announcing begin here, after
     * publishing the request, can race a fast completed/failed demux seek and
     * re-arm its discard hold with no remaining request to clear it. */''')
    demux = player + 'media/src/evo_demux.c'
    replace(demux, '#include <stdatomic.h>', '#include <stdatomic.h>\n#include <math.h>')
    replace(demux, '''    if (
        !play_fmt ||
        (video_stream_index < 0 && audio_stream_index < 0)
    ) {''', '''    if (
        !isfinite(target_seconds) || target_seconds > 1000000.0 ||
        !play_fmt ||
        (video_stream_index < 0 && audio_stream_index < 0)
    ) {''')
    replace(demux, '#include <math.h>', '#include <math.h>\n#include "tube_seek_timeline.h"')
    replace(demux, '''    pp_playback_notify_seek_begin(
        &g_pp_pb,
        (int64_t)(target_seconds * 1000000.0)
    );''', '''    const int seek_stream = video_stream_index >= 0 ? video_stream_index : audio_stream_index;
    int64_t absolute_target_us, seek_timestamp;
    if (!tube_seek_times(play_fmt, seek_stream, target_seconds,
                         &absolute_target_us, &seek_timestamp)) {
        prospero_seek_in_progress = 0;
        if (!restore_paused) pp_playback_resume(&g_pp_pb);
        player_paused = restore_paused ? 1 : 0;
        controls_last_used_ms = now_ms();
        return 0;
    }
    pp_playback_notify_seek_begin(&g_pp_pb, absolute_target_us);''')
    replace(demux, '''    double decoder_seek_seconds =
        target_seconds;''', '')
    replace(demux, '''    int seek_stream =
        video_stream_index >= 0
            ? video_stream_index
            : audio_stream_index;

    AVRational time_base =
        play_fmt->streams[
            seek_stream
        ]->time_base;

    int64_t seek_timestamp =
        (int64_t)(
            decoder_seek_seconds /
            av_q2d(time_base)
        );''', '''    /* seek_timestamp and the video discard gate share absolute packet PTS.
     * The committed UI base and audio discard target remain relative. */''')
    audio = player + 'media/src/evo_audio_out.c'
    replace(audio, '#include <math.h>', '#include <math.h>\n#include "tube_seek_timeline.h"')
    replace(audio, '    const double landed = first_audio_pts_seconds;', '''    const int origin_stream = video_stream_index >= 0 ? video_stream_index : audio_stream_index;
    const double origin = play_fmt && origin_stream >= 0 &&
        (unsigned)origin_stream < play_fmt->nb_streams
        ? (double)tube_media_origin_us(play_fmt, origin_stream) / AV_TIME_BASE : 0.0;
    const double landed = first_audio_pts_seconds - origin;''')
    replace(audio, '''            if (pkt->pts != AV_NOPTS_VALUE && play_fmt && audio_stream_index >= 0)
                pkt_seconds = (double)pkt->pts *
                    av_q2d(play_fmt->streams[audio_stream_index]->time_base);''', '''            if (pkt->pts != AV_NOPTS_VALUE && play_fmt && audio_stream_index >= 0) {
                const int origin_stream = video_stream_index >= 0 ? video_stream_index : audio_stream_index;
                pkt_seconds = (double)pkt->pts *
                    av_q2d(play_fmt->streams[audio_stream_index]->time_base) -
                    (double)tube_media_origin_us(play_fmt, origin_stream) / AV_TIME_BASE;
            }''')
