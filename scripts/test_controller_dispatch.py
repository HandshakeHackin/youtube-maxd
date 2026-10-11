"""Compile shipping application dispatch and provider stick bindings together."""
from pathlib import Path
import os
import subprocess
import tempfile

work = Path(os.environ.get("PROSPEROTUBE_NATIVE_WORK", Path.home() / ".cache/prosperotube/native"))
player = work / "projects/evoplayer"
app = (player / "core/src/Application.cpp").read_text()
start = app.index("            /* ProsperoTube controller routing:")
end = app.index("            lastButtons = padData.buttons;", start)
dispatch = app[start:end]
host = (player / "core/src/screens/ProviderHostScreen.cpp").read_text()
input_start = host.index("bool ProviderHostScreen::handleInput(")
voice_start = host.index("    (void)held;", input_start)
voice_end = host.index("    /* Discovery takes", voice_start)
voice_controls = host[voice_start:voice_end]
stick_start = host.index("        if ((pressed & PadButtons::L3)", input_start)
start = host.rindex('    if (m_providerId == "youtube") {', input_start, stick_start)
end = host.index("    if (pressed & PadButtons::Square)", start)
bindings = host[start:end]
screen = (player / "core/src/screens/PlayerScreen.cpp").read_text()
start = screen.index("    // ProsperoTube highlight input (")
end = screen.index("    // End ProsperoTube highlight input.", start)
highlight = screen[start:end]

