"""Drive the generated pickers' actual pad handlers and grouping/tab blocks."""
from pathlib import Path
import os, subprocess
root = Path(__file__).resolve().parents[1]
work = Path(subprocess.check_output(["python3",str(root/"scripts/prepare_native.py")],text=True).strip())
player = work/"projects/evoplayer"
out = root/"build/tests/afterglow-pickers"
out.mkdir(parents=True,exist_ok=True)

def function(source, signature):
    a=source.index(signature); b=source.index("{",a); depth=1; i=b+1
    while depth:
        depth += (source[i]=="{")-(source[i]=="}"); i+=1
    return source[a:i]

audio=(player/"core/src/screens/AudioTrackPickerScreen.cpp").read_text()
sub=(player/"core/src/screens/SubtitlePickerScreen.cpp").read_text()
# Full actual handlers/navigators, the grouping methods, and the existing secondary/size actions.
methods="\n".join(function(audio,f"{result} AudioTrackPickerScreen::{name}") for result,name in [
    ("int","afterglowGroup("),("const char*","afterglowGroupTitle("),("std::string","afterglowChoiceLabel("),
    ("bool","afterglowCurrent("),("void","groupAfterglowTracks("),("void","navigate("),("bool","handleInput(")])
methods += "\n"+"\n".join(function(sub,f"{result} SubtitlePickerScreen::{name}") for result,name in [
    ("void","navigate("),("void","activateSecondary("),("void","cycleSize("),("bool","handleInput("),("void","toggleTubeTranslations(")])
