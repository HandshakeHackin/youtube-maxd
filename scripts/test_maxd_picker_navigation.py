#!/usr/bin/env python3
"""Run the generated picker's real Back branches with minimal UI stubs.
SPDX-License-Identifier: GPL-3.0-or-later
"""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
work = Path(subprocess.check_output(['python3', str(root/'scripts/prepare_native.py')], text=True).strip())
text = (work/'projects/evoplayer/core/src/screens/AudioTrackPickerScreen.cpp').read_text()
start = text.index('        if (m_tubeExtraGroup == -7) {')
end = text.index('        if (m_tubeExtraGroup != -1) {', start)
rescue = text[start:end]
start = end
end = text.index('// End YouTube Max’d extra option back.', start)
back = text[start:end]
source = r'''
#include <cassert>
#include <vector>
#define EVO_RMLUI_LIST_ROWS 12
enum { KindExtraGroup, KindChapterList, KindMaxdTools, KindMaxdHealth, Notice, Lower, Safe };
struct Entry { int kind,streamIndex; };
struct Picker {
 int m_tubeExtraGroup=-1,m_selectedIndex=0,m_activeIndex=0,m_scrollOffset=0;
 bool m_maxdRescueSafe=false;
 std::vector<Entry> m_tracks;
 void refreshTracks() {
  if(m_tubeExtraGroup==-6)m_tracks={{Notice,0},{Lower,0},{Safe,0},{Notice,0}};
  else m_tracks={{KindExtraGroup,0},{KindChapterList,0},{KindMaxdTools,0},{KindMaxdHealth,0}};
 }
 bool goBack() {
''' + rescue + back + r'''
 return false;
 }
};
int main() {
 Picker p;
 p.m_tubeExtraGroup=-5;assert(p.goBack());assert(p.m_tubeExtraGroup==-1 && p.m_selectedIndex==2);
 p.m_tubeExtraGroup=-6;assert(p.goBack());assert(p.m_tubeExtraGroup==-1 && p.m_selectedIndex==3);
 p.m_tubeExtraGroup=-7;assert(p.goBack());assert(p.m_tubeExtraGroup==-6 && p.m_selectedIndex==1);
 p.m_tubeExtraGroup=-7;p.m_maxdRescueSafe=true;assert(p.goBack());assert(p.m_tubeExtraGroup==-6 && p.m_selectedIndex==2);
 assert(p.goBack());assert(p.m_tubeExtraGroup==-1 && p.m_selectedIndex==3);
 p.m_tubeExtraGroup=-2;assert(p.goBack());assert(p.m_selectedIndex==1);
}
'''
with tempfile.TemporaryDirectory(prefix='maxd-picker-nav-') as directory:
    src = Path(directory)/'test.cpp'
    binary = Path(directory)/'test'
    src.write_text(source)
    subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-Wall','-Wextra','-Werror',str(src),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print('PASS: actual picker Back branches restore transcript/health focus and cancel rescue into its parent panel')
