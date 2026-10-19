#!/usr/bin/env python3
"""Compile the shipping UI-thread remote adapter against a controlled player."""
# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[1]
header = (root / 'native/maxd/phone_remote_playback.hpp').read_text()
code = header[header.index('inline std::string maxd_phone_search;'):header.index('inline int maxd_phone_capture')]
source = r'''
#include "maxd/phone_remote.h"
#include <cassert>
#include <cmath>
#include <string>
#include <cstring>
#include <algorithm>
static bool offer=false;
struct tube_next_offer_t{};static int tube_next_offer(tube_next_offer_t*){return offer;}
static int tube_manual_navigation_pending(){return 0;}static int tube_playlist_eof_pending(){return 0;}
static const char *tube_sponsor_prompt(double){return nullptr;}
static bool scoped=true,keyboard=false,casting=false,afterglow=true,background=false,voice=false;
static unsigned selection=7;static int queue_action,keys,returns,stops;static std::string text;
int tube_maxd_remote_scope(){return scoped;}
int tube_maxd_remote_queue(int action,const char *id){queue_action=action;text=id;return 1;}
void tube_maxd_remote_queue_state(MaxdRemoteState *s){s->count=0;}
static int evo_keyboard_is_open(){return keyboard;}
static int tube_phone_owns_media(){return casting;}
static int tube_afterglow_enabled(){return afterglow;}
static int tube_caption_selection(){return selection;}
static const char *tube_browse_profile(){return "guest";}
static int tube_phone_video(const char*,char out[12]){std::strcpy(out,"abcdefghijk");return 1;}
static int tube_background_play_begin(unsigned,const char*){background=true;return 1;}
static void tube_background_play_status(const char*,bool){}
enum {TUBE_VOICE_IDLE};static int tube_voice_state(){return voice?1:TUBE_VOICE_IDLE;}
namespace evo {
enum class ScreenId{EmbyBrowse,Player,Other};
namespace PadButtons {enum{Up=1,Down=2,Left=4,Right=8,Cross=16,Circle=32};}
struct Player{bool active=true,paused=false,live=false,scrub=false;double target=-1;
 bool isActive(){return active;}bool isPaused(){return paused;}bool isLiveSource(){return live;}
 bool isScrubbing(){return scrub;}bool isSeekable(){return !live;}
 double getPositionSeconds(){return 60;}double getDurationSeconds(){return 120;}
 std::string getCurrentFilePath(){return "private-stream-url";}std::string getDisplayTitle(){return "private title";}
 void setPaused(bool p){paused=p;}void stopPlayback(){active=false;++stops;}
 bool seekTo(double seconds,bool keepPaused){target=seconds;assert(keepPaused==paused);return true;}} player;
struct ProviderHostScreen {static inline bool busy=false;
 static bool playbackStartupBusy(){return busy;}
 bool submitPhoneSearch(const char *query){text=query;return true;}} host;
struct Manager{ScreenId current=ScreenId::EmbyBrowse;
 ScreenId getCurrentScreenId(){return current;}ProviderHostScreen *getScreen(ScreenId){return &host;}
 void handleInput(unsigned,unsigned,unsigned){++keys;}
 void returnFromPlayback(){++returns;current=ScreenId::EmbyBrowse;}} manager;
struct Application{Player *getPlaybackController(){return &player;}Manager *getScreenManager(){return &manager;}} app;
}
''' + code + r'''
int main(){
 MaxdRemoteCommand c{};c.kind=MAXD_REMOTE_PAUSE;c.selection=7;
 assert(maxd_phone_apply(&evo::app,&c) && evo::player.paused);
 c.selection=8;assert(!maxd_phone_apply(&evo::app,&c));c.selection=7;
 c.kind=MAXD_REMOTE_SEEK;c.seconds=999;assert(maxd_phone_apply(&evo::app,&c));assert(evo::player.target==119.95 && evo::player.paused);
 evo::player.live=true;assert(!maxd_phone_apply(&evo::app,&c));evo::player.live=false;
 c.kind=MAXD_REMOTE_RESUME;casting=true;assert(!maxd_phone_apply(&evo::app,&c));casting=false;
 keyboard=true;assert(!maxd_phone_apply(&evo::app,&c));keyboard=false;
 voice=true;assert(!maxd_phone_apply(&evo::app,&c));voice=false;
 evo::ProviderHostScreen::busy=true;assert(!maxd_phone_apply(&evo::app,&c));evo::ProviderHostScreen::busy=false;
 scoped=false;assert(!maxd_phone_apply(&evo::app,&c));scoped=true;
 assert(maxd_phone_apply(&evo::app,&c) && !evo::player.paused);
 c.kind=MAXD_REMOTE_SEARCH;std::strcpy(c.text,"pause");assert(maxd_phone_apply(&evo::app,&c) && text=="pause" && !evo::player.paused);
 offer=true;assert(!maxd_phone_apply(&evo::app,&c));offer=false;
 evo::manager.current=evo::ScreenId::Player;assert(maxd_phone_apply(&evo::app,&c));assert(background && returns==1 && stops==0 && maxd_phone_search=="pause");
 c.kind=MAXD_REMOTE_KEY;std::strcpy(c.text,"UP");assert(maxd_phone_apply(&evo::app,&c) && keys==2);
 c.kind=MAXD_REMOTE_NEXT;std::strcpy(c.text,"abcdefghijk");assert(maxd_phone_apply(&evo::app,&c) && queue_action==MAXD_REMOTE_NEXT && text=="abcdefghijk");
 MaxdRemoteState state{};maxd_phone_snapshot(&evo::app,&state);assert(state.active && state.selection==7);
 evo::ProviderHostScreen::busy=true;state={};maxd_phone_snapshot(&evo::app,&state);assert(!state.active && state.busy);
 puts("PASS: shipping phone adapter pause/resume, stale selection, paused seeking/clamp, live/startup/keyboard/voice/casting guards, literal text search, background return and queue routing");
}
'''
destination = root / 'build/tests/phone-controls.cpp'
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_text(source)
exe = destination.with_suffix('')
subprocess.run(['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(root/'native'),str(destination),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