scaffold = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>
enum class ScreenId { EmbyBrowse, Player, Settings };
namespace PadButtons {
constexpr uint32_t L3=2, R3=4, Cross=16, TouchPad=32, L2=64, R2=128, Circle=256, Triangle=512;
}
static bool keyboard, web, capture_ok=true;
static unsigned captures, key_updates, key_calls, screen_calls;
static uint32_t key_pressed, screen_pressed, screen_held, screen_released;
static std::vector<int> discussion;
static bool discussion_handles=true;
static int evo_rmlui_provider_discussion_key(int key) { discussion.push_back(key); return discussion_handles; }
static int evo_keyboard_is_open() { return keyboard; }
static int evo_webui_active() { return web; }
static void evo_keyboard_update() { ++key_updates; }
static void evo_keyboard_handle_input(uint32_t pressed) { ++key_calls; key_pressed=pressed; }
static bool evo_capture_screenshot(std::string& path) { ++captures; path="shot.bmp"; return capture_ok; }
static void evo_bt(const char*, ...) {}
static void evo_boot_log_flush() {}
static void toast(const char*,const char*) {}
static void notifyOnScreen(const char*) {}
static void evo_feedback(int) {}
enum { EVO_FB_CONFIRM, EVO_FB_BOUNDARY };
static std::string m_providerId="youtube";
enum { TUBE_VOICE_IDLE, TUBE_VOICE_RECORDING, TUBE_VOICE_TRANSCRIBING };
static int voice_state;
static bool m_voiceHeld, voice_admitted=true;
static unsigned voice_begins, voice_finishes, voice_cancels, voice_pauses;
static int tube_voice_state() { return voice_state; }
static bool tube_voice_begin(unsigned owner, long long) {
    assert(owner==0); ++voice_begins;
    if (!voice_admitted) return false;
    voice_state=TUBE_VOICE_RECORDING; return true;
}
static void tube_voice_finish() { ++voice_finishes; voice_state=TUBE_VOICE_TRANSCRIBING; }
static void tube_voice_cancel() { ++voice_cancels; voice_state=TUBE_VOICE_IDLE; }
struct VoicePlayback { bool isActive() { return true; } void setPaused(bool value) { assert(value); ++voice_pauses; } };
struct Application {
    static Application& getInstance() { static Application app; return app; }
    VoicePlayback* getPlaybackController() { static VoicePlayback pb; return &pb; }
};
static bool providerInput(uint32_t pressed,uint32_t held,uint32_t released) {
'''
scaffold += voice_controls + bindings + r'''
    return false;
}
static unsigned highlights, seeks;
static bool pb_prebuffer_hold;
static struct { bool seek_discarding=false; } g_pp_pb;
static int evo_pb_is_complete() { return 0; }
static int evo_pb_is_paused() { return 0; }
static int evo_demux_is_seeking() { return 0; }
static double tube_highlight_target(double,double) { ++highlights; return 12; }
static void tube_highlight_seek_failed(double) { assert(false); }
static void tube_highlight_retry_idle() {}
static int tube_sponsor_show_message() { return 0; }
static const char* tube_sponsor_prompt(double) { return nullptr; }
struct Playback {
    bool isLiveSource() { return false; }
    bool isScrubbing() { return false; }
    double getPositionSeconds() { return 1; }
    double getDurationSeconds() { return 100; }
    bool seekTo(double target,bool keepPaused) { assert(target==12 && !keepPaused); ++seeks; return true; }
};
static Playback state, *playback=&state;
static bool playerInput(uint32_t pressed) {
'''
scaffold += highlight + r'''
    return false;
}
struct Manager {
    ScreenId id=ScreenId::EmbyBrowse;
    ScreenId getCurrentScreenId() { return id; }
    void handleInput(uint32_t pressed,uint32_t held,uint32_t released) {
        ++screen_calls; screen_pressed=pressed; screen_held=held; screen_released=released;
        if (id==ScreenId::EmbyBrowse) providerInput(pressed,held,released);
        if (id==ScreenId::Player) playerInput(pressed);
    }
};
static Manager manager;
static Manager* m_screenManager=&manager;
static void route(uint32_t pressed,uint32_t held=0,uint32_t released=0) {
    const bool hasInput=pressed || held || released;
'''
scaffold += dispatch + r'''
}
static void reset(ScreenId id=ScreenId::EmbyBrowse) {
    manager.id=id; keyboard=web=false; capture_ok=true;
    captures=key_updates=key_calls=screen_calls=highlights=seeks=0;
    key_pressed=screen_pressed=screen_held=screen_released=0;
    discussion.clear(); m_providerId="youtube";
    discussion_handles=voice_admitted=true; m_voiceHeld=false; voice_state=TUBE_VOICE_IDLE;
    voice_begins=voice_finishes=voice_cancels=voice_pauses=0;
}
int main() {
    reset(); route(PadButtons::R3,0x100,0x200);
    assert(captures==0 && screen_calls==1 && discussion==std::vector<int>{0});
    assert(screen_pressed==PadButtons::R3 && screen_held==0x100 && screen_released==0x200);
    reset(); route(PadButtons::L3);
    assert(captures==0 && discussion==std::vector<int>{5});
    reset(); route(PadButtons::L3|PadButtons::R3);
    assert(captures==0 && screen_pressed==(PadButtons::L3|PadButtons::R3));
    assert(discussion==std::vector<int>{5}); // Existing provider precedence.
    reset(); route(PadButtons::TouchPad);
    assert(discussion==std::vector<int>{4});
    reset(); route(PadButtons::Triangle);
    assert(!captures && screen_pressed==PadButtons::Triangle && discussion==std::vector<int>{6});
    reset(); route(PadButtons::Triangle|PadButtons::L3);
    assert(discussion==std::vector<int>{6} && !voice_begins); // Replay paging precedes compose.
    reset(); discussion_handles=false; route(PadButtons::Triangle);
    assert(discussion==std::vector<int>{6} && !voice_begins && !captures);
    reset(); m_providerId="other"; route(PadButtons::Triangle); assert(discussion.empty());
    reset(); route(PadButtons::L2); assert(discussion==std::vector<int>{1});
    reset(); route(PadButtons::R2); assert(discussion==std::vector<int>{2});
    reset(); route(PadButtons::Cross);
    assert(!captures && screen_pressed==PadButtons::Cross && discussion.empty());
    reset(); m_providerId="other"; route(PadButtons::R3);
    assert(!captures && screen_pressed==PadButtons::R3 && discussion.empty());
    for (auto id:{ScreenId::Player,ScreenId::Settings}) {
        for (auto bit:{PadButtons::L3,PadButtons::R3}) {
            reset(id); route(bit|PadButtons::Cross);
            const bool screenshot=id==ScreenId::Settings || bit==PadButtons::L3;
            assert(captures==unsigned(screenshot) && screen_pressed==(screenshot ? PadButtons::Cross : bit|PadButtons::Cross));
            assert(highlights==unsigned(!screenshot) && seeks==highlights);
            reset(id); capture_ok=false; route(bit|PadButtons::Cross);
            assert(captures==unsigned(screenshot) && screen_pressed==(screenshot ? PadButtons::Cross : bit|PadButtons::Cross));
            reset(id); keyboard=true; route(bit|PadButtons::Cross);
            assert(!captures && !screen_calls && key_updates==1 && key_calls==1);
            assert(key_pressed==(bit|PadButtons::Cross));
            reset(id); web=true; route(bit);
            assert(!captures && !screen_calls && !key_calls);
        }
    }
    reset(ScreenId::Player); route(PadButtons::L3|PadButtons::R3);
    assert(captures==1 && screen_pressed==PadButtons::R3 && highlights==1 && seeks==1);
    reset(); keyboard=true; route(PadButtons::L3);
    assert(!captures && !screen_calls && key_pressed==PadButtons::L3);
    reset(); route(0); assert(!captures && !screen_calls && discussion.empty());
    reset(); discussion_handles=false; route(PadButtons::L3);
    assert(!captures && voice_begins==1 && voice_pauses==1 && m_voiceHeld);
    discussion.clear(); route(0,0,PadButtons::L3);
    assert(voice_finishes==1 && !m_voiceHeld && voice_state==TUBE_VOICE_TRANSCRIBING && discussion.empty());
    route(PadButtons::R3); assert(discussion.empty());
    route(PadButtons::Circle); assert(voice_cancels==1 && voice_state==TUBE_VOICE_IDLE);
    reset(); discussion_handles=false; voice_admitted=false; route(PadButtons::L3);
    assert(voice_begins==1 && !voice_pauses && !m_voiceHeld);
    reset(); discussion_handles=false; route(PadButtons::L3); route(PadButtons::Circle);
    assert(voice_cancels==1 && !voice_finishes && !m_voiceHeld);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="tube-controller-dispatch-") as directory:
    source = Path(directory) / "test.cpp"
    binary = Path(directory) / "test"
    source.write_text(scaffold)
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("PASS: shipping global dispatch preserves provider stick bindings and keyboard/browser ownership; screenshots remain on other screens")
