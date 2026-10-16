/* Main-thread phone/player adapter. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_PHONE_H
#define TUBE_PHONE_H
#include "tube_remote.h"
#ifdef __cplusplus
extern "C" {
#endif
#ifndef TUBE_PHONE_NEXT_T
#define TUBE_PHONE_NEXT_T
/* Next video with its optional YouTube playlist occurrence (empty = none). */
typedef struct { char video[12], playlist[101]; int index; } tube_phone_next_t;
#endif
typedef struct {
    void *context;
    /* snapshot returns true only for the selected provider/profile/source. */
    int (*snapshot)(void *, tube_remote_player *);
    int (*pause)(void *, int);
    int (*seek)(void *, double);
    int (*stop)(void *);
    int (*volume)(void *, int);
    int (*key)(void *, const char *);
    /* 1=applied, 2=deferred; polling returns 0=pending, 1=applied, -1=failed. */
    int (*captions)(void *, const char *, unsigned, int);
    void (*browser)(void *);
    /* Default keeps playback running if the phone disconnects. */
    int stop_on_disconnect;
    int (*captions_status)(void *);
    /* 1=accepted; started=0 retires this owner's voice request only. */
    int (*voice)(void *, int started, unsigned owner);
    /* Explicit opt-in closes the app for a current receiver disconnect. */
    int close_on_disconnect;
    int (*close_app)(void *);
    void (*activity)(void *);
    /* Optional owner/selection-scoped asynchronous next lookup.
     * 0=pending, 1=verified video in next, -1=terminal/error. A returned playlist
     * occurrence may repeat the ended video ID at a different position. */
    int (*recommendation)(void *, unsigned owner, unsigned selection,
                          const char *video, const char *playlist, int index, tube_phone_next_t *next);
    void (*recommendation_cancel)(void *);
} tube_phone_controls;
void tube_phone_tick(const tube_phone_controls *controls);
/* Explicit monotonic clock for deterministic adapter verification. */
void tube_phone_tick_at(const tube_phone_controls *controls, int64_t now_ms);
/* Consumed by the normal provider resolver, then acknowledged after startup. */
unsigned tube_phone_take(char video[12], double *position);
int tube_phone_launch_pending(void);
int tube_phone_retry_waiting(void);
/* Call only for the provider's typed, current-scope video_unavailable result.
 * 0=not current/invalid, 1=next occurrence after five seconds, 2=queue exhausted,
 * 3=waiting up to thirty seconds for a scoped playlist window refill. */
int tube_phone_launch_unavailable(unsigned ticket, const char *video, int64_t now_ms);
int tube_phone_launch_current(unsigned ticket);
/* Read-only failure diagnostic: 0 valid, 1 phase, 2 ticket, 3 owner,
 * 4 command, 5 interrupted wait, 6 previous owner, 7 cancelled, 8 cleared. */
int tube_phone_launch_reason(unsigned ticket);
int tube_phone_launch_bind(unsigned ticket, unsigned selection);
int tube_phone_launch_finish(unsigned ticket, int success);
/* Called only after the decoder/presentation engine proves natural EOF. */
int tube_phone_finished(void);
/* Same drained-EOF contract, with explicit monotonic time for verification. */
int tube_phone_finished_at(int64_t now_ms);
int tube_phone_owns_media(void);
void tube_phone_clear(void);
void tube_phone_cancel(void);
#ifdef __cplusplus
}
#endif
#endif
