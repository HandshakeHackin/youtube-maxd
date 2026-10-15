/* Opt-in sparse raw-luma identity; no allocation or pixel writes.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_LUMA_DIAG_H
#define TUBE_LUMA_DIAG_H
#include <stddef.h>
#include <stdint.h>

static inline int tube_luma_diag_sample(const void *y, int pitch_bytes,
    uint32_t width, uint32_t height, int ten_bit,
    void (*invalidate)(const void *, size_t), uint64_t *hash)
{
    if (!hash) return 0;
    *hash = 0;
    if (!y || !invalidate || pitch_bytes <= 0 || !width || !height) return 0;
    const uint64_t bpp = ten_bit ? 2u : 1u;
    const uint64_t row_bytes = (uint64_t)width * bpp;
    if (row_bytes < 64u || row_bytes > (uint64_t)pitch_bytes) return 0;
    const uintptr_t base = (uintptr_t)y;
    const uint64_t extent = (uint64_t)(height - 1u) * (unsigned)pitch_bytes + row_bytes;
    if (extent > UINTPTR_MAX - base) return 0;
    uint64_t result = UINT64_C(14695981039346656037);
    for (unsigned gy = 0; gy < 9; ++gy) {
        const uint64_t row_index = ((uint64_t)(2u * gy + 1u) * height) / 18u;
        const uintptr_t row = base + (uintptr_t)(row_index * (unsigned)pitch_bytes);
        const uintptr_t first = (row + 63u) & ~(uintptr_t)63u;
        const uintptr_t last = (row + (uintptr_t)row_bytes - 64u) & ~(uintptr_t)63u;
        if (first > last) return 0; /* No complete aligned line inside active pixels. */
        for (unsigned gx = 0; gx < 16; ++gx) {
            const uint64_t x = ((uint64_t)(2u * gx + 1u) * width) / 32u;
            uintptr_t line = (row + (uintptr_t)(x * bpp)) & ~(uintptr_t)63u;
            if (line < first) line = first;
            if (line > last) line = last;
            const uint8_t *pixels = (const uint8_t *)line;
            invalidate(pixels, 64u);
            for (unsigned i = 0; i < 64; ++i) {
                result ^= pixels[i];
                result *= UINT64_C(1099511628211);
            }
        }
    }
    *hash = result;
    return 1;
}
#endif
