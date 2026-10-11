/* Actual generated PS5 player blocks for exit, D-pad actions and the options
 * picker, linked with the real preference module. */
#include "tube_player_extra.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace PadButtons { constexpr uint32_t Left=1, Right=2, Up=4, Down=8, Circle=16; }
static uint64_t clockMs=10000;
static uint64_t NowMs() { return clockMs; }
static std::string title, notice;
static void toast(const char *heading,const char *message) { title=heading; notice=message; }
enum { EVO_FB_CONFIRM };
static int feedback;
static void evo_feedback(int) { ++feedback; }
static bool phoneOwns;
static int tube_phone_owns_media() { return phoneOwns; }
static int navigation, navigationResult=1;
static int tube_manual_navigation_begin(int direction) { navigation=direction; return navigationResult; }

struct Playback {
    bool scrub=false, speedReady=true, live=false, paused=false, seekOk=true, keptPaused=false;
    int speed=100, stops=0, changes=0;
    double position=0, seekTarget=-1;
    bool isScrubbing() const { return scrub; }
    bool isActive() const { return true; }
    bool isLiveSource() const { return live; }
    bool isPaused() const { return paused; }
    int playbackSpeedPercent() const { return speed; }
    bool canChangePlaybackSpeed() const { return speedReady; }
    bool changePlaybackSpeed(int target) { ++changes; speed=target; return true; }
    void stopPlayback() { ++stops; }
    double getPositionSeconds() const { return position; }
    bool seekTo(double target,bool keepPaused) { seekTarget=target; keptPaused=keepPaused; return seekOk; }
};
struct ScreenManager { int returns=0; void returnFromPlayback() { ++returns; } };

/* 0 = the player's own handling follows, 1 = handled, 2 = stop confirmation prompt. */
struct Screen {
    Playback mock;
    ScreenManager manager;
    ScreenManager *screenMgr=&manager;
    int m_osdVisibilityAlpha=0;
    bool m_tubeSuppressControls=false;
    int input(uint32_t pressed) {
        auto playback=&mock;
        if (pressed & PadButtons::Circle) {
#include "exit.inc"
            return 2;
        }
#include "dpad.inc"
        return 0;
    }
};

enum Kind { KindOther, KindExtraGroup, KindExtraChoice, KindChapterList, KindChapter };
struct Entry { Kind kind=KindOther; int streamIndex=-1; std::string label, detail, badge; };
constexpr int EVO_RMLUI_LIST_ROWS=8;
struct Picker {
    Playback mock;
    std::vector<Entry> m_tracks;
    int m_selectedIndex=0, m_activeIndex=0, m_scrollOffset=0, m_tubeExtraGroup=-1;
    bool other=false, closed=false;
    void refreshTracks() {
        m_tracks.clear(); m_activeIndex=0;
        auto playback=&mock;
#include "lists.inc"
        Entry quality; quality.label="AUTO"; m_tracks.push_back(quality);
#include "rows.inc"
    }
    void activate() {
        auto playback=&mock;
        const Entry e=m_tracks[m_selectedIndex];
#include "selection.inc"
        { other=true; }
        closed=true;
    }
    bool back() {
#include "back.inc"
        return false;
    }
    int find(Kind kind,int index) const {
        for (int i=0;i<static_cast<int>(m_tracks.size());++i)
            if (m_tracks[i].kind==kind && (kind==KindChapterList || m_tracks[i].streamIndex==index)) return i;
        return -1;
    }
};

static void test_exit() {
    tube_player_extra_select(nullptr);
    Screen ask;
    assert(ask.input(PadButtons::Circle)==2 && !ask.mock.stops && !ask.manager.returns);   /* today's prompt */

    assert(tube_extra_set_exit_mode(TUBE_EXIT_MODE_DOUBLE));
    Screen twice; notice.clear();
    assert(twice.input(PadButtons::Circle)==1 && !twice.mock.stops && notice=="Press Circle again to exit");
    clockMs+=500;
    assert(twice.input(PadButtons::Circle)==1 && twice.mock.stops==1 && twice.manager.returns==1);
    clockMs+=500;
    assert(twice.input(PadButtons::Circle)==1 && twice.mock.stops==1);       /* armed again, not stopped */
    clockMs+=TUBE_EXIT_DOUBLE_MS+1;
    assert(twice.input(PadButtons::Circle)==1 && twice.mock.stops==1);       /* too slow */

    assert(tube_extra_set_exit_mode(TUBE_EXIT_MODE_SINGLE));
    Screen once; feedback=0;
    assert(once.input(PadButtons::Circle)==1 && once.mock.stops==1 && once.manager.returns==1 && feedback==1);
    once.screenMgr=nullptr;
    assert(once.input(PadButtons::Circle)==1 && once.mock.stops==2 && once.manager.returns==1);
    tube_player_extra_select(nullptr);
}

