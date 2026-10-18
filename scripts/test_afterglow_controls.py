"""Run generated Afterglow shortcut blocks, aliases and displaced detail actions."""
from pathlib import Path
import json, os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
work=Path(os.environ['PROSPEROTUBE_NATIVE_WORK'])/'projects/evoplayer'
screen=(work/'core/src/screens/PlayerScreen.cpp').read_text().replace('YouTube Max’d','ProsperoTube')
audio=(work/'core/src/screens/AudioTrackPickerScreen.cpp').read_text().replace('YouTube Max’d','ProsperoTube')
def part(source,start,end):
 a=source.index(start);return source[a:source.index(end,a)]
mapcode=part(screen,'#if defined(EVO_AGC_DEVICE)\n    // ProsperoTube H1:', '    // ProsperoTube D-pad actions')
aliases=part(screen,'    if (pressed & PadButtons::Down) {','    /*\n     * Subtitle delay sync:')
offer=part(screen,'#if defined(EVO_AGC_DEVICE)\n    if (tube_afterglow_enabled()', '    if (pressed && (tube_playlist_eof_pending()')
rows=part(audio,'    // ProsperoTube H1 details:', '    // End ProsperoTube H1 details rows.')
actions=part(audio,'    // ProsperoTube H1 details actions.', '    // End ProsperoTube H1 details actions.')
# Those dispatch paths must stay before the remap, in the same live PlayerScreen.
for marker in ('tube_next_offer_input(', 'tube_sponsor_confirm(', 'tube_sponsor_undo_started(',
               'playback->cancelScrub();', 'playback->confirmScrub()', 'if (pressed & PadButtons::Circle) {'):
 assert screen.index(marker)<screen.index('// ProsperoTube H1:')
mount=(root/'native/afterglow/player/app/mount.cpp').read_text()
for button,label in [('triangle','Up next'),('square','Captions'),('options','Options')]:
 assert f'ui::Button::{button}, ui::Button::none, tr("{label}")' in mount
# Classic draws the same Triangle meaning in both of its control rows.
rml=(work/'assets/rml/playback.rml').read_text();kit=(work/'ui_kit/src/evo_hui_osd.cpp').read_text()
assert 'btn_triangle.png' in part(rml,'<div id="controls-row">','</body>') and '>UP NEXT</span>' in rml and 'ASPECT: FIT' not in rml
assert 'hints[n++] = {ui::Button::triangle, "Up next"};' in kit
for name in ("ridge", "live"):
 fixture=json.loads((root/f"tests/afterglow/fixtures/signed-in/player/{name}.json").read_text())
 hints={item["button"]:item["label"] for item in fixture["hints"]}
 assert hints["triangle"]=="Up next" and hints["square"]=="Captions" and hints["options"]=="Options"
