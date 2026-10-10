/* PS5 kernel RNG pattern adapted from ProsperoLight platform/ps5/ps5_entropy.c.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_RANDOM_H
#define TUBE_RANDOM_H
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#if defined(EVO_TARGET_PS5)
#ifdef __cplusplus
extern "C" {
#endif
int sysctlbyname(const char *, void *, size_t *, const void *, size_t);
#ifdef __cplusplus
}
#endif
#endif
static inline int tube_random_bytes(void *output, size_t length) {
    if (!output && length) return -1;
#if defined(EVO_TARGET_PS5)
    unsigned char block[64], *destination = output;
    size_t remaining = length;
    while (remaining) {
        size_t received = sizeof block, wanted = remaining < sizeof block ? remaining : sizeof block;
        if (sysctlbyname("kern.rng_pseudo", block, &received, NULL, 0) || received < wanted || received > sizeof block) {
            memset(block, 0, sizeof block); memset(output, 0, length); return -1;
        }
        memcpy(destination, block, wanted); destination += wanted; remaining -= wanted;
    }
    memset(block, 0, sizeof block);
#else
    if (length) arc4random_buf(output, length);
#endif
    return 0;
}
#endif
