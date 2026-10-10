/* Pitch-preserving PCM tempo stage. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_AUDIO_TEMPO_H
#define TUBE_AUDIO_TEMPO_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct tube_audio_tempo tube_audio_tempo;
/* Single decoder-thread ownership. Input/output: interleaved S16, 48 kHz,
 * 1..8 channels. Rate is fixed for the lifetime of a stream/seek epoch.
 * Recreate after a seek or rate change; never carry queued PCM across epochs.
 * Callback data is borrowed only for the call. A negative return cancels the
 * stage permanently; no rejected PCM is retried. No internal threads are used. */
typedef int (*tube_tempo_output)(void *user, const int16_t *pcm, int samples);
tube_audio_tempo *tube_tempo_create(int channels, double rate);
int tube_tempo_feed(tube_audio_tempo *tempo, const int16_t *pcm, int samples,
                    tube_tempo_output output, void *user);
/* Flush once at natural EOF. Repeated finishes do not duplicate the tail. */
int tube_tempo_finish(tube_audio_tempo *tempo, tube_tempo_output output, void *user);
void tube_tempo_destroy(tube_audio_tempo *tempo);

#ifdef __cplusplus
}
#endif
#endif
