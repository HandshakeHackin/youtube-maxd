// QR code encoder for short links: byte mode, versions 1 to 10, all four
// error-correction levels, automatic version and mask selection.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Written for ProsperoTube from the QR Code symbology (ISO/IEC 18004); no
// third-party implementation is included. Header-only so the C provider and
// the C++ screens share one copy. Everything is bounded and allocation-free.
//
// The result is a module bitmap without a quiet zone: the caller draws at
// least four light modules around it (tube_qr_page.h does that for the UI).
#ifndef TUBE_QR_H
#define TUBE_QR_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define TUBE_QR_MIN_VERSION 1
#define TUBE_QR_MAX_VERSION 10
#define TUBE_QR_MAX_SIZE (17 + 4 * TUBE_QR_MAX_VERSION)
#define TUBE_QR_MAX_CODEWORDS 346
/* Largest payload any supported symbol holds (version 10, level L). */
#define TUBE_QR_MAX_PAYLOAD 271
#define TUBE_QR_MASK_AUTO (-1)

typedef enum {
    TUBE_QR_LEVEL_L = 0, /* recovers about 7% */
    TUBE_QR_LEVEL_M = 1, /* about 15%; the default for on-screen links */
    TUBE_QR_LEVEL_Q = 2, /* about 25% */
    TUBE_QR_LEVEL_H = 3  /* about 30% */
} tube_qr_level;

typedef struct {
    int version;       /* 1..TUBE_QR_MAX_VERSION, 0 when empty */
    int size;          /* modules per side: 17 + 4 * version */
    int mask;          /* 0..7 */
    tube_qr_level level;
    /* Row-major, stride `size`, 1 = dark. */
    uint8_t modules[TUBE_QR_MAX_SIZE * TUBE_QR_MAX_SIZE];
} tube_qr_code;

/* Dark (1) or light (0); anything outside the symbol is light. */
static inline int tube_qr_module(const tube_qr_code *code, int x, int y) {
    if (!code || x < 0 || y < 0 || x >= code->size || y >= code->size) return 0;
    return code->modules[y * code->size + x];
}

static inline int tube_qr_total_codewords(int version) {
    static const uint16_t total[TUBE_QR_MAX_VERSION] = {26, 44, 70, 100, 134, 172, 196, 242, 292, 346};
    return version < TUBE_QR_MIN_VERSION || version > TUBE_QR_MAX_VERSION ? 0 : total[version - 1];
}

static inline int tube_qr_ec_per_block(int version, tube_qr_level level) {
    static const uint8_t length[4][TUBE_QR_MAX_VERSION] = {
        {7, 10, 15, 20, 26, 18, 20, 24, 30, 18},
        {10, 16, 26, 18, 24, 16, 18, 22, 22, 26},
        {13, 22, 18, 26, 18, 24, 18, 22, 20, 24},
        {17, 28, 22, 16, 22, 28, 26, 26, 24, 28}};
    if (version < TUBE_QR_MIN_VERSION || version > TUBE_QR_MAX_VERSION || (int)level < 0 || (int)level > 3) return 0;
    return length[level][version - 1];
}

static inline int tube_qr_block_count(int version, tube_qr_level level) {
    static const uint8_t count[4][TUBE_QR_MAX_VERSION] = {
        {1, 1, 1, 1, 1, 2, 2, 2, 2, 4},
        {1, 1, 1, 2, 2, 4, 4, 4, 5, 5},
        {1, 1, 2, 2, 4, 4, 6, 6, 8, 8},
        {1, 1, 2, 4, 4, 4, 5, 6, 8, 8}};
    if (version < TUBE_QR_MIN_VERSION || version > TUBE_QR_MAX_VERSION || (int)level < 0 || (int)level > 3) return 0;
    return count[level][version - 1];
}

static inline int tube_qr_data_codewords(int version, tube_qr_level level) {
    int blocks = tube_qr_block_count(version, level);
    return blocks ? tube_qr_total_codewords(version) - tube_qr_ec_per_block(version, level) * blocks : 0;
}

/* Payload bytes a symbol holds in byte mode; 0 for an unsupported symbol. */
static inline size_t tube_qr_capacity(int version, tube_qr_level level) {
    int data = tube_qr_data_codewords(version, level);
    int header = 4 + (version < 10 ? 8 : 16);
    return data > 0 ? (size_t)((data * 8 - header) / 8) : 0;
}

