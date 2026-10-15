#!/usr/bin/env python3
"""Check buffered diagnostic lifecycle and generated hooks without console I/O."""
from pathlib import Path
import csv
import os
import subprocess
import tempfile

from flip_timing_diag_overlay import DIAGNOSTIC_C, apply

isolated = os.environ.get("PROSPEROTUBE_DIAG_ISOLATED") == "1"
if not isolated:
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
    assert app.index("tube_flip_diag_begin();") < app.index("tube_flip_diag_finish(")
    assert "if (new_frame) tube_flip_diag_begin" not in app
    assert "have, new_frame, f.held, smoothing_active);" in app
    assert "tube_flip_diag_blit(tube_blit_result);" in app
    assert app.index("tube_flip_diag_pixels(f.y") < app.index("int tube_blit_result = evo_agc_blit_yuv")
    assert "evo_agc_runtime_cache_flush, &r->source_luma_hash);" in runtime
    assert "tube_flip_diag_source(0, 0, INT64_MIN, 0, 0, 0, 0, 0);" in app
    assert "tube_flip_diag_pacer(evo_agc_runtime_get_refresh_rate(), tube_pacer.active," in runtime
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
static unsigned logs, opens, invalidations;
static void evo_agc_runtime_cache_flush(const void *address, size_t bytes) {
    assert(address && bytes == 64 && ((uintptr_t)address % 64) == 0);
    ++invalidations;
}
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
static void begin_source(uint64_t epoch, uint64_t serial, int64_t pts, uint64_t fetch,
    int have, int fresh, int held, int smoothing, int blit)
{
    tube_flip_diag_source(epoch, serial, pts, fetch, have, fresh, held, smoothing);
    if (blit != INT32_MIN) tube_flip_diag_blit(blit);
    tube_flip_diag_begin();
}
int main(void) {
    uint64_t raw[8] = {11,22,33,44,55,66,77,88};
    static _Alignas(64) uint8_t luma[128*18], original[128*18];
    for (unsigned i=0; i<sizeof luma; ++i) luma[i]=(uint8_t)(i*37u+i/128u);
    memcpy(original,luma,sizeof luma);
    begin_source(1,1,0,0,1,1,0,0,0); tube_flip_diag_finish(1);
    assert(!tube_flip_diag_count && !logs && !opens);
    tube_flip_diag_active(1);
    /* A UI/no-source attempt is recorded explicitly; no stale video metadata. */
    begin_source(0,0,INT64_MIN,0,0,0,0,0,INT32_MIN);
    tube_flip_diag_submit(43,0); tube_flip_diag_finish(1);
    assert(tube_flip_diag_count==1 && !tube_flip_diag_records[0].have_source);
    assert(tube_flip_diag_records[0].blit_rc==INT32_MIN);
    tube_flip_diag_source(7,1,-500,90,1,1,0,0);
    tube_flip_diag_pixels(luma,128,128,18,0);
    assert(invalidations==144 && tube_flip_diag_source_record.sample_valid);
    uint64_t expected_hash=tube_flip_diag_source_record.source_luma_hash;
    assert(memcmp(luma,original,sizeof luma)==0);
    tube_flip_diag_blit(0); tube_flip_diag_begin();
    tube_flip_diag_suspend(1); tube_flip_diag_suspend(0);
    tube_flip_diag_gpu(2,1); tube_flip_diag_submit(44,1);
    tube_flip_diag_pacer(13,1,1,150.0,2,-1,-500);
    tube_flip_diag_submitted(0); tube_flip_diag_observed(raw,1);
    tube_flip_diag_waited(1); tube_flip_diag_finish(1);
    struct tube_flip_diag_record *r = &tube_flip_diag_records[1];
    assert(r->epoch==7 && r->serial==1 && r->pts==-500 && r->fetch_us==90);
    assert(r->sample_valid && r->source_luma_hash==expected_hash && r->sample_us==1);
    assert(r->source_y==(uint64_t)(uintptr_t)luma && r->source_pitch==128);
    assert(r->source_width==128 && r->source_height==18 && !r->source_ten_bit);
    assert(r->have_source && r->new_frame && !r->held && !r->blit_rc && !r->smoothing_active);
    assert(r->begin_us < r->present_enter_us && r->present_enter_us < r->suspend_enter_us);
    assert(r->suspend_enter_us < r->suspend_exit_us && r->suspend_exit_us < r->gpu_done_us);
    assert(r->gpu_done_us < r->submit_us && r->submit_us < r->observed_us && r->observed_us < r->return_us);
    assert(r->gpu_waits==2 && r->gpu_retired==1 && r->flip_arg==44 && r->backbuffer==1);
    assert(r->flip_rc==0 && r->present_retired==1 && r->vblank_waits==1);
    assert(r->refresh_mode==13 && r->pacer_active && r->pacer_pending && r->pacer_target_us==150);
    assert(r->pacer_misses==2 && r->pacer_drift_steps==-1 && memcmp(r->status,raw,sizeof raw)==0);
    /* Repeated source on the second buffer gets its own complete record. */
    tube_flip_diag_source(7,1,-500,110,1,0,0,0);
    tube_flip_diag_pixels(luma,128,128,18,0);
    assert(tube_flip_diag_source_record.sample_valid &&
        tube_flip_diag_source_record.source_luma_hash==expected_hash);
    tube_flip_diag_blit(0); tube_flip_diag_begin();
    tube_flip_diag_submit(45,0); tube_flip_diag_pacer(13,1,0,150.0,2,-1,-600);
    tube_flip_diag_observed(raw,2); tube_flip_diag_finish(1);
    assert(r->flip_arg==44 && r->vblank_waits==1 && tube_flip_diag_count==3);
    r=&tube_flip_diag_records[2];
    assert(r->epoch==7 && r->serial==1 && !r->new_frame && r->flip_arg==45 && r->backbuffer==0);
    assert(!r->pacer_pending && r->pacer_active && r->buffer_pts_us==-600);
    /* Held serial zero and interpolated repeats must not disappear either. */
    begin_source(7,0,-500,120,1,0,1,0,0); tube_flip_diag_finish(1);
    assert(tube_flip_diag_records[3].held && !tube_flip_diag_records[3].serial);
    begin_source(7,2,42000,130,1,1,0,1,0);
    tube_flip_diag_pacer(13,0,0,0,0,0,42000); tube_flip_diag_finish(1);
    assert(tube_flip_diag_records[4].smoothing_active && !tube_flip_diag_records[4].pacer_active);
    tube_flip_diag_active(1);
    assert(tube_flip_diag_count==5 && !logs && !opens);
    /* Failure attempts remain distinct from successful video draws/retirement. */
    begin_source(8,1,0,200,1,1,0,0,-7); tube_flip_diag_gpu(500,0); tube_flip_diag_finish(0);
    r=&tube_flip_diag_records[5];
    assert(r->epoch==8 && r->serial==1 && !r->submit_us && !r->observed_us && r->blit_rc==-7);
    assert(r->gpu_waits==500 && !r->gpu_retired && r->flip_rc==INT32_MIN && !r->present_retired);
    begin_source(8,1,0,210,1,1,0,0,0); tube_flip_diag_submit(46,0);
    tube_flip_diag_submitted(-5); tube_flip_diag_finish(0);
    assert(tube_flip_diag_records[6].flip_rc==-5 && !tube_flip_diag_records[6].observed_us);
    begin_source(8,2,42000,220,1,1,0,0,0); tube_flip_diag_submit(47,0);
    tube_flip_diag_submitted(0); tube_flip_diag_waited(120); tube_flip_diag_finish(0);
    assert(tube_flip_diag_records[7].vblank_waits==120 && !tube_flip_diag_records[7].present_retired);
    begin_source(8,2,42000,230,1,0,0,0,INT32_MIN); tube_flip_diag_finish(1);
    assert(tube_flip_diag_records[8].blit_rc==INT32_MIN && !tube_flip_diag_records[8].new_frame);
    assert(!tube_flip_diag_records[8].sample_valid && !tube_flip_diag_records[8].source_y);
    tube_flip_diag_source(8,2,42000,230,1,0,0,0);
    tube_flip_diag_pixels(luma,4,128,18,0);
    assert(!tube_flip_diag_source_record.sample_valid && invalidations==288);
    tube_flip_diag_pixels(luma,128,64,18,1);
    assert(tube_flip_diag_source_record.sample_valid && invalidations==432);
    assert(memcmp(luma,original,sizeof luma)==0);
    for (unsigned i=9; i<TUBE_FLIP_DIAG_CAPACITY; ++i) {
        begin_source(8,i,42000,240,1,1,0,0,0); tube_flip_diag_finish(1);
    }
    begin_source(8,9000,42000,250,1,1,0,0,0); tube_flip_diag_finish(1);
    assert(tube_flip_diag_count==8192 && tube_flip_diag_overflow==1);
    assert(tube_flip_diag_records[1].epoch==7 && tube_flip_diag_records[8191].serial==8191);
    unsigned before_invalidations=invalidations;
    tube_flip_diag_pixels(luma,128,128,18,0);
    assert(invalidations==before_invalidations); /* Ring full: no pointless sampling. */
    assert(!logs && !opens);
    tube_flip_diag_active(0);
    assert(logs==1 && opens==1 && !tube_flip_diag_current);
    assert(rename("records.csv","full.csv")==0);
    tube_flip_diag_active(0); assert(logs==1 && opens==1);
    tube_flip_diag_active(1); tube_flip_diag_active(0); assert(logs==1 && opens==1);
    tube_flip_diag_active(1); begin_source(9,1,0,1,1,1,0,0,0); tube_flip_diag_finish(1);
    tube_flip_diag_active(0);
    assert(logs==2 && opens==2 && tube_flip_diag_count==1 && tube_flip_diag_session==3);
    tube_flip_diag_active(1); begin_source(10,1,0,1,1,1,0,0,0); tube_flip_diag_finish(1);
    refuse_file=1; tube_flip_diag_active(0); assert(logs==3 && opens==3);
    puts("All-present diagnostic identity, repeats, failures, capacity and deferred I/O: PASS");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="flip-timing-diag-") as directory:
    directory = Path(directory)
    source = directory / "test.c"
    executable = directory / "test"
    source.write_text(program)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsanitize=undefined",
                    "-I", str(Path(__file__).resolve().parents[1] / "native"), str(source), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], cwd=directory, check=True)
    with (directory / "full.csv").open() as stream:
        records = list(csv.DictReader(stream))
    assert len(records) == 8192 and records[1]["pts_us"] == "-500"
    assert records[0]["have_source"] == "0" and records[0]["blit_rc"] == str(-(1 << 31))
    assert records[1]["new_frame"] == "1" and records[2]["new_frame"] == "0"
    assert records[1]["status7"] == "88" and records[1]["pacer_drift_steps"] == "-1"
    assert records[2]["flip_arg"] == "45" and records[2]["pacer_pending"] == "0"
    assert records[1]["sample_valid"] == "1" and records[1]["source_y"] != "0"
    assert records[2]["source_luma_hash"] == records[1]["source_luma_hash"]
    assert records[1]["source_pitch"] == "128" and records[1]["source_width"] == "128"
    assert records[1]["source_height"] == "18" and records[1]["sample_us"] == "1"
    assert records[8]["sample_valid"] == "0" and records[8]["source_y"] == "0"
    assert records[1]["buffer_pts_us"] == "-500" and records[2]["buffer_pts_us"] == "-600"
    assert records[3]["held"] == "1" and records[4]["smoothing_active"] == "1"
    assert records[5]["flip_rc"] == str(-(1 << 31)) and records[5]["blit_rc"] == "-7"
    assert records[7]["vblank_waits"] == "120" and records[-1]["serial"] == "8191"
    assert all(None not in row for row in records)
    with (directory / "records.csv").open() as stream:
        records = list(csv.DictReader(stream))
    assert len(records) == 1 and records[0]["session"] == "3" and records[0]["epoch"] == "9"
print("Flip diagnostic isolated buffer tests: PASS" if isolated else "Flip diagnostic hook placement and no extra VideoOut calls: PASS")
