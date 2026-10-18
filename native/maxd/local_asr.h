/* Bounded, asynchronous on-device speech recognition. GPL-3.0-or-later. */
#ifndef MAXD_LOCAL_ASR_H
#define MAXD_LOCAL_ASR_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Main-thread API; model verification and inference belong to one worker.
 * No microphone, network, persistent audio or subprocess is used here. */
int maxd_asr_available(const char *model);
int maxd_asr_start(const char *model, const int16_t *pcm, size_t samples);
/* 0 pending; 1 transcript copied in full; -1 unavailable/failed/cancelled.
 * A too-small output buffer returns -1 without consuming the result. */
int maxd_asr_poll(char *query, size_t capacity);
void maxd_asr_cancel(void);
void maxd_asr_release(void);
const char *maxd_asr_error(void);
void maxd_asr_shutdown(void);
#ifdef __cplusplus
}
#endif
#endif
