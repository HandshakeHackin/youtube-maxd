/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "lightbar.h"
#include "brand.h"
#include <math.h>

static const YmLightColor red = {YM_RED_RGB};
static const YmLightColor soft_red = {YM_SOFT_RED_RGB};
static const YmLightColor white = {YM_WHITE_RGB};

static uint8_t channel(double value) {
    return (uint8_t)(value < 0 ? 0 : value > 255 ? 255 : value + 0.5);
}
YmLightColor ym_light_blend(YmLightColor from, YmLightColor to, double amount) {
    if (amount < 0)
        amount = 0;
    if (amount > 1)
        amount = 1;
    return (YmLightColor){channel(from.r + (to.r - from.r) * amount),
                          channel(from.g + (to.g - from.g) * amount),
                          channel(from.b + (to.b - from.b) * amount)};
}
static YmLightColor dim(YmLightColor color, double brightness) {
    return ym_light_blend((YmLightColor){0, 0, 0}, color, brightness);
}
static double breath(uint64_t elapsed, unsigned period) {
    const double tau = 6.283185307179586;
    return (1.0 - cos(tau * (double)(elapsed % period) / period)) * 0.5;
}
static YmLightColor cycle(uint64_t elapsed) {
    const YmLightColor colors[] = {red, soft_red, white, red};
    uint64_t phase = elapsed % 9000;
    unsigned section = (unsigned)(phase / 3000);
    /* Cosine interpolation gives each palette stop a smooth arrival/departure. */
    double mix = (1.0 - cos(3.141592653589793 * (double)(phase % 3000) / 3000.0)) * 0.5;
    return ym_light_blend(colors[section], colors[section + 1], mix);
}
YmLightColor ym_light_color(YmLightState state, uint64_t elapsed) {
    switch (state) {
    case YM_LIGHT_MENU:
        return dim(cycle(elapsed), 0.65);
    case YM_LIGHT_LOADING:
        return dim(cycle(elapsed + 3000), 0.22 + 0.48 * breath(elapsed, 1800));
    case YM_LIGHT_PLAYING:
        return dim(red, 0.35);
    case YM_LIGHT_PAUSED:
        return dim(white, 0.16);
    case YM_LIGHT_SIGN_IN:
        return dim(ym_light_blend(white, soft_red, breath(elapsed, 3000)), 0.30 + 0.30 * breath(elapsed, 3000));
    case YM_LIGHT_ERROR:
        return dim(red, 0.28 + 0.35 * breath(elapsed, 2600));
    case YM_LIGHT_OFF:
    default:
        return (YmLightColor){0, 0, 0};
    }
}
