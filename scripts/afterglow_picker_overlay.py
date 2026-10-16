"""Afterglow groups the real playback choices and gives captions two language tabs."""
import shutil

AUDIO_METHODS = r"""
// ProsperoTube Afterglow picker groups: all entries still use their existing activation paths.
int AudioTrackPickerScreen::afterglowGroup(const Entry& entry) const {
    switch (entry.kind) {
    case KindAuto: case KindVideo: return KindVideo;
    case KindAudio: case KindSpeed: case KindChat: case KindPlaybackMode:
    case KindResize: case KindZoom: case KindAspect: case KindPixel: case KindRotate:
    case KindTime: case KindSeek: case KindSeekInterval: case KindCross: case KindHide: case KindSleep:
        return entry.kind;
    default: return -1;
    }
}
const char* AudioTrackPickerScreen::afterglowGroupTitle(int group) const {
    switch (group) {
    case KindVideo: return "Quality"; case KindAudio: return "Audio"; case KindSpeed: return "Speed";
    case KindChat: return "Live chat"; case KindPlaybackMode: return "Playback mode";
    case KindResize: return "Video size"; case KindZoom: return "Zoom";
    case KindAspect: return "Aspect ratio"; case KindPixel: return "Display correction";
    case KindRotate: return "Rotation"; case KindTime: return "Time display";
    case KindSeek: return "Seeking"; case KindSeekInterval: return "Seek interval";
    case KindCross: return "Cross button"; case KindHide: return "Hide controls";
    case KindSleep: return "Sleep timer"; default: return "";
    }
}
std::string AudioTrackPickerScreen::afterglowChoiceLabel(const Entry& entry) const {
    const auto colon = entry.label.find(": ");
    return colon == std::string::npos ? entry.label : entry.label.substr(colon+2);
}
bool AudioTrackPickerScreen::afterglowCurrent(const Entry& entry) const {
    auto playback = Application::getInstance().getPlaybackController();
    if (entry.kind == KindAuto) return playback && !playback->isVideoQualityPinned();
    if (entry.kind == KindVideo) return playback && playback->isVideoQualityPinned() && entry.badge == "PLAYING";
    return entry.badge == "SELECTED" || entry.badge == "PLAYING" ||
        (entry.kind == KindAudio && playback && entry.streamIndex == playback->getActiveAudioStream());
}
void AudioTrackPickerScreen::groupAfterglowTracks() {
    if (!tube_afterglow_picker_active() || m_tubeExtraGroup != -1) return;
    m_afterglowChoices = m_tracks;
    std::vector<Entry> grouped;
    int active = 0;
    for (int i = 0; i < static_cast<int>(m_tracks.size()); ++i) {
        const Entry& entry = m_tracks[i];
        const int group = afterglowGroup(entry);
        if (m_afterglowGroup >= 0) {
            if (group != m_afterglowGroup) continue;
            if (afterglowCurrent(entry)) active = static_cast<int>(grouped.size());
            grouped.push_back(entry);
            continue;
        }
        int index = static_cast<int>(grouped.size());
        if (group >= 0) {
            for (int j = 0; j < static_cast<int>(grouped.size()); ++j)
                if (grouped[j].kind == KindAfterglowGroup && grouped[j].streamIndex == group) index = j;
            if (index == static_cast<int>(grouped.size())) {
                Entry summary; summary.kind = KindAfterglowGroup; summary.streamIndex = group;
                summary.label = afterglowGroupTitle(group); summary.detail = entry.detail;
                grouped.push_back(std::move(summary));
            }
            if (afterglowCurrent(entry)) grouped[index].label = std::string(afterglowGroupTitle(group)) + ": " + afterglowChoiceLabel(entry);
        } else grouped.push_back(entry);
        if (i == m_activeIndex) active = index;
    }
    m_tracks = std::move(grouped); m_activeIndex = active;
}
// End ProsperoTube Afterglow picker groups.
"""

