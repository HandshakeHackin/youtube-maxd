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
    p.tracks_[1].tag.clear();assert(p.focus(&f) && f.text=="French");
    p.tracks_[1].focused=false;p.tracks_[0].focused=true;
    assert(p.focus(&f) && f.text=="English (current) [PRIMARY]");
    p.tracks_[0].focused=false;assert(!p.focus(&f) && f.focusable.empty());
}
'''
(out/'test.cpp').write_text(fixture)
subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g',str(out/'test.cpp'),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True)
print('PASS: actual native picker snapshot exposes primary/secondary tags and preserves focus semantics')
