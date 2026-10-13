/* SHA-256 over data that arrives in pieces (FIPS 180-4), for the update download.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_UPDATE_SHA256_H
#define TUBE_UPDATE_SHA256_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint32_t state[8];
    uint64_t length;            /* bytes taken so far */
    unsigned char block[64];
    size_t used;
} tube_update_sha256;

static inline uint32_t tube_update_sha256_rotr(uint32_t value, unsigned bits) { return value >> bits | value << (32 - bits); }

static inline void tube_update_sha256_block(tube_update_sha256 *sha, const unsigned char *p) {
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t w[64], v[8];
    for (int i = 0; i < 16; ++i)
        w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16 | (uint32_t)p[i * 4 + 2] << 8 | p[i * 4 + 3];
    for (int i = 16; i < 64; ++i) {
        const uint32_t a = tube_update_sha256_rotr(w[i - 15], 7) ^ tube_update_sha256_rotr(w[i - 15], 18) ^ w[i - 15] >> 3;
        const uint32_t b = tube_update_sha256_rotr(w[i - 2], 17) ^ tube_update_sha256_rotr(w[i - 2], 19) ^ w[i - 2] >> 10;
        w[i] = w[i - 16] + a + w[i - 7] + b;
    }
    memcpy(v, sha->state, sizeof v);
    for (int i = 0; i < 64; ++i) {
        const uint32_t s1 = tube_update_sha256_rotr(v[4], 6) ^ tube_update_sha256_rotr(v[4], 11) ^ tube_update_sha256_rotr(v[4], 25);
        const uint32_t t1 = v[7] + s1 + ((v[4] & v[5]) ^ (~v[4] & v[6])) + k[i] + w[i];
        const uint32_t s0 = tube_update_sha256_rotr(v[0], 2) ^ tube_update_sha256_rotr(v[0], 13) ^ tube_update_sha256_rotr(v[0], 22);
        const uint32_t t2 = s0 + ((v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]));
        v[7] = v[6]; v[6] = v[5]; v[5] = v[4]; v[4] = v[3] + t1;
        v[3] = v[2]; v[2] = v[1]; v[1] = v[0]; v[0] = t1 + t2;
    }
    for (int i = 0; i < 8; ++i) sha->state[i] += v[i];
}

static inline void tube_update_sha256_init(tube_update_sha256 *sha) {
    static const uint32_t start[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(sha->state, start, sizeof start);
    sha->length = 0; sha->used = 0;
}

static inline void tube_update_sha256_add(tube_update_sha256 *sha, const void *data, size_t size) {
    const unsigned char *p = (const unsigned char *)data;
    sha->length += size;
    while (size) {
        if (!sha->used && size >= 64) { tube_update_sha256_block(sha, p); p += 64; size -= 64; continue; }
        size_t take = 64 - sha->used;
        if (take > size) take = size;
        memcpy(sha->block + sha->used, p, take);
        sha->used += take; p += take; size -= take;
        if (sha->used == 64) { tube_update_sha256_block(sha, sha->block); sha->used = 0; }
    }
}

/* Ends the hash; `hex` receives 64 lower-case digits and a terminator. */
static inline void tube_update_sha256_finish(tube_update_sha256 *sha, char hex[65]) {
    static const char digits[] = "0123456789abcdef";
    const uint64_t bits = sha->length * 8;
    unsigned char tail[72] = {0x80};
    const size_t pad = (sha->used < 56 ? 56 : 120) - sha->used;
    for (int i = 0; i < 8; ++i) tail[pad + i] = (unsigned char)(bits >> (56 - 8 * i));
    tube_update_sha256_add(sha, tail, pad + 8);
    for (int i = 0; i < 32; ++i) {
        const unsigned char byte = (unsigned char)(sha->state[i / 4] >> (24 - 8 * (i % 4)));
        hex[i * 2] = digits[byte >> 4]; hex[i * 2 + 1] = digits[byte & 15];
    }
    hex[64] = 0;
}
#endif
