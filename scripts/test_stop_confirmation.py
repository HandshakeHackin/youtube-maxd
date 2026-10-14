"""Execute generated modal entry/actions/input for Afterglow and Classic."""
from pathlib import Path
import os
import subprocess
import tempfile

work = Path(os.environ.get('PROSPEROTUBE_NATIVE_WORK', Path.home() / '.cache/prosperotube/native'))
source = (work / 'projects/evoplayer/core/src/screens/ModalDialogScreen.cpp').read_text()

def method(name, next_name):
    start = source.index(name)
    return source[start:source.index(next_name, start)]

entry = method('void ModalDialogScreen::onEnter()', 'void ModalDialogScreen::onExit()')
action = method('void ModalDialogScreen::executeAction(', 'bool ModalDialogScreen::handleInput(')
inputs = method('bool ModalDialogScreen::handleInput(', 'void ModalDialogScreen::update(')
assert '#if defined(EVO_AGC_DEVICE)' in entry
assert '#if defined(EVO_AGC_DEVICE)\n#include "tube_afterglow.h"\n#endif' in source

code = r'''
#include <cassert>
#include <cstdint>
#include <string>
static bool afterglow;
#if defined(EVO_AGC_DEVICE)
static int tube_afterglow_enabled() { return afterglow; }
#endif
namespace evo {
enum class ModalType { ExitConfirm, ResumePrompt };
enum class ModalDialogEvent { Open };
enum class ScreenId { Player };
namespace PadButtons { constexpr unsigned Left=1,Right=2,Up=4,Down=8,Cross=16,Circle=32,Triangle=64; }
constexpr int EVO_FB_CANCEL=1,EVO_FB_CONFIRM=2,EVO_FB_MOVE=3;
static void evo_feedback(int) {}
struct StatefulScreen { void onEnter() {} };
struct Playback { int stops=0,starts=0;double at=-1;std::string path;
 void stopPlayback(){++stops;}void startPlayback(const std::string&p,double t){++starts;path=p;at=t;}} playback;
struct Manager { int backs=0,returns=0,players=0;
 void navigateBack(ScreenId){++backs;}void returnFromPlayback(){++returns;}void navigateTo(ScreenId){++players;}} manager;
struct Application { static Application&getInstance(){static Application a;return a;}
 Playback*getPlaybackController(){return &playback;}Manager*getScreenManager(){return &manager;} };
struct ModalDialogScreen : StatefulScreen {
 ModalType m_type;int m_focusedButton=-1;std::string m_resumePath="video";double m_resumeTimestamp=42;
 struct Fsm { void postEvent(ModalDialogEvent){} } m_modalFsm;
 explicit ModalDialogScreen(ModalType t):m_type(t){}
 void onEnter();void executeAction(int);bool handleInput(uint32_t,uint32_t,uint32_t);
};
'''+entry+action+inputs+r'''
static void reset(){playback=Playback{};manager=Manager{};}
static void press(ModalDialogScreen&d,unsigned p){assert(d.handleInput(p,0,0));}
}
int main(){using namespace evo;
 for(bool enabled:{false,true}) {
  afterglow=enabled;reset();ModalDialogScreen exit(ModalType::ExitConfirm);exit.onEnter();
#if defined(EVO_AGC_DEVICE)
  const int initial=enabled?0:1;
#else
  const int initial=1;
#endif
  assert(exit.m_focusedButton==initial);press(exit,PadButtons::Cross);
  assert(playback.stops==initial && manager.returns==initial && manager.backs==1-initial);
  for(unsigned direction:{PadButtons::Left,PadButtons::Right,PadButtons::Up,PadButtons::Down}) {
   reset();exit.onEnter();press(exit,direction);assert(exit.m_focusedButton==1-initial);press(exit,PadButtons::Cross);
   assert(playback.stops==1-initial && manager.returns==1-initial && manager.backs==initial);
  }
  reset();exit.onEnter();press(exit,PadButtons::Circle);assert(!playback.stops && manager.backs==1);
  // Re-entry resets a previous selection rather than keeping Stop selected.
  press(exit,PadButtons::Left);exit.onEnter();assert(exit.m_focusedButton==initial);
  reset();ModalDialogScreen resume(ModalType::ResumePrompt);resume.onEnter();assert(resume.m_focusedButton==0);
  press(resume,PadButtons::Cross);assert(playback.starts==1 && playback.at==42 && playback.path=="video" && manager.players==1 && !playback.stops);
  reset();resume.onEnter();press(resume,PadButtons::Right);press(resume,PadButtons::Cross);assert(playback.starts==1 && playback.at==0 && manager.players==1);
  reset();resume.onEnter();press(resume,PadButtons::Circle);assert(playback.starts==1 && playback.at==0 && manager.players==1);
  reset();resume.onEnter();press(resume,PadButtons::Triangle);assert(manager.returns==1 && !playback.starts);
  assert(!resume.handleInput(0,0,0));
 }
}
'''
with tempfile.TemporaryDirectory(prefix='tube-stop-focus-') as temp:
    temp = Path(temp)
    (temp / 'test.cpp').write_text(code)
    for define in ([], ['-DEVO_AGC_DEVICE=1']):
        binary = temp / ('device' if define else 'host')
        subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer', *define,
                        str(temp / 'test.cpp'), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
print('PASS: generated stop entry/input uses Keep watching in Afterglow; Classic, Circle and resume actions retain their behavior; device and host guards pass')
