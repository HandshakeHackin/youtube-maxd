#!/usr/bin/env python3
"""Check buffered diagnostic lifecycle and generated hooks without console I/O."""
from pathlib import Path
import csv
import os
import subprocess
import tempfile

from flip_timing_diag_overlay import DIAGNOSTIC_C, apply

work = Path(os.environ.get("PROSPEROTUBE_NATIVE_WORK", str(Path.home() / ".cache/prosperotube/native")))
player = "projects/evoplayer/"
names = ["media/include/evo_agc_runtime.h", "media/src/evo_agc_runtime.c", "core/src/Application.cpp"]
sources = {player + name: (work / player / name).read_text() for name in names}
runtime_path = player + names[1]
if "TUBE_FLIP_DIAG_CAPACITY" not in sources[runtime_path]:
    before = sources[runtime_path]
    def replace(path, old, new, count=1):
        assert sources[path].count(old) == count, (path, old, sources[path].count(old))
        sources[path] = sources[path].replace(old, new, count)
    apply(replace, player)
    for call in ("sceVideoOutGetFlipStatus(", "sceVideoOutWaitVblank(", "sceVideoOutSubmitFlip("):
        assert before.count(call) == sources[runtime_path].count(call), call
runtime = sources[runtime_path]
app = sources[player + names[2]]
assert "tube_flip_diag_active(playbackActive);" in app
assert app.index("tube_flip_diag_begin(f.tube_epoch") < app.index("tube_flip_diag_finish(")
assert runtime.index("tube_flip_diag_submit((uint64_t)") < runtime.index("int32_t fliprc = sceVideoOutSubmitFlip(")
assert "status[3] == (uint64_t)g_agc_dev.flip_arg) {\n                        tube_flip_diag_observed(status, fwaits);" in runtime
assert "tube_flip_diag_active(0);" in runtime[runtime.index("void evo_agc_runtime_shutdown(void)"):]
assert 'if os.environ.get("PROSPEROTUBE_FLIP_TIMING_DIAG") == "1":' in Path(__file__).with_name("prepare_native.py").read_text()