code=r'''
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
#include <utility>
namespace PadButtons{enum:unsigned{Triangle=1,Square=2,Options=4,Down=8,R2=16,Cross=32,Circle=64,Left=128,Right=256,Up=512};}
enum class ScreenId{Player,SubtitlePicker,AudioTrackPicker,MediaInfo};
enum class ViewMode{Fit,Fill,Stretch};
static bool enabled=true,phone=false,ask=false,undo=false,offered=false;
static int requested=-1,stats=0;
static constexpr int TUBE_PICKER_OPTIONS=0,TUBE_PICKER_SUGGESTIONS=1;
static int tube_phone_owns_media(){return phone;}
static void tube_extra_picker_request(int n){requested=n;}
#if defined(EVO_AGC_DEVICE)
static int tube_afterglow_enabled(){return enabled;}
struct tube_sponsor_skip_t{};
static int tube_sponsor_last_skip(tube_sponsor_skip_t*){return undo;}
static const char*tube_sponsor_prompt(double){return ask?"ask":nullptr;}
static int tube_next_offer_input(int,bool,bool){return offered;}
#endif
static int tube_afterglow_picker_active(){return enabled;}
static void toast(const char*,const char*){}
static void toggleStatsForNerds(){++stats;}
static bool m_showStatsForNerds;
struct Base{virtual ~Base()=default;};
struct PlayerScreen:Base{void toggleStatsForNerds(){++stats;}} playerScreen;
struct Playback{bool scrub=false;int aspects=0;bool isScrubbing(){return scrub;}double getPositionSeconds(){return 1;}
 void cycleViewMode(){++aspects;}ViewMode getViewMode(){return ViewMode::Fit;}} controller;
struct Manager{int navigations=0,backs=0;ScreenId at=ScreenId::Player;
 void navigateTo(ScreenId s){++navigations;at=s;}void navigateBack(ScreenId s){++backs;at=s;}
 Base*getScreen(ScreenId){return &playerScreen;}} manager;
static Playback*playback=&controller;static Manager*screenMgr=&manager;
static bool dispatch(unsigned pressed){
'''+offer+mapcode+aliases+r'''
 return false;
}
enum Kind{KindAfterglowDetails,KindAfterglowStats,KindOther};
struct Entry{Kind kind=KindOther;std::string label;};static std::vector<Entry>m_tracks;
static void makeRows(){m_tracks.clear();
'''+rows+r'''
}
static void activate(Kind kind){auto sm=&manager;Entry e;e.kind=kind;
'''+actions+r'''
}
static void reset(){controller=Playback{};manager=Manager{};phone=ask=undo=offered=false;requested=-1;stats=0;}
int main(){
 for(bool mode:{false,true}){enabled=mode;
#if defined(EVO_AGC_DEVICE)
 const bool mapped=mode;
#else
 const bool mapped=false;
#endif
 for(unsigned key:{PadButtons::Triangle,PadButtons::Square,PadButtons::Options}){
  reset();assert(dispatch(key));
  if(mapped){assert(controller.aspects==0&&stats==0&&manager.navigations==1);
   assert(manager.at==(key==PadButtons::Square?ScreenId::SubtitlePicker:ScreenId::AudioTrackPicker));
   assert(requested==(key==PadButtons::Triangle?TUBE_PICKER_SUGGESTIONS:key==PadButtons::Options?TUBE_PICKER_OPTIONS:-1));
  }else{assert(controller.aspects==0);assert(stats==(key==PadButtons::Options));
   // Classic: Triangle lists the suggested videos; video sizing lives in the playback options.
   assert(manager.navigations==(key!=PadButtons::Options));
   if(key==PadButtons::Square)assert(manager.at==ScreenId::MediaInfo);
   if(key==PadButtons::Triangle){assert(manager.at==ScreenId::AudioTrackPicker&&requested==TUBE_PICKER_SUGGESTIONS);
    reset();phone=true;assert(dispatch(key));assert(!manager.navigations&&!controller.aspects&&requested==-1);
    reset();controller.scrub=true;assert(dispatch(key));assert(!manager.navigations&&!controller.aspects&&requested==-1);}}
#if defined(EVO_AGC_DEVICE)
  if(mapped)for(int owner=0;owner<5;++owner){reset();controller.scrub=owner==0;phone=owner==1;ask=owner==2;undo=owner==3;offered=owner==4;
   assert(dispatch(key));assert(!manager.navigations&&!controller.aspects&&!stats&&requested==-1);}
#endif
 }
 for(unsigned key:{PadButtons::Down,PadButtons::R2}){reset();assert(dispatch(key));
  assert(manager.at==(key==PadButtons::Down?ScreenId::SubtitlePicker:ScreenId::AudioTrackPicker));
  reset();controller.scrub=true;assert(dispatch(key));assert(!manager.navigations);}
 makeRows();assert(m_tracks.size()==(mode?2u:0u));
 if(mode){assert(m_tracks[0].label=="Details"&&m_tracks[1].label=="Playback details");reset();activate(KindAfterglowDetails);
  assert(manager.at==ScreenId::MediaInfo&&!stats);reset();activate(KindAfterglowStats);
  assert(stats==1&&manager.backs==1&&manager.at==ScreenId::Player);activate(KindAfterglowStats);assert(stats==2);}
 }
}
'''
with tempfile.TemporaryDirectory(prefix='tube-afterglow-map-') as temp:
 temp=Path(temp);(temp/'test.cpp').write_text(code)
 for define in ([],['-DEVO_AGC_DEVICE=1']):
  binary=temp/('device' if define else 'host')
  subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',*define,str(temp/'test.cpp'),'-o',str(binary)],check=True)
  subprocess.run([str(binary)],check=True)
print('PASS: generated Afterglow map, Classic bindings (Triangle = suggested videos), aliases, modal ownership, hints and media/statistics actions; host/device guards')
