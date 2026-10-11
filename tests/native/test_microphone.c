/* Real capture lifecycle with deterministic PS5 AudioIn substitutes. */
#include "tube_microphone.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>

static pthread_t main_thread;
static int mode, opens, closes, inputs, gated, reached, resume, clocks;
extern int __real_clock_gettime(clockid_t clock, struct timespec *value);
int __wrap_clock_gettime(clockid_t clock, struct timespec *value) {
    if (mode != 7) return __real_clock_gettime(clock, value);
    assert(clock == CLOCK_MONOTONIC);
    value->tv_sec = clocks++ ? 13 : 0; value->tv_nsec = 0; return 0;
}
extern int __real_pthread_create(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
int __wrap_pthread_create(pthread_t *thread, const pthread_attr_t *attributes,
                         void *(*function)(void *), void *context) {
    return mode == 8 ? EAGAIN : __real_pthread_create(thread, attributes, function, context);
}
static void delay(void) { struct timespec t = {0, 1000000}; nanosleep(&t, NULL); }
int32_t sceUserServiceGetInitialUser(int32_t *user) {
    assert(!pthread_equal(main_thread, pthread_self()));
    if (mode == 1) return -1;
    *user = mode == 9 ? -1 : 42; return 0;
}
int32_t sceAudioInOpen(int32_t user, int32_t type, int32_t index,
                       uint32_t grain, uint32_t frequency, uint32_t format) {
    assert(!pthread_equal(main_thread, pthread_self()));
    assert(user == 42 && type == 5 && index == 0 && grain == 256 && frequency == 16000 && format == 0);
    ++opens; return mode == 2 ? -1 : 7;
}
int32_t sceAudioInInput(int32_t handle, void *destination) {
    assert(handle == 7 && !pthread_equal(main_thread, pthread_self()));
    int16_t *pcm = destination;
    ++inputs;
    if (gated && inputs == gated) {
        __atomic_store_n(&reached, 1, __ATOMIC_RELEASE);
        while (!__atomic_load_n(&resume, __ATOMIC_ACQUIRE)) delay();
    }
    for (int i = 0; i < 256; ++i) pcm[i] = mode == 3 ? 0 : mode == 6 ? 63 : (int16_t)(i % 2 ? -32768 : 100);
    return mode == 4 ? -1 : 0;
}
int32_t sceAudioInClose(int32_t handle) {
    assert(handle == 7 && !pthread_equal(main_thread, pthread_self()));
    ++closes; return mode == 5 ? -1 : 0;
}
static int poll(const int16_t **pcm, size_t *count) {
    for (int i = 0; i < 3000; ++i) {
        int rc = tube_microphone_poll(pcm, count);
        if (rc) return rc;
        delay();
    }
    assert(!"capture did not finish"); return 0;
}
static void await_gate(void) {
    for (int i = 0; i < 3000; ++i) {
        if (__atomic_load_n(&reached, __ATOMIC_ACQUIRE)) return;
        delay();
    }
    assert(!"input gate not reached");
}
static void reset(int next_mode, int gate) {
    tube_microphone_shutdown();
    mode = next_mode; opens = closes = inputs = clocks = 0; gated = gate;
    __atomic_store_n(&reached, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&resume, 0, __ATOMIC_RELEASE);
}
int main(void) {
    main_thread = pthread_self();
    const int16_t *pcm = NULL; size_t count = 0;
    assert(tube_microphone_poll(&pcm, &count) == -1 && !pcm && !count);
    assert(!tube_microphone_begin(499) && !tube_microphone_begin(10001));
    reset(0, 0);
    assert(tube_microphone_begin(500));
    assert(!tube_microphone_begin(1000));
    assert(poll(&pcm, &count) == 1 && count == 8192 && pcm[1] == -32768);
    assert(opens == 1 && closes == 1);
    const int16_t *saved = pcm;
    assert(tube_microphone_poll(&pcm, &count) == 1 && pcm == saved);
    tube_microphone_cancel();
    assert(tube_microphone_poll(&pcm, &count) == -1 && !pcm && !count);
    for (size_t i = 0; i < 8192; ++i) assert(saved[i] == 0);
    tube_microphone_release();
    reset(0, 0);
    assert(tube_microphone_begin(10000));
    assert(poll(&pcm, &count) == 1 && count == 160000);
    saved = pcm; tube_microphone_release();
    for (size_t i = 0; i < 160000; ++i) assert(saved[i] == 0);
    for (int failure = 1; failure <= 7; ++failure) {
        reset(failure, 0); assert(tube_microphone_begin(500));
        assert(poll(&pcm, &count) == -1 && !pcm && !count);
        assert(closes == (failure >= 3));
        tube_microphone_release();
    }
    reset(8, 0); assert(!tube_microphone_begin(500));
    assert(tube_microphone_poll(&pcm, &count) == -1 && opens == 0);
    reset(9, 0); assert(tube_microphone_begin(500));
    assert(poll(&pcm, &count) == -1 && opens == 0);
    reset(0, 1); assert(tube_microphone_begin(6000)); await_gate();
    assert(tube_microphone_poll(&pcm, &count) == 0 && !pcm && !count);
    tube_microphone_cancel(); tube_microphone_release();
    assert(!tube_microphone_begin(500)); /* storage still owned by input worker */
    __atomic_store_n(&resume, 1, __ATOMIC_RELEASE);
    assert(poll(&pcm, &count) == -1 && closes == 1);
    tube_microphone_release();
    reset(0, 1); assert(tube_microphone_begin(6000)); await_gate();
    tube_microphone_finish(); __atomic_store_n(&resume, 1, __ATOMIC_RELEASE);
    assert(poll(&pcm, &count) == -1); /* early release rejects short recording */
    reset(0, 34); assert(tube_microphone_begin(6000)); await_gate();
    tube_microphone_finish(); __atomic_store_n(&resume, 1, __ATOMIC_RELEASE);
    assert(poll(&pcm, &count) == 1 && count == 34 * 256 && closes == 1);
    tube_microphone_shutdown(); assert(saved[0] == 0);
    reset(0, 0); assert(tube_microphone_begin(500)); tube_microphone_shutdown();
    assert(tube_microphone_poll(&pcm, &count) == -1 && !pcm && !count);
    assert(tube_microphone_begin(500) && poll(&pcm, &count) == 1);
    tube_microphone_shutdown();
    puts("microphone: capture, bounds, errors, silence, cancellation, finish, wipe and restart passed");
    return 0;
}
