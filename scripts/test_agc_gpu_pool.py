#!/usr/bin/env python3
"""Exercise strict AGC allocations and legacy CPU fallback from generated C."""
from pathlib import Path
import os
import subprocess
import tempfile

work = Path(os.environ.get("PROSPEROTUBE_NATIVE_WORK", str(Path.home() / ".cache/prosperotube/native")))
player = work / "projects/evoplayer"
header = (player / "media/include/evo_direct_mem.h").read_text()
direct = (player / "media/src/evo_direct_mem.c").read_text()
rml = (player / "ui_rml/src/evo_rmlui_render_agc.cpp").read_text()
hui = (player / "ui_kit/src/hui_agc_batch.cpp").read_text()
assert "void *evo_direct_mem_alloc_gpu(size_t bytes);" in header
assert direct.count("void *evo_direct_mem_alloc_gpu(size_t bytes)") == 1
assert rml.count("agc_alloc_gpu(") == 4  # helper and three GPU consumers
assert hui.count("evo_direct_mem_alloc_gpu(") == 2
assert "evo_direct_mem_alloc(need)" in (player / "media/src/evo_playback.c").read_text()

harness = r'''#include <stdarg.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
void evo_boot_log(const char *fmt, ...) { (void)fmt; }
#include "evo_direct_mem.c"
int main(void)
{
    const size_t mb = 1024u * 1024u;
    assert(evo_direct_mem_init(2u * mb) == 0);
    void *gpu = evo_direct_mem_alloc_gpu(mb + mb / 2u);
    assert(gpu);
    evo_direct_mem_stats_t stats;
    evo_direct_mem_get_stats(&stats);
    assert(stats.num_allocations == 1);
    assert(evo_direct_mem_alloc_gpu(mb) == NULL);
    evo_direct_mem_get_stats(&stats);
    assert(stats.num_allocations == 1); /* fallback was rejected and freed */
    void *cpu = evo_direct_mem_alloc(mb);
    assert(cpu); /* legacy CPU consumers still spill */
    evo_direct_mem_free(cpu);
    evo_direct_mem_free(gpu);
    gpu = evo_direct_mem_alloc_gpu(mb);
    assert(gpu); /* freed pool region is usable again */
    evo_direct_mem_free(gpu);
    evo_direct_mem_shutdown();
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="agc-gpu-pool-") as directory:
    c = Path(directory) / "test.c"
    binary = Path(directory) / "test"
    c.write_text(harness)
    subprocess.run(["cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-pthread",
                    "-I", str(player / "media/include"),
                    "-I", str(player / "media/src"), str(c), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("generated AGC strict GPU pool and CPU fallback: PASS")
