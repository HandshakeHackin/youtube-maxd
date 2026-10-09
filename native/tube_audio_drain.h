/* ProsperoTube - audio EOF observation, invalidated by open/seek/track changes.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef TUBE_AUDIO_DRAIN_H
#define TUBE_AUDIO_DRAIN_H
#include <stdatomic.h>
#include <stdint.h>

enum { TUBE_AUDIO_DECODED = 1, TUBE_AUDIO_OUTPUT = 2, TUBE_AUDIO_FAILED = 4 };
typedef struct { _Atomic uint64_t value; } tube_audio_drain;

static inline uint64_t tube_audio_epoch(const tube_audio_drain *state)
{
    return atomic_load_explicit(&state->value, memory_order_acquire) >> 3;
}

static inline void tube_audio_reset(tube_audio_drain *state)
{
    uint64_t old = atomic_load_explicit(&state->value, memory_order_relaxed);
    while (!atomic_compare_exchange_weak_explicit(&state->value, &old,
            ((old >> 3) + 1) << 3, memory_order_acq_rel, memory_order_relaxed)) {}
}

static inline void tube_audio_mark(tube_audio_drain *state, uint64_t epoch, unsigned flag)
{
    uint64_t old = atomic_load_explicit(&state->value, memory_order_acquire);
    while ((old >> 3) == epoch) {
        if (atomic_compare_exchange_weak_explicit(&state->value, &old, old | flag,
                memory_order_acq_rel, memory_order_acquire)) return;
    }
}

static inline unsigned tube_audio_flags(const tube_audio_drain *state, uint64_t epoch)
{
    uint64_t value = atomic_load_explicit(&state->value, memory_order_acquire);
    return (value >> 3) == epoch ? (unsigned)(value & 7) : 0;
}

static inline int tube_audio_drained(const tube_audio_drain *state)
{
    return (atomic_load_explicit(&state->value, memory_order_acquire) & 7) ==
           (TUBE_AUDIO_DECODED | TUBE_AUDIO_OUTPUT);
}
#endif
