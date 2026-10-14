/* Actual generated decoder-output validation, without Sony service calls.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "sce/sce_videodec2.h"
#include "tube_native_drain_constants.inc"
struct dec_slot { uint32_t max_w, max_h; };
struct desc { int is_vp9; };
typedef struct {
    struct dec_slot *slot;
    struct desc *desc;
    uint8_t *frame_mem;
    size_t frame_size;
} evo_vdec_native;
#include "tube_native_surface.inc"
int main(void) {
    struct dec_slot slot = {3840, 2176};
    struct desc desc = {1};
    evo_vdec_native n = {&slot, &desc, (uint8_t *)(uintptr_t)0x10000000, 0xc00000};
    SceVideodec2OutputInfo out = {0};
    out.valid = out.picture_count = 1;
    out.buffer = n.frame_mem;
    out.width = out.pitch = out.pitch_bytes = 3840;
    out.height = 2160;
    assert(tube_native_surface_valid(&n, &out, 3840, 0));
    for (unsigned i = 0; i < FRAME_POOL_SLOTS; ++i) {
        out.buffer = (void *)((uintptr_t)n.frame_mem + i * n.frame_size);
        assert(tube_native_surface_valid(&n, &out, 3840, 0));
    }
    out.buffer = (void *)((uintptr_t)n.frame_mem + FRAME_POOL_SLOTS * n.frame_size);
    assert(!tube_native_surface_valid(&n, &out, 3840, 0));
    out.buffer = (void *)((uintptr_t)n.frame_mem - 0x4000);
    assert(!tube_native_surface_valid(&n, &out, 3840, 0));
    out.buffer = (void *)((uintptr_t)n.frame_mem + 256);
    assert(!tube_native_surface_valid(&n, &out, 3840, 0));
    out.buffer = n.frame_mem;
    out.error = 1; assert(!tube_native_surface_valid(&n, &out, 3840, 0)); out.error = 0;
    out.picture_count = 2; assert(!tube_native_surface_valid(&n, &out, 3840, 0));
    desc.is_vp9 = 0; assert(tube_native_surface_valid(&n, &out, 3840, 0));
    out.picture_count = 1;
    out.height = 2177; assert(!tube_native_surface_valid(&n, &out, 3840, 0));
    out.height = 2160;
    assert(!tube_native_surface_valid(&n, &out, 3839, 0));
    out.pitch_bytes = 7680;
    assert(!tube_native_surface_valid(&n, &out, 7680, 0));
    n.frame_size = 0x1800000;
    assert(tube_native_surface_valid(&n, &out, 7680, 1));
    n.frame_size = 3840u * 2160u;
    assert(!tube_native_surface_valid(&n, &out, 7680, 1));
    n.frame_size = SIZE_MAX;
    assert(!tube_native_surface_valid(&n, &out, 7680, 1));
    out.height = 0; assert(!tube_native_surface_valid(&n, &out, 7680, 1));
    puts("PASS: native surface pool membership, slot extent, profile pitch, dimensions, errors and interlaced compatibility");
}
