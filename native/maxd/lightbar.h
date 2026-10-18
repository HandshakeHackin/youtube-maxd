/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef YOUTUBE_MAXD_LIGHTBAR_H
#define YOUTUBE_MAXD_LIGHTBAR_H
#include <stdint.h>

typedef struct {
    uint8_t r, g, b;
} YmLightColor;
typedef enum {
    YM_LIGHT_MENU,
    YM_LIGHT_LOADING,
    YM_LIGHT_PLAYING,
    YM_LIGHT_PAUSED,
    YM_LIGHT_SIGN_IN,
    YM_LIGHT_ERROR,
    YM_LIGHT_OFF
} YmLightState;

#ifdef __cplusplus
extern "C" {
#endif
/* Pure, deterministic animation; elapsed is time since entering this state. */
YmLightColor ym_light_color(YmLightState state, uint64_t elapsed_ms);
YmLightColor ym_light_blend(YmLightColor from, YmLightColor to, double amount);
#ifdef __cplusplus
}
#endif
#endif
