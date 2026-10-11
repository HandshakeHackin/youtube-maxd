/* Player volume boost, loudness normalisation and the audio delay line. */
#include "tube_audio_delay.h"
#include "tube_volume.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void test_gain(void) {
    const int16_t input[] = { -32768, -20000, -100, 0, 99, 12000, 20000, 32767 };
    enum { COUNT = sizeof input / sizeof *input };
    int16_t output[COUNT], phone[COUNT];

    /* Up to unity the result is the phone attenuation, sample for sample. */
    for (int volume = 0; volume <= 100; ++volume) {
        tube_volume_pcm(phone, input, COUNT, volume);
        tube_volume_pcm_gain(output, input, COUNT, tube_volume_total_milli(volume, TUBE_VOLUME_UNITY));
        assert(!memcmp(phone, output, sizeof output));
    }
    tube_volume_pcm_gain(output, input, COUNT, TUBE_VOLUME_UNITY);
    assert(!memcmp(input, output, sizeof output));
    tube_volume_pcm_gain(output, input, COUNT, -5);
    for (int i = 0; i < COUNT; ++i) assert(!output[i]);

    /* The two volumes multiply and stay inside 0..300 %. */
    assert(tube_volume_total_milli(100, 3000) == 3000 && tube_volume_total_milli(50, 3000) == 1500);
    assert(tube_volume_total_milli(100, 9000) == 3000 && tube_volume_total_milli(140, 1000) == 1000);
    assert(tube_volume_total_milli(-1, 1000) == 0 && tube_volume_total_milli(100, -1) == 0);

    /* A boost is linear below the knee and never leaves the 16-bit range. */
    tube_volume_pcm_gain(output, input, COUNT, 2000);
    assert(output[2] == -200 && output[3] == 0 && output[4] == 198 && output[5] == 24000);
    for (int gain = 1001; gain <= 3000; gain += 37) {
        int16_t previous = -32768;
        for (int sample = -32768; sample <= 32767; sample += 7) {
            const int16_t in = (int16_t)sample;
            int16_t out;
            tube_volume_pcm_gain(&out, &in, 1, gain);
            assert(out >= previous);                       /* monotonic: no wrap, no fold */
            assert(out > -32768);
            assert((sample < 0) == (out < 0) || !out || !sample);
            previous = out;
        }
    }
    tube_volume_pcm_gain(output, input, COUNT, 3000);
    assert(output[7] > 31000 && output[7] < 32767 && output[0] < -31000 && output[0] > -32768);
    tube_volume_pcm_gain(output, input, COUNT, 50000);    /* clamped to 300 % */
    assert(output[5] == tube_volume_limit(36000));
    assert(tube_volume_limit(TUBE_VOLUME_KNEE) == TUBE_VOLUME_KNEE && tube_volume_limit(-TUBE_VOLUME_KNEE) == -TUBE_VOLUME_KNEE);
    assert(tube_volume_limit(TUBE_VOLUME_KNEE + 1) == TUBE_VOLUME_KNEE && tube_volume_limit(98304) < 32767);

    /* Loudness only attenuates: +6 dB halves, quiet or unknown stays at unity. */
    assert(tube_volume_normalization_milli(0) == 1000 && tube_volume_normalization_milli(-7.5) == 1000);
    assert(tube_volume_normalization_milli(NAN) == 1000 && tube_volume_normalization_milli(INFINITY) == 1000);
    assert(tube_volume_normalization_milli(6.0206) == 500 && tube_volume_normalization_milli(20) == 100);
    assert(tube_volume_normalization_milli(3) == 708 && tube_volume_normalization_milli(60) == 1);
    assert(tube_volume_normalization_milli(500) == 1);
}

