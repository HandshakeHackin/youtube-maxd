/* Actual generated PS5 player blocks for exit, D-pad actions, the options
 * picker, the in-player video actions, the chapter notification and the error
 * fixer, linked with the real preference module. */
#include "tube_player_extra.h"
#include "tube_background_play.h"
#include "tube_provider.h"
#include <algorithm>
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
int tube_manual_navigation_begin(int direction) { navigation=direction; return navigationResult; }

struct Playback {
    bool scrub=false, speedReady=true, live=false, paused=false, seekOk=true, keptPaused=false;
    int speed=100, stops=0, changes=0;
    double position=0, seekTarget=-1;
    bool isScrubbing() const { return scrub; }
    bool isActive() const { return true; }
    bool isLiveSource() const { return live; }
    bool isMusicMode() const { return false; }
    bool isPaused() const { return paused; }
    int playbackSpeedPercent() const { return speed; }
    bool canChangePlaybackSpeed() const { return speedReady; }
    bool changePlaybackSpeed(int target) { ++changes; speed=target; return true; }
    void stopPlayback() { ++stops; }
    double getPositionSeconds() const { return position; }
    bool seekTo(double target,bool keepPaused) { seekTarget=target; keptPaused=keepPaused; return seekOk; }
};
enum class ScreenId { Player, AudioTrackPicker };
struct ScreenManager {
    int returns=0, backs=0, opened=-1; bool backOk=true;
    void returnFromPlayback() { ++returns; }
    void navigateTo(ScreenId screen) { opened=static_cast<int>(screen); }
    bool navigateBack() { ++backs; return backOk; }
};

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

    /* Down lists suggestions while the controls are visible; Up and hidden controls keep their actions. */
    assert(tube_extra_set_keys_ud(TUBE_KEYS_SUGGESTIONS));
    Screen related; related.m_osdVisibilityAlpha=255;
    assert(related.input(PadButtons::Up)==0 && related.manager.opened==-1 && tube_extra_take_picker_request()==TUBE_PICKER_OPTIONS);
    assert(related.input(PadButtons::Down)==1 && related.manager.opened==static_cast<int>(ScreenId::AudioTrackPicker));
    assert(tube_extra_take_picker_request()==TUBE_PICKER_SUGGESTIONS && !related.m_tubeSuppressControls);
    related.manager.opened=-1; related.m_osdVisibilityAlpha=0;
    assert(related.input(PadButtons::Down)==0 && related.manager.opened==-1);
    related.m_osdVisibilityAlpha=255; phoneOwns=true;
    assert(related.input(PadButtons::Down)==0 && related.manager.opened==-1);
    phoneOwns=false; related.screenMgr=nullptr;
    assert(related.input(PadButtons::Down)==0);
    related.screenMgr=&related.manager;
    assert(tube_extra_set_suggestions(0) && related.input(PadButtons::Down)==0 && related.manager.opened==-1);
    tube_player_extra_select(nullptr);
}

