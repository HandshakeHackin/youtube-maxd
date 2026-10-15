#!/usr/bin/env python3
"""Check optional AGC binding logs identify a faulting texture range."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

work = Path(os.environ.get("PROSPEROTUBE_NATIVE_WORK", str(Path.home() / ".cache/prosperotube/native")))
player = work / "projects/evoplayer"
header = (player / "media/include/evo_agc_runtime.h").read_text()
runtime = (player / "media/src/evo_agc_runtime.c").read_text()
rml = (player / "ui_rml/src/evo_rmlui_render_agc.cpp").read_text()
hui = (player / "ui_kit/src/hui_agc_batch.cpp").read_text()
assert "#ifdef TUBE_GPU_BIND_TRACE" in header
assert "#define EVO_AGC_TRACE_BIND(origin, address, bytes, descriptor) ((void)0)" in header
assert 'EVO_AGC_TRACE_BIND("rml-gpu-pool"' in rml
assert 'EVO_AGC_TRACE_BIND("hui-gpu-pool"' in hui
assert '"video-direct-y"' in runtime and '"video-direct-uv"' in runtime
start = runtime.index("void evo_agc_runtime_trace_binding(")
brace = runtime.index("{", start)
depth = 1
end = brace + 1
while depth:
    depth += (runtime[end] == "{") - (runtime[end] == "}")
    end += 1
function = runtime[start:end]
source = r'''#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
static char line[512];
static struct { int initialized, frame_active; unsigned current_slot; unsigned long long frame_counter; } g_agc_dev = {1,1,2,17};
static void evo_boot_log(const char *fmt, ...) {
    va_list args; va_start(args, fmt); vsnprintf(line, sizeof line, fmt, args); va_end(args);
}
''' + function + r'''
int main(void) {
    evo_agc_runtime_trace_binding("hui-gpu-pool", 0x2a50f0000ULL, 0x20000, 0x21000ULL);
    unsigned long long base=0, descriptor=0; size_t bytes=0;
    char origin[64]; unsigned long long frame=0; unsigned slot=0;
    int n=sscanf(line,"gpu_bind frame=%llu slot=%u origin=%63s addr=0x%llx bytes=%zu desc=0x%llx",
                 &frame,&slot,origin,&base,&bytes,&descriptor);
    assert(n==6 && frame==17 && slot==2 && strcmp(origin,"hui-gpu-pool")==0);
    assert(0x2a50fb000ULL>=base && 0x2a50fb000ULL-base<bytes);
    assert(!(0x2a5248000ULL>=base && 0x2a5248000ULL-base<bytes));
    assert(descriptor==0x21000ULL);
    g_agc_dev.frame_active=0; line[0]=0;
    evo_agc_runtime_trace_binding("hui-gpu-pool",base,bytes,descriptor);
    assert(line[0]==0);
}
'''
with tempfile.TemporaryDirectory(prefix="gpu-bind-trace-") as directory:
    src = Path(directory) / "trace.c"
    exe = Path(directory) / "trace"
    src.write_text(source)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("optional AGC bind trace address correlation: PASS")