static void test_delay(void) {
    assert(tube_audio_delay_frames(0) == 0 && tube_audio_delay_frames(250) == 12000);
    assert(tube_audio_delay_frames(-250) == 0 && tube_audio_delay_frames(99999) == TUBE_AUDIO_DELAY_MAX_FRAMES);
    assert(tube_audio_delay_video_hold(0) == 0 && tube_audio_delay_video_hold(250) == 0);
    assert(tube_audio_delay_video_hold(-250) == 0.25 && tube_audio_delay_video_hold(-99999) == 2.0);
    assert(tube_audio_delay_clamp_ms(-5000) == -TUBE_AUDIO_DELAY_MAX_MS && tube_audio_delay_clamp_ms(17) == 17);

    enum { BLOCK = 8, CAPACITY = 40, CHANNELS = 2 };
    int16_t *ring = malloc(CAPACITY * CHANNELS * sizeof *ring);
    assert(ring);
    for (int i = 0; i < CAPACITY * CHANNELS; ++i) ring[i] = 0x5555;   /* dirty storage must not be played */
    tube_audio_delay line;
    tube_audio_delay_init(&line, ring, CAPACITY, CHANNELS);
    int16_t in[BLOCK * CHANNELS], out[BLOCK * CHANNELS], silence[BLOCK * CHANNELS] = {0};

    /* No delay: the caller's block is played untouched. */
    for (int i = 0; i < BLOCK * CHANNELS; ++i) in[i] = (int16_t)(i + 1);
    assert(tube_audio_delay_process(&line, in, out, BLOCK, CHANNELS, 0, 1) == in && !tube_audio_delay_tail(&line));

    /* A stream of numbered frames comes out exactly `delay` frames later. */
    const size_t delay = 12;
    int next = 1, expected = 1;
    for (int block = 0; block < 20; ++block) {
        for (int i = 0; i < BLOCK; ++i, ++next) { in[i * 2] = (int16_t)next; in[i * 2 + 1] = (int16_t)-next; }
        const int16_t *played = tube_audio_delay_process(&line, in, out, BLOCK, CHANNELS, delay, 1);
        assert(played == out && tube_audio_delay_tail(&line) == delay);
        for (int i = 0; i < BLOCK; ++i) {
            const int frame = block * BLOCK + i;
            if (frame < (int)delay) assert(!played[i * 2] && !played[i * 2 + 1]);   /* silence first, never stale data */
            else { assert(played[i * 2] == expected && played[i * 2 + 1] == -expected); ++expected; }
        }
    }
    /* At the end, filler silence pushes the remaining frames out, then the tail is empty. */
    while (tube_audio_delay_tail(&line)) {
        const size_t before = tube_audio_delay_tail(&line);
        const int16_t *played = tube_audio_delay_process(&line, silence, out, BLOCK, CHANNELS, delay, 0);
        for (int i = 0; i < BLOCK; ++i)
            if (expected < next) { assert(played[i * 2] == expected); ++expected; } else assert(!played[i * 2]);
        assert(tube_audio_delay_tail(&line) < before);
    }
    assert(expected == next);

    /* A reset (seek) drops queued audio: only silence leaves afterwards. */
    for (int i = 0; i < BLOCK * CHANNELS; ++i) in[i] = 77;
    tube_audio_delay_process(&line, in, out, BLOCK, CHANNELS, delay, 1);
    tube_audio_delay_reset(&line);
    assert(!tube_audio_delay_tail(&line));
    const int16_t *played = tube_audio_delay_process(&line, silence, out, BLOCK, CHANNELS, delay, 0);
    for (int i = 0; i < BLOCK * CHANNELS; ++i) assert(!played[i]);

    /* Switching the delay off returns to pass-through without stale output. */
    tube_audio_delay_process(&line, in, out, BLOCK, CHANNELS, delay, 1);
    assert(tube_audio_delay_process(&line, in, out, BLOCK, CHANNELS, 0, 1) == out && out[0] == 77);
    assert(tube_audio_delay_process(&line, in, out, BLOCK, CHANNELS, 0, 1) == in);
    played = tube_audio_delay_process(&line, silence, out, BLOCK, CHANNELS, delay, 0);
    for (int i = 0; i < BLOCK * CHANNELS; ++i) assert(!played[i]);

    /* A delay longer than the line is bounded; a changed layout starts clean. */
    tube_audio_delay_reset(&line);
    played = tube_audio_delay_process(&line, in, out, BLOCK, CHANNELS, 100000, 1);
    assert(played == out && tube_audio_delay_tail(&line) == CAPACITY - BLOCK && !out[0]);
    played = tube_audio_delay_process(&line, in, out, BLOCK, 1, delay, 1);
    for (int i = 0; i < BLOCK; ++i) assert(!played[i]);
    /* Unusable arguments fall back to pass-through. */
    assert(tube_audio_delay_process(&line, in, out, BLOCK, 3, delay, 1) == in);
    assert(tube_audio_delay_process(&line, in, out, CAPACITY, CHANNELS, delay, 1) == in);
    free(ring);
}

int main(void) {
    test_gain();
    test_delay();
    puts("PASS: volume boost limiter, loudness normalisation and audio delay line");
    return 0;
}
