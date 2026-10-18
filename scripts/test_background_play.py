"""Run generated browse entry, ownership, hidden lifecycle, EOF and return input."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
work=Path(os.environ['PROSPEROTUBE_NATIVE_WORK'])/'projects/evoplayer'
app=(work/'core/src/Application.cpp').read_text().replace('YouTube Max’d','ProsperoTube')
screen=(work/'core/src/screens/PlayerScreen.cpp').read_text().replace('YouTube Max’d','ProsperoTube')
host=(work/'core/src/screens/ProviderHostScreen.cpp').read_text().replace('YouTube Max’d','ProsperoTube')
picker=(work/'core/src/screens/AudioTrackPickerScreen.cpp').read_text().replace('YouTube Max’d','ProsperoTube')
controller_source=(work/'core/src/services/PlaybackController.cpp').read_text().replace('YouTube Max’d','ProsperoTube')
def part(s,a,b):return s[s.index(a):s.index(b,s.index(a))]
appcode=part(app,'        pp_video_frame tube_present_source{}','        // End ProsperoTube background ownership.')
# Family time and controller feedback are tested independently. Keep the actual
# lease/hidden-player code around the unchanged screen-manager update.
a=appcode.index('        if(tube_maxd_family_tick(')
b=appcode.index('        m_screenManager->update(frameDeltaMs);',a)
appcode=appcode[:a]+appcode[b:]
a=appcode.index('        // YouTube Max\'d: use the existing native pad')
b=appcode.index('        // Browser screens do not tick PlayerScreen.',a)
appcode=appcode[:a]+appcode[b:]
retirement=part(app,'            if (tube_hidden_serial && evo_agc_runtime_last_present_retired())','            // End ProsperoTube hidden discard.')
assert app.index(retirement)>app.index('            evo_agc_runtime_present();')
eof=part(screen,'    if (tube_background_play_active()) {','    // End ProsperoTube background completion.')
entry=part(picker,'        tube_next_offer_t nextOffer;\n        if (tube_afterglow_picker_active()', '    } else if (e.kind == KindVideoNavigation)')
sound=(root/'native/afterglow/sound/app/sound_app.cpp').read_text()
sound_context=next(line for line in sound.splitlines() if 'step.situation.playing =' in line)
back=part(host,'        if (tube_background_play_active() && pressed==PadButtons::L3','        tube_afterglow_input(pressed, held);')
# Keep supported voice/keyboard/questions ahead of the new return action.
assert host.index('voice == TUBE_VOICE_RECORDING')<host.index(back)
assert host.index('if (m_discovering) return true;')<host.index(back)
update=part(screen,'void PlayerScreen::update(','void PlayerScreen::feedPerformanceHud')
assert update.count('sm->returnFromPlayback();')==5
assert update.count('if (!tube_background_play_active()) sm->returnFromPlayback();')==4
assert update.index(eof)<update.index('tube_completion_playback_state(')<update.index('tube_finish_drained_and_next(')
assert 'if (!tube_background_play_active() && tube_queue_recovery(' in update
assert 'else if (!tube_background_play_active() && tube_buffering_tick(' in update
launch=part(host,'int launch_choice(int idx)','bool choice_is_hls')
assert launch.index('tube_background_play_clear();')<launch.index('p->stream_chosen(')<launch.index('pthread_create(')
assert 'if (backgroundReplace)' in launch

# Compile the actual command-entry invalidation statements as well as return/EOF paths.
commands=[]
for signature in ('void PlaybackController::setPaused(bool paused)',
                  'void PlaybackController::stopPlayback()',
                  'void PlaybackController::beginScrub()',
                  'bool PlaybackController::seekTo(double targetSeconds, bool keepPaused)'):
    commands.append(part(controller_source,signature+' {', '\n    if (').split('{',1)[1])
assert 'setPaused(!isPaused());' in part(controller_source,'void PlaybackController::togglePause()', 'void PlaybackController::setPaused(')
source=r"""
#include "tube_background_play.h"
#include <cassert>
#include <atomic>
#include <thread>
#include <cstdint>
#include <string>
static unsigned selection=7;static std::string profile="guest";
static bool afterglow=true,phone=false,offer=false,ask=false,pending=false,keyboard=false,canreturn=true;
static unsigned tube_caption_selection(){return selection;}
static const char*tube_browse_profile(){return profile.c_str();}
static int tube_afterglow_enabled(){return afterglow;}static int tube_afterglow_picker_active(){return afterglow;}
static int tube_phone_owns_media(){return phone;}
struct tube_next_offer_t{};static int tube_next_offer(tube_next_offer_t*){return offer;}
static int tube_manual_navigation_pending(){return pending;}static int tube_playlist_eof_pending(){return pending;}
static const char*tube_sponsor_prompt(double){return ask?"ask":nullptr;}
static int evo_keyboard_is_open(){return keyboard;}static int tube_afterglow_background_return_allowed(){return canreturn;}
struct Controller{bool active=true,paused=false,scrub=false;int stops=0;std::string title="Video title";
 bool isActive(){return active;}bool isPaused(){return paused;}bool isScrubbing(){return scrub;}
 void stopPlayback(){"""+commands[1]+r"""++stops;active=false;}
 void setPaused(bool b){"""+commands[0]+r"""paused=b;}
 void beginScrub(){"""+commands[2]+r"""scrub=true;}
 void seek(){"""+commands[3]+r"""}
 void togglePause(){setPaused(!isPaused());}
 double getPositionSeconds(){return 1;}const std::string&getDisplayTitle(){return title;}} controller;