activation=function(audio,"void AudioTrackPickerScreen::activateSelection(")
prefix=activation[:activation.index("    const Entry e =")]
speed=function(activation,"else if (e.kind == KindSpeed)")
speed=speed[speed.index("{"):]
methods += "\n"+prefix+"\n    (void)sm; const Entry e=m_tracks[m_selectedIndex]; if (e.kind==KindSpeed) "+speed+"\n}\n"
filter=function(sub,"if (tube_afterglow_picker_active() && m_afterglowTab == 1)")
(out/"evo/screens").mkdir(parents=True,exist_ok=True)
(out/"evo/screens/StatefulScreen.hpp").write_text('''#pragma once
#include <cstdint>
namespace evo { enum class ScreenId { Player,AudioTrackPicker,SubtitlePicker };
class StatefulScreen { public: explicit StatefulScreen(const char*){} virtual ~StatefulScreen()=default;
virtual ScreenId getScreenId() const=0; virtual void onEnter(){} virtual void onExit(){}
virtual bool handleInput(uint32_t,uint32_t,uint32_t)=0; virtual void update(double){}
virtual void render(uint32_t*,int,int)=0; }; }
''')
scaffold=r"""
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <utility>
#define private public
#include "evo/screens/AudioTrackPickerScreen.hpp"
#include "evo/screens/SubtitlePickerScreen.hpp"
#undef private
namespace PadButtons { enum : uint32_t { Up=1,Down=2,Cross=4,Circle=8,Triangle=16,Square=32,L1=64,R1=128,Left=256,Right=512,TouchPadLeft=1024,TouchPadRight=2048,L2=4096 }; }
enum { EVO_FB_MOVE,EVO_FB_BOUNDARY,EVO_FB_CANCEL,EVO_FB_CONFIRM,EVO_FB_ERROR,EVO_RMLUI_LIST_ROWS=8,TUBE_PLAYER_ACTIONS=0,TUBE_MENU_OPENED=1,PROSPERO_SECONDARY_NONE=-2 };
static bool enabled=true;
static int tube_afterglow_picker_active(){return enabled;}
static void evo_feedback(int){}
static void toast(const char*,const char*){}
static int tube_player_menu_root(){return 1;}
static int tube_player_menu_begin(int){return 0;}
static void tube_player_menu_close(){}
static const char* tube_caption_track_language(int n){static const char* values[]={"en","es","de"};assert(n>=0&&n<3);return values[n];}
static std::string secondary;
static int secondaryRequests=0,primaryRequests=0,primarySync=0,secondarySync=0,prospero_subtitle_face=2;
static int prospero_secondary_subtitle_active(){return !secondary.empty();}
static int tube_caption_secondary_begin(const char* language){secondary=language;++secondaryRequests;return 1;}
static void tube_caption_secondary_clear(){secondary.clear();}
static void tube_secondary_subtitle_clear(){secondary.clear();}
static int prospero_secondary_subtitle_select(int){secondary.clear();return 1;}
static void prospero_secondary_nudge_delay(int delta){secondarySync+=delta;}
static void prospero_subtitle_nudge_delay(int delta){primarySync+=delta;}
namespace evo {
struct Playback {bool pinned=false; bool isVideoQualityPinned(){return pinned;} int speed=100; int getActiveAudioStream(){return 3;} bool changePlaybackSpeed(int value){speed=value;return true;} };
struct Screens {int backs=0; bool navigateBack(){++backs;return true;} void navigateTo(ScreenId){} };
struct Application {Playback playback;Screens screens;static Application& getInstance(){static Application app;return app;} Playback* getPlaybackController(){return &playback;} Screens* getScreenManager(){return &screens;} };
static std::vector<AudioTrackPickerScreen::Entry> fixture;
AudioTrackPickerScreen::AudioTrackPickerScreen():StatefulScreen("audio"){}
void AudioTrackPickerScreen::onEnter(){} void AudioTrackPickerScreen::onExit(){}
void AudioTrackPickerScreen::update(double){} void AudioTrackPickerScreen::render(uint32_t*,int,int){}
void AudioTrackPickerScreen::refreshTracks(){m_tracks=fixture;m_activeIndex=0;groupAfterglowTracks();}
SubtitlePickerScreen::SubtitlePickerScreen():StatefulScreen("captions"){}
void SubtitlePickerScreen::onEnter(){} void SubtitlePickerScreen::onExit(){}
void SubtitlePickerScreen::update(double){} void SubtitlePickerScreen::render(uint32_t*,int,int){}
void SubtitlePickerScreen::activateSelection(){++primaryRequests;}
"""
scaffold+="\nvoid SubtitlePickerScreen::refreshTracks(){\n"+r"""
    m_tracks={{-2,"Off","",false},{kAutoSyncTrack,"Auto-sync","",false},
        {-1000,"English","",secondary=="en"},{-1001,"Español","",secondary=="es"},{-1002,"Deutsch","",secondary=="de"}};
    m_activeTrackIndex=2;
"""+filter+"\n}\n"+methods+"\n}\n"
scaffold+=r"""
int main(){
    using A=evo::AudioTrackPickerScreen;
    auto make=[](A::Kind kind,int index,const char* label,const char* badge="") { A::Entry e;e.kind=kind;e.streamIndex=index;e.label=label;e.badge=badge;return e; };
    evo::fixture={make(A::KindAuto,-1,"AUTO","PLAYING"),make(A::KindVideo,1,"1080p","PLAYING"),make(A::KindAudio,3,"English"),
        make(A::KindSpeed,100,"Speed: 1x","PLAYING"),make(A::KindSpeed,125,"Speed: 1.25x"),make(A::KindSpeed,200,"Speed: 2x"),
        make(A::KindTubeAction,0,"Video actions")};
    A picker; picker.refreshTracks(); assert(picker.m_tracks.size()==4 && picker.m_tracks[0].label=="Quality: AUTO");
    evo::Application::getInstance().playback.pinned=true;picker.refreshTracks();assert(picker.m_tracks[0].label=="Quality: 1080p");
    evo::Application::getInstance().playback.pinned=false;picker.refreshTracks();
    int speed=-1;
    for(int i=0;i<(int)picker.m_tracks.size();++i)if(picker.m_tracks[i].streamIndex==A::KindSpeed&&picker.m_tracks[i].kind==A::KindAfterglowGroup)speed=i;
    assert(speed>=0 && picker.m_tracks[speed].label=="Speed: 1x");
    picker.m_selectedIndex=speed;
    assert(picker.handleInput(PadButtons::Cross,0,0));
    assert(picker.m_afterglowGroup==A::KindSpeed && picker.m_tracks.size()==3 && picker.m_selectedIndex==0);
    assert(picker.handleInput(PadButtons::Down,0,0));
    assert(picker.handleInput(PadButtons::Cross,0,0));
    assert(evo::Application::getInstance().playback.speed==125);
    assert(picker.handleInput(PadButtons::Circle,0,0));
    assert(picker.m_afterglowGroup==-1 && picker.m_tracks[picker.m_selectedIndex].streamIndex==A::KindSpeed);
    evo::fixture.erase(std::remove_if(evo::fixture.begin(),evo::fixture.end(),[](const A::Entry& e){return e.kind==A::KindSpeed;}),evo::fixture.end());
    picker.refreshTracks(); for(const auto& e:picker.m_tracks)assert(e.streamIndex!=A::KindSpeed || e.kind!=A::KindAfterglowGroup);
    enabled=false;picker.refreshTracks();assert(picker.m_tracks.size()==evo::fixture.size());enabled=true;
    evo::SubtitlePickerScreen captions;captions.refreshTracks();assert(captions.m_tracks.size()==5);
    assert(captions.handleInput(PadButtons::R1,0,0));
    assert(captions.m_afterglowTab==1 && captions.m_tracks.size()==4 && captions.m_selectedIndex==0);
    assert(captions.handleInput(PadButtons::Down,0,0));
    assert(captions.handleInput(PadButtons::Down,0,0));
    assert(captions.handleInput(PadButtons::Cross,0,0));assert(secondary=="es" && secondaryRequests==1 && primaryRequests==0);
    assert(captions.handleInput(PadButtons::Right,0,0));assert(secondarySync==100 && primarySync==0);
    assert(captions.handleInput(PadButtons::Square,0,0));assert(prospero_subtitle_face==3);
    assert(captions.handleInput(PadButtons::L1,0,0));assert(captions.m_afterglowTab==0 && captions.m_tracks.size()==5);
    assert(captions.handleInput(PadButtons::Cross,0,0));assert(primaryRequests==1);
    assert(captions.handleInput(PadButtons::Left,0,0));assert(primarySync==-100 && secondarySync==100);
    captions.handleInput(PadButtons::R1,0,0);captions.m_selectedIndex=0;captions.handleInput(PadButtons::Cross,0,0);assert(secondary.empty());
    enabled=false;captions.m_afterglowTab=0;captions.refreshTracks();captions.handleInput(PadButtons::R1,0,0);
    assert(captions.m_afterglowTab==0); // Classic retains its existing secondary-sync binding.
    puts("PASS: generated picker input, real grouped speed selection/back/availability, language tabs/selection/off/sync/size, Classic fallback");
}
"""
(out/"test.cpp").write_text(scaffold)
subprocess.run(["g++","-std=c++17","-Wall","-Wextra","-Werror","-g","-O1","-fsanitize=address,undefined","-fno-omit-frame-pointer",
    "-I"+str(out),"-I"+str(player/"core/include"),str(out/"test.cpp"),"-o",str(out/"test")],check=True)
subprocess.run([str(out/"test")],check=True,env={**os.environ,"ASAN_OPTIONS":"detect_leaks=1"})
