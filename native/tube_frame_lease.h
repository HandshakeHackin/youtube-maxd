/* Fixed native caller-frame pools; references outlive publication and GPU submission.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_FRAME_LEASE_H
#define TUBE_FRAME_LEASE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define TUBE_FRAME_POOL_SLOTS 8
int tube_frame_pool_register(void *base,size_t slot_bytes,unsigned slots);
/* Index >=0 claimed, -1 all busy, -2 unknown pool. */
int tube_frame_pool_claim(void *base,unsigned first);
/* 1 held, 0 not native, -1 malformed/unowned native pointer or overflow. */
int tube_frame_lease_hold(const void *pointer);
void tube_frame_lease_release(const void *pointer);
unsigned tube_frame_pool_busy(const void *base);
/* Refuses to forget a pool with any outstanding owner. */
int tube_frame_pool_unregister(const void *base);
#ifdef __cplusplus
}
#endif
#endif