/* Multiplication in GF(2^8) modulo x^8 + x^4 + x^3 + x^2 + 1. */
static inline uint8_t tube_qr_gf_mul(uint8_t a, uint8_t b) {
    unsigned product = 0, factor = a;
    while (b) {
        if (b & 1) product ^= factor;
        factor <<= 1;
        if (factor & 0x100) factor ^= 0x11D;
        b >>= 1;
    }
    return (uint8_t)product;
}

/* Reed-Solomon check bytes for one block; `count` is at most 30. */
static inline void tube_qr_rs_remainder(const uint8_t *data, int length, int count, uint8_t *out) {
    uint8_t generator[31];
    uint8_t root = 1;
    memset(generator, 0, sizeof generator);
    generator[0] = 1; /* coefficients from the highest power down */
    for (int degree = 0; degree < count; degree++) {
        generator[degree + 1] = tube_qr_gf_mul(generator[degree], root);
        for (int j = degree; j >= 1; j--) generator[j] ^= tube_qr_gf_mul(generator[j - 1], root);
        root = tube_qr_gf_mul(root, 2);
    }
    memset(out, 0, (size_t)count);
    for (int i = 0; i < length; i++) {
        uint8_t factor = (uint8_t)(data[i] ^ out[0]);
        memmove(out, out + 1, (size_t)count - 1);
        out[count - 1] = 0;
        for (int j = 0; j < count; j++) out[j] ^= tube_qr_gf_mul(generator[j + 1], factor);
    }
}

static inline void tube_qr_put(tube_qr_code *code, uint8_t *fixed, int x, int y, int dark) {
    if (x < 0 || y < 0 || x >= code->size || y >= code->size) return;
    code->modules[y * code->size + x] = (uint8_t)(dark != 0);
    fixed[y * code->size + x] = 1;
}

/* Level and mask, protected by a (15,5) BCH code, written in both places. */
static inline void tube_qr_put_format(tube_qr_code *code, uint8_t *fixed) {
    static const uint8_t level_bits[4] = {1, 0, 3, 2};
    unsigned data = ((unsigned)level_bits[code->level] << 3) | (unsigned)code->mask, rest = data << 10;
    int size = code->size;
    for (int bit = 14; bit >= 10; bit--) if (rest & (1u << bit)) rest ^= 0x537u << (bit - 10);
    unsigned word = ((data << 10) | rest) ^ 0x5412;
    for (int i = 0; i < 15; i++) {
        int dark = (int)((word >> i) & 1);
        if (i < 6) tube_qr_put(code, fixed, 8, i, dark);
        else if (i == 6) tube_qr_put(code, fixed, 8, 7, dark);
        else if (i == 7) tube_qr_put(code, fixed, 8, 8, dark);
        else if (i == 8) tube_qr_put(code, fixed, 7, 8, dark);
        else tube_qr_put(code, fixed, 14 - i, 8, dark);
        if (i < 8) tube_qr_put(code, fixed, size - 1 - i, 8, dark);
        else tube_qr_put(code, fixed, 8, size - 15 + i, dark);
    }
    tube_qr_put(code, fixed, 8, size - 8, 1);
}

/* Finder, timing and alignment patterns plus the format and version areas. */
static inline void tube_qr_put_function_patterns(tube_qr_code *code, uint8_t *fixed) {
    int size = code->size, version = code->version;
    for (int i = 8; i < size - 8; i++) {
        tube_qr_put(code, fixed, i, 6, i % 2 == 0);
        tube_qr_put(code, fixed, 6, i, i % 2 == 0);
    }
    for (int corner = 0; corner < 3; corner++) {
        int cx = corner == 1 ? size - 4 : 3, cy = corner == 2 ? size - 4 : 3;
        for (int dy = -4; dy <= 4; dy++) for (int dx = -4; dx <= 4; dx++) {
            int ring = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
            tube_qr_put(code, fixed, cx + dx, cy + dy, ring != 2 && ring != 4);
        }
    }
    if (version >= 2) {
        /* Up to version 6 one pattern near the bottom-right corner; versions
         * 7 to 10 add the centre line. Larger versions are not supported. */
        int centres[3], count = version >= 7 ? 3 : 2;
        centres[0] = 6; centres[count - 1] = size - 7;
        if (count == 3) centres[1] = (6 + size - 7) / 2;
        for (int a = 0; a < count; a++) for (int b = 0; b < count; b++) {
            if ((a == 0 && b == 0) || (a == 0 && b == count - 1) || (a == count - 1 && b == 0)) continue;
            for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) {
                int ring = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
                tube_qr_put(code, fixed, centres[a] + dx, centres[b] + dy, ring != 1);
            }
        }
    }
    tube_qr_put_format(code, fixed);
    if (version >= 7) {
        /* Version number under an (18,6) BCH code, in two 6x3 areas. */
        unsigned rest = (unsigned)version << 12;
        for (int bit = 17; bit >= 12; bit--) if (rest & (1u << bit)) rest ^= 0x1F25u << (bit - 12);
        unsigned word = ((unsigned)version << 12) | rest;
        for (int i = 0; i < 18; i++) {
            int dark = (int)((word >> i) & 1), a = size - 11 + i % 3, b = i / 3;
            tube_qr_put(code, fixed, a, b, dark);
            tube_qr_put(code, fixed, b, a, dark);
        }
    }
}

