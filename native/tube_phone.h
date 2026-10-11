/* Main-thread phone/player adapter. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_PHONE_H
#define TUBE_PHONE_H
#include "tube_remote.h"
#ifdef __cplusplus
extern "C" {
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
} tube_phone_controls;
void tube_phone_tick(const tube_phone_controls *controls);
/* Consumed by the normal provider resolver, then acknowledged after startup. */
unsigned tube_phone_take(char video[12], double *position);
int tube_phone_launch_current(unsigned ticket);
int tube_phone_launch_finish(unsigned ticket, int success);
/* Called only after the decoder/presentation engine proves natural EOF. */
int tube_phone_finished(void);
int tube_phone_owns_media(void);
void tube_phone_clear(void);
void tube_phone_cancel(void);
#ifdef __cplusplus
}
#endif
#endif
