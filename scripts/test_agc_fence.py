#!/usr/bin/env python3
"""Exercise the generated AGC fence helpers with delayed and missing markers."""
from pathlib import Path
import os
import subprocess
import tempfile

work = Path(os.environ.get("PROSPEROTUBE_NATIVE_WORK", str(Path.home() / ".cache/prosperotube/native")))
source = (work / "projects/evoplayer/media/src/evo_agc_runtime.c").read_text()
begin = source.index("static int agc_wait_slot_fence(")
end = source.index("int evo_agc_runtime_gpu_data_idle(", begin)
helpers = source[begin:end]

# These checks connect the exercised helpers to the actual call paths. A
# successful helper alone would not protect a slot if its caller ignored it.
assert "if (!agc_emit_frame_fence(&g_agc_dev.current_cb, slot, marker))" in source
assert "dropping unsubmitted frame" in source
assert "if (!retired) {\n                evo_boot_log(\"agc frame_end" in source
assert "return; /* no flip, slot advance, source retirement or command reuse */" in source
assert "return; /* preserve this sealed slot and its live GPU mappings */" in source
assert "if (!agc_wait_gpu_idle(500))" in source
assert "if (!g_agc_dev.frame_active) return -1;" in source

harness = r'''#include <assert.h>
#include <stdint.h>
#include <stddef.h>
typedef int SceAgcCommandBuffer;
static uint32_t fence;
static struct { uint32_t *fences[1]; } g_agc_dev = {{&fence}};
static unsigned sleeps, release_calls, flush_calls;
static unsigned marker_after_sleep;
static uint32_t marker_to_write;
static int color_result, release_result;
static uint64_t emitted_address;
static uint32_t emitted_marker;
static void evo_agc_runtime_cache_flush(const void *p, size_t n)
{ assert(p == &fence && n == 4); ++flush_calls; }
static void sceKernelUsleep(unsigned n)
{ assert(n == 1000); if (++sleeps == marker_after_sleep) fence = marker_to_write; }
static int evo_agc_writer_flush_color_target(SceAgcCommandBuffer *cb)
{ assert(cb); return color_result; }
static int evo_agc_writer_release_mem(SceAgcCommandBuffer *cb, uint64_t addr, uint32_t marker)
{ assert(cb); ++release_calls; emitted_address = addr; emitted_marker = marker; return release_result; }
'''+helpers+r'''
int main(void)
{
    SceAgcCommandBuffer cb = 1;
    unsigned waited = 99;
    color_result = -1;
    assert(!agc_emit_frame_fence(&cb, 0, 7));
    assert(release_calls == 0);
    color_result = 0; release_result = -1;
    assert(!agc_emit_frame_fence(&cb, 0, 7));
    assert(release_calls == 1);
    release_result = 0;
    assert(agc_emit_frame_fence(&cb, 0, 7));
    assert(release_calls == 2);
    assert(emitted_address == (uint64_t)(uintptr_t)&fence && emitted_marker == 7);

    fence = 0; sleeps = flush_calls = 0; marker_after_sleep = 100;
    assert(!agc_wait_slot_fence(0, 7, 3, 1000, &waited));
    assert(waited == 3 && sleeps == 3 && flush_calls == 3 && fence == 0);
    marker_after_sleep = sleeps + 1; marker_to_write = 7;
    assert(agc_wait_slot_fence(0, 7, 3, 1000, &waited));
    assert(waited == 1 && fence == 7);
    sleeps = 0;
    assert(agc_wait_slot_fence(0, 7, 3, 1000, &waited));
    assert(waited == 0 && sleeps == 0);
    assert(!agc_wait_slot_fence(0, 0, 3, 1000, &waited));
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="agc-fence-") as directory:
    c = Path(directory) / "fence.c"
    binary = Path(directory) / "fence"
    c.write_text(harness)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(c), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("generated AGC fence failure/timeout/retry: PASS")
