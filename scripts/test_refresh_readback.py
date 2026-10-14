#!/usr/bin/env python3
"""Exercise the actual VideoOut refresh probe with conflicting status reports."""
from pathlib import Path
import subprocess
import tempfile

from refresh_readback_overlay import apply_refresh_readback_overlay

upstream = Path(__file__).resolve().parents[1] / "third_party/evo/projects/evoplayer"
sources = {name: (upstream / name).read_text() for name in
           ("media/src/evo_agc_runtime.c", "core/src/Application.cpp")}
def replace(path, old, new):
    name = path.removeprefix("projects/evoplayer/")
    assert name in sources and sources[name].count(old) == 1
    sources[name] = sources[name].replace(old, new, 1)
apply_refresh_readback_overlay(replace, "projects/evoplayer/")
source = sources["media/src/evo_agc_runtime.c"]
start = source.index("        evo_vo_output_status vout;", source.index("agc display 120hz support probe"))
end = source.index("        evo_boot_log_flush();", start) + len("        evo_boot_log_flush();")
probe = source[start:end]
post_config = next(line.strip() for line in source.splitlines()
                   if "g_agc_dev.current_refresh_rate = (int)(vorc == 0" in line)

harness = r'''#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef struct { unsigned resolution, dynamic_range; uint64_t refresh_rate, flags; } evo_vo_output_status;
static struct { int video_handle, current_refresh_rate, is_120hz, supports_120hz;
               int display_dynamic_range, display_is_hdr; unsigned display_resolution_token; } g_agc_dev;
static int32_t next_rc, reads;
static uint64_t next_refresh;
static int32_t sceVideoOutGetOutputStatus(int handle, evo_vo_output_status *status)
{
    assert(handle == 7);
    ++reads;
    status->refresh_rate = next_refresh;
    status->dynamic_range = 2;
    return next_rc;
}
static void evo_boot_log(const char *format, ...) { (void)format; }
static void evo_boot_log_flush(void) {}
static void probe(int resolution_hint_120, int output_rc, uint64_t output_refresh)
{
    g_agc_dev.video_handle = 7;
    g_agc_dev.is_120hz = resolution_hint_120;
    g_agc_dev.supports_120hz = resolution_hint_120;
    g_agc_dev.current_refresh_rate = resolution_hint_120 ? 13 : 3;
    next_rc = output_rc;
    next_refresh = output_refresh;
''' + probe + r'''
}
static void post_config(int status_rc, uint64_t output_refresh, uint64_t resolution_refresh)
{
    int rc = 0, vorc = status_rc;
    evo_vo_output_status vout = {0};
    struct { uint64_t refresh_rate; } vres = {0};
    vout.refresh_rate = output_refresh;
    vres.refresh_rate = resolution_refresh;
    if (rc == 0) {
''' + post_config + r'''
    }
}
int main(void)
{
    probe(1, 0, 3);  /* .40: resolution says 120, live output says default. */
    assert(g_agc_dev.is_120hz == 0 && g_agc_dev.current_refresh_rate == 3);
    probe(0, 0, 13); /* Live output wins in the other direction too. */
    assert(g_agc_dev.is_120hz == 1 && g_agc_dev.current_refresh_rate == 13);
    assert(g_agc_dev.supports_120hz == 1);
    probe(1, 0, 0);  /* A successful unknown output must not reuse the stale hint. */
    assert(g_agc_dev.is_120hz == 0 && g_agc_dev.current_refresh_rate == 0);
    probe(1, -1, 0); /* Fall back only if OutputStatus is unavailable. */
    assert(g_agc_dev.is_120hz == 1 && g_agc_dev.current_refresh_rate == 13);
    assert(reads == 4);
    post_config(0, 13, 13);
    assert(g_agc_dev.current_refresh_rate == 13);
    post_config(0, 3, 13); /* ResolutionStatus stays 13 after output returns to 3. */
    assert(g_agc_dev.current_refresh_rate == 3);
    post_config(-1, 0, 13);
    assert(g_agc_dev.current_refresh_rate == 13);
    post_config(0, 0, 13);
    assert(g_agc_dev.current_refresh_rate == 13);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="refresh-readback-") as directory:
    c = Path(directory) / "check.c"
    exe = Path(directory) / "check"
    c.write_text(harness)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    str(c), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("VideoOut refresh probe: PASS")

# The first cadence evaluation must reconcile a 120-Hz boot output before 25/50 fps playback.
app = sources["core/src/Application.cpp"]
begin = app.index("            static bool s_want_120 =")
end = app.index("\n        }\n\n        bool hasAnim", begin)
cadence = app[begin:end]
cpp = r'''#include <cassert>
#include <cmath>
#include <cstdlib>
static int actual_120, configure_calls;
static double frame_rate;
static int evo_agc_runtime_is_120hz() { return actual_120; }
static int evo_agc_runtime_mode_switch_pending() { return 0; }
static int evo_agc_runtime_set_120hz(int enable)
{
    ++configure_calls;
    actual_120 = enable;
    return 0;
}
static double evo_pb_video_fps() { return frame_rate; }
static void evo_bt(const char *, ...) {}
struct Controller { bool isActive() const { return true; } bool isMusicMode() const { return false; } };
struct App {
    Controller controller;
    Controller *m_playbackController = &controller;
    void tick(bool isPlayer, bool isSurround) {
''' + cadence + r'''
    }
};
int main(int argc, char **argv)
{
    assert(argc == 3);
    actual_120 = std::atoi(argv[1]);
    const int initial_120 = actual_120;
    frame_rate = std::atof(argv[2]);
    App app;
    app.tick(true, false);
    if (initial_120 == 1 && frame_rate == 30.0)
        assert(configure_calls == 0);
    else
        assert(configure_calls == 1);
    assert(actual_120 == (frame_rate == 30.0));
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="refresh-cadence-") as directory:
    c = Path(directory) / "check.cpp"
    exe = Path(directory) / "check"
    c.write_text(cpp)
    subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    str(c), "-o", str(exe)], check=True)
    for actual, fps in (("1", "25"), ("1", "50"), ("1", "30"), ("0", "30")):
        subprocess.run([str(exe), actual, fps], check=True)
print("First-playback cadence reconciliation: PASS")