program = r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static unsigned logs, opens;
static int refuse_file;
static uint64_t tick = 100;
static int diag_clock_gettime(clockid_t clock_id, struct timespec *ts) {
    assert(clock_id == CLOCK_MONOTONIC);
    ts->tv_sec = 0; ts->tv_nsec = (long)(tick++ * 1000u); return 0;
}
static FILE *diag_fopen(const char *path, const char *mode) {
    ++opens;
    return refuse_file ? NULL : fopen(path, mode);
}
#define clock_gettime diag_clock_gettime
#define fopen diag_fopen
#define evo_boot_log(...) (++logs)
''' + DIAGNOSTIC_C.replace('"/data/prosperotube/flip_timing_diag.csv"', '"records.csv"') + r'''
int main(void) {
    uint64_t raw[8] = {11,22,33,44,55,66,77,88};
    tube_flip_diag_begin(1,1,0,0);
    tube_flip_diag_finish(1);
    assert(!tube_flip_diag_count && !logs && !opens);
    tube_flip_diag_active(1);
    tube_flip_diag_begin(1,0,0,0);
    assert(!tube_flip_diag_count);
    tube_flip_diag_begin(7,1,-500,90);
    tube_flip_diag_suspend(1); tube_flip_diag_suspend(0);
    tube_flip_diag_gpu(2,1); tube_flip_diag_submit(44,1);
    tube_flip_diag_submitted(0); tube_flip_diag_observed(raw,1);
    tube_flip_diag_waited(1); tube_flip_diag_finish(1);
    struct tube_flip_diag_record *r = &tube_flip_diag_records[0];
    assert(r->epoch==7 && r->serial==1 && r->pts==-500 && r->fetch_us==90);
    assert(r->begin_us < r->suspend_enter_us && r->suspend_enter_us < r->suspend_exit_us);
    assert(r->suspend_exit_us < r->gpu_done_us && r->gpu_done_us < r->submit_us);
    assert(r->submit_us < r->observed_us && r->observed_us < r->return_us);
    assert(r->gpu_waits==2 && r->gpu_retired==1 && r->flip_arg==44 && r->backbuffer==1);
    assert(r->flip_rc==0 && r->present_retired==1 && r->vblank_waits==1);
    assert(memcmp(r->status,raw,sizeof raw)==0);
    /* Duplicate presents cannot amend the first-picture record. */
    tube_flip_diag_submit(45,0); tube_flip_diag_observed(raw,2); tube_flip_diag_finish(1);
    assert(r->flip_arg==44 && r->vblank_waits==1 && tube_flip_diag_count==1);
    tube_flip_diag_active(1); /* pause/menu retains the session */
    assert(tube_flip_diag_count==1 && !logs && !opens);
    /* Seek epoch/serial reset stays explicit; GPU failure never fabricates a flip. */
    tube_flip_diag_begin(8,1,0,200); tube_flip_diag_gpu(500,0); tube_flip_diag_finish(0);
    r = &tube_flip_diag_records[1];
    assert(r->epoch==8 && r->serial==1 && !r->submit_us && !r->observed_us);
    assert(r->gpu_waits==500 && !r->gpu_retired && r->flip_rc==INT32_MIN && !r->present_retired);
    tube_flip_diag_begin(8,1,0,210); tube_flip_diag_submit(46,0);
    tube_flip_diag_submitted(-5); tube_flip_diag_finish(0);
    r = &tube_flip_diag_records[2];
    assert(r->flip_rc==-5 && r->submit_us && !r->observed_us && !r->present_retired);
    tube_flip_diag_begin(8,2,42000,220); tube_flip_diag_submit(47,0);
    tube_flip_diag_submitted(0); tube_flip_diag_waited(120); tube_flip_diag_finish(0);
    r = &tube_flip_diag_records[3];
    assert(r->vblank_waits==120 && !r->observed_us && !r->present_retired);
    for (unsigned i=4; i<TUBE_FLIP_DIAG_CAPACITY; ++i) {
        tube_flip_diag_begin(8,i,42000,230); tube_flip_diag_finish(1);
    }
    tube_flip_diag_begin(8,2000,42000,240); tube_flip_diag_finish(1);
    assert(tube_flip_diag_count==2048 && tube_flip_diag_overflow==1);
    assert(tube_flip_diag_records[0].epoch==7 && tube_flip_diag_records[1023].serial==1023);
    assert(!logs && !opens);
    tube_flip_diag_active(0);
    assert(logs==1 && opens==1 && !tube_flip_diag_current);
    assert(rename("records.csv","full.csv")==0);
    tube_flip_diag_active(0);
    assert(logs==1 && opens==1);
    /* Empty playback does not erase the previous useful dump. */
    tube_flip_diag_active(1); tube_flip_diag_active(0);
    assert(logs==1 && opens==1);
    tube_flip_diag_active(1); tube_flip_diag_begin(9,1,0,1); tube_flip_diag_finish(1);
    tube_flip_diag_active(0);
    assert(logs==2 && opens==2 && tube_flip_diag_count==1 && tube_flip_diag_session==3);
    tube_flip_diag_active(1); tube_flip_diag_begin(10,1,0,1); tube_flip_diag_finish(1);
    refuse_file=1; tube_flip_diag_active(0);
    assert(logs==3 && opens==3);
    puts("Flip diagnostic lifecycle, failures, capacity, deferred I/O: PASS");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="flip-timing-diag-") as directory:
    directory = Path(directory)
    source = directory / "test.c"
    executable = directory / "test"
    source.write_text(program)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsanitize=undefined",
                    str(source), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], cwd=directory, check=True)
    with (directory / "full.csv").open() as stream:
        records = list(csv.DictReader(stream))
    assert len(records) == 2048 and records[0]["pts_us"] == "-500"
    assert records[0]["status7"] == "88" and records[1]["flip_rc"] == str(-(1 << 31))
    assert records[3]["vblank_waits"] == "120" and records[-1]["serial"] == "2047"
    with (directory / "records.csv").open() as stream:
        records = list(csv.DictReader(stream))
    assert len(records) == 1 and records[0]["session"] == "3" and records[0]["epoch"] == "9"
print("Flip diagnostic hook placement and no extra VideoOut calls: PASS")

