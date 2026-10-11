/* Audio delay (SmartTube "audio shift") for the PS5 output path.
 * A positive delay holds the PCM back in a delay line after the media clock
 * has counted it, so the picture leads the sound. A negative delay cannot be
 * made from PCM that does not exist yet: the video side waits for the audio
 * clock minus a hold instead.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_AUDIO_DELAY_H
#define TUBE_AUDIO_DELAY_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define TUBE_AUDIO_DELAY_MAX_MS 2000
#define TUBE_AUDIO_DELAY_RATE 48000
#define TUBE_AUDIO_DELAY_MAX_FRAMES (TUBE_AUDIO_DELAY_MAX_MS * (TUBE_AUDIO_DELAY_RATE / 1000))

static inline int tube_audio_delay_clamp_ms(int ms) {
    return ms < -TUBE_AUDIO_DELAY_MAX_MS ? -TUBE_AUDIO_DELAY_MAX_MS : ms > TUBE_AUDIO_DELAY_MAX_MS ? TUBE_AUDIO_DELAY_MAX_MS : ms;
}
/* PCM frames to hold back at 48 kHz (0 for a negative delay). */
static inline size_t tube_audio_delay_frames(int ms) {
    ms = tube_audio_delay_clamp_ms(ms);
    return ms > 0 ? (size_t)ms * (TUBE_AUDIO_DELAY_RATE / 1000) : 0;
}
/* Seconds the picture waits behind the audio clock (0 for a positive delay). */
static inline double tube_audio_delay_video_hold(int ms) {
    ms = tube_audio_delay_clamp_ms(ms);
    return ms < 0 ? -ms / 1000.0 : 0.0;
}

typedef struct {
    int16_t *ring;         /* capacity * channels_max samples, owned by the caller */
    size_t capacity;       /* frames */
    int channels_max;
    int channels;          /* layout of the stored frames */
    size_t write;          /* next frame to write */
    size_t tail;           /* frames of real audio still waiting inside the line */
    int dirty;             /* the ring holds something other than silence */
} tube_audio_delay;

static inline void tube_audio_delay_init(tube_audio_delay *line, int16_t *ring, size_t capacity, int channels_max) {
    memset(line, 0, sizeof *line);
    line->ring = ring; line->capacity = capacity; line->channels_max = channels_max;
    line->dirty = 1; /* the caller's storage is not known to be silent */
}

/* Forget everything queued: a seek, a new stream or a stop. */
static inline void tube_audio_delay_reset(tube_audio_delay *line) {
    if (line->dirty && line->ring)
        memset(line->ring, 0, line->capacity * (size_t)line->channels_max * sizeof(int16_t));
    line->write = 0; line->tail = 0; line->dirty = 0;
}

static inline size_t tube_audio_delay_tail(const tube_audio_delay *line) { return line->tail; }

/* Pass one block through the line. `content` is zero for filler silence that
 * only pushes the remaining real audio out. Returns the block to play: `in`
 * itself while no delay is active and nothing is waiting, otherwise `out`. */
static inline const int16_t *tube_audio_delay_process(tube_audio_delay *line, const int16_t *in, int16_t *out,
                                                      size_t frames, int channels, size_t delay_frames, int content) {
    if (!line->ring || channels < 1 || channels > line->channels_max || !frames || frames >= line->capacity) {
        line->tail = 0;
        return in;
    }
    if (delay_frames > line->capacity - frames) delay_frames = line->capacity - frames;
    if (channels != line->channels) {
        tube_audio_delay_reset(line);
        line->channels = channels;
    }
    if (!delay_frames && !line->tail) {
        if (line->dirty) tube_audio_delay_reset(line);
        return in;
    }
    const size_t width = (size_t)channels;
    const size_t start = line->write;
    for (size_t done = 0; done < frames;) {
        const size_t at = (start + done) % line->capacity;
        size_t run = line->capacity - at;
        if (run > frames - done) run = frames - done;
        memcpy(line->ring + at * width, in + done * width, run * width * sizeof(int16_t));
        done += run;
    }
    const size_t read = (start + line->capacity - delay_frames) % line->capacity;
    for (size_t done = 0; done < frames;) {
        const size_t at = (read + done) % line->capacity;
        size_t run = line->capacity - at;
        if (run > frames - done) run = frames - done;
        memcpy(out + done * width, line->ring + at * width, run * width * sizeof(int16_t));
        done += run;
    }
    line->write = (start + frames) % line->capacity;
    if (content) { line->tail = delay_frames; line->dirty = 1; }
    else line->tail = line->tail > frames ? line->tail - frames : 0;
    return out;
}
#endif
