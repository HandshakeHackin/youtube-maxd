/* Adapted from EVO's controller feedback API; preserves its sound/settings contract.
 * Original API copyright its EVO contributors. Max'd effects added 2026-10-17.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "evo_feedback.h"
#include "maxd_feedback.h"
#include <stdint.h>

typedef struct { uint8_t r, g, b, a; } ScePadColor;
int scePadSetLightBar(int handle, const ScePadColor *color);

static int pad = -1, sound_on = 1, light_on = 1, foreground = 1, sent, failed;
static void (*sound)(int);
static YmLightState state = YM_LIGHT_MENU;
static YmLightColor last, from;
static uint64_t clock_ms, since, last_write, error_until, retry_after;
static const int sounds[EVO_FB_COUNT] = {1, 1, 3, 2, 2, 3, 4, 3};

static void push(YmLightColor color, int immediate) {
    if (pad < 0) return;
    if (failed) {
        if (clock_ms < retry_after) return;
        failed = sent = 0;
    }
    if (sent && color.r == last.r && color.g == last.g && color.b == last.b) return;
    if (!immediate && sent && clock_ms - last_write < 50) return;
    const ScePadColor out = {color.r, color.g, color.b, 0};
    if (scePadSetLightBar(pad, &out) != 0) { failed = 1; retry_after = clock_ms + 5000; return; }
    last = color; sent = 1; last_write = clock_ms;
}
static void off(void) { push((YmLightColor){0, 0, 0}, 1); }

void evo_feedback_init(int handle, void (*sfx)(int)) {
    if (pad >= 0 && pad != handle) off();
    pad = handle; sound = sfx; sent = failed = 0;
    state = YM_LIGHT_MENU; from = (YmLightColor){0, 0, 0}; since = clock_ms;
    foreground = 1; error_until = 0;
}
void evo_feedback_set_pad(int handle) {
    if (pad == handle) return;
    off(); pad = handle; sent = failed = 0;
    from = (YmLightColor){0, 0, 0}; since = clock_ms;
}
void evo_feedback(evo_feedback_kind kind) {
    if (kind < 0 || kind >= EVO_FB_COUNT) return;
    if (sound_on && sound) sound(sounds[kind]);
    if (kind == EVO_FB_ERROR) error_until = clock_ms + 2200;
}
void ym_feedback_state(YmLightState next, int active, uint64_t now) {
    clock_ms = now;
    if (next < YM_LIGHT_MENU || next > YM_LIGHT_OFF) next = YM_LIGHT_MENU;
    if (error_until > now && next != YM_LIGHT_OFF) next = YM_LIGHT_ERROR;
    if (foreground != !!active) {
        foreground = !!active;
        if (!foreground) off();
        from = (YmLightColor){0, 0, 0}; since = now; sent = 0;
    }
    if (state != next) { from = sent ? last : (YmLightColor){0, 0, 0}; state = next; since = now; }
}
void evo_feedback_tick(uint64_t now) {
    clock_ms = now;
    if (!foreground || !light_on || state == YM_LIGHT_OFF) { off(); return; }
    const uint64_t elapsed = now >= since ? now - since : 0;
    YmLightColor color = ym_light_color(state, elapsed);
    if (elapsed < 300) color = ym_light_blend(from, color, (double)elapsed / 300.0);
    push(color, 0);
}
void evo_feedback_set_sound(int enabled) { sound_on = !!enabled; }
void evo_feedback_set_lightbar(int enabled) {
    light_on = !!enabled;
    if (!light_on) off();
    from = (YmLightColor){0, 0, 0}; since = clock_ms;
}
int evo_feedback_sound_enabled(void) { return sound_on; }
int evo_feedback_lightbar_enabled(void) { return light_on; }
void evo_feedback_refresh_lightbar(void) { evo_feedback_tick(clock_ms); }
void ym_feedback_shutdown(void) { foreground = 0; error_until = 0; off(); }