static void test_dpad() {
    tube_player_extra_select(nullptr);
    tube_extra_video_end();
    for (uint32_t key : {PadButtons::Left,PadButtons::Right,PadButtons::Up,PadButtons::Down}) {
        Screen plain; assert(plain.input(key)==0 && !plain.m_tubeSuppressControls);   /* defaults: untouched */
    }

    /* Quick skip only with hidden controls, and only for the chosen kind of video. */
    assert(tube_extra_set_keys_lr(TUBE_KEYS_SKIP_VIDEOS) && tube_extra_set_keys_ud(TUBE_KEYS_SKIP_SHORTS));
    Screen skip; navigation=0;
    assert(skip.input(PadButtons::Right)==1 && navigation==1 && notice=="Next video" && skip.m_tubeSuppressControls);
    assert(skip.input(PadButtons::Left)==1 && navigation==-1 && notice=="Previous video");
    navigation=0;
    assert(skip.input(PadButtons::Down)==0 && !navigation);                  /* not a Short */
    skip.m_osdVisibilityAlpha=255;
    assert(skip.input(PadButtons::Right)==0 && !navigation);                 /* controls shown: seek */
    skip.m_osdVisibilityAlpha=0; skip.mock.scrub=true;
    assert(skip.input(PadButtons::Right)==0 && !navigation);
    skip.mock.scrub=false;
    tube_extra_set_short(1);
    assert(skip.input(PadButtons::Right)==0 && !navigation);
    assert(skip.input(PadButtons::Down)==1 && navigation==1 && skip.input(PadButtons::Up)==1 && navigation==-1);
    tube_extra_set_short(0);
    navigationResult=0;
    assert(skip.input(PadButtons::Right)==1 && notice=="No video available in this direction");
    navigationResult=1; phoneOwns=true; navigation=0;
    assert(skip.input(PadButtons::Right)==1 && !navigation && notice=="No video available in this direction");
    phoneOwns=false;

    /* Volume and speed remaps also work while the controls are visible. */
    assert(tube_extra_set_keys_lr(TUBE_KEYS_VOLUME) && tube_extra_set_keys_ud(TUBE_KEYS_SPEED));
    Screen adjust; adjust.m_osdVisibilityAlpha=255;
    assert(adjust.input(PadButtons::Right)==1 && tube_extra_volume()==105 && title=="VOLUME" && notice=="105%");
    assert(!adjust.m_tubeSuppressControls);
    assert(adjust.input(PadButtons::Left)==1 && adjust.input(PadButtons::Left)==1 && tube_extra_volume()==95);
    assert(adjust.input(PadButtons::Up)==1 && adjust.mock.speed==105 && adjust.mock.changes==1 && notice=="1.05x");
    assert(adjust.input(PadButtons::Down)==1 && adjust.input(PadButtons::Down)==1 && adjust.mock.speed==95);
    adjust.mock.speed=400; adjust.mock.changes=0;
    assert(adjust.input(PadButtons::Up)==1 && !adjust.mock.changes && notice=="4x");     /* end of the list */
    adjust.mock.speed=100; adjust.mock.speedReady=false;
    assert(adjust.input(PadButtons::Up)==1 && !adjust.mock.changes && adjust.mock.speed==100 && title=="SPEED");
    assert(notice.find("Unable")==0);
    assert(tube_extra_set_keys_ud(TUBE_KEYS_VOLUME));
    assert(adjust.input(PadButtons::Up)==1 && tube_extra_volume()==100 && adjust.input(PadButtons::Down)==1 && tube_extra_volume()==95);
    tube_player_extra_select(nullptr);
}

