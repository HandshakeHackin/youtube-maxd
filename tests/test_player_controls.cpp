/* Actual generated PS5 input and visibility, including debounce and reopen gates. */
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
namespace PadButtons { constexpr uint32_t Cross=1, Right=2; }
static uint64_t clockMs=10000;
static uint64_t NowMs() { return clockMs; }
static int mode=0, hide=3000, lastSpeed=125;
static int tube_player_ok_mode() { return mode; }
static int tube_player_hide_ms() { return hide; }
static int tube_last_playback_speed() { return lastSpeed; }
static std::string notice;
static void toast(const char *,const char *message) { notice=message; }
struct Playback {
    bool paused=false, music=false, scrub=false, speedReady=true, speedSucceeds=true;
    int speed=100, toggles=0, reopens=0;
    void togglePause() { paused=!paused; ++toggles; }
    bool isPaused() const { return paused; }
    bool isMusicMode() const { return music; }
    bool isScrubbing() const { return scrub; }
    int playbackSpeedPercent() const { return speed; }
    bool canChangePlaybackSpeed() const { return speedReady; }
    bool changePlaybackSpeed(int target) { ++reopens; if (!speedSucceeds) return false; speed=target; return true; }
};
struct Screen {
    Playback mock;
    int m_osdVisibilityAlpha=0;
    uint64_t m_controlsLastUsedMs=0, m_lastPlayPauseToggleMs=0;
    bool m_tubeSuppressControls=false, m_showStatsForNerds=false;
    bool input(uint32_t pressed) {
        auto playback=&mock;
#include "activity.inc"
#include "cross.inc"
        return false;
    }
    bool visible(bool ended=false,bool isBuffering=false) {
        auto playback=&mock; const auto now=NowMs(); const bool reallyPaused=mock.paused;
#include "visibility.inc"
        return wantsControls;
    }
};
int main() {
    for (mode=0;mode<4;++mode) {
        Screen s; notice.clear(); assert(!s.visible()); assert(s.input(PadButtons::Cross));
        assert(s.mock.toggles==(mode==1 || mode==2));
        assert(s.mock.reopens==(mode==3));
        assert(s.visible()==(mode<2));
        assert(s.mock.speed==(mode==3 ? 125 : 100));
        if (mode==2 || mode==3) {
            assert(s.visible(true) && s.visible(false,true));
            s.m_showStatsForNerds=true; assert(s.visible()); s.m_showStatsForNerds=false;
            s.mock.scrub=true; assert(s.visible()); s.mock.scrub=false;
            s.mock.music=true; assert(s.visible()); s.mock.music=false;
            s.input(PadButtons::Right); assert(s.visible() && !s.m_tubeSuppressControls);
        }
        Screen shown; shown.m_osdVisibilityAlpha=1;
        shown.input(PadButtons::Cross); assert(shown.mock.paused && shown.mock.reopens==0 && shown.visible());
        Screen scrub; scrub.mock.scrub=true;
        scrub.input(PadButtons::Cross); assert(!scrub.m_tubeSuppressControls && scrub.mock.reopens==0);
    }
    mode=3;
    Screen toggle; toggle.mock.speed=150; toggle.input(PadButtons::Cross);
    assert(toggle.mock.speed==100); clockMs+=150; toggle.input(PadButtons::Cross);
    assert(toggle.mock.speed==125); toggle.input(PadButtons::Cross); assert(toggle.mock.reopens==2);
    Screen denied; denied.mock.speedReady=false; denied.input(PadButtons::Cross);
    assert(!denied.mock.reopens && !denied.mock.toggles && !notice.empty() && !denied.visible());
    Screen failed; failed.mock.speedSucceeds=false; failed.input(PadButtons::Cross);
    assert(failed.mock.reopens==1 && failed.mock.speed==100 && !failed.mock.toggles);
    mode=2; Screen pause; pause.input(PadButtons::Cross); assert(pause.mock.paused && !pause.visible());
    clockMs+=150; pause.input(PadButtons::Cross); assert(!pause.mock.paused && !pause.visible());
    mode=0; Screen timeout; timeout.input(PadButtons::Cross);
    clockMs+=2999; assert(timeout.visible()); ++clockMs; assert(!timeout.visible());
    hide=0; assert(timeout.visible()); hide=15000;
    timeout.input(PadButtons::Right); clockMs+=14999; assert(timeout.visible()); ++clockMs; assert(!timeout.visible());
    mode=2; Screen chord; chord.input(PadButtons::Cross|PadButtons::Right);
    assert(chord.visible() && !chord.m_tubeSuppressControls);
    puts("PASS: shipping hidden/visible Cross modes, speed gates, debounce, timeouts and urgent overlays");
}
