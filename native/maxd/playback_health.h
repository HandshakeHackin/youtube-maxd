/* Session-only measured playback health. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MAXD_PLAYBACK_HEALTH_H
#define MAXD_PLAYBACK_HEALTH_H
#include <stdint.h>
#include <string.h>
typedef struct {
    uint64_t last,frames,dropped,elapsed,waiting,seen,lost;
    int primed,state,last_action;
} MaxdPlaybackHealth;
enum { MAXD_HEALTH_LEARNING,MAXD_HEALTH_STABLE,MAXD_HEALTH_BUFFERING,MAXD_HEALTH_DROPPING };
static inline void maxd_health_sample(MaxdPlaybackHealth *s,uint64_t now,uint64_t frames,uint64_t dropped,int suppressed,int buffering) {
    if(suppressed || !s->primed || now<s->last || now-s->last>1500 || frames<s->frames || dropped<s->dropped) {
        s->last=now;s->frames=frames;s->dropped=dropped;s->primed=!suppressed;
        s->elapsed=s->waiting=s->seen=s->lost=0;s->state=MAXD_HEALTH_LEARNING;return;
    }
    uint64_t dt=now-s->last;s->last=now;
    s->elapsed+=dt;s->waiting+=buffering?dt:0;
    s->seen+=frames-s->frames;s->lost+=dropped-s->dropped;
    s->frames=frames;s->dropped=dropped;
    if(s->waiting>=3000)s->state=MAXD_HEALTH_BUFFERING;
    else if(s->elapsed>=8000)s->state=s->seen>=48 && s->lost*100>=s->seen*8?MAXD_HEALTH_DROPPING:MAXD_HEALTH_STABLE;
    if(s->elapsed>=8000)s->elapsed=s->waiting=s->seen=s->lost=0;
}
#endif