AUDIO_PREVIEW = r"""
#if defined(EVO_AGC_DEVICE)
    tube_afterglow_options_begin("");
    if (tube_afterglow_picker_active() && m_tubeExtraGroup == -1 && m_afterglowGroup < 0 &&
        m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_tracks.size())) {
        const Entry& selected = m_tracks[m_selectedIndex];
        if (selected.kind == KindAfterglowGroup) {
            tube_afterglow_options_begin(afterglowGroupTitle(selected.streamIndex));
            for (const Entry& choice : m_afterglowChoices)
                if (afterglowGroup(choice) == selected.streamIndex)
                    tube_afterglow_options_choice(afterglowChoiceLabel(choice).c_str(), afterglowCurrent(choice));
        } else if (selected.kind == KindExtraGroup) {
            tube_afterglow_options_begin(tube_extra_group_title(selected.streamIndex));
            for (int i=0; i<tube_extra_group_count(selected.streamIndex); ++i) {
                char label[96]; tube_extra_group_label(selected.streamIndex,i,label,sizeof label);
                tube_afterglow_options_choice(label, i==tube_extra_group_selected(selected.streamIndex));
            }
        } else if (selected.kind == KindPresetList) {
            int height=0,fps=0,codec=0;
            tube_player_quality(&height,&fps,&codec);
            tube_afterglow_options_begin("Video presets");
            for (int i=0; i<tube_preset_count(); ++i) {
                char label[48]; tube_preset_label(i,label,sizeof label);
                tube_afterglow_options_choice(label,i==tube_preset_find(height,fps,codec));
            }
        } else if (selected.kind == KindChapterList) {
            auto playback=Application::getInstance().getPlaybackController();
            const int current=playback?tube_extra_chapter_at(playback->getPositionSeconds()):-1;
            tube_afterglow_options_begin("Chapters");
            for (int i=0; i<tube_extra_chapter_count(); ++i) {
                char label[512]; tube_extra_chapter_label(i,label,sizeof label);
                tube_afterglow_options_choice(label,i==current);
            }
        }
    }
#endif
"""

