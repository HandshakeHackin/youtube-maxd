"""Check ProsperoTube's default and EVO's supported video cadences."""

from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PLAYER = ROOT / "third_party/evo/projects/evoplayer"


def main():
    sources = {
        "core/src/Application.cpp": (PLAYER / "core/src/Application.cpp").read_text(),
        "core/include/evo/services/SettingsService.hpp": (PLAYER / "core/include/evo/services/SettingsService.hpp").read_text(),
        "media/src/evo_agc_runtime.c": (PLAYER / "media/src/evo_agc_runtime.c").read_text(),
    }

    def replace(path, old, new, count=1):
        source = sources[path.removeprefix("projects/evoplayer/")]
        assert source.count(old) == count, (path, old)
        sources[path.removeprefix("projects/evoplayer/")] = source.replace(old, new)

    import sys
    sys.path.insert(0, str(ROOT / "scripts"))
    from auto_frame_rate_overlay import apply_auto_frame_rate_overlay
    apply_auto_frame_rate_overlay(replace, "projects/evoplayer/")

    settings = sources["core/include/evo/services/SettingsService.hpp"]
    runtime = sources["media/src/evo_agc_runtime.c"]
    assert "m_refreshRateMode = RefreshRateMode::PlaybackOnly;" in settings
    assert "g_agc_dev.supports_120hz = (sup120 > 0) ? 1 : 0;" in runtime
    patched_app = sources["core/src/Application.cpp"]
    assert "m_screenManager->isPlaybackScreen(m_screenManager->getCurrentScreenId())" in patched_app
    assert "const int want = (inPlaybackSession && setting_on" in patched_app
    assert "} else if (!inPlaybackSession) {\n                want = false;" in patched_app
    playback_screens = (PLAYER / "core/src/screens/ScreenManager.cpp").read_text().split(
        "bool ScreenManager::isPlaybackScreen(ScreenId screenId) const {", 1)[1].split("\nint ScreenManager::railSectionCount", 1)[0]
    for menu in ("ExitConfirm", "MediaInfo", "SubtitlePicker", "AudioTrackPicker"):
        assert f"case ScreenId::{menu}:" in playback_screens

    app = patched_app
    hdr_decision = "const int want = (inPlaybackSession" + app.split(
        "const int want = (inPlaybackSession", 1)[1].split(";", 1)[0] + ";"
    start = app.index("const double k = 119.88 / fps;")
    end = app.index("\n", app.index("want = kr >= 1.0", start))
    decision = app[start:end]
    def policy_block(marker):
        start = app.index(marker)
        end = app.index("{", start) + 1
        depth = 1
        while depth:
            depth += (app[end] == "{") - (app[end] == "}")
            end += 1
        return app[start:end]

    mode_policy = policy_block("        {\n            static int  s_hdr_want") + "\n" + policy_block(
        "if (m_settingsService && m_settingsService->getRefreshRateMode() == RefreshRateMode::PlaybackOnly")
    policy_fixture = r"""
enum class RefreshRateMode { Off, PlaybackOnly, Always };
enum class HdrOutputMode { Off, Auto };
struct Settings {
    RefreshRateMode refresh = RefreshRateMode::PlaybackOnly;
    HdrOutputMode hdr = HdrOutputMode::Auto;
    RefreshRateMode getRefreshRateMode() const { return refresh; }
    HdrOutputMode getHdrOutputMode() const { return hdr; }
} settings;
struct Playback { bool isMusicMode() const { return false; } } playback;
Settings *m_settingsService = &settings;
Playback *m_playbackController = &playback;
static bool actual120, actual_hdr, pending;
static int refresh_failures, hdr_failures, refresh_calls, hdr_calls, notices;
static int transfer = 1;
static double rate = 24;
static long long clock_ms;
long long now_ms() { return clock_ms; }
int evo_agc_runtime_is_120hz() { return actual120; }
int evo_agc_runtime_supports_120hz() { return 1; }
int evo_agc_runtime_mode_switch_pending() { return pending; }
int evo_agc_runtime_hdr_output_active() { return actual_hdr; }
int evo_agc_runtime_last_video_trc() { return transfer; }
double evo_pb_video_fps() { return rate; }
int evo_agc_runtime_set_120hz(int want) {
    ++refresh_calls;
    if (refresh_failures) { if (refresh_failures > 0) --refresh_failures; return -1; }
    actual120 = want; return 0;
}
int evo_agc_runtime_set_hdr_output(int want) {
    ++hdr_calls;
    if (hdr_failures) { if (hdr_failures > 0) --hdr_failures; return -1; }
    actual_hdr = want; return 0;
}
void evo_bt(const char *, ...) {}
void toast(const char *, const char *) { ++notices; }
void tick(bool inPlaybackSession = true) {
    bool playbackActive = inPlaybackSession, isPlayer = inPlaybackSession, isSurround = false;
""" + mode_policy + r"""
}
void advance(long long ms) { clock_ms += ms; tick(); }
void test_mode_failures() {
    // A failed entry does not become applied state; retry after one second.
    refresh_failures = 1; tick();
    assert(refresh_calls == 1 && !actual120);
    advance(999); assert(refresh_calls == 1);
    advance(1); assert(refresh_calls == 2 && actual120);
    // Exiting 120 Hz gets the same bounded recovery.
    rate = 25; refresh_failures = 1; tick();
    assert(refresh_calls == 3 && actual120);
    advance(999); assert(refresh_calls == 3);
    advance(1); assert(refresh_calls == 4 && !actual120);
    // Permanent refusal makes at most three requests, at 0, 1 and 3 seconds.
    rate = 24; refresh_failures = -1; tick();
    advance(999); assert(refresh_calls == 5);
    advance(1); assert(refresh_calls == 6);
    advance(1999); assert(refresh_calls == 6);
    advance(1); assert(refresh_calls == 7);
    advance(10000); assert(refresh_calls == 7 && !actual120);
    // A changed request gets a new budget; external Settings changes are read back.
    rate = 25; tick(); rate = 24; refresh_failures = 0; tick();
    assert(refresh_calls == 8 && actual120);
    actual120 = false; tick(); assert(refresh_calls == 9 && actual120);
    // Pending HDR changes still precede the rate switch, with the original bound.
    rate = 25; pending = true;
    for (int i = 0; i < 30; ++i) tick();
    assert(refresh_calls == 9);
    tick(); assert(refresh_calls == 10 && !actual120); pending = false;
    settings.refresh = RefreshRateMode::Off;
    // HDR entry retries preserve one user notice for this failure episode/session.
    transfer = 16; hdr_failures = 1; tick();
    assert(hdr_calls == 1 && !actual_hdr && notices == 1);
    advance(999); assert(hdr_calls == 1);
    advance(1); assert(hdr_calls == 2 && actual_hdr && notices == 1);
    transfer = -1; tick(); assert(hdr_calls == 2 && actual_hdr); // playback menu
    transfer = 1; hdr_failures = 1; tick();
    assert(hdr_calls == 3 && actual_hdr);
    advance(1000); assert(hdr_calls == 4 && !actual_hdr);
    transfer = 18; tick(); assert(hdr_calls == 5 && actual_hdr);
    // A refused HDR exit must not retry every frame or misreport applied state.
    transfer = 1; hdr_failures = -1; tick();
    advance(999); assert(hdr_calls == 6 && actual_hdr);
    advance(1); assert(hdr_calls == 7);
    advance(1999); assert(hdr_calls == 7);
    advance(1); assert(hdr_calls == 8);
    advance(10000); assert(hdr_calls == 8 && actual_hdr);
    transfer = 16; tick(); transfer = 1; hdr_failures = 0; tick();
    assert(hdr_calls == 9 && !actual_hdr);
    // External mode changes must not leave the policy's previous cache stale.
    actual_hdr = true; tick(); assert(hdr_calls == 10 && !actual_hdr);
    tick(false); transfer = 16; hdr_failures = -1; tick();
    advance(1000); advance(2000); advance(10000);
    assert(hdr_calls == 13 && !actual_hdr && notices == 2);
}
"""
    program = f"""
#include <cassert>
#include <cmath>
#include <initializer_list>
bool wants_hdr(bool inPlaybackSession, bool setting_on, int trc, bool s_hdr_want) {{
    {hdr_decision}
    return want;
}}
bool wants_120(double fps) {{
    bool want = false;
    if (fps > 1.0) {{ {decision} }}
    return want;
}}
{policy_fixture}
int main() {{
    test_mode_failures();
    assert(wants_hdr(true, true, -1, true)); // Playback menu has no current frame.
    assert(!wants_hdr(true, true, 1, true));
    assert(!wants_hdr(false, true, -1, true));
    assert(!wants_hdr(true, false, 16, true));
    for (double fps : {{23.976, 24.0, 29.97, 30.0, 59.94, 60.0}})
        assert(wants_120(fps));
    for (double fps : {{0.0, 1.0, 25.0, 50.0}})
        assert(!wants_120(fps));
}}
"""
    with tempfile.TemporaryDirectory() as directory:
        source = Path(directory) / "test.cpp"
        binary = Path(directory) / "test"
        source.write_text(program)
        subprocess.run(["g++", "-std=c++17", str(source), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    print("automatic frame-rate/HDR policy, bounded failure retries and applied-state recovery: pass")


if __name__ == "__main__":
    main()
