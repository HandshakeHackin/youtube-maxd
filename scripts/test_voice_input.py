"""Exercise the generated app's edge/hold dispatch with its real gesture helper."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WORK = Path(os.environ["PROSPEROTUBE_NATIVE_WORK"]) / "projects/evoplayer"
app = (WORK / "core/src/Application.cpp").read_text()
start = app.index("                if (hasInput || ((held & PadButtons::L3)")
end = app.index("\n                }", start) + len("\n                }")
dispatch = app[start:end]
host = (WORK / "core/src/screens/ProviderHostScreen.cpp").read_text()
allowed_start = host.index('    const bool voiceAllowed = m_providerId == "youtube"')
allowed = host[allowed_start:host.index(';', allowed_start) + 1]
assert host.index("maxd_voice_gesture(") < host.index("tube_afterglow_input(pressed, held)")
assert "tube_voice_state() == TUBE_VOICE_TRANSCRIBING || (frame < 10)" in app
code = r'''
#include "voice_gesture.h"
#include <cassert>
#include <cstdint>
#include <string>
namespace PadButtons { enum {L3=2}; }
enum class ScreenId {EmbyBrowse, Player, Settings};
enum class SignIn {Idle, Suggest, Signing};
static std::string m_providerId="youtube";
static bool m_opened=true,m_picking=false,m_web=false,m_resolving=false,m_tunePending=false,m_streamPick=false,m_discovering=false;
static SignIn m_signIn=SignIn::Idle;
static bool keyboard=false,panel=false,afterglow=false,afterglow_allowed=true;
bool evo_keyboard_is_open(){return keyboard;}
bool evo_rmlui_provider_panel_open(){return panel;}
bool tube_afterglow_shown(){return afterglow;}
bool tube_afterglow_voice_allowed(){return afterglow_allowed;}
bool canVoice(){
''' + allowed + r'''
 return voiceAllowed;
}
struct Manager {
 ScreenId at=ScreenId::EmbyBrowse;
 MaxdVoiceGesture gesture{};
 int64_t time=0;
 int result=0, calls=0;
 ScreenId getCurrentScreenId(){return at;}
 void handleInput(uint32_t pressed,uint32_t held,uint32_t released){
  ++calls; result=maxd_voice_gesture(&gesture,pressed&2,held&2,released&2,0,time);
 }
};
void frame(Manager*m_screenManager,bool hasInput,uint32_t pressed,uint32_t held,uint32_t released){
''' + dispatch + r'''
}
int main(){
 assert(canVoice()); panel=true; assert(!canVoice()); panel=false;
 keyboard=true; assert(!canVoice()); keyboard=false;
 afterglow=true; afterglow_allowed=false; assert(!canVoice()); afterglow_allowed=true; assert(canVoice());
 m_signIn=SignIn::Signing; assert(!canVoice()); m_signIn=SignIn::Idle;
 m_resolving=true; assert(!canVoice()); m_resolving=false;
 Manager m;
 frame(&m,true,2,2,0); assert(m.result==MAXD_VOICE_WAIT);
 m.time=200; frame(&m,false,0,2,0); assert(m.result==MAXD_VOICE_WAIT);
 m.time=350; frame(&m,false,0,2,0); assert(m.result==MAXD_VOICE_START);
 m.time=1000; frame(&m,true,2,2,0);
 m.time=1100; frame(&m,true,0,0,2); assert(m.result==MAXD_VOICE_TAP);
 m.at=ScreenId::Player; int calls=m.calls;
 frame(&m,false,0,2,0); assert(m.calls==calls+1); // playback voice commands also sample holds
 frame(&m,true,2,2,0); assert(m.calls==calls+2);
 m.at=ScreenId::Settings; calls=m.calls;
 frame(&m,false,0,2,0); assert(m.calls==calls); // unrelated screens remain edge-driven
 frame(&m,true,2,2,0); assert(m.calls==calls+1);
}
'''
with tempfile.TemporaryDirectory() as temp:
    source = Path(temp) / "test.cpp"
    source.write_text(code)
    binary = Path(temp) / "test"
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-I" + str(ROOT / "native/maxd"),
                    str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("PASS: native app dispatches held L3 between button edges, preserves taps/player input, and keeps voice status drawing")
