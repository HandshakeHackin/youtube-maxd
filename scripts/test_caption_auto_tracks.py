"""Run the generated caption picker rows: automatic tracks labelled, the translation into the
interface language first, the other translations behind one row that opens them."""
from pathlib import Path
import os, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
work = Path(subprocess.check_output(["python3", str(root / "scripts/prepare_native.py")], text=True).strip())
picker = (work / "projects/evoplayer/core/src/screens/SubtitlePickerScreen.cpp").read_text()

def part(source, start, end):
    a = source.index(start); return source[a:source.index(end, a) + len(end)]

rows = part(picker, "        // ProsperoTube caption rows (extracted by host tests).", "        // End ProsperoTube caption rows.")
toggle = part(picker, "void SubtitlePickerScreen::toggleTubeTranslations() {", "// End ProsperoTube translation list.")
toggle = toggle.replace("SubtitlePickerScreen::", "Picker::").replace("// End ProsperoTube translation list.", "")
# The opening row stays on the picker for both lines of dialogue (Cross and the second-line action).
selection = part(picker, "void SubtitlePickerScreen::activateSelection() {", "    evo_feedback(EVO_FB_CONFIRM);")
assert "if (trackId == kTubeMoreTranslations) { toggleTubeTranslations(); return; }" in selection
secondary = part(picker, "void SubtitlePickerScreen::activateSecondary() {", "    if (entry.trackId <= -1000) {")
assert "if (entry.trackId == kTubeMoreTranslations) { toggleTubeTranslations(); return; }" in secondary
render = part(picker, "void SubtitlePickerScreen::render(", "evo_rmlui_update_subtitles(&params);")
assert 'params.tracks[i].tag = "AUTO";' in render and "params.tracks[i].is_action = m_tracks[idx].trackId == kTubeMoreTranslations;" in render
assert ': tube_caption_track_automatic(-1000-m_tracks[idx].trackId) ? "AUTO" : "";' in render  # Afterglow's tags
assert '#include "tube_i18n.h"' in picker

code = r'''
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
enum { EVO_FB_CONFIRM };
static void evo_feedback(int) {}
struct Track { const char *language, *name; bool automatic, translated; const char *target; };
static std::vector<Track> tracks = {
    {"en", "English", false, false, ""}, {"de", "German (auto-generated)", true, false, ""},
    {"fr-en", "French", true, true, "fr"}, {"es-en", "Spanish", true, true, "es"}, {"pt-en", "Portuguese", true, true, "pt"}};
static std::string interface_language = "fr", playing, second;
static int prospero_subtitle_enabled = 0, prospero_subtitle_count = 0;
static int tube_caption_track_count() { return static_cast<int>(tracks.size()); }
static const char* tube_caption_track_language(int i) { return tracks[i].language; }
static const char* tube_caption_track_name(int i) { return tracks[i].name; }
static int tube_caption_track_automatic(int i) { return tracks[i].automatic; }
static int tube_caption_track_translated(int i) { return tracks[i].translated; }
static const char* tube_i18n_language() { return interface_language.c_str(); }
static int tube_caption_track_translation_for(const char* tag) {
    for (int i = 0; i < tube_caption_track_count(); ++i) if (tracks[i].translated && !std::strcmp(tracks[i].target, tag)) return i;
    return -1;
}
static const char* tube_phone_caption_language() { return playing.c_str(); }
static int prospero_secondary_subtitle_active() { return !second.empty(); }
static const char* tube_caption_secondary_language() { return second.c_str(); }
struct SubtitleTrackEntry { int trackId = 0; std::string label, detail; bool isSecondary = false; };
struct Picker {
    static constexpr int kTubeMoreTranslations = -999, kVisibleRows = 8;
    std::vector<SubtitleTrackEntry> m_tracks;
    int m_activeTrackIndex = 0, m_selectedIndex = 0, m_scrollOffset = 0, m_tubeTracks = 0;
    bool m_tubeTranslations = false;
    void refreshTracks() {
        m_tracks.clear(); m_activeTrackIndex = 0;
        SubtitleTrackEntry off; off.trackId = -2; off.label = "SUBTITLES OFF"; m_tracks.push_back(off);
        m_tubeTracks = tube_caption_track_count();
'''+rows+r'''
    }
    void toggleTubeTranslations();
    std::vector<std::string> labels() const { std::vector<std::string> out; for (auto& t : m_tracks) out.push_back(t.label); return out; }
};
'''+toggle+r'''
int main() {
    Picker p; p.refreshTracks();
    const std::vector<std::string> closed = {"SUBTITLES OFF", "English", "German (auto-generated)", "Auto-translate: French", "Auto-translate: other languages"};
    assert(p.labels() == closed);
    assert(p.m_tracks[1].detail == "en" && p.m_tracks[2].detail == "de (auto-generated)" && p.m_tracks[3].detail == "fr-en (auto-translated)");
    assert(p.m_tracks[3].trackId == -1002 && p.m_tracks[4].trackId == Picker::kTubeMoreTranslations && p.m_tracks[4].detail == "2 languages");
    p.m_selectedIndex = 4; p.toggleTubeTranslations();
    assert(p.m_tubeTranslations && p.m_selectedIndex == 4);
    assert((p.labels() == std::vector<std::string>{"SUBTITLES OFF", "English", "German (auto-generated)", "Auto-translate: French",
                                                    "Auto-translate: fewer languages", "Spanish", "Portuguese"}));
    assert(p.m_tracks[5].trackId == -1003 && p.m_tracks[6].detail == "pt-en (auto-translated)");
    p.toggleTubeTranslations(); assert(p.labels() == closed);
    // The translation in use is never hidden behind the closed row, and it is the current row.
    prospero_subtitle_enabled = prospero_subtitle_count = 1; playing = "es-en";
    Picker q; q.refreshTracks(); assert(q.m_tubeTranslations && q.m_tracks[q.m_activeTrackIndex].label == "Spanish");
    // So is a translation on the second line.
    playing = "en"; second = "pt-en";
    Picker s; s.refreshTracks(); assert(s.m_tubeTranslations && s.m_tracks[6].isSecondary && s.m_activeTrackIndex == 1);
    second.clear(); prospero_subtitle_enabled = prospero_subtitle_count = 0;
    // No translation into the interface language: every translation is behind the row.
    interface_language = "en";
    Picker e; e.refreshTracks();
    assert((e.labels() == std::vector<std::string>{"SUBTITLES OFF", "English", "German (auto-generated)", "Auto-translate: other languages"}));
    assert(e.m_tracks[3].detail == "3 languages");
    // A video without translations has no Auto-translate rows.
    tracks.resize(2); Picker n; n.refreshTracks();
    assert((n.labels() == std::vector<std::string>{"SUBTITLES OFF", "English", "German (auto-generated)"}));
    std::puts("PASS: generated caption rows: automatic tracks labelled, Auto-translate into the interface language, other languages on demand, translation in use kept in sight");
}
'''
with tempfile.TemporaryDirectory(prefix="tube-caption-rows-") as temp:
    temp = Path(temp); (temp / "test.cpp").write_text(code)
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-fno-omit-frame-pointer", str(temp / "test.cpp"), "-o", str(temp / "test")], check=True)
    subprocess.run([str(temp / "test")], check=True)
