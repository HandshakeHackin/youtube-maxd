"""Expose scoped YouTube caption languages in EVO's actual subtitle picker."""
def apply_caption_controls_overlay(replace, player):
    picker = player + 'core/src/screens/SubtitlePickerScreen.cpp'
    header = player + 'core/include/evo/screens/SubtitlePickerScreen.hpp'
    replace(picker, '#include "evo_subtitle.h"', '#include "evo_subtitle.h"\n#include "tube_provider.h"\n#include "tube_i18n.h"\n#include "evo/interfaces/IPlaybackController.hpp"')
    replace(header, '    int m_scrollOffset = 0;', '''    int m_scrollOffset = 0;
    unsigned m_tubeRevision = 0;
    int m_tubeTracks = -1;
    bool m_tubeEnabled = false;
    bool m_tubeTranslations = false;              /* "Auto-translate: other languages" is open */
    static constexpr int kTubeMoreTranslations = -999;''')
    replace(picker, '    m_tracks.clear();', '''    m_tubeRevision = tube_caption_revision();
    m_tubeTracks = tube_caption_track_count();
    m_tubeEnabled = prospero_subtitle_enabled != 0;
    m_tracks.clear();''')
    replace(picker, '    // 2. External SRT if available\n    if (prospero_subtitle_count > 0) {', '''    char playingVideo[12]{};
    auto playback = Application::getInstance().getPlaybackController();
    const bool youtube = playback && playback->isActive() &&
        tube_phone_video(playback->getCurrentFilePath().c_str(), playingVideo) && m_tubeTracks > 0;
    if (youtube) {
        // ProsperoTube caption rows (extracted by host tests).
        /* As SmartTube lists them: the human tracks, YouTube's automatic tracks,
         * then its machine translations: the one into the interface language
         * first, the other languages behind one row that opens them. */
        const int ownLanguage = tube_caption_track_translation_for(tube_i18n_language());
        int translations = 0;
        auto add = [&](int i, const char* prefix) {
            SubtitleTrackEntry track;
            track.trackId = -1000 - i;
            const char* language = tube_caption_track_language(i);
            const char* name = tube_caption_track_name(i);
            track.label = std::string(prefix) + (*name ? name : language);
            track.detail = language;
            track.isSecondary = prospero_secondary_subtitle_active() && !std::strcmp(language, tube_caption_secondary_language());
            if (tube_caption_track_translated(i)) track.detail += " (auto-translated)";
            else if (tube_caption_track_automatic(i)) track.detail += " (auto-generated)";
            if (prospero_subtitle_enabled && prospero_subtitle_count && !std::strcmp(language, tube_phone_caption_language()))
                m_activeTrackIndex = static_cast<int>(m_tracks.size());
            m_tracks.push_back(std::move(track));
        };
        for (int i = 0; i < m_tubeTracks; ++i) {
            if (!tube_caption_track_translated(i)) { add(i, ""); continue; }
            if (i == ownLanguage) continue;
            ++translations;
            /* A translation in use, or the second line, stays in sight. */
            const char* language = tube_caption_track_language(i);
            if ((prospero_subtitle_enabled && prospero_subtitle_count && !std::strcmp(language, tube_phone_caption_language())) ||
                (prospero_secondary_subtitle_active() && !std::strcmp(language, tube_caption_secondary_language())))
                m_tubeTranslations = true;
        }
        if (ownLanguage >= 0) add(ownLanguage, "Auto-translate: ");
        if (translations) {
            SubtitleTrackEntry more;
            more.trackId = kTubeMoreTranslations;
            more.label = m_tubeTranslations ? "Auto-translate: fewer languages" : "Auto-translate: other languages";
            char count[48];
            std::snprintf(count, sizeof count, "%d languages", translations);
            more.detail = count;
            m_tracks.push_back(std::move(more));
            for (int i = 0; m_tubeTranslations && i < m_tubeTracks; ++i)
                if (tube_caption_track_translated(i) && i != ownLanguage) add(i, "");
        }
        // End ProsperoTube caption rows.
    }
    // 2. External SRT if available
    if (!youtube && prospero_subtitle_count > 0) {''')
    replace(picker, '    int trackId = m_tracks[m_selectedIndex].trackId;\n', '''    int trackId = m_tracks[m_selectedIndex].trackId;
    if (trackId == kTubeMoreTranslations) { toggleTubeTranslations(); return; }
''')
    replace(picker, 'void SubtitlePickerScreen::navigate(int delta) {', '''// ProsperoTube translation list (extracted by host tests).
void SubtitlePickerScreen::toggleTubeTranslations() {
    m_tubeTranslations = !m_tubeTranslations;
    evo_feedback(EVO_FB_CONFIRM);
    refreshTracks();
    m_selectedIndex = 0;
    for (int i = 0; i < static_cast<int>(m_tracks.size()); ++i) if (m_tracks[i].trackId == kTubeMoreTranslations) m_selectedIndex = i;
    m_scrollOffset = std::max(0, m_selectedIndex - kVisibleRows + 1);
}
// End ProsperoTube translation list.

void SubtitlePickerScreen::navigate(int delta) {''')
    replace(header, '    void refreshTracks();', '    void refreshTracks();\n    void toggleTubeTranslations();')
    replace(picker, 'void SubtitlePickerScreen::onExit() {\n    StatefulScreen::onExit();', 'void SubtitlePickerScreen::onExit() {\n    m_tubeTranslations = false;\n    StatefulScreen::onExit();')
    replace(picker, '    if (trackId == -2) {', '''    if (trackId <= -1000) {
        const char* language = tube_caption_track_language(-1000 - trackId);
        int result = 0;
        if (*language && prospero_subtitle_count && !std::strcmp(language, tube_phone_caption_language()))
            result = prospero_subtitle_select_primary(-1);
        else if (*language) result = tube_caption_switch_begin(language, 0, 0);
        if (!result) { evo_feedback(EVO_FB_ERROR); toast("CAPTIONS", "Couldn't select this language"); return; }
        toast("CAPTIONS", result == 2 ? "Loading..." : language);
    } else if (trackId == -2) {''')
    # "Remember subtitles per channel" also remembers that they were turned off.
    replace(picker, '        // Off\n        prospero_subtitle_enabled = 0;', '''        // Off
        prospero_subtitle_enabled = 0;
        char tubeVideo[12]{};
        auto tubePlayback = Application::getInstance().getPlaybackController();
        if (tubePlayback && tubePlayback->isActive() && tube_phone_video(tubePlayback->getCurrentFilePath().c_str(), tubeVideo))
            tube_remember_captions(tubeVideo, 0);''')
    # A machine translation is named in the row's tag while no line tag takes its place.
    replace(picker, '''                                m_tracks[idx].trackId != kAutoSyncTrack) ? "PRIMARY" : "";''',
            '''                                m_tracks[idx].trackId != kAutoSyncTrack) ? "PRIMARY" : "";
        if (!*params.tracks[i].tag && m_tracks[idx].trackId <= -1000 && tube_caption_track_translated(-1000 - m_tracks[idx].trackId))
            params.tracks[i].tag = "TRANSLATED";
        else if (!*params.tracks[i].tag && m_tracks[idx].trackId <= -1000 && tube_caption_track_automatic(-1000 - m_tracks[idx].trackId))
            params.tracks[i].tag = "AUTO";
        params.tracks[i].is_action = m_tracks[idx].trackId == kTubeMoreTranslations;''')
    replace(picker, '    if (entry.trackId == kAutoSyncTrack) {', '''    if (entry.trackId == kTubeMoreTranslations) { toggleTubeTranslations(); return; }
    if (entry.trackId <= -1000) {
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