static inline int tube_qr_mask_bit(int mask, int x, int y) {
    switch (mask) {
    case 0: return (x + y) % 2 == 0;
    case 1: return y % 2 == 0;
    case 2: return x % 3 == 0;
    case 3: return (x + y) % 3 == 0;
    case 4: return (y / 2 + x / 3) % 2 == 0;
    case 5: return x * y % 2 + x * y % 3 == 0;
    case 6: return (x * y % 2 + x * y % 3) % 2 == 0;
    default: return ((x + y) % 2 + x * y % 3) % 2 == 0;
    }
}

static inline void tube_qr_apply_mask(tube_qr_code *code, const uint8_t *fixed, int mask) {
    for (int y = 0; y < code->size; y++) for (int x = 0; x < code->size; x++)
        if (!fixed[y * code->size + x] && tube_qr_mask_bit(mask, x, y)) code->modules[y * code->size + x] ^= 1;
}

/* Penalty for one line of modules: long runs and finder-like sequences. */
static inline long tube_qr_line_penalty(const uint8_t *line, int size) {
    static const uint8_t finder_like[2][11] = {{1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0}, {0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1}};
    long penalty = 0;
    for (int start = 0; start < size;) {
        int end = start + 1;
        while (end < size && line[end] == line[start]) end++;
        if (end - start >= 5) penalty += 3 + (end - start - 5);
        start = end;
    }
    for (int start = 0; start + 11 <= size; start++)
        for (int pattern = 0; pattern < 2; pattern++)
            if (!memcmp(line + start, finder_like[pattern], 11)) penalty += 40;
    return penalty;
}

/* The standard's four mask evaluation rules; lower is better. */
static inline long tube_qr_penalty(const tube_qr_code *code) {
    uint8_t column[TUBE_QR_MAX_SIZE];
    long penalty = 0, dark = 0, total;
    int size = code ? code->size : 0;
    if (size < 21 || size > TUBE_QR_MAX_SIZE) return -1;
    total = (long)size * size;
    for (int y = 0; y < size; y++) penalty += tube_qr_line_penalty(code->modules + y * size, size);
    for (int x = 0; x < size; x++) {
        for (int y = 0; y < size; y++) { column[y] = code->modules[y * size + x]; dark += column[y]; }
        penalty += tube_qr_line_penalty(column, size);
    }
    for (int y = 0; y + 1 < size; y++) for (int x = 0; x + 1 < size; x++) {
        const uint8_t *at = code->modules + y * size + x;
        if (at[0] == at[1] && at[0] == at[size] && at[0] == at[size + 1]) penalty += 3;
    }
    /* Ten points for every full 5% the dark share is away from one half. */
    long distance = dark * 20 - total * 10;
    if (distance < 0) distance = -distance;
    return penalty + 10 * (distance / total);
}

/* Encode `length` bytes. `min_version` raises the smallest symbol considered
 * and `mask` is 0..7 or TUBE_QR_MASK_AUTO. Returns 0 on success; on failure
 * (-1: bad argument or payload too long for version 10 at this level) the
 * result is cleared. */
