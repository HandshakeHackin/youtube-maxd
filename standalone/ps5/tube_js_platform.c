/* Clock, timers, secure random bytes and hashes for the embedded JavaScript
 * service. See tube_js_platform.h.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_js_platform.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

/* The console path is also built on the host by the tests, against a stand-in
 * for the kernel call, so the checks around it run under the sanitizers. */
#if defined(EVO_TARGET_PS5) || defined(__PROSPERO__) || defined(TUBE_JS_CONSOLE_RANDOM)
#define TUBE_JS_USE_KERNEL_RNG 1
int sysctlbyname(const char *, void *, size_t *, const void *, size_t);
#else
#include <errno.h>
#include <sys/random.h>
#endif

/* ---- Clocks ---- */

uint64_t tube_js_monotonic_ns(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) return 0;
    return (uint64_t)now.tv_sec * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
}

uint64_t tube_js_monotonic_ms(void) { return tube_js_monotonic_ns() / UINT64_C(1000000); }

int64_t tube_js_wall_ms(void) {
    struct timeval now;
    if (gettimeofday(&now, NULL)) return 0;
    return (int64_t)now.tv_sec * 1000 + (int64_t)now.tv_usec / 1000;
}

/* ---- Secure random bytes ---- */

int tube_js_random_bytes(void *output, size_t length) {
    if (!output) return length ? -1 : 0;
#ifdef TUBE_JS_USE_KERNEL_RNG
    /* Same checked use of the console's kernel generator as native/tube_random.h:
     * one request yields at most 64 bytes, and a short or oversized answer is a
     * failure, never a partly filled result. */
    unsigned char block[64], *destination = output;
    size_t remaining = length;
    while (remaining) {
        size_t received = sizeof block, wanted = remaining < sizeof block ? remaining : sizeof block;
        if (sysctlbyname("kern.rng_pseudo", block, &received, NULL, 0) || received < wanted ||
            received > sizeof block) {
            memset(block, 0, sizeof block);
            memset(output, 0, length);
            return -1;
        }
        memcpy(destination, block, wanted);
        destination += wanted;
        remaining -= wanted;
    }
    memset(block, 0, sizeof block);
    return 0;
#else
    unsigned char *destination = output;
    size_t done = 0;
    while (done < length) {
        ssize_t got = getrandom(destination + done, length - done, 0);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) {
            memset(output, 0, length);
            return -1;
        }
        done += (size_t)got;
    }
    return 0;
#endif
}

/* ---- SHA-1 (FIPS 180-4) ---- */

static uint32_t rotl32(uint32_t value, unsigned count) { return value << count | value >> (32 - count); }
static uint32_t rotr32(uint32_t value, unsigned count) { return value >> count | value << (32 - count); }
static uint32_t load_be32(const unsigned char *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | (uint32_t)p[3];
}
static void store_be32(unsigned char *p, uint32_t value) {
    p[0] = (unsigned char)(value >> 24); p[1] = (unsigned char)(value >> 16);
    p[2] = (unsigned char)(value >> 8); p[3] = (unsigned char)value;
}
static void store_be64(unsigned char *p, uint64_t value) {
    store_be32(p, (uint32_t)(value >> 32));
    store_be32(p + 4, (uint32_t)value);
}

static void sha1_block(uint32_t state[5], const unsigned char block[64]) {
    uint32_t w[80], a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
    for (int i = 0; i < 16; i++) w[i] = load_be32(block + 4 * i);
    for (int i = 16; i < 80; i++) w[i] = rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) { f = (b & c) | (~b & d); k = UINT32_C(0x5A827999); }
        else if (i < 40) { f = b ^ c ^ d; k = UINT32_C(0x6ED9EBA1); }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = UINT32_C(0x8F1BBCDC); }
        else { f = b ^ c ^ d; k = UINT32_C(0xCA62C1D6); }
        uint32_t next = rotl32(a, 5) + f + e + k + w[i];
        e = d; d = c; c = rotl32(b, 30); b = a; a = next;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

void tube_js_sha1_init(tube_js_sha1 *context) {
    static const uint32_t initial[5] = {UINT32_C(0x67452301), UINT32_C(0xEFCDAB89), UINT32_C(0x98BADCFE),
                                        UINT32_C(0x10325476), UINT32_C(0xC3D2E1F0)};
    memcpy(context->state, initial, sizeof initial);
    context->length = 0;
    context->used = 0;
}

