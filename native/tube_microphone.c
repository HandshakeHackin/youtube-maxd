/* Uses the runtime-proven PS5 AudioIn ABI from ps5-audio-decoding-research.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_microphone.h"
#include <pthread.h>
#include <string.h>
#include <time.h>

extern int32_t sceUserServiceGetInitialUser(int32_t *user);
extern int32_t sceAudioInOpen(int32_t user, int32_t type, int32_t index,
                            uint32_t grain, uint32_t frequency, uint32_t format);
extern int32_t sceAudioInInput(int32_t handle, void *destination);
extern int32_t sceAudioInClose(int32_t handle);

enum { RATE = 16000, GRAIN = 256, MAX_SAMPLES = RATE * 10, MIN_SAMPLES = RATE / 2 };
static int16_t recording[MAX_SAMPLES];
static pthread_t worker;
static int joined = 1, active, result, done, cancelled, finish;
static size_t captured, limit;

static void wipe(void) {
    volatile unsigned char *bytes = (volatile unsigned char *)recording;
    for (size_t i = 0; i < sizeof recording; ++i) bytes[i] = 0;
    captured = 0;
}
static int64_t now_ms(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) return -1;
    return (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
static void *capture(void *unused) {
    (void)unused;
    int32_t user = -1, handle = -1;
    int peak = 0, failed = 0;
    int64_t started = now_ms();
    result = -1;
    if (started < 0 || __atomic_load_n(&cancelled, __ATOMIC_ACQUIRE) ||
        sceUserServiceGetInitialUser(&user) < 0 || user < 0) goto completed;
    /* VoiceRecognition=5, signed-16 mono=0, controller routing index=0. */
    handle = sceAudioInOpen(user, 5, 0, GRAIN, RATE, 0);
    if (handle < 0) goto completed;
    while (captured + GRAIN <= limit &&
           !__atomic_load_n(&cancelled, __ATOMIC_ACQUIRE) &&
           !__atomic_load_n(&finish, __ATOMIC_ACQUIRE)) {
        int64_t current = now_ms();
        if (current < started || current - started > 12000) { failed = 1; break; }
        int16_t block[GRAIN] = {0};
        int input = sceAudioInInput(handle, block);
        if (input < 0) failed = 1;
        if (!failed && !__atomic_load_n(&cancelled, __ATOMIC_ACQUIRE)) {
            memcpy(recording + captured, block, sizeof block);
            captured += GRAIN;
            for (size_t i = 0; i < GRAIN; ++i) {
                int value = block[i]; if (value < 0) value = -value;
                if (value > peak) peak = value;
            }
        }
        /* Stack PCM is not retained after each input call. */
        volatile int16_t *clear = block;
        for (size_t i = 0; i < GRAIN; ++i) clear[i] = 0;
        if (failed) break;
    }
    if (sceAudioInClose(handle) < 0) failed = 1;
    if (!failed && !__atomic_load_n(&cancelled, __ATOMIC_ACQUIRE) &&
        captured >= MIN_SAMPLES && peak >= 64) result = 1;
completed:
    if (result != 1) wipe();
    __atomic_store_n(&done, 1, __ATOMIC_RELEASE);
    return NULL;
}
int tube_microphone_begin(unsigned milliseconds) {
    if (active || milliseconds < 500 || milliseconds > 10000) return 0;
    wipe(); result = -1;
    limit = ((size_t)milliseconds * RATE / 1000 + GRAIN - 1) / GRAIN * GRAIN;
    if (limit > MAX_SAMPLES) limit = MAX_SAMPLES / GRAIN * GRAIN;
    __atomic_store_n(&cancelled, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&finish, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&done, 0, __ATOMIC_RELEASE);
    if (pthread_create(&worker, NULL, capture, NULL) != 0) return 0;
    joined = 0; active = 1; return 1;
}
void tube_microphone_finish(void) { __atomic_store_n(&finish, 1, __ATOMIC_RELEASE); }
void tube_microphone_cancel(void) { __atomic_store_n(&cancelled, 1, __ATOMIC_RELEASE); }
static int join_finished(void) {
    if (!joined) {
        if (pthread_join(worker, NULL) != 0) return 0;
        joined = 1;
    }
    return 1;
}
int tube_microphone_poll(const int16_t **pcm, size_t *samples) {
    if (pcm) *pcm = NULL;
    if (samples) *samples = 0;
    if (!active) return -1;
    if (!__atomic_load_n(&done, __ATOMIC_ACQUIRE)) return 0;
    if (!join_finished()) return -1;
    if (__atomic_load_n(&cancelled, __ATOMIC_ACQUIRE)) { wipe(); result = -1; }
    if (result == 1) { if (pcm) *pcm = recording; if (samples) *samples = captured; }
    return result;
}
void tube_microphone_release(void) {
    if (active && !__atomic_load_n(&done, __ATOMIC_ACQUIRE)) { tube_microphone_cancel(); return; }
    if (!join_finished()) return;
    wipe(); active = 0; result = -1;
}
void tube_microphone_shutdown(void) {
    tube_microphone_cancel();
    if (!join_finished()) return;
    wipe(); active = 0; result = -1;
}
