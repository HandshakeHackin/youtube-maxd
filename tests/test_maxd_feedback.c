/* Native pad bridge: transitions, write budget, settings, disconnect and failures.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "maxd_feedback.h"
#include "evo_feedback.h"
#include <assert.h>
#include <stdio.h>

typedef struct { unsigned char r,g,b,a; } ScePadColor;
static unsigned calls, sound_calls;
static int last_pad, reject;
static ScePadColor color;
int scePadSetLightBar(int pad, const ScePadColor *rgb) {
    ++calls; last_pad=pad; color=*rgb;
    return reject ? -1 : 0;
}
static void sfx(int kind) { assert(kind>=1 && kind<=4); ++sound_calls; }
static void step(YmLightState state, int focused, unsigned ms) {
    ym_feedback_state(state,focused,ms); evo_feedback_tick(ms);
}
int main(void) {
    evo_feedback_init(7,sfx);
    for (unsigned t=0;t<=1000;++t) step(YM_LIGHT_MENU,1,t);
    assert(calls<=22 && calls>=10 && last_pad==7);
    step(YM_LIGHT_PLAYING,1,1100); step(YM_LIGHT_PLAYING,1,1500);
    YmLightColor expected=ym_light_color(YM_LIGHT_PLAYING,400);
    assert(color.r==expected.r && color.g==expected.g && color.b==expected.b);
    unsigned before=calls;
    for (unsigned t=1501;t<=2000;++t) step(YM_LIGHT_PLAYING,1,t);
    assert(calls==before);
    step(YM_LIGHT_PAUSED,1,2100); step(YM_LIGHT_PAUSED,1,2500);
    expected=ym_light_color(YM_LIGHT_PAUSED,400);
    assert(color.r==expected.r && color.g==expected.g && color.b==expected.b);
    step(YM_LIGHT_SIGN_IN,1,2600); step(YM_LIGHT_SIGN_IN,1,3000);
    assert(color.r>=color.b && color.r>0); /* listening follows red through white */
    evo_feedback_set_lightbar(0);
    assert(!color.r && !color.g && !color.b);
    before=calls; step(YM_LIGHT_MENU,1,3500); assert(calls==before);
    evo_feedback_set_lightbar(1); step(YM_LIGHT_MENU,1,3900);
    assert(color.r || color.b);
    step(YM_LIGHT_MENU,0,4000); assert(!color.r && !color.g && !color.b);
    step(YM_LIGHT_MENU,1,4100); step(YM_LIGHT_MENU,1,4500);
    assert(color.r || color.b);
    evo_feedback(EVO_FB_ERROR); assert(sound_calls==1);
    step(YM_LIGHT_MENU,1,4600); step(YM_LIGHT_MENU,1,5000);
    assert(color.r>color.b);
    evo_feedback_set_sound(0); evo_feedback(EVO_FB_CONFIRM); assert(sound_calls==1);
    evo_feedback_set_pad(-1); before=calls;
    step(YM_LIGHT_MENU,1,6000); assert(calls==before);
    evo_feedback_set_pad(8); step(YM_LIGHT_MENU,1,6500); assert(last_pad==8);
    reject=1; step(YM_LIGHT_LOADING,1,7000); step(YM_LIGHT_LOADING,1,7400);
    before=calls; step(YM_LIGHT_LOADING,1,8000); assert(calls==before);
    reject=0; step(YM_LIGHT_LOADING,1,12500); assert(calls>before && last_pad==8);
    evo_feedback_set_pad(9); step(YM_LIGHT_MENU,1,12600);
    step(YM_LIGHT_MENU,1,13000); assert(last_pad==9 && (color.r || color.b));
    ym_feedback_shutdown(); assert(!color.r && !color.g && !color.b);
    puts("Max'd native light-bar bridge: passed");
}
