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
typedef struct evo_vdec_native {
    int fatal, tube_drain_finished, ro_count, flushing;
    unsigned tube_drain_calls, au_ring;
    uint8_t *frame_mem;
    size_t frame_size;
    void *dec;
} evo_vdec_native;
static unsigned remaining, calls, fail_call;
static int tube_native_claim_frame(evo_vdec_native*n,unsigned first){int slot=tube_frame_pool_claim(n->frame_mem,first);assert(slot>=0);return slot;}
static void nwd_enter(const char *stage, unsigned deadline) {
    assert(!strcmp(stage, "Flush") && deadline == NWD_DECODE_MS);
}
static void nwd_leave(void) {}
static void ro_harvest(evo_vdec_native *n, const SceVideodec2OutputInfo *out) {
    assert(out->valid && out->picture_count && n->ro_count < RO_SLOTS);
    ++n->ro_count;
}
int sceVideodec2Flush(void *decoder, SceVideodec2FrameBuffer *frame, SceVideodec2OutputInfo *out) {
    assert(decoder && frame->size == sizeof(*frame) && out->size == sizeof(*out));
    ++calls;
    if (calls == fail_call) return -1;
    if (remaining) { --remaining; out->valid = out->picture_count = 1; }
    return 0;
}
#include "tube_native_drain_functions.inc"
static evo_vdec_native fresh(void) {
    static uint8_t memory[FRAME_POOL_SLOTS * 16];
    remaining = calls = fail_call = 0;
    assert(tube_frame_pool_unregister(memory));
    assert(tube_frame_pool_register(memory,16,FRAME_POOL_SLOTS));
    return (evo_vdec_native){.flushing = 1, .frame_mem = memory, .frame_size = 16, .dec = memory};
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
    puts("PASS: actual native flush body, reorder backpressure, terminal EOF, failure and bounded runaway protection");
}
