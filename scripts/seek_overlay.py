"""Keep seek clock commits and begin/end ownership on the demux thread."""


def apply_seek_overlay(replace, player):
    controller = player + 'core/src/services/PlaybackController.cpp'
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