def apply(root, work, replace, player):
    shutil.copyfile(root / "native/afterglow/player/app/picker.h", work / player / "addons/include/tube_afterglow_picker.h")
    audio = player + "core/src/screens/AudioTrackPickerScreen.cpp"
    header = player + "core/include/evo/screens/AudioTrackPickerScreen.hpp"
    replace(header, "KindAuto = 0, KindVideo", "KindAfterglowGroup = -1, KindAuto = 0, KindVideo")
    replace(header, "    void refreshTracks();", """    int afterglowGroup(const Entry&) const;
    const char* afterglowGroupTitle(int) const;
    std::string afterglowChoiceLabel(const Entry&) const;
    bool afterglowCurrent(const Entry&) const;
    void groupAfterglowTracks();
    int m_afterglowGroup = -1;
    std::vector<Entry> m_afterglowChoices;
    void refreshTracks();""")
    replace(audio, '#include "tube_provider.h"', '#include "tube_provider.h"\n#include "tube_afterglow_picker.h"')
    replace(audio, "    StatefulScreen::onEnter();\n", "    StatefulScreen::onEnter();\n    m_afterglowGroup = -1;\n")
    replace(audio, "    // End ProsperoTube player buttons filter.\n}",
            "    // End ProsperoTube player buttons filter.\n    groupAfterglowTracks();\n}")
    replace(audio, "void AudioTrackPickerScreen::navigate(int delta)", AUDIO_METHODS + "\nvoid AudioTrackPickerScreen::navigate(int delta)")
    replace(audio, "    const Entry e = m_tracks[m_selectedIndex];", """    if (m_tracks[m_selectedIndex].kind == KindAfterglowGroup) {
        m_afterglowGroup = m_tracks[m_selectedIndex].streamIndex;
        refreshTracks(); m_selectedIndex = m_activeIndex;
        m_scrollOffset = std::max(0,m_selectedIndex-EVO_RMLUI_LIST_ROWS+1);
        evo_feedback(EVO_FB_CONFIRM); return;
    }
    const Entry e = m_tracks[m_selectedIndex];""")
    replace(audio, "    if (pressed & PadButtons::Circle) {\n", """    if (pressed & PadButtons::Circle) {
        if (tube_afterglow_picker_active() && m_afterglowGroup >= 0) {
            const int group = m_afterglowGroup; m_afterglowGroup = -1;
            refreshTracks(); m_selectedIndex = 0;
            for (int i=0; i<static_cast<int>(m_tracks.size()); ++i)
                if (m_tracks[i].kind==KindAfterglowGroup && m_tracks[i].streamIndex==group) m_selectedIndex=i;
            m_scrollOffset=std::max(0,m_selectedIndex-EVO_RMLUI_LIST_ROWS+1);
            evo_feedback(EVO_FB_CANCEL); return true;
        }
""")
    replace(audio, "    params.subtitle = \"Select quality, audio, playback speed or live chat\";", """    if (tube_afterglow_picker_active() && m_afterglowGroup >= 0) params.title = afterglowGroupTitle(m_afterglowGroup);
    params.subtitle = "Select quality, audio, playback speed or live chat";""")
    replace(audio, "            params.rows[i].has_chevron = 0;", "            params.rows[i].has_chevron = m_tracks[idx].kind == KindAfterglowGroup || m_tracks[idx].kind == KindExtraGroup;")
    replace(audio, "    evo_rmlui_update_list(&params);", AUDIO_PREVIEW + "\n    evo_rmlui_update_list(&params);")
    subtitles = player + "core/src/screens/SubtitlePickerScreen.cpp"
    subheader = player + "core/include/evo/screens/SubtitlePickerScreen.hpp"
    replace(subheader, "    std::vector<SubtitleTrackEntry> m_tracks;", "    int m_afterglowTab = 0;\n    std::vector<SubtitleTrackEntry> m_tracks;")
    replace(subtitles, '#include "tube_provider.h"', '#include "tube_provider.h"\n#include "tube_afterglow_picker.h"')
    replace(subtitles, "    StatefulScreen::onEnter();\n", "    StatefulScreen::onEnter();\n    m_afterglowTab = 0;\n")
    replace(subtitles, "    if (!prospero_subtitle_enabled) {\n        m_activeTrackIndex = 0;\n    }", """    if (!prospero_subtitle_enabled) {
        m_activeTrackIndex = 0;
    }
    if (tube_afterglow_picker_active() && m_afterglowTab == 1) {
        m_tracks.erase(std::remove_if(m_tracks.begin(),m_tracks.end(),
            [](const SubtitleTrackEntry& row) { return row.trackId==kAutoSyncTrack; }),m_tracks.end());
        m_activeTrackIndex=0;
        for (int i=1; i<static_cast<int>(m_tracks.size()); ++i)
            if (m_tracks[i].isSecondary) m_activeTrackIndex=i;
    }""")
    replace(subtitles, "    if (pressed & PadButtons::Up) {", """    if (tube_afterglow_picker_active()) {
        if (pressed & (PadButtons::L1 | PadButtons::R1)) {
            m_afterglowTab = (pressed & PadButtons::R1) ? 1 : 0;
            refreshTracks(); m_selectedIndex=m_activeTrackIndex;
            m_scrollOffset=std::max(0,m_selectedIndex-kVisibleRows+1);
            evo_feedback(EVO_FB_MOVE); return true;
        }
        if ((pressed & PadButtons::Cross) && m_afterglowTab==1) { activateSecondary(); return true; }
        if (pressed & PadButtons::Square) { cycleSize(); return true; }
        if (m_afterglowTab==1 && (pressed & (PadButtons::Left | PadButtons::Right | PadButtons::L2 | PadButtons::TouchPadLeft | PadButtons::TouchPadRight))) {
            if (!prospero_secondary_subtitle_active()) evo_feedback(EVO_FB_ERROR);
            else {
                prospero_secondary_nudge_delay((pressed & (PadButtons::Right | PadButtons::TouchPadRight)) ? 100 : -100);
                evo_feedback(EVO_FB_MOVE);
            }
            return true;
        }
    }
    if (pressed & PadButtons::Up) {""")
    replace(subtitles, '        if (m_tracks[idx].trackId == kAutoSyncTrack) {', """        if (tube_afterglow_picker_active())
            params.tracks[i].tag = m_tracks[idx].trackId > -1000 ? "" : tube_caption_track_translated(-1000-m_tracks[idx].trackId) ? "TRANSLATED"
                                 : tube_caption_track_automatic(-1000-m_tracks[idx].trackId) ? "AUTO" : "";
        if (m_tracks[idx].trackId == kAutoSyncTrack) {""")
    replace(subtitles, "    evo_rmlui_update_subtitles(&params);", """#if defined(EVO_AGC_DEVICE)
    tube_afterglow_caption_tab(m_afterglowTab,m_scrollOffset,total);
#endif
    evo_rmlui_update_subtitles(&params);""")