static Controller*m_playbackController=&controller;
static bool drained=false,sponsor=false,completionBusy=false;static int completion_consumed;
static void hidden_eof(){auto playback=&controller;
 const bool ended=(drained && !controller.paused) || sponsor;
"""+eof+r"""
 ++completion_consumed;
}
enum class ScreenId{Player,Browser};
struct Hidden{int updates=0;bool fail=false;void update(double){++updates;if(fail)controller.stopPlayback();else hidden_eof();}} hidden;
struct Manager{ScreenId current=ScreenId::Browser;int returns=0,foreground=0,updates=0;
 ScreenId getCurrentScreenId(){return current;}Hidden*getScreen(ScreenId){return &hidden;}
 void update(double){++updates;}void returnFromPlayback(){++returns;current=ScreenId::Browser;}
 void navigateTo(ScreenId s){++foreground;current=s;}} manager;
static Manager*m_screenManager=&manager;
namespace Application{struct App{Manager*getScreenManager(){return &manager;}Controller*getPlaybackController(){return &controller;}};static App&getInstance(){static App a;return a;}}
struct pp_video_frame{const void*tube_lease=nullptr;int ready=1;unsigned tube_epoch=2,tube_serial=3;};static int g_pp_pb,fetches,retired;
static int pp_playback_get_video_frame(int*,pp_video_frame*f){++fetches;f->ready=1;return 1;}
static void pp_playback_note_retired(int*,unsigned e,unsigned s){assert(e==2&&s==3);++retired;}
static bool m_softClose=false;static double frameDeltaMs=16;
static bool present_retired=true;static int evo_agc_runtime_last_present_retired(){return present_retired;}
static void tick(){
"""+appcode+retirement+r"""
}
enum {KindBackgroundBrowse,KindMaxdPip};static struct{int kind=KindBackgroundBrowse;}e;
static void enter(){auto playback=&controller;auto sm=&manager;
"""+entry+r"""
}
namespace PadButtons{constexpr unsigned L3=2,Cross=1;}
static bool back(unsigned pressed){
"""+back+r"""
 return false;
}
static bool sounds_playing(){struct{struct{bool playing=false;}situation;}step;
"""+sound_context+r"""
 return step.situation.playing;
}
static void start(){controller.active=true;controller.paused=false;controller.scrub=false;drained=sponsor=completionBusy=false;selection=7;profile="guest";enter();assert(tube_background_play_active());}
int main(){
    start();assert(tube_background_play_take_browser_resume()&&!tube_background_play_take_browser_resume());assert(manager.returns==1&&controller.stops==0);tick();assert(hidden.updates==1&&fetches==1&&retired==1);
    assert(std::string(tube_background_play_title())=="Video title");assert(sounds_playing());
    int ack=retired;present_retired=false;tick();assert(retired==ack);present_retired=true;tick();assert(retired==ack+1);
    drained=true;tick();assert(controller.paused&&completion_consumed==3&&tube_background_play_active());
    // Paused natural EOF no longer reports ended; it still must retain our pause ownership.
    tick();tick();assert(completion_consumed==5&&manager.foreground==0&&!sounds_playing());
    keyboard=true;assert(!back(PadButtons::L3));keyboard=false;
    canreturn=false;assert(!back(PadButtons::L3));canreturn=true;
    assert(!back(PadButtons::L3|PadButtons::Cross));assert(back(PadButtons::L3));
    assert(!tube_background_play_active()&&manager.current==ScreenId::Player&&controller.active&&!controller.paused);
    assert(drained);hidden_eof();assert(completion_consumed==6);
    // Natural EOF and SponsorBlock EOF return only the pause that hidden EOF issued.
    for(int kind=0;kind<2;++kind){manager.current=ScreenId::Browser;start();drained=!kind;sponsor=kind;
        tick();tick();tick();assert(controller.paused);assert(back(PadButtons::L3));
        assert(!controller.paused&&!tube_background_play_active());
    }
    manager.current=ScreenId::Browser;start();controller.setPaused(true);
    tick();assert(back(PadButtons::L3));assert(controller.paused); // Original manual pause survives.
    for(int command=0;command<4;++command){manager.current=ScreenId::Browser;start();drained=true;tick();
        assert(controller.paused);
        if(command==0)controller.setPaused(true); // Repeated explicit pause supersedes EOF.
        if(command==1)controller.togglePause();
        if(command==2)controller.beginScrub();
        if(command==3)controller.seek();
        assert(back(PadButtons::L3));assert(controller.paused==(command!=1));
    }
    manager.current=ScreenId::Browser;start();drained=true;tick();controller.stopPlayback();
    assert(!tube_background_play_take_end_pause());
    manager.current=ScreenId::Browser;start();drained=true;tick();
    manager.current=ScreenId::Player;tick();assert(!controller.paused&&!tube_background_play_active());
    manager.current=ScreenId::Browser;start();drained=true;tick();
    tube_background_play_begin(selection,profile.c_str());assert(!tube_background_play_take_end_pause());
    tube_background_play_end_pause(1);tube_background_play_clear();assert(!tube_background_play_take_end_pause());
    start();drained=true;tick();controller.paused=false;drained=false;tick();assert(!tube_background_play_take_end_pause());
    tube_background_play_clear();
    for(int reason=0;reason<6;++reason){manager.current=ScreenId::Browser;start();int stops=controller.stops;
        if(reason==0)selection=8;if(reason==1)profile="other";if(reason==2)afterglow=false;
        if(reason==3)m_softClose=true;if(reason==4)controller.active=false;if(reason==5)phone=true;
        tick();assert(!tube_background_play_active());assert(controller.stops==stops+(reason<4));
        afterglow=true;m_softClose=false;phone=false;
    }
    start();manager.current=ScreenId::Player;int foregroundStops=controller.stops;tick();assert(!tube_background_play_active()&&controller.stops==foregroundStops);manager.current=ScreenId::Browser;
    start();int before=fetches;hidden.fail=true;tick();assert(fetches==before);hidden.fail=false;tick();assert(!tube_background_play_active());
    for(int reason=0;reason<7;++reason){controller.active=true;afterglow=true;
        if(reason==0)phone=true;if(reason==1)offer=true;if(reason==2)ask=true;if(reason==3)pending=true;
        if(reason==4)controller.scrub=true;if(reason==5)selection=0;if(reason==6)afterglow=false;
        enter();assert(!tube_background_play_active());phone=offer=ask=pending=false;controller.scrub=false;selection=7;afterglow=true;
    }
    assert(!tube_background_play_begin(0,"guest")&&!tube_background_play_begin(7,nullptr));
    assert(!tube_background_play_begin(7,std::string(33,'x').c_str()));
    // The start worker may cancel a pause while the UI replaces/consumes ownership.
    // False cancellation must not read owner, which remains confined to this thread.
    std::atomic<bool> finished{false};
    std::thread canceller([&]{while(!finished.load(std::memory_order_relaxed))
        tube_background_play_end_pause(0);tube_background_play_end_pause(0);});
    for(int i=0;i<10000;++i){
        assert(tube_background_play_begin(7,"guest"));tube_background_play_end_pause(1);
        const int resume=tube_background_play_take_end_pause();assert(resume==0||resume==1);
        tube_background_play_clear();
    }
    assert(tube_background_play_begin(7,"guest"));tube_background_play_end_pause(1);
    finished.store(true,std::memory_order_relaxed);canceller.join();
    assert(tube_background_play_active()&&!tube_background_play_take_end_pause());
}
"""
with tempfile.TemporaryDirectory(prefix='tube-background-') as temp:
    temp=Path(temp);(temp/'test.cpp').write_text(source)
    subprocess.run(['g++','-std=c++17','-pthread','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(root/'native'),str(temp/'test.cpp'),'-x','c++',str(root/'native/tube_background_play.c'),'-o',str(temp/'test')],check=True)
    subprocess.run([str(temp/'test')],check=True)
print('PASS generated background entry/ownership/hidden-update/discard/EOF-no-consumption/return and protected voice/modal input')