static void test_picker() {
    tube_player_extra_select(nullptr);
    tube_extra_video_end();
    Picker picker;
    picker.refreshTracks();
    assert(static_cast<int>(picker.m_tracks.size())==1+TUBE_EXTRA_BUTTON_FIRST); /* no chapters: no chapter row; button switches live in settings */
    assert(picker.find(KindExtraGroup,TUBE_EXTRA_BUTTON_FIRST)<0 && picker.find(KindExtraGroup,TUBE_EXTRA_DIM_TIMEOUT)>0);
    assert(picker.find(KindChapterList,0)<0);
    int row=picker.find(KindExtraGroup,TUBE_EXTRA_VOLUME);
    assert(row>0 && picker.m_tracks[row].label=="Player volume: 100%");
    assert(picker.m_tracks[picker.find(KindExtraGroup,TUBE_EXTRA_LOOP_SHORTS)].label=="Loop Shorts: Off");
    /* The video buffer is a row of the in-player options too, as it is of SmartTube's playback settings. */
    assert(picker.find(KindExtraGroup,TUBE_EXTRA_VIDEO_BUFFER)>0 && picker.m_tracks[picker.find(KindExtraGroup,TUBE_EXTRA_VIDEO_BUFFER)].label=="Video buffer: Medium");

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

/* ---- In-player video actions: provider entry points as the picker sees them. ---- */
struct Menu {
    int status=-1, begun=-1, opened=-1, closes=0, searches=0;
    int beginResult=TUBE_MENU_OPENED, openResult=TUBE_MENU_OPENED, nextStatus=1;
    bool root=false;
    std::string title, message;
    std::vector<tube_player_menu_row_t> rows, next;
};
static Menu menu;
static std::string actionLabel[TUBE_PLAYER_ACTION_COUNT];
static tube_player_menu_row_t menuRow(const char *id,const char *title,int folder) {
    tube_player_menu_row_t row{};
    std::snprintf(row.id,sizeof row.id,"%s",id); std::snprintf(row.title,sizeof row.title,"%s",title);
    std::snprintf(row.detail,sizeof row.detail,"detail of %s",title); row.folder=folder;
    return row;
}
int tube_player_action(int action,char *label,size_t labelSize,char *detail,size_t detailSize) {
    if (action<0 || action>=TUBE_PLAYER_ACTION_COUNT || actionLabel[action].empty()) return 0;
    std::snprintf(label,labelSize,"%s",actionLabel[action].c_str()); std::snprintf(detail,detailSize,"about %d",action);
    return 1;
}
static void menuListed(const std::string &listTitle) { menu.status=menu.nextStatus; menu.rows=menu.next; menu.title=listTitle; }
int tube_player_menu_begin(int action) {
    menu.begun=action;
    if (menu.beginResult==TUBE_MENU_OPENED) { menu.root=action==TUBE_PLAYER_ACTIONS; menuListed(actionLabel[action]); }
    return menu.beginResult;
}
int tube_player_menu_open(int row) {
    menu.opened=row;
    if (menu.openResult==TUBE_MENU_OPENED) { const std::string listTitle=menu.rows[row].title; menu.root=false; menuListed(listTitle); }
    return menu.openResult;
}
int tube_player_menu_status(void) { return menu.status; }
int tube_player_menu_count(void) { return menu.status==1 ? static_cast<int>(menu.rows.size()) : 0; }
const tube_player_menu_row_t *tube_player_menu_row(int index) {
    return index>=0 && index<tube_player_menu_count() ? &menu.rows[index] : nullptr;
}
const char *tube_player_menu_title(void) { return menu.title.c_str(); }
const char *tube_player_menu_message(void) { return menu.message.c_str(); }
int tube_player_menu_root(void) { return menu.root; }
void tube_player_menu_close(void) { ++menu.closes; menu.status=-1; menu.rows.clear(); menu.root=false; }
void tube_player_request_search(void) { ++menu.searches; }
static std::string failure;
extern "C" int evo_failure_reason(char *out,size_t size) { std::snprintf(out,size,"%s",failure.c_str()); return !failure.empty(); }
/* As the provider since 38d2840: the maximum resolution is fixed at 2160p and a preset's height
 * cannot lower it; frame rate and codec are the profile's. */
static const int limitHeight=2160;
static int limitFps=0, limitCodec=0, qualityResult=1, reloadResult=1, reloads=0, reloadSubtitles=-1;
static double reloadPosition=-1;
void tube_player_quality(int *height,int *fps,int *codec) { if(height)*height=limitHeight; if(fps)*fps=limitFps; if(codec)*codec=limitCodec; }
int tube_player_set_quality(int,int fps,int codec) {
    if (!qualityResult) return 0;
    limitFps=fps; limitCodec=codec; return 1;
}
int tube_player_reload(double position,int,int subtitles) { ++reloads; reloadPosition=position; reloadSubtitles=subtitles; return reloadResult; }
static int prospero_subtitle_enabled=1;
static int fixResult, fixError=-1, fixSubtitles=-1, fixes;
static double fixPosition=-1;
int tube_error_fix_begin(int error,double position,int,int subtitles) {
    ++fixes; fixError=error; fixPosition=position; fixSubtitles=subtitles; return fixResult;
}
static bool demuxSeeking, audioFailed;
static int evo_demux_is_seeking() { return demuxSeeking; }
static int evo_audio_has_failed() { return audioFailed; }
static struct { int seek_discarding=0; } g_pp_pb;
static int pb_prebuffer_hold;

static bool afterglowPicker;
static int tube_afterglow_picker_active() { return afterglowPicker; }

enum OptionKind { KindAuto, KindVideo, KindAudio, KindSpeed, KindChat, KindSeekInterval, KindResize, KindZoom, KindAspect,
                  KindPixel, KindRotate, KindMirror, KindPlaybackMode, KindVideoNavigation, KindTubeAction, KindTubeMenuRow,
                  KindTubeNotice, KindSearch, KindScreenOff, KindPresetList, KindPreset, KindAfterglowDetails, KindAfterglowStats };
struct Options {
    struct Entry { OptionKind kind=KindAudio; int streamIndex=-1; std::string label, detail, badge; };
    Playback mock;
    ScreenManager manager;
    std::vector<Entry> m_tracks, others;
    int activeOther=-1;
    int m_selectedIndex=0, m_activeIndex=0, m_scrollOffset=0, m_tubeExtraGroup=-1;
    int m_tubeMenuAction=-1, m_tubeMenuSeen=-2, m_tubeMenuRows=0, m_tubeMenuKeep=0;
    bool m_tubeMenuRoot=false, m_tubeMenuDirect=false, other=false;
    void refreshTracks() {
        m_tracks.clear(); m_activeIndex=0;
        auto playback=&mock;
#include "menu_lists.inc"
#include "menu_rows.inc"
        for (int i=0;i<static_cast<int>(others.size());++i) {
            if (i==activeOther) m_activeIndex=static_cast<int>(m_tracks.size());
            m_tracks.push_back(others[i]);
        }
#include "filter.inc"
    }
    void enter() {
#include "entry.inc"
        refreshTracks();
        m_selectedIndex=m_activeIndex;
    }
    void activate() {
        auto playback=&mock; auto sm=&manager;
        const Entry e=m_tracks[m_selectedIndex];
#include "menu_selection.inc"
        other=true;
    }
    bool back() {
#include "menu_back.inc"
        return false;
    }
    void update() {
#include "menu_refresh.inc"
    }
    int find(OptionKind kind,int index=-99) const {
        for (int i=0;i<static_cast<int>(m_tracks.size());++i)
            if (m_tracks[i].kind==kind && (index==-99 || m_tracks[i].streamIndex==index)) return i;
        return -1;
    }
    void choose(OptionKind kind,int index=-99) { m_selectedIndex=find(kind,index); assert(m_selectedIndex>=0); activate(); }
};

static void offerAll() {
    static const char *const labels[TUBE_PLAYER_ACTION_COUNT]={"Video actions","Like video","Dislike video","Subscribe on YouTube",
        "Add to a YouTube playlist","Playback queue","Related videos","Open description","Share link",
        "Exclude channel from SponsorBlock","Open channel","Open playlist"};
    for (int i=0;i<TUBE_PLAYER_ACTION_COUNT;++i) actionLabel[i]=labels[i];
}

static void test_action_rows() {
    tube_player_extra_select(nullptr); offerAll(); menu=Menu(); phoneOwns=false; limitFps=limitCodec=0;
    Options options; options.enter();
    /* SmartTube's defaults: the queue, the share link and SponsorBlock wait for their switches. */
    const int expected[]={TUBE_PLAYER_ACTIONS,TUBE_PLAYER_LIKE,TUBE_PLAYER_DISLIKE,TUBE_PLAYER_SUBSCRIBE,TUBE_PLAYER_PLAYLIST_ADD,
                          TUBE_PLAYER_SUGGESTIONS,TUBE_PLAYER_PLAYLIST,TUBE_PLAYER_DESCRIPTION,TUBE_PLAYER_CHANNEL};
    assert(options.m_tracks.size()==12);
    for (int i=0;i<9;++i) assert(options.m_tracks[i].kind==KindTubeAction && options.m_tracks[i].streamIndex==expected[i]);
    assert(options.m_tracks[1].label=="Like video" && options.m_tracks[1].detail=="about 1");
    assert(options.m_tracks[9].kind==KindSearch && options.m_tracks[10].kind==KindScreenOff);
    assert(options.m_tracks[11].kind==KindPresetList && options.m_tracks[11].label=="Video presets: Disabled");
    assert(menu.begun==-1 && !menu.closes);                    /* listing the options asks the companion nothing */
    assert(tube_extra_set_buttons((1<<TUBE_BUTTON_COUNT)-1));
    options.refreshTracks();
    assert(options.m_tracks.size()==15 && options.find(KindTubeAction,TUBE_PLAYER_QUEUE)==5 && options.find(KindTubeAction,TUBE_PLAYER_SHARE)>0);
    assert(options.find(KindTubeAction,TUBE_PLAYER_SPONSOR)>0);
    /* Each switch hides its own row; "Video actions" stays. */
    assert(tube_extra_set_buttons(0));
    options.refreshTracks();
    assert(options.m_tracks.size()==4 && options.m_tracks[0].streamIndex==TUBE_PLAYER_ACTIONS && options.find(KindSearch)<0 && options.find(KindScreenOff)<0);
    assert(options.find(KindTubeAction,TUBE_PLAYER_SUGGESTIONS)==1 && options.find(KindTubeAction,TUBE_PLAYER_PLAYLIST)==2);
    assert(tube_extra_set_suggestions(0));
    options.refreshTracks();
    assert(options.m_tracks.size()==2 && options.find(KindTubeAction,TUBE_PLAYER_SUGGESTIONS)<0);
    tube_player_extra_select(nullptr);
    /* What the menu does not offer for this video and profile has no row. */
    actionLabel[TUBE_PLAYER_LIKE].clear(); actionLabel[TUBE_PLAYER_CHANNEL].clear();
    options.refreshTracks();
    assert(options.m_tracks.size()==10 && options.find(KindTubeAction,TUBE_PLAYER_LIKE)<0 && options.find(KindTubeAction,TUBE_PLAYER_CHANNEL)<0);
    offerAll();
    /* A phone session keeps its video: only the preferences are offered. */
    phoneOwns=true; options.refreshTracks();
    assert(options.m_tracks.size()==1 && options.m_tracks[0].kind==KindPresetList);
    phoneOwns=false;
    limitFps=60; limitCodec=TUBE_CODEC_VP9; options.refreshTracks();
    assert(options.m_tracks.back().label=="Video presets: 2160p 60 fps VP9");
    limitCodec=TUBE_CODEC_HEVC; options.refreshTracks();
    assert(options.m_tracks.back().label=="Video presets: Custom limits");
    limitFps=limitCodec=0;

    /* Rows of other features: a switched-off button removes them and the cursor follows its row. */
    Options filtered;
    for (OptionKind kind : {KindAuto,KindAudio,KindChat,KindVideoNavigation,KindVideoNavigation,KindPlaybackMode,KindResize,KindZoom,KindAspect,
                            KindPixel,KindRotate,KindMirror,KindSeekInterval,KindSpeed,KindSpeed}) {
        Options::Entry e; e.kind=kind; e.streamIndex=static_cast<int>(filtered.others.size()); filtered.others.push_back(e);
    }
    filtered.others[3].streamIndex=-1; filtered.others[4].streamIndex=1;      /* previous, next */
    filtered.activeOther=14;
    filtered.enter();
    const int base=12;
    assert(filtered.m_tracks.size()==base+15u && filtered.m_activeIndex==base+14 && filtered.m_selectedIndex==base+14);
    assert(tube_extra_set_buttons(TUBE_BUTTONS_DEFAULT & ~(1<<TUBE_BUTTON_CHAT) & ~(1<<TUBE_BUTTON_PREVIOUS) & ~(1<<TUBE_BUTTON_ZOOM) & ~(1<<TUBE_BUTTON_FLIP)));
    filtered.refreshTracks();
    assert(filtered.m_tracks.size()==base+8u && filtered.find(KindChat)<0 && filtered.find(KindResize)<0 && filtered.find(KindZoom)<0);
    assert(filtered.find(KindAspect)<0 && filtered.find(KindPixel)<0 && filtered.find(KindMirror)<0 && filtered.find(KindRotate)>0);
    assert(filtered.find(KindVideoNavigation,1)>0 && filtered.find(KindVideoNavigation,-1)<0);
    assert(filtered.m_tracks[filtered.m_activeIndex].kind==KindSpeed && filtered.m_tracks[filtered.m_activeIndex].streamIndex==14);
    assert(tube_extra_set_buttons(TUBE_BUTTONS_DEFAULT & ~(1<<TUBE_BUTTON_SPEED) & ~(1<<TUBE_BUTTON_NEXT) & ~(1<<TUBE_BUTTON_REPEAT) &
                                  ~(1<<TUBE_BUTTON_ROTATE) & ~(1<<TUBE_BUTTON_SEEK_INTERVAL)));
    filtered.refreshTracks();
    assert(filtered.find(KindSpeed)<0 && filtered.find(KindPlaybackMode)<0 && filtered.find(KindRotate)<0 && filtered.find(KindSeekInterval)<0);
    assert(filtered.find(KindVideoNavigation,-1)>0 && filtered.find(KindVideoNavigation,1)<0 && filtered.find(KindAuto)>0 && filtered.find(KindAudio)>0);
    assert(filtered.m_activeIndex==0);                         /* its row is gone */
    tube_player_extra_select(nullptr);
}

static void test_afterglow_detail_rows() {
    tube_player_extra_select(nullptr); offerAll(); menu=Menu(); phoneOwns=false; afterglowPicker=false;
    Options options; options.enter();
    const auto classicCount=options.m_tracks.size();
    assert(options.find(KindAfterglowDetails)<0 && options.find(KindAfterglowStats)<0);
    afterglowPicker=true; options.refreshTracks();
    assert(options.m_tracks.size()==classicCount+2);
    const int details=options.find(KindAfterglowDetails), stats=options.find(KindAfterglowStats);
    assert(details==static_cast<int>(classicCount) && stats==details+1);
    assert(options.m_tracks[details].label=="Details" && options.m_tracks[stats].label=="Playback details");
    options.refreshTracks();
    assert(options.m_tracks.size()==classicCount+2); /* refreshing never duplicates shortcuts */
    assert(tube_extra_set_buttons(0)); options.refreshTracks();
    assert(options.find(KindAfterglowDetails)>=0 && options.find(KindAfterglowStats)>=0);
    phoneOwns=true; options.refreshTracks();
    assert(options.m_tracks.size()==3 && options.m_tracks[0].kind==KindPresetList);
    assert(options.m_tracks[1].kind==KindAfterglowDetails && options.m_tracks[2].kind==KindAfterglowStats);
    afterglowPicker=false; options.refreshTracks();
    assert(options.m_tracks.size()==1 && options.m_tracks[0].kind==KindPresetList);
    phoneOwns=false; tube_player_extra_select(nullptr);
}

static void test_action_lists() {
    tube_player_extra_select(nullptr); offerAll(); menu=Menu(); phoneOwns=false; failure.clear();
    Options options; options.enter();
    /* A direct action lists its page; while the companion answers the video keeps playing. */
    menu.nextStatus=0;
    options.choose(KindTubeAction,TUBE_PLAYER_SUGGESTIONS);
    assert(menu.begun==TUBE_PLAYER_SUGGESTIONS && options.m_tubeExtraGroup==-3 && !options.other && !options.mock.stops);
    assert(options.m_tracks.size()==1 && options.m_tracks[0].kind==KindTubeNotice && options.m_tracks[0].label=="Loading...");
    options.activate();
    assert(menu.opened==-1 && !options.other && !options.mock.stops);          /* the notice row is inert */
    options.update();
    assert(options.m_tracks.size()==1);                                        /* nothing new yet */
    menu.rows={menuRow("bcdefghijkl","Second",0),menuRow("library:filter:x","From this channel",1),menuRow("more:","More results",1)};
    menu.status=1;
    options.update();
    assert(options.m_tracks.size()==3 && options.m_tracks[0].kind==KindTubeMenuRow && options.m_tracks[0].label=="Second");
    assert(options.m_tracks[0].badge=="VIDEO" && options.m_tracks[1].badge.empty() && options.m_tracks[0].detail=="detail of Second");
    assert(options.m_selectedIndex==0);
    /* The next page keeps the cursor where the new rows begin. */
    menu.nextStatus=0; menu.next=menu.rows;
    options.m_selectedIndex=2; options.activate();
    assert(menu.opened==2 && options.m_tracks.size()==1 && options.m_selectedIndex==0 && options.m_tubeMenuKeep==2);
    menu.rows={menuRow("bcdefghijkl","Second",0),menuRow("library:filter:x","From this channel",1),menuRow("defghijklmn","Third",0),menuRow("efghijklmno","Fourth",0)};
    menu.status=1; options.update();
    assert(options.m_tracks.size()==4 && options.m_selectedIndex==2 && options.m_tracks[2].label=="Third");
    /* A folder opens in place; a failed list says why. */
    menu.nextStatus=-1; menu.next.clear(); failure="The companion did not answer.";
    options.m_selectedIndex=1; options.activate();
    assert(menu.opened==1 && options.m_tracks.size()==1 && options.m_tracks[0].label=="The companion did not answer." && options.m_tracks[0].detail=="Circle returns");
    menu.message="Choose an account profile first"; options.refreshTracks();
    assert(options.m_tracks[0].label=="Choose an account profile first");
    menu.message.clear(); failure.clear(); options.refreshTracks();
    assert(options.m_tracks[0].label=="This action is unavailable right now");
    /* Circle returns to the options, on the row the list was opened from. */
    assert(options.back() && menu.closes==1 && options.m_tubeExtraGroup==-1);
    assert(options.m_tracks[options.m_selectedIndex].kind==KindTubeAction && options.m_tracks[options.m_selectedIndex].streamIndex==TUBE_PLAYER_SUGGESTIONS);
    assert(!options.back());

    /* A chosen video stops this one; the host starts it from the browser. */
    menu.nextStatus=1; menu.next={menuRow("bcdefghijkl","Second",0)};
    options.choose(KindTubeAction,TUBE_PLAYER_SUGGESTIONS);
    menu.openResult=TUBE_MENU_LEAVE; options.m_selectedIndex=0; options.activate();
    assert(options.mock.stops==1 && options.manager.returns==1 && !options.other);
    menu.openResult=TUBE_MENU_NOTHING; options.activate();
    assert(options.mock.stops==1 && title!="VIDEO ACTIONS");                    /* an inert row of a list says nothing */
    menu.openResult=TUBE_MENU_OPENED;

    /* "Video actions": pages return to the actions, the actions return to the options. */
    options=Options(); menu=Menu(); options.enter();
    menu.next={menuRow("account:confirm:like:abcdefghijk","Like video",1),menuRow("local:video:1:share:abcdefghijk","Share link",1)};
    options.choose(KindTubeAction,TUBE_PLAYER_ACTIONS);
    assert(options.m_tubeMenuRoot && menu.root && options.m_tracks.size()==2 && menu.title=="Video actions");
    menu.next={menuRow("account:notice:video","https://youtu.be/abcdefghijk",1)};
    options.m_selectedIndex=1; options.activate();
    assert(!menu.root && options.m_tracks.size()==1 && options.m_tracks[0].label=="https://youtu.be/abcdefghijk" && menu.title=="Share link");
    menu.next={menuRow("account:confirm:like:abcdefghijk","Like video",1),menuRow("local:video:1:share:abcdefghijk","Share link",1)};
    menu.begun=-1;
    assert(options.back() && menu.begun==TUBE_PLAYER_ACTIONS && menu.root && options.m_tubeExtraGroup==-3 && options.m_tracks.size()==2 && !menu.closes);
    assert(options.back() && menu.closes==1 && options.m_tubeExtraGroup==-1 && options.m_selectedIndex==0);
    /* An action the menu cannot open says so and stays in the options. */
    menu.beginResult=TUBE_MENU_NOTHING; title.clear();
    options.choose(KindTubeAction,TUBE_PLAYER_LIKE);
    assert(options.m_tubeExtraGroup==-1 && title=="VIDEO ACTIONS" && !options.other);
    /* Open channel and Search leave the player. */
    menu.beginResult=TUBE_MENU_LEAVE;
    options.choose(KindTubeAction,TUBE_PLAYER_CHANNEL);
    assert(options.mock.stops==1 && options.manager.returns==1);
    options.choose(KindSearch);
    assert(menu.searches==1 && options.mock.stops==2 && options.manager.returns==2 && !options.other);
    menu.beginResult=TUBE_MENU_OPENED;

    /* Down in the player opens the picker on the suggestions; Circle then returns to the video. */
    options=Options(); menu=Menu(); menu.next={menuRow("bcdefghijkl","Second",0)};
    tube_extra_picker_request(TUBE_PICKER_SUGGESTIONS);
    options.enter();
    assert(options.m_tubeExtraGroup==-3 && options.m_tubeMenuDirect && menu.begun==TUBE_PLAYER_SUGGESTIONS && options.m_tracks.size()==1);
    assert(!options.back() && menu.closes==1 && options.m_tubeExtraGroup==-1);  /* the picker's own Back follows */
    options=Options(); menu=Menu(); menu.beginResult=TUBE_MENU_NOTHING;
    tube_extra_picker_request(TUBE_PICKER_SUGGESTIONS);
    options.enter();
    assert(options.m_tubeExtraGroup==-1 && !options.m_tubeMenuDirect && options.m_tracks.size()>1);   /* nothing to list: the options */
    menu=Menu();
    tube_player_extra_select(nullptr);
}

static void test_presets_and_screen_off() {
    tube_player_extra_select(nullptr); offerAll(); menu=Menu(); phoneOwns=false;
    limitFps=60; limitCodec=TUBE_CODEC_VP9; qualityResult=reloadResult=1; reloads=0;
    Options options; options.mock.position=42.5; options.enter();
    options.choose(KindPresetList);
    assert(options.m_tubeExtraGroup==-4 && static_cast<int>(options.m_tracks.size())==tube_preset_count() && !options.other);
    const int current=tube_preset_find(2160,60,TUBE_CODEC_VP9);
    assert(current==1 && options.m_selectedIndex==current && options.m_tracks[current].badge=="SELECTED" && options.m_tracks[0].label=="Disabled");
    assert(tube_preset_find(1080,60,TUBE_CODEC_VP9)<0 && tube_preset_find(720,30,TUBE_CODEC_AVC)<0);   /* every preset keeps 2160p */
    assert(options.back() && options.m_tubeExtraGroup==-1 && options.m_tracks[options.m_selectedIndex].kind==KindPresetList);
    /* A preset is saved and applied at once: the video reopens at its position. */
    options.activate();
    options.m_selectedIndex=tube_preset_find(2160,30,TUBE_CODEC_AV1); options.activate();
    assert(limitFps==30 && limitCodec==TUBE_CODEC_AV1 && reloads==1 && reloadPosition==42.5 && reloadSubtitles==1);
    assert(options.mock.stops==1 && options.manager.returns==1 && !options.manager.backs && notice=="2160p 30 fps AV1 - reloading this video");
    /* "Disabled" lifts the frame rate and codec and keeps the maximum quality. */
    reloadResult=0;
    options.m_selectedIndex=0; options.activate();
    assert(!limitFps && !limitCodec && options.mock.stops==1 && options.manager.backs==1);
    assert(notice=="Disabled - used from the next video");
    qualityResult=0; options.m_selectedIndex=1; options.activate();
    assert(notice=="Unable to save this preset" && !limitFps && reloads==2 && options.manager.backs==2);
    qualityResult=1; phoneOwns=true; options.m_selectedIndex=1; options.activate();
    assert(limitFps==60 && limitCodec==TUBE_CODEC_VP9 && reloads==2 && options.mock.stops==1);            /* a phone session is not reopened */
    phoneOwns=false; reloadResult=1;

    /* Screen off: at once, and the picker returns to the video. */
    options=Options(); options.enter();
    tube_extra_dim_reset(0);
    assert(!tube_extra_screen_is_off());
    options.manager.backOk=false;
    options.choose(KindScreenOff);
    assert(tube_extra_screen_is_off() && options.manager.backs==1 && options.manager.opened==static_cast<int>(ScreenId::Player) && !options.other);
    assert(tube_extra_dim_tick(10,TUBE_DIM_PLAYING)==100);
    tube_extra_dim_reset(0);
    limitFps=limitCodec=0;
    tube_player_extra_select(nullptr);
}

struct Application {
    static Application &getInstance() { static Application app; return app; }
    ScreenManager manager;
    ScreenManager *getScreenManager() { return &manager; }
};
struct Fixer {
    Playback mock;
    int m_osdVisibilityAlpha=0;
    tube_buffering_t m_tubeBuffering{};
    bool stopped=false;
    void chapter() {
        auto playback=&mock;
#include "chapter.inc"
    }
    void decoder() {
        auto playback=&mock;
#include "decoder.inc"
    }
    /* One frame of the player's update; true when it left for the browser. */
    void buffering(uint64_t now,bool stalled,bool reallyPaused=false,bool ended=false) {
        auto playback=&mock;
        stopped=true;
#include "buffering.inc"
        stopped=false;
    }
};

static void test_chapter_notification() {
    tube_player_extra_select(nullptr); tube_extra_video_end();
    const double starts[]={0,60,600};
    const char *titles[]={"Intro","Middle",""};
    assert(tube_extra_set_chapter_list(starts,titles,3)==3);
    Fixer player; title.clear(); notice.clear();
    player.mock.position=10; player.chapter();
    assert(title.empty());                                       /* off by default, as in SmartTube */
    assert(tube_extra_set_chapter_notice(1));
    player.m_osdVisibilityAlpha=255; player.chapter();
    assert(title.empty());                                       /* the controls already show the title */
    player.m_osdVisibilityAlpha=0; player.chapter();
    assert(title=="CHAPTER - R1 SKIPS IT" && notice=="Intro");
    title.clear(); player.chapter();
    assert(title.empty());                                       /* once per chapter */
    player.mock.position=61; player.mock.paused=true; player.chapter();
    assert(title.empty());
    player.mock.paused=false; player.mock.scrub=true; player.chapter();
    player.mock.scrub=false; demuxSeeking=true; player.chapter();
    demuxSeeking=false; g_pp_pb.seek_discarding=1; player.chapter();
    g_pp_pb.seek_discarding=0; pb_prebuffer_hold=1; player.chapter();
    assert(title.empty());                                       /* not while a seek settles or the stream waits */
    pb_prebuffer_hold=0; player.chapter();
    assert(notice=="Middle");
    player.mock.position=700; player.chapter();
    assert(notice=="10:00  Chapter 3");                          /* an untitled chapter is named by its time */
    title.clear(); player.mock.live=true; player.mock.position=10; player.chapter();
    assert(title.empty());
    tube_extra_video_end(); tube_player_extra_select(nullptr);
}

static void test_error_fixer_blocks() {
    tube_player_extra_select(nullptr); phoneOwns=false; fixes=0; prospero_subtitle_enabled=1;
    Fixer player; player.mock.position=33; title.clear();
    /* Decoder failure: the fixer arms a reload before the player's own stop. */
    fixResult=TUBE_FIX_SAFE_VIDEO; audioFailed=false;
    player.decoder();
    assert(fixes==1 && fixError==TUBE_ERROR_VIDEO_DECODER && fixPosition==33 && fixSubtitles==1 && title=="PLAYBACK" && notice.find("AVC")!=std::string::npos);
    fixResult=TUBE_FIX_NONE; audioFailed=true; title.clear();
    player.decoder();
    assert(fixes==2 && fixError==TUBE_ERROR_AUDIO_DECODER && title.empty());   /* nothing to try: the player's message follows */
    phoneOwns=true; player.decoder();
    assert(fixes==2);
    phoneOwns=false; audioFailed=false;

    /* Long buffering: 20 s of waiting, then the reload leaves for the browser. */
    ScreenManager &screens=Application::getInstance().manager;
    uint64_t now=1000; fixResult=TUBE_FIX_RELOAD|TUBE_FIX_SUBTITLES_OFF; title.clear();
    player.buffering(now,false);
    for (int i=0;i<1249;++i) { now+=16; player.buffering(now,true); assert(!player.stopped && fixes==2); }
    now+=16; player.buffering(now,true);
    assert(player.stopped && fixes==3 && fixError==TUBE_ERROR_BUFFERING && player.mock.stops==1 && screens.returns==1);
    assert(title=="PLAYBACK" && notice=="Buffering too long. Captions off; reloading this stream...");
    /* Paused, finished or seeking is not buffering; a seek starts the count again. */
    pb_prebuffer_hold=1;
    for (int i=0;i<1300;++i) { now+=16; player.buffering(now,false,true); }
    for (int i=0;i<1300;++i) { now+=16; player.buffering(now,false,false,true); }
    assert(fixes==3);
    for (int i=0;i<1000;++i) { now+=16; player.buffering(now,false); }
    player.mock.scrub=true; now+=16; player.buffering(now,false); player.mock.scrub=false;
    for (int i=0;i<1000;++i) { now+=16; player.buffering(now,false); }
    assert(fixes==3 && !player.stopped);
    demuxSeeking=true; now+=16; player.buffering(now,false); demuxSeeking=false;
    /* Nothing left to try: one message, and the video is left as it is. */
    fixResult=TUBE_FIX_NONE; title.clear();
    for (int i=0;i<1260 && fixes==3;++i) { now+=16; player.buffering(now,false); }
    assert(fixes==4 && !player.stopped && player.mock.stops==1 && title=="PLAYBACK" && notice.find("Buffering too long. Check")==0);
    /* Switched off: today's behaviour, without a message. */
    assert(tube_extra_set_error_fixer(0)); title.clear();
    for (int i=0;i<1260 && fixes==4;++i) { now+=16; player.buffering(now,false); }
    assert(fixes==5 && title.empty() && !player.stopped);
    phoneOwns=true;
    for (int i=0;i<1300;++i) { now+=16; player.buffering(now,false); }
    assert(fixes==5);
    phoneOwns=false; pb_prebuffer_hold=0;
    tube_player_extra_select(nullptr);
}

int main() {
    test_exit();
    test_dpad();
    test_picker();
    test_action_rows();
    test_afterglow_detail_rows();
    test_action_lists();
    test_presets_and_screen_off();
    test_chapter_notification();
    test_error_fixer_blocks();
    puts("PASS: shipping player exit, D-pad actions, option groups, chapter picker, video actions, presets, chapter notification and error fixer");
}
