/* A long Touchpad hold preserves short taps and fires once per hold.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MAXD_QUICK_SWITCH_H
#define MAXD_QUICK_SWITCH_H
#include <stdint.h>
typedef struct { int tracking,opened; int64_t since; } MaxdQuickSwitch;
enum { MAXD_QUICK_NONE,MAXD_QUICK_OPEN,MAXD_QUICK_TAP,MAXD_QUICK_WAIT };
static inline int maxd_quick_switch(MaxdQuickSwitch *s,int pressed,int held,int released,int cancel,int64_t now) {
    if(cancel || now<0 || (s->tracking && now<s->since)){s->tracking=s->opened=0;return MAXD_QUICK_NONE;}
    if(pressed&&!s->tracking){s->tracking=1;s->opened=0;s->since=now;}
    if(!s->tracking)return MAXD_QUICK_NONE;
    if(released||!held){int tap=!s->opened;s->tracking=s->opened=0;return tap?MAXD_QUICK_TAP:MAXD_QUICK_NONE;}
    if(!s->opened && now-s->since>=650){s->opened=1;return MAXD_QUICK_OPEN;}
    return MAXD_QUICK_WAIT;
}
#endif
