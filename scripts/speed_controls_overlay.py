"""In-player speed picker and owner-thread reopen at the displayed media position."""


def apply_speed_controls_overlay(replace, player):
    interface = player + 'core/include/evo/interfaces/IPlaybackController.hpp'
    header = player + 'core/include/evo/services/PlaybackController.hpp'
    controller = player + 'core/src/services/PlaybackController.cpp'
    replace(interface, '    virtual bool switchAudioTrack(int streamIndex) = 0;', '''    virtual bool switchAudioTrack(int streamIndex) = 0;
    virtual bool canChangePlaybackSpeed() const = 0;
    virtual int playbackSpeedPercent() const = 0;
    virtual bool changePlaybackSpeed(int percent) = 0;''')
    replace(header, '    bool switchAudioTrack(int streamIndex) override;', '''    bool switchAudioTrack(int streamIndex) override;
    bool canChangePlaybackSpeed() const override;
    int playbackSpeedPercent() const override;
    bool changePlaybackSpeed(int percent) override;''')
    replace(header, '    int m_requestedAudioStream = -1;', '''    int m_requestedAudioStream = -1;
    int m_tubeRequestedSpeed = -1; /* scoped to an owner-thread speed reopen */''')
    replace(controller, 'source.provider == "youtube" && !source.is_live ? tube_playback_speed() : 100',
            'source.provider == "youtube" && !source.is_live ? (m_tubeRequestedSpeed > 0 ? m_tubeRequestedSpeed : tube_playback_speed_for(source.item_id.c_str())) : 100')
    replace(controller, '''    if (m_source.isProvider()) {
        const evo_provider_t* prov = evo_provider_find(m_source.provider.c_str());
        if (prov && (prov->caps & EVO_PROVIDER_CAP_PROGRESS) && prov->report_progress)
            prov->report_progress(m_source.item_id.c_str(),
                                  static_cast<int64_t>(getPositionSeconds()),
                                  static_cast<int64_t>(m_durationSeconds),
                                  EVO_PROVIDER_PLAY_STOP);''', '''    if (m_source.isProvider() && m_tubeRequestedSpeed < 0) {
        const evo_provider_t* prov = evo_provider_find(m_source.provider.c_str());
        if (prov && (prov->caps & EVO_PROVIDER_CAP_PROGRESS) && prov->report_progress)
            prov->report_progress(m_source.item_id.c_str(),
                                  static_cast<int64_t>(getPositionSeconds()),
                                  static_cast<int64_t>(m_durationSeconds),
                                  EVO_PROVIDER_PLAY_STOP);''')
    replace(controller, 'bool PlaybackController::switchAudioTrack(int streamIndex) {', '''// ProsperoTube speed reopen (extracted by the host ownership test).
bool PlaybackController::canChangePlaybackSpeed() const {
    return m_source.provider == "youtube" && !m_source.is_live && !m_source.url.empty() &&
        !m_currentFilePath.empty() && std::isfinite(m_durationSeconds) && m_durationSeconds > 0 &&
        !isScrubbing() && !evo_pb_is_complete() &&
        !evo_demux_is_seeking() && !g_pp_pb.seek_discarding && !pb_prebuffer_hold;
}
int PlaybackController::playbackSpeedPercent() const {
    return static_cast<int>(evo_audio_rate() * 100.0 + .5);
}
bool PlaybackController::changePlaybackSpeed(int percent) {
    bool valid = false;
    for (int i = 0; i < tube_speed_count(); ++i) if (tube_speed_at(i) == percent) valid = true;
    if (!valid || !canChangePlaybackSpeed()) return false;
    if (percent == playbackSpeedPercent()) return true;

    const PlaybackSource savedSource = m_source;
    const double resumeAt = getPositionSeconds();
    const double savedDuration = m_durationSeconds;
    if (!std::isfinite(resumeAt) || resumeAt < 0 || resumeAt >= savedDuration) return false;
    const bool wasPaused = isPaused();
    const int savedSubEnabled = prospero_subtitle_enabled;
    const int savedSubUseExt = prospero_subtitle_use_external;
    const int savedSubStream = prospero_embedded_subtitle_stream_index;
    m_requestedAudioStream = audio_stream_index;
    if (!savedSubUseExt && savedSubStream >= 0) prospero_subtitle_requested_stream = savedSubStream;
    m_tubeRequestedSpeed = percent;
    const bool opened = startPlaybackSource(savedSource, resumeAt);
    m_tubeRequestedSpeed = -1;
    m_requestedAudioStream = -1;
    prospero_subtitle_requested_stream = -2;
    if (!opened) {
        // A reopen keeps the provider session alive. Only a failed reopen
        // becomes a terminal stop (stopPlayback suppressed it while scoped).
        const evo_provider_t* prov = evo_provider_find(savedSource.provider.c_str());
        if (prov && (prov->caps & EVO_PROVIDER_CAP_PROGRESS) && prov->report_progress)
            prov->report_progress(savedSource.item_id.c_str(), static_cast<int64_t>(resumeAt),
                                  static_cast<int64_t>(savedDuration), EVO_PROVIDER_PLAY_STOP);
        return false;
    }

    prospero_subtitle_enabled = savedSubEnabled;
    prospero_subtitle_use_external = savedSubUseExt;
    if (!savedSubUseExt && savedSubStream >= 0) prospero_embedded_subtitle_stream_index = savedSubStream;
    if (wasPaused) setPaused(true);
    tube_remember_playback_speed(savedSource.item_id.c_str(), percent);
    return true;
}
// End ProsperoTube speed reopen.

bool PlaybackController::switchAudioTrack(int streamIndex) {''')
    picker = player + 'core/src/screens/AudioTrackPickerScreen.cpp'
    replace(picker, '#include "evo_nav.h"', '#include "evo_nav.h"\n#include "tube_provider.h"')
    replace(player + 'core/include/evo/screens/AudioTrackPickerScreen.hpp',
            'enum Kind { KindAuto = 0, KindVideo, KindAudio };',
            'enum Kind { KindAuto = 0, KindVideo, KindAudio, KindSpeed };')
    replace(picker, '\n}\n\nvoid AudioTrackPickerScreen::navigate(int delta)', '''
    if (playback->canChangePlaybackSpeed()) {
        const int current = playback->playbackSpeedPercent();
        const int target = current == 100 ? tube_last_playback_speed() : 100;
        int toggleIndex = -1;
        if (target != current) {
            Entry e; e.kind = KindSpeed; e.streamIndex = target;
            char label[48]; std::snprintf(label, sizeof label, "Restore %.3gx", target / 100.0);
            e.label = target == 100 ? "Normal speed" : label; e.detail = "Toggle normal and your last selected speed";
            toggleIndex = static_cast<int>(m_tracks.size()); m_tracks.push_back(std::move(e));
        }
        for (int i = 0; i < tube_speed_count(); ++i) {
            Entry e; e.kind = KindSpeed; e.streamIndex = tube_speed_at(i);
            char label[48]; std::snprintf(label, sizeof label, "Speed: %.3gx", e.streamIndex / 100.0);
            e.label = label; e.detail = "Keep this position and preserve audio pitch";
            if (e.streamIndex == playback->playbackSpeedPercent()) {
                e.badge = "PLAYING"; m_activeIndex = static_cast<int>(m_tracks.size());
            }
            m_tracks.push_back(std::move(e));
        }
        if (toggleIndex >= 0) m_activeIndex = toggleIndex;
    }
}

void AudioTrackPickerScreen::navigate(int delta)''')
    replace(picker, '    if (e.kind == KindAuto) {', '''    if (e.kind == KindSpeed) {
        if (playback->changePlaybackSpeed(e.streamIndex)) toast("SPEED", e.label.c_str());
        else toast("SPEED", "Unable to change speed; try again when playback is ready");
    } else if (e.kind == KindAuto) {''')
    replace(picker, '    params.title = m_hasQuality ? "QUALITY & AUDIO" : "AUDIO TRACKS";',
            '    params.title = "PLAYBACK OPTIONS";')
    replace(picker, '''    params.subtitle = m_hasQuality ? "Pick the video quality and the audio track"
                                   : "Select the audio stream for this file";''',
            '    params.subtitle = "Select quality, audio or playback speed";')
    replace(player + 'ui_rml/src/evo_rmlui_provider.cpp', 'id != "settings:speed"',
            'id != "settings:speed" && id != "settings:speed-mode"')
