/* Hold-to-talk without stealing L3 taps. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MAXD_VOICE_GESTURE_H
#define MAXD_VOICE_GESTURE_H
#include <stdint.h>
typedef struct { int armed; int64_t since; } MaxdVoiceGesture;
enum { MAXD_VOICE_NONE, MAXD_VOICE_WAIT, MAXD_VOICE_START, MAXD_VOICE_TAP, MAXD_VOICE_CANCEL };
static inline int maxd_voice_gesture(MaxdVoiceGesture *g, int pressed, int held,
                                     int released, int cancel, int64_t now) {
    if (cancel || now < 0 || (g->armed && now < g->since)) {
        int active = g->armed; g->armed = 0;
        return active ? MAXD_VOICE_CANCEL : MAXD_VOICE_NONE;
    }
    if (pressed && !g->armed) { g->armed = 1; g->since = now; }
    if (!g->armed) return MAXD_VOICE_NONE;
    if (released || !held) { g->armed = 0; return MAXD_VOICE_TAP; }
    if (now - g->since >= 350) { g->armed = 0; return MAXD_VOICE_START; }
    return MAXD_VOICE_WAIT;
}
#endif
