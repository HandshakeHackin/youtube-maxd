/* Host test of the console's random-bytes path: tube_js_platform.c built with
 * TUBE_JS_CONSOLE_RANDOM, against a stand-in for the kernel call that can be
 * told to misbehave. The real call is only ever made on a console.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_js_platform.c"

#include <stdio.h>

static int mode, calls;
static unsigned char next_byte;

int sysctlbyname(const char *name, void *old, size_t *old_length, const void *new_value, size_t new_length) {
    calls++;
    if (strcmp(name, "kern.rng_pseudo") || new_value || new_length || *old_length != 64) return -1;
    if (mode == 1) return -1;                                  /* the call fails */
    if (mode == 2 && calls == 2) { *old_length = 16; return 0; }   /* a short answer on the second block */
    if (mode == 3) { *old_length = 128; return 0; }            /* an answer larger than the buffer */
    unsigned char *bytes = old;
    for (size_t i = 0; i < 64; i++) bytes[i] = ++next_byte;
    return 0;
}

int main(void) {
    int failures = 0;
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); } } while (0)
    unsigned char output[150], zero[150] = {0};
    memset(output, 0xEE, sizeof output);
    CHECK(tube_js_random_bytes(output, 100) == 0 && calls == 2);
    int in_order = 1;
    for (int i = 0; i < 100; i++) in_order &= output[i] == (unsigned char)(i + 1);
    CHECK(in_order);                       /* 64 bytes of the first block, then 36 of the second */
    CHECK(output[100] == 0xEE);            /* and not one byte past what was asked for */
    calls = 0;
    CHECK(tube_js_random_bytes(output, 64) == 0 && calls == 1);
    calls = 0;
    CHECK(tube_js_random_bytes(output, 0) == 0 && calls == 0);
    for (mode = 1; mode <= 3; mode++) {
        calls = 0;
        memset(output, 0xEE, sizeof output);
        CHECK(tube_js_random_bytes(output, 100) == -1);
        CHECK(!memcmp(output, zero, 100));  /* a failure leaves zeros, never half an answer */
        CHECK(output[100] == 0xEE);
    }
    printf("test_random_console: %d failures\n", failures);
    return failures ? 1 : 0;
}
