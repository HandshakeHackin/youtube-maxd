"""The quality and the audio track of the playing YouTube video in the playback options.

Each quality and each audio track is a stream of its own (the built-in
service names one manifest per choice), so choosing one reopens the video at
its position through the provider's bounded reopen, as a preset does. Both
interfaces use these rows: Classic lists them, Afterglow groups them under
Quality and Audio. Nothing is kept for the next video (see native/tube_provider.h).
"""

ROWS = r'''    // ProsperoTube stream choices (extracted by host tests).
    bool tubeAudio = false;
    if (playback->isActive() && !tube_phone_owns_media() && tube_player_quality_count() > 1) {
        m_hasQuality = true;
        const int playing = tube_player_quality_playing(), pinned = tube_player_quality_pinned();
        Entry automatic;
        automatic.kind = KindTubeQuality; automatic.streamIndex = -1; automatic.label = "AUTO";
        automatic.detail = "ProsperoTube picks the best quality available";
        if (pinned < 0) { automatic.badge = "SELECTED"; m_activeIndex = static_cast<int>(m_tracks.size()); }
        m_tracks.push_back(std::move(automatic));
        for (int i = 0; i < tube_player_quality_count(); ++i) {
            Entry e; e.kind = KindTubeQuality; e.streamIndex = i;
            char label[48]; tube_player_quality_label(i, label, sizeof label);
            e.label = label; e.detail = tube_player_quality_detail(i);
            if (i == playing) e.badge = "PLAYING";
            if (i == pinned) m_activeIndex = static_cast<int>(m_tracks.size());
            m_tracks.push_back(std::move(e));
        }
    }
    if (playback->isActive() && !tube_phone_owns_media() && tube_player_audio_count() > 1) {
        tubeAudio = true;   /* the file itself holds one of them: EVO's own audio rows would name only that one */
        const int playing = tube_player_audio_playing();
        for (int i = 0; i < tube_player_audio_count(); ++i) {
            Entry e; e.kind = KindTubeAudio; e.streamIndex = i;
            e.label = tube_player_audio_name(i);
            const int kind = tube_player_audio_kind(i);
            e.detail = tube_player_audio_language(i);
            if (kind & TUBE_AUDIO_ORIGINAL) e.detail += " - original";
            else if (kind & TUBE_AUDIO_DESCRIPTIVE) e.detail += " - audio description";
            else if (kind & TUBE_AUDIO_DUBBED) e.detail += " - dubbed";
            if (i == playing) {
                e.badge = "PLAYING";
                if (!m_hasQuality) m_activeIndex = static_cast<int>(m_tracks.size());
            }
            m_tracks.push_back(std::move(e));
        }
    }
    // End ProsperoTube stream choices.
'''

SELECTION = r'''    } else if (e.kind == KindTubeQuality || e.kind == KindTubeAudio) {
        // ProsperoTube stream choice selection (extracted by host tests).
        const bool quality = e.kind == KindTubeQuality;
        const char* title = quality ? "QUALITY" : "AUDIO TRACK";
        const int result = quality
            ? tube_player_quality_select(e.streamIndex, playback->getPositionSeconds(), playback->isLiveSource(), prospero_subtitle_enabled)
            : tube_player_audio_select(e.streamIndex, playback->getPositionSeconds(), playback->isLiveSource(), prospero_subtitle_enabled);
        if (result == 1) {
            /* As a preset does: the video reopens at its position with the stream chosen. */
            toast(title, (e.label + " - reloading this video").c_str());
            playback->stopPlayback();
            if (sm) sm->returnFromPlayback();
            return;
        }
        toast(title, result == 2 ? e.label.c_str() : "SWITCH FAILED");
        // End ProsperoTube stream choice selection.
'''


def apply(replace, player):
    header = player + 'core/include/evo/screens/AudioTrackPickerScreen.hpp'
    picker = player + 'core/src/screens/AudioTrackPickerScreen.cpp'
    replace(header, 'KindAfterglowDetails, KindAfterglowStats };', 'KindAfterglowDetails, KindAfterglowStats, KindTubeQuality, KindTubeAudio };')
    replace(picker, '    const auto variants = playback->getVideoVariants();\n    if (variants.size() > 1) {',
            ROWS + '    const auto variants = playback->getVideoVariants();\n    if (variants.size() > 1) {')
    replace(picker, '    for (const auto& t : playback->getAudioTracks()) {\n',
            '    for (const auto& t : playback->getAudioTracks()) {\n        if (tubeAudio) break;\n')
    replace(picker, '    } else if (e.kind == KindAuto) {\n        /* Back to EVO', SELECTION + '    } else if (e.kind == KindAuto) {\n        /* Back to EVO')
    # Afterglow groups them with the other qualities and audio tracks.
    replace(picker, '    case KindAuto: case KindVideo: return KindVideo;',
            '    case KindAuto: case KindVideo: case KindTubeQuality: return KindVideo;\n    case KindTubeAudio: return KindAudio;')