/* The two hashes share their 64-byte block buffering. */
#define HASH_UPDATE(context, data, length, block_function)                                 \
    do {                                                                                   \
        const unsigned char *input = (const unsigned char *)(data);                        \
        size_t left = (length);                                                            \
        (context)->length += left;                                                         \
        if ((context)->used) {                                                             \
            size_t take = 64 - (context)->used;                                            \
            if (take > left) take = left;                                                  \
            memcpy((context)->block + (context)->used, input, take);                       \
            (context)->used += take; input += take; left -= take;                          \
            if ((context)->used == 64) {                                                   \
                block_function((context)->state, (context)->block);                        \
                (context)->used = 0;                                                       \
            }                                                                              \
        }                                                                                  \
        while (left >= 64) { block_function((context)->state, input); input += 64; left -= 64; } \
        if (left) { memcpy((context)->block, input, left); (context)->used = left; }       \
    } while (0)

#define HASH_PAD(context, block_function)                                                  \
    do {                                                                                   \
        uint64_t bits = (context)->length * 8;                                             \
        (context)->block[(context)->used++] = 0x80;                                        \
        if ((context)->used > 56) {                                                        \
            memset((context)->block + (context)->used, 0, 64 - (context)->used);           \
            block_function((context)->state, (context)->block);                            \
            (context)->used = 0;                                                           \
        }                                                                                  \
        memset((context)->block + (context)->used, 0, 56 - (context)->used);               \
        store_be64((context)->block + 56, bits);                                           \
        block_function((context)->state, (context)->block);                                \
    } while (0)

void tube_js_sha1_update(tube_js_sha1 *context, const void *data, size_t length) {
    if (!length) return;
    HASH_UPDATE(context, data, length, sha1_block);
}

void tube_js_sha1_final(tube_js_sha1 *context, unsigned char digest[20]) {
    HASH_PAD(context, sha1_block);
    for (int i = 0; i < 5; i++) store_be32(digest + 4 * i, context->state[i]);
    memset(context, 0, sizeof *context);
}

void tube_js_sha1_digest(const void *data, size_t length, unsigned char digest[20]) {
    tube_js_sha1 context;
    tube_js_sha1_init(&context);
    tube_js_sha1_update(&context, data, length);
    tube_js_sha1_final(&context, digest);
}

/* ---- SHA-256 (FIPS 180-4) ---- */

static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static void sha256_block(uint32_t state[8], const unsigned char block[64]) {
    uint32_t w[64], v[8];
    for (int i = 0; i < 16; i++) w[i] = load_be32(block + 4 * i);
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    memcpy(v, state, sizeof v);
    for (int i = 0; i < 64; i++) {
        uint32_t s1 = rotr32(v[4], 6) ^ rotr32(v[4], 11) ^ rotr32(v[4], 25);
        uint32_t choice = (v[4] & v[5]) ^ (~v[4] & v[6]);
        uint32_t t1 = v[7] + s1 + choice + sha256_k[i] + w[i];
        uint32_t s0 = rotr32(v[0], 2) ^ rotr32(v[0], 13) ^ rotr32(v[0], 22);
        uint32_t majority = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
        uint32_t t2 = s0 + majority;
        v[7] = v[6]; v[6] = v[5]; v[5] = v[4]; v[4] = v[3] + t1;
        v[3] = v[2]; v[2] = v[1]; v[1] = v[0]; v[0] = t1 + t2;
    }
    for (int i = 0; i < 8; i++) state[i] += v[i];
}

