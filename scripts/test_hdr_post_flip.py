#!/usr/bin/env python3
"""Compile the real one-shot post-flip VideoOut readback with a fake VideoOut."""
from pathlib import Path
import subprocess
import tempfile

from hdr_post_flip_overlay import apply_hdr_post_flip_overlay

source = (Path(__file__).resolve().parents[1] /
          "third_party/evo/projects/evoplayer/media/src/evo_agc_runtime.c").read_text()
def replace(path, old, new):
    global source
    assert path.endswith("media/src/evo_agc_runtime.c")
    assert source.count(old) == 1
    source = source.replace(old, new, 1)
apply_hdr_post_flip_overlay(replace, "projects/evoplayer/")
begin = source.index("static void agc_mode_switch_presented(void)\n{")
end = source.index("int evo_agc_runtime_mode_switch_pending(void)", begin)
function = source[begin:end]
assert source.index("agc_mode_switch_presented();") > source.index("sceVideoOutWaitVblank(g_agc_dev.video_handle);")
assert "agc post-flip:" in function and "sceVideoOutGetOutputStatus" in function

harness = r'''#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
typedef struct { unsigned dynamic_range; unsigned long long refresh_rate; } evo_vo_output_status;
static struct {
    const char *mode_switch_what;
    int64_t mode_switch_t0_us;
    int mode_switch_block_ms;
    int video_handle;
} g_agc_dev;
static int reads, logged;
static unsigned next_range;
static int64_t agc_now_us(void) { return 11000; }
static int32_t sceVideoOutGetOutputStatus(int handle, evo_vo_output_status *status)
{
    assert(handle == 7);
    ++reads;
    status->dynamic_range = next_range;
    status->refresh_rate = 13;
    return 0;
}
static void evo_boot_log(const char *format, ...)
{
    char line[256];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (strstr(line, "agc post-flip:")) {
        ++logged;
        assert(strstr(line, "out_rc=0"));
        assert(strstr(line, next_range == 2 ? "dynamic_range=2" : "dynamic_range=1"));
    }
}
''' + function + r'''
int main(void)
{
    g_agc_dev.video_handle = 7;
    g_agc_dev.mode_switch_what = "HDR10 on";
    next_range = 2;
    agc_mode_switch_presented();
    assert(reads == 1 && logged == 1 && !g_agc_dev.mode_switch_what);
    agc_mode_switch_presented();
    assert(reads == 1 && logged == 1);
    g_agc_dev.mode_switch_what = "HDR10 off";
    next_range = 1;
    agc_mode_switch_presented();
    assert(reads == 2 && logged == 2 && !g_agc_dev.mode_switch_what);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="hdr-post-flip-") as directory:
    c = Path(directory) / "check.c"
    exe = Path(directory) / "check"
    c.write_text(harness)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    str(c), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("HDR post-flip VideoOut readback: PASS")
