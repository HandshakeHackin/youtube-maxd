/* Main-thread native voice search pipeline. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_VOICE_H
#define TUBE_VOICE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { TUBE_VOICE_IDLE, TUBE_VOICE_RECORDING, TUBE_VOICE_TRANSCRIBING,
       TUBE_VOICE_READY, TUBE_VOICE_FAILED };
/* Source/token must come from the provider's validated LAN configuration. */
void tube_voice_configure(const char *source, const char *token, const char *profile);
/* owner=0 for local search, otherwise a current Lounge owner. */
int tube_voice_begin(unsigned owner, int64_t now_ms);
void tube_voice_finish(void);
void tube_voice_cancel(void);
void tube_voice_tick(int64_t now_ms);
int tube_voice_state(void);
unsigned tube_voice_owner(void);
/* Ready result remains owned until cancelled or consumed; main thread only. */
const char *tube_voice_result(void);
/* Consumes the result only when the entire UTF-8 query fits the destination. */
int tube_voice_take(char *query, size_t capacity);
void tube_voice_shutdown(void);
#ifdef __cplusplus
}
#endif
#endif
