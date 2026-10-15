#!/usr/bin/env python3
"""Compile real generated decode/drain code with a delayed coded-picture oracle."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

work = Path(os.environ.get('PROSPEROTUBE_NATIVE_WORK', str(Path.home() / '.cache/prosperotube/native')))
source_path = Path(sys.argv[1]) if len(sys.argv) > 1 else work / 'projects/evoplayer/media/src/evo_vdec_native.c'
source = source_path.read_text()


def function(signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


patched = 'static void tube_native_output(' in source
reset = function('void evo_vdec_native_flush(')
if patched:
    assert 'v->tube_vp9_head = v->tube_vp9_count = 0;' in reset
    assert reset.index('v->tube_vp9_head =') < reset.index('slot_renew_decoder(')
reset_start = reset.index('    v->pts_n')
reset_end = reset.index('    v->au_ring', reset_start)

program = r'''
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define PTS_POOL 16
#define INPUT_SLOT_BYTES 64
#define PIPELINE_BUFFER_COUNT 8
#define FRAME_POOL_SLOTS 8
#define RO_SLOTS 16
#define SCE_VIDEODEC2_CODEC_HEVC 2
#define HEVC_NAL_IRAP_HI 23
#define NAT_HEVC10 4
#define NAT_VP92 5
#define NWD_DECODE_MS 100
#define NWD_RESET_MS 100
#define POST_FLUSH_ERR_TOLERANCE 5
typedef struct { size_t size; void *au; uint64_t au_size, pts, dts; int attached; } SceVideodec2InputData;
typedef struct { size_t size, buffer_size; void *buffer; unsigned accepted; } SceVideodec2FrameBuffer;
typedef struct { size_t size; void *buffer; unsigned valid, picture_count, error, width, height, pitch, pitch_bytes; } SceVideodec2OutputInfo;
typedef struct { int is_vp9, codec_type, idx; } nat_codec_desc;
typedef struct { struct { unsigned profile; } cfg; } dec_slot;
typedef struct evo_vdec_native {
    const nat_codec_desc *desc;
    dec_slot *slot;
    unsigned au_ring, dec_calls, dec_fails, first_valid_logged, first_err_logged;
    unsigned first_pic_err_logged, since_flush_out, post_flush_errs, frames_out;
    unsigned disp_w, disp_h, tube_drain_calls;
    int fatal, need_irap, dv_strip_logged, tube_drain_finished, ro_count;
    void *dovi, *dec;
    uint8_t *input_mem, *frame_mem;
    size_t frame_size;
    int64_t pts_pool[PTS_POOL]; int pts_n;
    struct { int64_t pts; int present; } tube_vp9_pending[PTS_POOL];
    unsigned tube_vp9_head, tube_vp9_count;
    int tube_vp9_bsf_pending;
} evo_vdec_native;
static int hardware[256], queued, latency = 4, fail_decode, fail_flush, no_output;
static int bad_surface, harvest_fail, unaccepted, empty_picture;
static int visible[256], count, releases, decode_calls;
static int64_t visible_pts[256];
static void note(const char *format, ...) { (void)format; }
#define nwd_start_once() ((void)0)
#define nwd_enter(a,b) ((void)0)
#define nwd_test_hang(a) ((void)0)
#define nwd_leave() ((void)0)
#define drop_undecodable_leading(n,a,s) 0
#define hevc_copy_base_layer(dst,src,size,stripped,dups) (memcpy(dst,src,size), (size))
#define hevc_au_nal_type(a,s) 19
#define tube_native_claim_frame(n,i) ((int)((i) % FRAME_POOL_SLOTS))
#define dovi_note_au(n,a,s,p) ((void)(s))
static void tube_frame_lease_release(const void *p) { (void)p; releases++; }
static void output(SceVideodec2OutputInfo *out) {
    out->valid = out->picture_count = 1;
    if (empty_picture) out->picture_count = 0;
    out->buffer = (void *)(uintptr_t)hardware[0];
    memmove(hardware, hardware + 1, (size_t)--queued * sizeof hardware[0]);
}
static int sceVideodec2Decode(void *dec, SceVideodec2InputData *in, SceVideodec2FrameBuffer *fb, SceVideodec2OutputInfo *out) {
    (void)dec; decode_calls++;
    if (fail_decode) return -1;
    hardware[queued++] = *(uint8_t *)in->au;
    if (!no_output && queued >= latency) { output(out); fb->accepted = !unaccepted; }
    return 0;
}
static int sceVideodec2Flush(void *dec, SceVideodec2FrameBuffer *fb, SceVideodec2OutputInfo *out) {
    (void)dec; (void)fb;
    if (fail_flush) return -1;
    if (queued && !no_output) output(out);
    return 0;
}
''' + function('static void pts_push(') + '\n' + function('static int64_t pts_take(') + r'''
static int tube_native_surface_valid(const evo_vdec_native *n, const SceVideodec2OutputInfo *out, uint32_t pitch, int ten_bit) {
    (void)n; (void)pitch; (void)ten_bit;
    return !bad_surface && out->valid && out->picture_count == 1 && !out->error && out->buffer;
}
static void ro_harvest(evo_vdec_native *n, const SceVideodec2OutputInfo *out) {
    if (harvest_fail) return;
    visible[count] = (int)(uintptr_t)out->buffer;
    visible_pts[count++] = pts_take(n);
    n->ro_count++;
}
'''
if patched:
    program += function('static void tube_native_output(') + '\n'
program += function('static int decode_one(') + '\n' + function('static void drain_decoder(')
program += '\n' + function('static int bitrd(') + '\n' + function('static int vp9_frame_is_shown(')
program += '\nstatic void seek_metadata(evo_vdec_native *v) {\n' + reset[reset_start:reset_end] + '\n}\n'
program += r'''
static nat_codec_desc vp9 = {1, 0, 0}, avc = {0, 0, 0};
static dec_slot slot;
static uint8_t inputs[PIPELINE_BUFFER_COUNT * INPUT_SLOT_BYTES], frames[128];
static evo_vdec_native fresh(const nat_codec_desc *desc) {
    evo_vdec_native n = {0};
    n.desc = desc; n.slot = &slot; n.input_mem = inputs; n.frame_mem = frames; n.frame_size = 16;
    queued = count = releases = decode_calls = fail_decode = fail_flush = no_output = 0;
    bad_surface = harvest_fail = unaccepted = empty_picture = 0;
    latency = 4;
    return n;
}
static void submit(evo_vdec_native *n, int id, int shown, int64_t pts) {
    uint8_t bytes = (uint8_t)id;
    assert(decode_one(n, &bytes, 1, pts, shown) == 0);
    n->ro_count = 0;
}
static int shown(int id) { return id != 12 && id != 24; }
int main(void) {
    const uint8_t hidden = 0x84, shown_inter = 0x86, show_existing = 0x88;
    assert(!vp9_frame_is_shown(&hidden, 1));
    assert(vp9_frame_is_shown(&shown_inter, 1) && vp9_frame_is_shown(&show_existing, 1));
    evo_vdec_native n = fresh(&vp9);
    /* Includes superframe-like hidden+shown pairs and materialized show-existing output. */
    for (int id = 1; id <= 30; id++) {
        submit(&n, id, shown(id), shown(id) ? id * 1000 : INT64_MIN);
        if (id <= 3) assert(count == 0);
    }
    drain_decoder(&n);
    assert(!n.fatal && n.tube_drain_finished && count == 28);
    int index = 0;
    for (int id = 1; id <= 30; id++) if (shown(id)) {
        if (visible[index] != id)
            fprintf(stderr, "wrong picture: expected %d / PTS %d, got %d / PTS %lld\n",
                    id, id * 1000, visible[index], (long long)visible_pts[index]);
        assert(visible[index] == id);
        assert(visible_pts[index++] == id * 1000);
    }
    assert(n.tube_vp9_count == 0 && n.pts_n == 0);
    /* Tail drain must suppress hidden AUs too, even when they are the final output. */
    n = fresh(&vp9);
    submit(&n, 1, 1, INT64_MIN); submit(&n, 2, 0, 0); submit(&n, 3, 1, 7); submit(&n, 4, 0, 0);
    drain_decoder(&n);
    assert(!n.fatal && count == 2 && visible[0] == 1 && visible[1] == 3);
    assert(visible_pts[0] == INT64_MIN && visible_pts[1] == 7);
    /* Equal PTS must preserve coded-picture identity; show-existing is a shown AU. */
    n = fresh(&vp9); latency = 1;
    submit(&n, 1, 1, 42); submit(&n, 2, 0, 42); submit(&n, 3, 1, 42);
    assert(count == 2 && visible[0] == 1 && visible[1] == 3);
    /* A seek discards pending metadata before a freshly reset decoder starts. */
    n = fresh(&vp9); submit(&n, 1, 0, 1); submit(&n, 2, 1, 2);
    seek_metadata(&n); queued = 0;
    submit(&n, 7, 1, 7); drain_decoder(&n);
    assert(count == 1 && visible[0] == 7 && visible_pts[0] == 7);
    /* Initial refusal can be retried; refusal with in-flight frames cannot be associated safely. */
    n = fresh(&vp9); fail_decode = 1;
    uint8_t byte = 1;
    assert(decode_one(&n, &byte, 1, 1, 1) == 0 && !n.fatal && !n.tube_vp9_count);
    fail_decode = 0; submit(&n, 2, 1, 2); fail_decode = 1;
    assert(decode_one(&n, &byte, 1, 1, 1) == -1 && n.fatal && !count);
    /* Queue bound is enforced before submitting an untrackable AU. */
    n = fresh(&vp9); no_output = 1;
    for (int i = 1; i <= PTS_POOL; i++) submit(&n, i, 1, i);
    assert(decode_one(&n, &byte, 1, 1, 1) == -1 && n.fatal && decode_calls == PTS_POOL);
    n = fresh(&vp9); submit(&n, 1, 1, 1); no_output = 1; drain_decoder(&n);
    assert(n.fatal && !n.tube_drain_finished);
    n = fresh(&vp9); submit(&n, 1, 1, 1); fail_flush = 1; drain_decoder(&n);
    assert(n.fatal && releases == 2);
    n = fresh(&vp9); latency = 1; bad_surface = 1;
    assert(decode_one(&n, &byte, 1, 1, 0) == -1 && n.fatal && !count);
    n = fresh(&vp9); latency = 1; harvest_fail = 1;
    assert(decode_one(&n, &byte, 1, 1, 1) == -1 && n.fatal && !count);
    n = fresh(&vp9); latency = 1; unaccepted = 1;
    assert(decode_one(&n, &byte, 1, 1, 1) == -1 && n.fatal && !count);
    n = fresh(&vp9); latency = 1; empty_picture = 1;
    assert(decode_one(&n, &byte, 1, 1, 1) == -1 && n.fatal && !count);
    n = fresh(&vp9); submit(&n, 1, 1, 1); empty_picture = 1; drain_decoder(&n);
    assert(n.fatal && !count && !n.tube_drain_finished);
    /* AVC uses the existing sorted PTS pool and unchanged harvest path. */
    n = fresh(&avc); latency = 1;
    submit(&n, 1, 1, 100); submit(&n, 2, 1, 200); drain_decoder(&n);
    assert(count == 2 && visible_pts[0] == 100 && visible_pts[1] == 200 && n.frames_out == 2);
    puts("VP9 delayed coded-picture identity, drain, seek, errors, queue bounds: PASS");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='vp9-output-order-') as directory:
    path = Path(directory) / 'test.c'
    exe = Path(directory) / 'test'
    path.write_text(program)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-fsanitize=undefined', str(path), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
