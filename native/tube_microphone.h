/* Bounded PS5 controller microphone capture. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_MICROPHONE_H
#define TUBE_MICROPHONE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Main-thread API. AudioIn open/input/close run only on the owned worker.
 * Application must already have initialized UserService. */
int tube_microphone_begin(unsigned milliseconds);
/* Finish a push-to-talk recording; less than 0.5 seconds is rejected. */
void tube_microphone_finish(void);
/* Cancellation discards samples, including an already completed recording. */
void tube_microphone_cancel(void);
/* 0=pending, 1=ready, -1=failed/cancelled/idle. No blocking joins while recording.
 * Ready PCM is 16 kHz signed-16 mono; pointer valid until release/shutdown. */
int tube_microphone_poll(const int16_t **pcm, size_t *samples);
void tube_microphone_release(void);
/* Final shutdown joins the owned worker before releasing its storage. */
void tube_microphone_shutdown(void);
#ifdef __cplusplus
}
#endif
#endif
