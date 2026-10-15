/* The generated native drain function is compiled unchanged; Sony calls and
 * reorder harvesting are bounded test doubles, not hardware acceptance.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sce/sce_videodec2.h"
#include "tube_native_drain_constants.inc"
#include "tube_frame_lease.h"
static const struct codec_desc { int is_vp9; } avc={0}, vp9={1};
typedef struct evo_vdec_native {
    const struct codec_desc *desc;
    unsigned tube_vp9_head, tube_vp9_count;
    struct { int present; } tube_vp9_pending[PTS_POOL];
    int fatal, tube_drain_finished, ro_count, flushing;
    unsigned tube_drain_calls, au_ring;
    uint8_t *frame_mem;
    size_t frame_size;
    void *dec;
} evo_vdec_native;
static unsigned remaining, calls, fail_call, vp9_calls, hidden_call, association_errors;
static int output_rejected, empty_picture;
static void note(const char *message) {
    assert(!strcmp(message,"EVO vdec native: VP9 drain ended with pending visible frames"));
    ++association_errors;
}
static int tube_native_claim_frame(evo_vdec_native*n,unsigned first){int slot=tube_frame_pool_claim(n->frame_mem,first);assert(slot>=0);return slot;}
static void nwd_enter(const char *stage, unsigned deadline) {
    assert(!strcmp(stage, "Flush") && deadline == NWD_DECODE_MS);
}
static void nwd_leave(void) {}
static void ro_harvest(evo_vdec_native *n, const SceVideodec2OutputInfo *out) {
    assert(out->valid && out->picture_count && n->ro_count < RO_SLOTS);
    ++n->ro_count;
}
/* The real identity handler is covered by test_vp9_output_order.py; this verifies drain dispatch. */
static void tube_native_output(evo_vdec_native *n, const SceVideodec2OutputInfo *out) {
    assert(n->desc->is_vp9 && out->valid && n->tube_vp9_count);
    ++vp9_calls;
    if (output_rejected) { n->fatal=1; return; }
    assert(out->picture_count==1);
    const unsigned head=n->tube_vp9_head;
    assert(!!n->tube_vp9_pending[head].present == (vp9_calls!=hidden_call));
    n->tube_vp9_head=(head+1u)%PTS_POOL;
    --n->tube_vp9_count;
    if (vp9_calls!=hidden_call) ro_harvest(n,out);
}
int sceVideodec2Flush(void *decoder, SceVideodec2FrameBuffer *frame, SceVideodec2OutputInfo *out) {
    assert(decoder && frame->size == sizeof(*frame) && out->size == sizeof(*out));
    ++calls;
    if (calls == fail_call) return -1;
    if (remaining) { --remaining; out->valid=1; out->picture_count=!empty_picture; out->buffer=frame->buffer; }
    return 0;
}
#include "tube_native_drain_functions.inc"
static evo_vdec_native fresh(void) {
    static uint8_t memory[FRAME_POOL_SLOTS * 16];
    remaining = calls = fail_call = vp9_calls = hidden_call = association_errors = 0;
    output_rejected = empty_picture = 0;
    assert(tube_frame_pool_unregister(memory));
    assert(tube_frame_pool_register(memory,16,FRAME_POOL_SLOTS));
    return (evo_vdec_native){.desc = &avc, .flushing = 1, .frame_mem = memory, .frame_size = 16, .dec = memory};
}
static void pending(evo_vdec_native *n,unsigned count,unsigned shown) {
    assert(count<=PTS_POOL);
    n->tube_vp9_count=count;
    for(unsigned i=0;i<count;++i)
        n->tube_vp9_pending[(n->tube_vp9_head+i)%PTS_POOL].present=!!(shown&(1u<<i));
}
static void test_vp9_dispatch(void) {
    evo_vdec_native n=fresh(); n.desc=&vp9; pending(&n,2,2); remaining=2; hidden_call=1;
    drain_decoder(&n);
    assert(vp9_calls==2 && calls==3 && !n.tube_vp9_count && n.ro_count==1 && n.tube_drain_finished);
    assert(!n.fatal && !association_errors && !tube_frame_pool_busy(n.frame_mem));
    assert(!evo_vdec_native_is_drained(&n)); n.ro_count=0; assert(evo_vdec_native_is_drained(&n));
    n=fresh(); n.desc=&vp9; pending(&n,2,3); remaining=1;
    drain_decoder(&n);
    assert(vp9_calls==1 && calls==2 && n.tube_vp9_count==1 && association_errors==1);
    assert(n.fatal && !n.tube_drain_finished && !evo_vdec_native_is_drained(&n));
    assert(!tube_frame_pool_busy(n.frame_mem));
    /* A hidden completion may remain at native EOF; no visible picture is lost. */
    n=fresh(); n.desc=&vp9; n.tube_vp9_head=PTS_POOL-1;
    pending(&n,2,1); remaining=1;
    drain_decoder(&n);
    assert(vp9_calls==1 && calls==2 && !n.tube_vp9_count && !n.tube_vp9_head);
    assert(!n.fatal && n.tube_drain_finished && n.ro_count==1 && !association_errors);
    assert(!tube_frame_pool_busy(n.frame_mem));
    n.ro_count=0; assert(evo_vdec_native_is_drained(&n));
    for (int malformed=0;malformed<2;++malformed) {
        n=fresh(); n.desc=&vp9; pending(&n,3,7); remaining=3;
        output_rejected=1; empty_picture=malformed;
        drain_decoder(&n);
        assert(vp9_calls==1 && calls==1 && remaining==2 && n.fatal && !n.tube_drain_finished);
        assert(!association_errors && !tube_frame_pool_busy(n.frame_mem));
        drain_decoder(&n); assert(calls==1); /* rejected output never reaches another flush */
    }
}
int main(void) {
    evo_vdec_native n = fresh();
    assert(!evo_vdec_native_is_drained(&n));
    drain_decoder(&n);
    assert(calls == 1 && evo_vdec_native_is_drained(&n));
    drain_decoder(&n); assert(calls == 1); // Terminal flush is not replayed.
    n = fresh(); remaining = RO_SLOTS * 3;
    drain_decoder(&n);
    assert(n.ro_count == RO_SLOTS - 1 && remaining && !n.tube_drain_finished);
    unsigned before = calls;
    drain_decoder(&n); assert(calls == before); // Full reorder window does not lose frames.
    while (!n.tube_drain_finished) {
        n.ro_count = 0; // receive returned all harvested pictures.
        drain_decoder(&n); assert(!n.fatal);
    }
    assert(!remaining && calls == RO_SLOTS * 3 + 1);
    assert(!evo_vdec_native_is_drained(&n)); // Harvested tail still awaits receive.
    n.ro_count = 0; assert(evo_vdec_native_is_drained(&n));
    n = fresh(); fail_call = 1;
    drain_decoder(&n); assert(n.fatal && !n.tube_drain_finished && !evo_vdec_native_is_drained(&n));
    n = fresh(); remaining = 1000;
    while (!n.fatal) { n.ro_count = 0; drain_decoder(&n); }
    assert(calls == 256 && !n.tube_drain_finished && !evo_vdec_native_is_drained(&n));
    assert(!vp9_calls && !association_errors && !tube_frame_pool_busy(n.frame_mem));
    test_vp9_dispatch();
    puts("PASS: actual native flush body, VP9 dispatch/error propagation, reorder backpressure, terminal EOF, failure and bounded runaway protection");
}
