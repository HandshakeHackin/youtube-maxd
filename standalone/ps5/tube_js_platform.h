/* Clock, timers, secure random bytes and hashes for the embedded JavaScript
 * service. Plain C, no engine dependency; the same sources build for the host
 * and for the PS5.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_JS_PLATFORM_H
#define TUBE_JS_PLATFORM_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* ---- Clocks ---- */
uint64_t tube_js_monotonic_ns(void);   /* never goes back; origin unspecified */
uint64_t tube_js_monotonic_ms(void);
int64_t tube_js_wall_ms(void);         /* milliseconds since the Unix epoch */

/* ---- Secure random bytes ----
 * 0 on success. On failure returns -1 and the output is all zero, so a caller
 * that forgets the check never uses a half-filled buffer as a secret. */
int tube_js_random_bytes(void *output, size_t length);

/* ---- Hashes (one-shot and streaming) ---- */
typedef struct tube_js_sha1 {
    uint32_t state[5];
    uint64_t length;
    unsigned char block[64];
    size_t used;
} tube_js_sha1;
void tube_js_sha1_init(tube_js_sha1 *context);
void tube_js_sha1_update(tube_js_sha1 *context, const void *data, size_t length);
void tube_js_sha1_final(tube_js_sha1 *context, unsigned char digest[20]);
void tube_js_sha1_digest(const void *data, size_t length, unsigned char digest[20]);

typedef struct tube_js_sha256 {
    uint32_t state[8];
    uint64_t length;
    unsigned char block[64];
    size_t used;
} tube_js_sha256;
void tube_js_sha256_init(tube_js_sha256 *context);
void tube_js_sha256_update(tube_js_sha256 *context, const void *data, size_t length);
void tube_js_sha256_final(tube_js_sha256 *context, unsigned char digest[32]);
void tube_js_sha256_digest(const void *data, size_t length, unsigned char digest[32]);

/* ---- Timers ----
 * A queue of deadlines for setTimeout/setInterval. It holds no callbacks: the
 * engine side keeps a table from timer id to function and asks this queue
 * which id is due. One thread only (the engine thread). */
typedef struct tube_js_timers tube_js_timers;
tube_js_timers *tube_js_timers_new(void);
void tube_js_timers_free(tube_js_timers *timers);
/* A new timer due `delay_ms` after `now_ms`; `repeat_ms` 0 = one shot,
 * otherwise it re-arms itself that long after each time it is popped.
 * Returns its id (never 0), or 0 when out of memory or at the timer limit. */
uint32_t tube_js_timers_add(tube_js_timers *timers, uint64_t now_ms, uint64_t delay_ms,
                            uint64_t repeat_ms);
int tube_js_timers_cancel(tube_js_timers *timers, uint32_t id);   /* 1 = was armed */
/* Milliseconds until the earliest timer: 0 = one is due, -1 = no timer. */
int tube_js_timers_next_delay(const tube_js_timers *timers, uint64_t now_ms);
/* The id of the earliest timer that is due (ties: the one armed first), or 0. */
uint32_t tube_js_timers_pop_due(tube_js_timers *timers, uint64_t now_ms);
size_t tube_js_timers_count(const tube_js_timers *timers);

#ifdef __cplusplus
}
#endif
#endif
