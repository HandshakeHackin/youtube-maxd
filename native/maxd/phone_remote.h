/* PS5-hosted remote. All entry points run on the app thread.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MAXD_PHONE_REMOTE_H
#define MAXD_PHONE_REMOTE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MAXD_REMOTE_QUEUE 64
typedef struct { char id[12],title[161]; } MaxdRemoteItem;
typedef struct {
    unsigned selection; int active,paused,live,busy;
    double position,duration;
    MaxdRemoteItem queue[MAXD_REMOTE_QUEUE]; int count;
} MaxdRemoteState;
enum { MAXD_REMOTE_PAUSE=1,MAXD_REMOTE_RESUME,MAXD_REMOTE_SEEK,MAXD_REMOTE_SEARCH,
       MAXD_REMOTE_KEY,MAXD_REMOTE_ADD,MAXD_REMOTE_REMOVE,MAXD_REMOTE_NEXT };
typedef struct { int kind; unsigned selection; double seconds; char text[801]; } MaxdRemoteCommand;
typedef struct {
    void *context;
    void (*snapshot)(void *,MaxdRemoteState *);
    /* 1 applied/accepted; 0 refused. Never called off the UI thread. */
    int (*apply)(void *,const MaxdRemoteCommand *);
    int (*capture)(void *);
    const char *(*report)(void *);
} MaxdRemoteControls;
int maxd_remote_start(const char *address,unsigned port,int64_t now);
void maxd_remote_stop(void);
void maxd_remote_tick(int64_t now,const MaxdRemoteControls *controls);
int maxd_remote_running(void);
int maxd_remote_paired(void);
const char *maxd_remote_join(void);
/* Provider bridge: profile/source changes revoke access before callbacks. */
int tube_maxd_remote_scope(void);
void tube_maxd_remote_address(const char *address);
int tube_maxd_remote_queue(int action,const char *id);
void tube_maxd_remote_queue_state(MaxdRemoteState *state);
#ifdef __cplusplus
}
#endif
#endif
