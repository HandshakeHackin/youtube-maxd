"""Expose scoped YouTube caption languages in EVO's actual subtitle picker."""
def apply_caption_controls_overlay(replace, player):
    picker = player + 'core/src/screens/SubtitlePickerScreen.cpp'
    header = player + 'core/include/evo/screens/SubtitlePickerScreen.hpp'
    replace(picker, '#include "evo_subtitle.h"', '#include "evo_subtitle.h"\n#include "tube_provider.h"\n#include "evo/interfaces/IPlaybackController.hpp"')
    replace(header, '    int m_scrollOffset = 0;', '''    int m_scrollOffset = 0;
    unsigned m_tubeRevision = 0;
    int m_tubeTracks = -1;
    bool m_tubeEnabled = false;''')
    replace(picker, '    m_tracks.clear();', '''    m_tubeRevision = tube_caption_revision();
    m_tubeTracks = tube_caption_track_count();
    m_tubeEnabled = prospero_subtitle_enabled != 0;
    m_tracks.clear();''')
    replace(picker, '    // 2. External SRT if available\n    if (prospero_subtitle_count > 0) {', '''    char playingVideo[12]{};
    auto playback = Application::getInstance().getPlaybackController();
    const bool youtube = playback && playback->isActive() &&
        tube_phone_video(playback->getCurrentFilePath().c_str(), playingVideo) && m_tubeTracks > 0;
    if (youtube) {
        for (int i = 0; i < m_tubeTracks; ++i) {
            SubtitleTrackEntry track;
            track.trackId = -1000 - i;
            const char* language = tube_caption_track_language(i);
            const char* name = tube_caption_track_name(i);
            track.label = *name ? name : language;
            track.detail = language;
            track.isSecondary = prospero_secondary_subtitle_active() && !std::strcmp(language, tube_caption_secondary_language());
            if (tube_caption_track_automatic(i)) track.detail += " (automatic)";
            if (prospero_subtitle_enabled && prospero_subtitle_count && !std::strcmp(language, tube_phone_caption_language()))
                m_activeTrackIndex = static_cast<int>(m_tracks.size());
            m_tracks.push_back(std::move(track));
        }
    }
    // 2. External SRT if available
    if (!youtube && prospero_subtitle_count > 0) {''')
    replace(picker, '    if (trackId == -2) {', '''    if (trackId <= -1000) {
        const char* language = tube_caption_track_language(-1000 - trackId);
        int result = 0;
        if (*language && prospero_subtitle_count && !std::strcmp(language, tube_phone_caption_language()))
            result = prospero_subtitle_select_primary(-1);
        else if (*language) result = tube_caption_switch_begin(language, 0, 0);
        if (!result) { evo_feedback(EVO_FB_ERROR); toast("CAPTIONS", "Couldn't select this language"); return; }
        toast("CAPTIONS", result == 2 ? "Loading..." : language);
    } else if (trackId == -2) {''')
    replace(picker, '    if (entry.trackId == kAutoSyncTrack) {', '''    if (entry.trackId <= -1000) {
        const char* language = tube_caption_track_language(-1000 - entry.trackId);
        if (entry.isSecondary) {
            tube_caption_secondary_clear(); tube_secondary_subtitle_clear();
        } else if (!tube_caption_secondary_begin(language)) {
            evo_feedback(EVO_FB_ERROR); toast("CAPTIONS", "Couldn't select a second language"); return;
        } else toast("CAPTIONS", "Loading second language...");
        evo_feedback(EVO_FB_CONFIRM); refreshTracks(); return;
    }
    if (entry.trackId == kAutoSyncTrack) {''')
    replace(picker, '        prospero_secondary_subtitle_select(PROSPERO_SECONDARY_NONE);',
        '        tube_caption_secondary_clear();\n        prospero_secondary_subtitle_select(PROSPERO_SECONDARY_NONE);')
    replace(picker, '    StatefulScreen::update(deltaMs);', '''    StatefulScreen::update(deltaMs);
    if (m_tubeRevision != tube_caption_revision() || m_tubeTracks != tube_caption_track_count() ||
        m_tubeEnabled != (prospero_subtitle_enabled != 0)) {
        const int selected = m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_tracks.size()) ? m_tracks[m_selectedIndex].trackId : -2;
        refreshTracks();
        m_selectedIndex = 0;
        for (int i = 0; i < static_cast<int>(m_tracks.size()); ++i) if (m_tracks[i].trackId == selected) m_selectedIndex = i;
        m_scrollOffset = std::max(0, m_selectedIndex - kVisibleRows + 1);
    }''')
