"""Execute the actual generated Quick Switch dispatch with safe UI stubs."""
import os
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
work=Path(os.environ.get('PROSPEROTUBE_NATIVE_WORK',root/'.deps/native-work'))/'projects/evoplayer'
source=(work/'core/src/screens/ProviderHostScreen.cpp').read_text()
a=source.index('    if (voiceAllowed && tube_voice_state()==TUBE_VOICE_IDLE) {')
b=source.index('    if (voiceAllowed) {',a)
block=source[a:b]
assert a < source.index('tube_afterglow_input(pressed, held);',a)
assert 'held & (PadButtons::L3 | PadButtons::TouchPad)' in (work/'core/src/Application.cpp').read_text()
code=r'''
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include "maxd/quick_switch.h"
namespace PadButtons {constexpr uint32_t TouchPad=1,Circle=2;}
static int voice,opened;static bool afterglow;
constexpr int TUBE_VOICE_IDLE=0;
static int tube_voice_state(){return voice;}
static bool tube_afterglow_shown(){return afterglow;}
static int tube_afterglow_open_quick_switch(){++opened;return 1;}
static void evo_rmlui_provider_push_folder(const char *id,const char *title){assert(!std::strcmp(id,"settings:quick-switch")&&!std::strcmp(title,"Quick Switch"));++opened;}
struct Host {MaxdQuickSwitch m_quickSwitch{};bool voiceAllowed=true;uint32_t seen=0;
bool input(uint32_t pressed,uint32_t held,uint32_t released) {
'''+block+r'''
seen=pressed;return false;
}};
int main(){
 for(afterglow=false;;afterglow=true){
  Host h;opened=0;assert(h.input(1,1,0));assert(!h.input(0,0,1)&&h.seen==1&&!opened);
  assert(h.input(1,1,0));h.m_quickSwitch.since-=700;
  assert(h.input(0,1,0)&&opened==1);assert(h.input(0,1,0)&&opened==1);assert(!h.input(0,0,1));
  assert(h.input(1,1,0));assert(!h.input(2,1,0)&&h.seen==2&&opened==1);
  h.voiceAllowed=false;assert(!h.input(1,1,0)&&opened==1);
  if(afterglow)break;
 }
}
'''
with tempfile.TemporaryDirectory(prefix='maxd-quick-switch-') as directory:
 path=Path(directory);(path/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(root/'native'),str(path/'test.cpp'),'-o',str(path/'test')],check=True)
 subprocess.run([str(path/'test')],check=True)
print('PASS: shipping Quick Switch Classic/Afterglow routing, held sampling, short-tap preservation, one-shot holds, Circle and unavailable-context guards')
