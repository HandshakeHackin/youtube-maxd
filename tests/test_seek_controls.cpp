/* Actual scrub code and EVO StateMachine, including hooks and request ownership. */
#include "evo/fsm/StateMachine.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <string>
#include <initializer_list>
#include <unistd.h>
namespace evo {
#include "enums.inc"
namespace PadButtons { constexpr uint32_t Left=1, Right=2, Cross=4, Circle=8; }
static uint64_t now=1000;
static uint64_t GetCurrentTimeMs() { return now; }
static uint64_t NowMs() { return now; }
static bool enginePaused=false, seeking=false, pb_prebuffer_hold=false;
static int pauses=0, resumes=0, player_paused=0, pb_scrub_hold=0, mode=0;
static struct { bool seek_discarding=false; } g_pp_pb;
static void pp_playback_pause(decltype(g_pp_pb)*) { enginePaused=true; ++pauses; }
static void pp_playback_resume(decltype(g_pp_pb)*) { enginePaused=false; ++resumes; }
static int evo_pb_is_paused() { return player_paused; }
static int evo_demux_is_seeking() { return seeking; }
static int tube_player_seek_mode() { return mode; }
static void evo_boot_log(const char*,...) {}
static std::string notice;
static void toast(const char*, const char *message) { notice=message; }
static double resume_base_offset_seconds=0;
static double evo_pb_position_s();
static char resume_file[]="/tmp/tube-seek-resume-XXXXXX";
static const char* evo_data_path(const char*) { return resume_file; }
static double reported=-1, recent_position=-1, chat_position=-1;
static bool chat_eligible;
enum { EVO_PROVIDER_CAP_PROGRESS=1, EVO_PROVIDER_PLAY_UPDATE=2 };
struct evo_provider_t {
    int caps=1;
    void (*report_progress)(const char*,int64_t,int64_t,int)=[](const char*,int64_t pos,int64_t,int) { reported=pos; };
} provider;
static const evo_provider_t* evo_provider_find(const char*) { return &provider; }
static void recent_add_or_update(const char *key,const char*,double pos,double) {
    assert(std::string(key)=="evo://youtube/abcdefghijk"); recent_position=pos;
}
static void recent_save() {}
struct Metadata { void cleanMediaTitle(const std::string&,std::string&,std::string&) {} };
struct Application {
    static Application& getInstance() { static Application a; return a; }
    Metadata* getMediaMetadataService() { return nullptr; }
};
static void tube_player_chat_tick_at(bool active,double pos) { chat_eligible=active; chat_position=pos; }
struct PlaybackController {
    StateMachine<PlaybackState,PlaybackEvent> m_playbackFsm{PlaybackState::Stopped,"PlaybackFSM"};
    bool active=true, live=false, accepted=true, m_tubeScrubWasPaused=false;
    int m_tubeScrubMode=0, requests=0, m_scrubHoldDirection=0;
    uint64_t m_scrubHoldStartMs=0, m_scrubHoldLastStepMs=0, m_scrubAutoCommitDeadlineMs=0;
    double physical=40, requested=-1, m_scrubTargetSeconds=0, m_durationSeconds=100;
    bool keepPaused=false;
    struct {
        std::string provider="youtube", item_id="abcdefghijk", title="Video";
        bool is_live=false;
        bool isProvider() const { return !provider.empty(); }
    } m_source;
    std::string m_currentFilePath="signed-url";
    double m_lastProgressReport=0;
    PlaybackController() { initStateMachine(); }
    bool isActive() const { return active; }
    bool isLiveSource() const { return live; }
    bool isScrubbing() const; bool isPaused() const;
    double getPositionSeconds() const; double getPlaybackPositionSeconds() const;
    double getDurationSeconds() const { return m_durationSeconds; }
    PlaybackState getPlaybackState() const { return m_playbackFsm.getCurrentState(); }
    bool isMusicMode() const { return false; }
    void saveResumePosition();
    bool seekTo(double target,bool paused) { ++requests; requested=target; keepPaused=paused; return accepted; }
    void initStateMachine(); void setPaused(bool); void beginScrub(); void moveScrub(double);
    bool confirmScrub(); void cancelScrub(); void resetScrubHold();
    void updateScrubHold(uint32_t); void tickScrubAutoCommit(); double clampScrubTarget(double) const;
    bool scrubNeedsConfirmation() const;
};
static PlaybackController* current;
static double evo_pb_position_s() { return current->physical; }
#include "scrub.inc"
struct Screen {
    PlaybackController *playback;
    uint64_t m_lastPlayPauseToggleMs=0;
    bool input(uint32_t pressed,uint32_t held=0,uint32_t released=0) {
#include "input.inc"
        return false;
    }
    void chat() {
#include "chat-clock.inc"
    }
    double phonePosition() {
        auto* pb=playback; struct Remote { double position=-1,duration=-1; } snapshot, *p=&snapshot;
#include "phone-clock.inc"
        assert(p->duration==100); return p->position;
    }
};
static void reset(PlaybackController& p,bool paused) {
    current=&p;
    enginePaused=seeking=pb_prebuffer_hold=g_pp_pb.seek_discarding=false;
    pauses=resumes=player_paused=pb_scrub_hold=0; now=1000;
    p.m_playbackFsm.postEvent(PlaybackEvent::Play);
    if (paused) p.m_playbackFsm.postEvent(PlaybackEvent::Pause);
    pauses=resumes=0;
}
}
int main() {
    using namespace evo;
    int temporary=mkstemp(resume_file); assert(temporary>=0); close(temporary);
    for (mode=0;mode<3;++mode) for (bool paused:{false,true}) {
        PlaybackController p; reset(p,paused); p.moveScrub(10);
        assert(p.isScrubbing() && p.m_scrubTargetSeconds==50 && p.m_tubeScrubWasPaused==paused);
        assert(pb_scrub_hold==int(mode!=2)); assert(enginePaused==(paused || mode!=2));
        assert(p.isPaused()==enginePaused);
        const auto pauseCount=pauses; p.beginScrub(); assert(pauses==pauseCount && p.m_tubeScrubWasPaused==paused);
        const int resumeBefore=resumes;
        now+=600; p.tickScrubAutoCommit();
        if (mode) { assert(p.isScrubbing() && !p.requests); assert(p.confirmScrub()); }
        assert(!p.isScrubbing() && p.isPaused()==paused && enginePaused==paused && !pb_scrub_hold);
        assert(p.requests==1 && p.requested==50 && p.keepPaused==paused);
        if (paused) assert(resumes==resumeBefore); // No transient unpause from the FSM.
        p.tickScrubAutoCommit(); assert(p.requests==1 && !p.confirmScrub());

        PlaybackController cancelled; reset(cancelled,paused); cancelled.moveScrub(-10);
        cancelled.physical=45; const int cancelResumes=resumes;
        Screen cancel{&cancelled}; assert(cancel.input(PadButtons::Circle|PadButtons::Cross));
        assert(!cancelled.requests && !cancelled.isScrubbing() && cancelled.isPaused()==paused);
        assert(cancelled.getPositionSeconds()==45 && !pb_scrub_hold && enginePaused==paused);
        assert(cancel.m_lastPlayPauseToggleMs==now && notice=="Cancelled");
        if (paused) assert(resumes==cancelResumes);

        PlaybackController denied; reset(denied,paused); denied.accepted=false; denied.moveScrub(10);
        const int deniedResumes=resumes; Screen failed{&denied}; assert(failed.input(PadButtons::Cross));
        assert(denied.requests==1 && !denied.isScrubbing() && denied.isPaused()==paused);
        assert(denied.getPositionSeconds()==40 && !pb_scrub_hold && enginePaused==paused);
        assert(notice=="Unable to seek; playback state restored");
        if (paused) assert(resumes==deniedResumes);
        PlaybackController released; reset(released,paused); released.moveScrub(10);
        Screen releaseScreen{&released};
        assert(!releaseScreen.input(0,PadButtons::Left,PadButtons::Right)); // Opposite held key keeps the gesture.
        const bool committed=releaseScreen.input(0,0,PadButtons::Right);
        assert(committed==(mode==0) && released.requests==int(mode==0));
        if (!mode) assert(released.keepPaused==paused && released.isPaused()==paused);
        else { assert(released.isScrubbing()); released.cancelScrub(); }
        for (bool requestedPause:{false,true}) {
            PlaybackController external; reset(external,paused); external.moveScrub(10);
            const auto externalResumes=resumes;
            external.setPaused(requestedPause);
            assert(!external.isScrubbing() && external.isPaused()==requestedPause);
            assert(!external.requests && !pb_scrub_hold && enginePaused==requestedPause);
            assert(external.getPositionSeconds()==40);
            if (requestedPause) assert(resumes==externalResumes);
        }
    }
    for (int gate=0;gate<5;++gate) {
        PlaybackController p; reset(p,false);
        if (gate==0) p.live=true;
        if (gate==1) p.active=false;
        if (gate==2) seeking=true;
        if (gate==3) g_pp_pb.seek_discarding=true;
        if (gate==4) pb_prebuffer_hold=true;
        p.moveScrub(10); assert(!p.isScrubbing() && !p.requests && !pb_scrub_hold);
    }
    for (mode=0;mode<3;++mode) for (bool paused:{false,true})
        for (auto oldState:{PlaybackState::Stopped,PlaybackState::Opening,PlaybackState::Seeking,PlaybackState::Finished}) {
        PlaybackController stale; reset(stale,paused); stale.m_playbackFsm.reset();
        if (oldState==PlaybackState::Opening) assert(stale.m_playbackFsm.postEvent(PlaybackEvent::Open));
        else if (oldState!=PlaybackState::Stopped) {
            assert(stale.m_playbackFsm.postEvent(PlaybackEvent::Play));
            assert(stale.m_playbackFsm.postEvent(oldState==PlaybackState::Seeking ? PlaybackEvent::Seek : PlaybackEvent::Finish));
        }
        enginePaused=paused; player_paused=paused;
        const auto oldResumes=resumes;
        stale.moveScrub(10); assert(stale.isScrubbing() && stale.m_tubeScrubWasPaused==paused);
        if (paused) assert(resumes==oldResumes);
        assert(enginePaused==(paused || mode!=2) && player_paused==int(paused));
        assert(stale.confirmScrub() && stale.keepPaused==paused && stale.isPaused()==paused);
    }
    PlaybackController p; reset(p,false); mode=2; p.moveScrub(10);
    p.moveScrub(NAN); p.moveScrub(INFINITY); assert(p.m_scrubTargetSeconds==50);
    p.updateScrubHold(PadButtons::Right); now+=600; p.updateScrubHold(PadButtons::Right);
    assert(p.m_scrubTargetSeconds==53 && !p.m_scrubAutoCommitDeadlineMs && !enginePaused);
    p.moveScrub(1000); assert(p.m_scrubTargetSeconds==100);
    p.moveScrub(-1000); assert(p.m_scrubTargetSeconds==0);
    Screen screen{&p}; assert(screen.input(PadButtons::Cross));
    assert(p.requests==1 && !screen.input(PadButtons::Circle));
    // Other EVO sources keep regular seeking even if YouTube prefers confirmation.
    PlaybackController local; reset(local,true); local.m_source.provider=""; local.moveScrub(10);
    assert(local.m_tubeScrubMode==0); now+=600; local.tickScrubAutoCommit(); assert(local.requests==1 && local.keepPaused);
    mode=1; PlaybackController retained; reset(retained,false); retained.moveScrub(10);
    mode=0; Screen retainedScreen{&retained}; assert(!retainedScreen.input(0,0,PadButtons::Left));
    now+=60000; retained.tickScrubAutoCommit(); assert(retained.isScrubbing() && !retained.requests);
    assert(retainedScreen.input(PadButtons::Cross) && retained.requests==1);
    mode=2; PlaybackController reporting; reset(reporting,false); reporting.moveScrub(30);
    reporting.physical=45; Screen clockScreen{&reporting};
    assert(reporting.getPositionSeconds()==70 && reporting.getPlaybackPositionSeconds()==45);
    resume_base_offset_seconds=5;
    assert(reporting.getPositionSeconds()==70 && reporting.getPlaybackPositionSeconds()==50);
    clockScreen.chat(); assert(chat_eligible && chat_position==50 && clockScreen.phonePosition()==50);
    reporting.saveResumePosition(); assert(reported==50 && recent_position==50);
    FILE* file=std::fopen(resume_file,"r"); assert(file); char key[80]; double saved=-1;
    assert(std::fscanf(file,"%79s%lf",key,&saved)==2 && saved==50);
    assert(std::string(key)=="evo://youtube/abcdefghijk"); std::fclose(file); unlink(resume_file);
    puts("PASS: shipping scrub/FSM modes, pause/commit/cancel/EOF ownership, actual-clock chat/phone/history and input priority");
}