static void test_picker() {
    tube_player_extra_select(nullptr);
    tube_extra_video_end();
    Picker picker;
    picker.refreshTracks();
    assert(static_cast<int>(picker.m_tracks.size())==1+TUBE_EXTRA_GROUPS);      /* no chapters: no chapter row */
    assert(picker.find(KindChapterList,0)<0);
    int row=picker.find(KindExtraGroup,TUBE_EXTRA_VOLUME);
    assert(row>0 && picker.m_tracks[row].label=="Player volume: 100%");
    assert(picker.m_tracks[picker.find(KindExtraGroup,TUBE_EXTRA_LOOP_SHORTS)].label=="Loop Shorts: Off");

    /* A group opens its choices in place with the cursor on the current one. */
    picker.m_selectedIndex=row; picker.activate();
    assert(!picker.closed && picker.m_tubeExtraGroup==TUBE_EXTRA_VOLUME && picker.m_tracks.size()==61);
    assert(picker.m_selectedIndex==20 && picker.m_tracks[20].badge=="SELECTED" && picker.m_scrollOffset==20-EVO_RMLUI_LIST_ROWS+1);
    /* Circle returns to all options, on the row that was opened. */
    assert(picker.back() && picker.m_tubeExtraGroup==-1 && picker.m_selectedIndex==row && tube_extra_volume()==100);
    assert(!picker.back());
    picker.activate();
    picker.m_selectedIndex=40; picker.activate();
    assert(picker.closed && !picker.other && tube_extra_volume()==200 && notice=="Player volume: 200% - boosted sound may distort");
    picker.closed=false; picker.m_tubeExtraGroup=-1; picker.refreshTracks();
    assert(picker.m_tracks[row].label=="Player volume: 200%");

    /* A rejected choice says so and changes nothing. */
    picker.m_selectedIndex=picker.find(KindExtraGroup,TUBE_EXTRA_AUDIO_DELAY_ENABLED); picker.activate();
    assert(picker.m_tracks.size()==2 && picker.m_selectedIndex==0);
    picker.m_selectedIndex=1; picker.activate();
    assert(picker.closed && notice=="Unable to save this preference" && !tube_extra_audio_delay_enabled());
    picker.closed=false; picker.m_tubeExtraGroup=-1; picker.refreshTracks();
    picker.m_selectedIndex=picker.find(KindExtraGroup,TUBE_EXTRA_SUBTITLE_STYLE); picker.activate();
    picker.m_selectedIndex=5; picker.activate();
    assert(tube_extra_subtitle_style_index()==5 && notice=="Subtitle style: Yellow on semitransparent background");

    /* Rows of other features still reach their own handling. */
    picker=Picker(); picker.refreshTracks(); picker.m_selectedIndex=0; picker.activate();
    assert(picker.other && picker.closed);

    /* Chapters: a row with the current title, a list, and a jump. */
    const double starts[]={0,60,600};
    const char *titles[]={"Intro","<Middle>",""};
    assert(tube_extra_set_chapter_list(starts,titles,3)==3);
    picker=Picker(); picker.mock.position=75; picker.refreshTracks();
    row=picker.find(KindChapterList,0);
    assert(row==1 && picker.m_tracks[row].detail=="<Middle>" && picker.m_tracks[row].badge=="3");
    picker.m_selectedIndex=row; picker.activate();
    assert(!picker.closed && picker.m_tubeExtraGroup==-2 && picker.m_tracks.size()==3 && picker.m_selectedIndex==1);
    assert(picker.m_tracks[1].label=="01:00  <Middle>" && picker.m_tracks[1].badge=="PLAYING" && picker.m_tracks[2].label=="10:00  Chapter 3");
    assert(picker.back() && picker.m_selectedIndex==row);
    picker.activate(); picker.m_selectedIndex=2; picker.mock.paused=true; picker.activate();
    assert(picker.closed && picker.mock.seekTarget==600 && picker.mock.keptPaused && title=="CHAPTER" && notice=="10:00  Chapter 3");
    picker.closed=false; picker.mock.seekOk=false; picker.activate();
    assert(notice=="Unable to jump to this chapter");
    picker.mock.seekOk=true; picker.mock.live=true; picker.mock.seekTarget=-1; picker.activate();
    assert(picker.mock.seekTarget==-1 && notice=="Unable to jump to this chapter");
    tube_extra_video_end();
    tube_player_extra_select(nullptr);
}

int main() {
    test_exit();
    test_dpad();
    test_picker();
    puts("PASS: shipping player exit, D-pad actions, option groups and chapter picker");
}