void tube_js_sha256_init(tube_js_sha256 *context) {
    static const uint32_t initial[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(context->state, initial, sizeof initial);
    context->length = 0;
    context->used = 0;
}

void tube_js_sha256_update(tube_js_sha256 *context, const void *data, size_t length) {
    if (!length) return;
    HASH_UPDATE(context, data, length, sha256_block);
}

void tube_js_sha256_final(tube_js_sha256 *context, unsigned char digest[32]) {
    HASH_PAD(context, sha256_block);
    for (int i = 0; i < 8; i++) store_be32(digest + 4 * i, context->state[i]);
    memset(context, 0, sizeof *context);
}

void tube_js_sha256_digest(const void *data, size_t length, unsigned char digest[32]) {
    tube_js_sha256 context;
    tube_js_sha256_init(&context);
    tube_js_sha256_update(&context, data, length);
    tube_js_sha256_final(&context, digest);
}

/* ---- Timers ---- */

enum { TIMER_LIMIT = 4096 };

typedef struct timer_entry {
    uint32_t id;
    uint64_t due_ms, repeat_ms, order;
} timer_entry;

struct tube_js_timers {
    timer_entry *entries;
    size_t count, capacity;
    uint32_t last_id;
    uint64_t order;
};

tube_js_timers *tube_js_timers_new(void) { return calloc(1, sizeof(tube_js_timers)); }

void tube_js_timers_free(tube_js_timers *timers) {
    if (!timers) return;
    free(timers->entries);
    free(timers);
}

static uint64_t add_saturating(uint64_t a, uint64_t b) { return a > UINT64_MAX - b ? UINT64_MAX : a + b; }

static int timer_index(const tube_js_timers *timers, uint32_t id) {
    for (size_t i = 0; i < timers->count; i++)
        if (timers->entries[i].id == id) return (int)i;
    return -1;
}

uint32_t tube_js_timers_add(tube_js_timers *timers, uint64_t now_ms, uint64_t delay_ms, uint64_t repeat_ms) {
    if (!timers || timers->count >= TIMER_LIMIT) return 0;
    if (timers->count == timers->capacity) {
        size_t capacity = timers->capacity ? timers->capacity * 2 : 16;
        timer_entry *grown = realloc(timers->entries, capacity * sizeof *grown);
        if (!grown) return 0;
        timers->entries = grown;
        timers->capacity = capacity;
    }
    do {   /* ids wrap after four billion timers; never 0, never one still armed */
        if (++timers->last_id == 0) timers->last_id = 1;
    } while (timer_index(timers, timers->last_id) >= 0);
    timer_entry *entry = &timers->entries[timers->count++];
    entry->id = timers->last_id;
    entry->due_ms = add_saturating(now_ms, delay_ms);
    entry->repeat_ms = repeat_ms;
    entry->order = timers->order++;
    return entry->id;
}

int tube_js_timers_cancel(tube_js_timers *timers, uint32_t id) {
    if (!timers || !id) return 0;
    int index = timer_index(timers, id);
    if (index < 0) return 0;
    timers->entries[index] = timers->entries[--timers->count];
    return 1;
}

static int timer_earliest(const tube_js_timers *timers) {
    int best = -1;
    for (size_t i = 0; i < timers->count; i++) {
        const timer_entry *entry = &timers->entries[i];
        if (best < 0 || entry->due_ms < timers->entries[best].due_ms ||
            (entry->due_ms == timers->entries[best].due_ms && entry->order < timers->entries[best].order))
            best = (int)i;
    }
    return best;
}

int tube_js_timers_next_delay(const tube_js_timers *timers, uint64_t now_ms) {
    if (!timers) return -1;
    int best = timer_earliest(timers);
    if (best < 0) return -1;
    uint64_t due = timers->entries[best].due_ms;
    if (due <= now_ms) return 0;
    return due - now_ms > (uint64_t)INT_MAX ? INT_MAX : (int)(due - now_ms);
}

uint32_t tube_js_timers_pop_due(tube_js_timers *timers, uint64_t now_ms) {
    if (!timers) return 0;
    int best = timer_earliest(timers);
    if (best < 0 || timers->entries[best].due_ms > now_ms) return 0;
    timer_entry *entry = &timers->entries[best];
    uint32_t id = entry->id;
    if (entry->repeat_ms) {
        /* From now, not from the missed deadline: a stalled loop does not get
         * a burst of catch-up calls. */
        entry->due_ms = add_saturating(now_ms, entry->repeat_ms);
        entry->order = timers->order++;
    } else {
        timers->entries[best] = timers->entries[--timers->count];
    }
    return id;
}

size_t tube_js_timers_count(const tube_js_timers *timers) { return timers ? timers->count : 0; }