static inline int tube_qr_encode_ex(const void *data, size_t length, tube_qr_level level, int min_version,
                                    int mask, tube_qr_code *out) {
    uint8_t fixed[TUBE_QR_MAX_SIZE * TUBE_QR_MAX_SIZE];
    uint8_t message[TUBE_QR_MAX_CODEWORDS], stream[TUBE_QR_MAX_CODEWORDS], check[30];
    const uint8_t *bytes = (const uint8_t *)data;
    int version, data_total, blocks, ec, short_length, short_blocks, used = 0, bit = 0, total_bits;
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if ((!bytes && length) || (int)level < 0 || (int)level > 3 || mask < TUBE_QR_MASK_AUTO || mask > 7 ||
        min_version < TUBE_QR_MIN_VERSION || min_version > TUBE_QR_MAX_VERSION)
        return -1;
    for (version = min_version; version <= TUBE_QR_MAX_VERSION; version++)
        if (length <= tube_qr_capacity(version, level)) break;
    if (version > TUBE_QR_MAX_VERSION) return -1;
    data_total = tube_qr_data_codewords(version, level);
    blocks = tube_qr_block_count(version, level);
    ec = tube_qr_ec_per_block(version, level);

    /* Mode 0100, the length, the bytes, a terminator, then alternating pads. */
    memset(message, 0, sizeof message);
    message[used++] = (uint8_t)(0x40 | (version < 10 ? length >> 4 : length >> 12));
    if (version >= 10) message[used++] = (uint8_t)(length >> 4);
    message[used] = (uint8_t)((length & 15) << 4);
    for (size_t i = 0; i < length; i++) {
        message[used++] |= (uint8_t)(bytes[i] >> 4);
        message[used] = (uint8_t)((bytes[i] & 15) << 4);
    }
    used++; /* the low nibble of this codeword is the terminator */
    for (int pad = 0; used < data_total; pad ^= 1) message[used++] = pad ? 0x11 : 0xEC;

    /* Blocks are interleaved codeword by codeword; shorter blocks come first. */
    short_length = data_total / blocks;
    short_blocks = blocks - data_total % blocks;
    used = 0;
    for (int i = 0; i <= short_length; i++)
        for (int block = 0; block < blocks; block++) {
            int offset = block * short_length + (block > short_blocks ? block - short_blocks : 0);
            if (i < short_length + (block >= short_blocks)) stream[used++] = message[offset + i];
        }
    for (int block = 0; block < blocks; block++) {
        int offset = block * short_length + (block > short_blocks ? block - short_blocks : 0);
        tube_qr_rs_remainder(message + offset, short_length + (block >= short_blocks), ec, check);
        for (int i = 0; i < ec; i++) stream[data_total + i * blocks + block] = check[i];
    }

    out->version = version;
    out->size = 17 + 4 * version;
    out->level = level;
    memset(fixed, 0, sizeof fixed);
    tube_qr_put_function_patterns(out, fixed);

    /* Two-module columns from the right edge, alternately up and down, with
     * the vertical timing column skipped. Left-over modules stay light. */
    total_bits = tube_qr_total_codewords(version) * 8;
    for (int right = out->size - 1, upward = 1; right >= 1; right -= 2, upward = !upward) {
        if (right == 6) right = 5;
        for (int step = 0; step < out->size; step++) {
            int y = upward ? out->size - 1 - step : step;
            for (int x = right; x >= right - 1; x--) {
                if (fixed[y * out->size + x] || bit >= total_bits) continue;
                out->modules[y * out->size + x] = (uint8_t)((stream[bit >> 3] >> (7 - (bit & 7))) & 1);
                bit++;
            }
        }
    }

    if (mask == TUBE_QR_MASK_AUTO) {
        long best = -1;
        for (int candidate = 0; candidate < 8; candidate++) {
            out->mask = candidate;
            tube_qr_put_format(out, fixed);
            tube_qr_apply_mask(out, fixed, candidate);
            long penalty = tube_qr_penalty(out);
            if (best < 0 || penalty < best) { best = penalty; mask = candidate; }
            tube_qr_apply_mask(out, fixed, candidate);
        }
    }
    out->mask = mask;
    tube_qr_put_format(out, fixed);
    tube_qr_apply_mask(out, fixed, mask);
    return 0;
}

static inline int tube_qr_encode(const void *data, size_t length, tube_qr_level level, tube_qr_code *out) {
    return tube_qr_encode_ex(data, length, level, TUBE_QR_MIN_VERSION, TUBE_QR_MASK_AUTO, out);
}

/* Text at level M, falling back to L when only that fits. */
static inline int tube_qr_encode_text(const char *text, tube_qr_code *out) {
    size_t length = text ? strlen(text) : 0;
    if (!text) { if (out) memset(out, 0, sizeof *out); return -1; }
    if (tube_qr_encode(text, length, TUBE_QR_LEVEL_M, out) == 0) return 0;
    return tube_qr_encode(text, length, TUBE_QR_LEVEL_L, out);
}

#endif
