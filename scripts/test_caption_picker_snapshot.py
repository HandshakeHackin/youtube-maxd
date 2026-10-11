"""Compile the actual generated native picker snapshot, including engine tags."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
work = Path(os.environ.get('PROSPEROTUBE_NATIVE_WORK', str(Path.home()/'.cache/prosperotube/native')))
source = (work/'projects/evoplayer/ui_kit/src/evo_hui_modals.cpp').read_text()
start = source.index('bool SubtitlePicker::focus(FocusInfo *out) const')
end = source.index('/* ------------------------------------------------------------ media info */', start)
out = root/'build/tests/caption-picker-snapshot'
out.mkdir(parents=True, exist_ok=True)
fixture = r'''
#include <cassert>
#include <string>
#include <vector>
#include <utility>
struct FocusInfo {
    std::vector<std::pair<std::string,std::string>> focusable;
    std::string id,text;int index=0,total=0,rect=0;
};
struct Track {std::string label,tag;bool current=false,focused=false;};
struct SubtitlePicker {
    std::vector<Track> tracks_;
    int track_rect(int i) const {return i;}
    bool focus(FocusInfo*) const;
};
'''+source[start:end]+r'''
int main() {
    SubtitlePicker p;FocusInfo f;
    p.tracks_={{"English","PRIMARY",true,false},{"French","SECONDARY",false,true}};
    assert(p.focus(&f) && f.text=="French [SECONDARY]" && f.index==2 && f.rect==1);
    assert(f.focusable.size()==2 && f.focusable[1].second=="French");
    p.tracks_[1].tag="TRANSLATED";assert(p.focus(&f) && f.text=="French [TRANSLATED]");
    p.tracks_[1].tag.clear();assert(p.focus(&f) && f.text=="French");
    p.tracks_[1].focused=false;p.tracks_[0].focused=true;
    assert(p.focus(&f) && f.text=="English (current) [PRIMARY]");
    p.tracks_[0].focused=false;assert(!p.focus(&f) && f.focusable.empty());
}
'''
(out/'test.cpp').write_text(fixture)
subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g',str(out/'test.cpp'),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True)
# The generated picker: Off is remembered for the channel, and a translated
# track is tagged only while it is neither line of dialogue.
picker = (work/'projects/evoplayer/core/src/screens/SubtitlePickerScreen.cpp').read_text()
off = picker.index('    } else if (trackId == -2) {')
off = picker[off:picker.index('    } else if (!prospero_subtitle_select_primary(trackId)) {', off)]
assert off.count('prospero_subtitle_enabled = 0;') == 1 and off.count('tube_remember_captions(tubeVideo, 0);') == 1
assert off.index('tube_phone_video(tubePlayback->getCurrentFilePath().c_str(), tubeVideo)') < off.index('tube_remember_captions(tubeVideo, 0);')
assert picker.count('tube_remember_captions(') == 1
tags = picker.index('params.tracks[i].tag = m_tracks[idx].isSecondary ? "SECONDARY"')
translated = picker.index('if (!*params.tracks[i].tag && m_tracks[idx].trackId <= -1000 && tube_caption_track_translated(-1000 - m_tracks[idx].trackId))', tags)
assert picker.index('params.tracks[i].tag = "TRANSLATED";', translated) < picker.index('if (m_tracks[idx].trackId == kAutoSyncTrack) {', translated)
assert picker.count('if (tube_caption_track_translated(i)) track.detail += " (translated)";') == 1
print('PASS: actual native picker snapshot exposes primary/secondary/translated tags, preserves focus semantics and remembers Off')
