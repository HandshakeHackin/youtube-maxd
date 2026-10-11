/* Profile password records: a random salt and an iterated SHA-256 of salt and
 * text, never the text. Format "v1$<16 hex>$<64 hex>". Main thread only.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_PASSWORD_H
#define TUBE_PASSWORD_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define TUBE_PASSWORD_RECORD 84   /* 3 + 16 + 1 + 64 characters, without the terminator */
#define TUBE_PASSWORD_ROUNDS 4096
#define TUBE_PASSWORD_MAX 600

static inline void tube_sha256(const unsigned char *data, size_t size, unsigned char out[32]) {
    static const uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
        0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
        0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
        0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
    uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    const uint64_t bits = (uint64_t)size * 8;
    const size_t total = (size + 9 + 63) / 64 * 64;
    for (size_t offset = 0; offset < total; offset += 64) {
        unsigned char block[64]; uint32_t w[64], s[8];
        for (size_t i = 0; i < 64; ++i) {
            const size_t at = offset + i;
            block[i] = at < size ? data[at] : at == size ? 0x80 : at >= total - 8 ? (unsigned char)(bits >> (8 * (total - 1 - at))) : 0;
        }
        for (int i = 0; i < 16; ++i)
            w[i] = (uint32_t)block[i*4] << 24 | (uint32_t)block[i*4+1] << 16 | (uint32_t)block[i*4+2] << 8 | block[i*4+3];
#define TUBE_ROR(x,n) ((x) >> (n) | (x) << (32 - (n)))
        for (int i = 16; i < 64; ++i)
            w[i] = w[i-16] + (TUBE_ROR(w[i-15],7) ^ TUBE_ROR(w[i-15],18) ^ w[i-15] >> 3) + w[i-7] +
                (TUBE_ROR(w[i-2],17) ^ TUBE_ROR(w[i-2],19) ^ w[i-2] >> 10);
        memcpy(s, h, sizeof s);
        for (int i = 0; i < 64; ++i) {
            const uint32_t a = s[0], e = s[4];
            const uint32_t t1 = s[7] + (TUBE_ROR(e,6) ^ TUBE_ROR(e,11) ^ TUBE_ROR(e,25)) + ((e & s[5]) ^ (~e & s[6])) + k[i] + w[i];
            const uint32_t t2 = (TUBE_ROR(a,2) ^ TUBE_ROR(a,13) ^ TUBE_ROR(a,22)) + ((a & s[1]) ^ (a & s[2]) ^ (s[1] & s[2]));
            memmove(s + 1, s, 7 * sizeof *s); s[4] += t1; s[0] = t1 + t2;
        }
#undef TUBE_ROR
        for (int i = 0; i < 8; ++i) h[i] += s[i];
    }
    for (int i = 0; i < 8; ++i) {
        out[i*4] = (unsigned char)(h[i] >> 24); out[i*4+1] = (unsigned char)(h[i] >> 16);
        out[i*4+2] = (unsigned char)(h[i] >> 8); out[i*4+3] = (unsigned char)h[i];
    }
}

static inline int tube_password_hex(const char *text, size_t count) {
    for (size_t i = 0; i < count; ++i)
        if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f'))) return 0;
    return 1;
}
/* A stored record is accepted only in the exact format written here. */
static inline int tube_password_record_valid(const char *record) {
    return record && strlen(record) == TUBE_PASSWORD_RECORD && !strncmp(record, "v1$", 3) &&
        tube_password_hex(record + 3, 16) && record[19] == '$' && tube_password_hex(record + 20, 64);
}
/* 1 = record written (out holds TUBE_PASSWORD_RECORD + 1 bytes). */
static inline int tube_password_make(const unsigned char salt[8], const char *text, char *out) {
    static const char digits[] = "0123456789abcdef";
    unsigned char buffer[8 + TUBE_PASSWORD_MAX], round[40], hash[32];
    const size_t length = text ? strlen(text) : 0;
    if (!salt || !out || !length || length > TUBE_PASSWORD_MAX) return 0;
    memcpy(buffer, salt, 8); memcpy(buffer + 8, text, length);
    tube_sha256(buffer, 8 + length, hash);
    for (int i = 1; i < TUBE_PASSWORD_ROUNDS; ++i) {
        memcpy(round, salt, 8); memcpy(round + 8, hash, 32); tube_sha256(round, sizeof round, hash);
    }
    memcpy(out, "v1$", 3);
    for (int i = 0; i < 8; ++i) { out[3 + i*2] = digits[salt[i] >> 4]; out[4 + i*2] = digits[salt[i] & 15]; }
    out[19] = '$';
    for (int i = 0; i < 32; ++i) { out[20 + i*2] = digits[hash[i] >> 4]; out[21 + i*2] = digits[hash[i] & 15]; }
    out[TUBE_PASSWORD_RECORD] = 0;
    memset(buffer, 0, sizeof buffer); memset(round, 0, sizeof round); memset(hash, 0, sizeof hash);
    return 1;
}
/* 1 = the text matches the record; the comparison does not stop at a difference. */
static inline int tube_password_check(const char *record, const char *text) {
    unsigned char salt[8]; char made[TUBE_PASSWORD_RECORD + 1]; unsigned difference = 0;
    if (!tube_password_record_valid(record)) return 0;
    for (int i = 0; i < 8; ++i) {
        const char high = record[3 + i*2], low = record[4 + i*2];
        salt[i] = (unsigned char)((high <= '9' ? high - '0' : high - 'a' + 10) << 4 | (low <= '9' ? low - '0' : low - 'a' + 10));
    }
    if (!tube_password_make(salt, text, made)) return 0;
    for (int i = 0; i < TUBE_PASSWORD_RECORD; ++i) difference |= (unsigned)(made[i] ^ record[i]);
    memset(made, 0, sizeof made);
    return !difference;
}
#endif
