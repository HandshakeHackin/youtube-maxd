/* S16 gain after mixing/tempo, before PS5 AudioOut. */
#ifndef TUBE_VOLUME_H
#define TUBE_VOLUME_H
#include <math.h>
#include <stddef.h>
#include <stdint.h>
static inline void tube_volume_pcm(int16_t *out, const int16_t *in, size_t count, int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    for (size_t i = 0; i < count; ++i) out[i] = (int16_t)((int32_t)in[i] * volume / 100);
}

/* Player volume in thousandths: 1000 = unchanged, up to 3000 (300 %). */
#define TUBE_VOLUME_UNITY 1000
#define TUBE_VOLUME_MAX_MILLI 3000
/* Boosted samples pass unchanged up to this level, then approach full scale
 * smoothly instead of wrapping or clipping flat. */
#define TUBE_VOLUME_KNEE 24576

static inline int16_t tube_volume_limit(int32_t value) {
    const int32_t magnitude = value < 0 ? -value : value;
    if (magnitude <= TUBE_VOLUME_KNEE) return (int16_t)value;
    const int64_t room = 32767 - TUBE_VOLUME_KNEE, over = (int64_t)magnitude - TUBE_VOLUME_KNEE;
    const int32_t limited = (int32_t)(TUBE_VOLUME_KNEE + room * over / (room + over));
    return (int16_t)(value < 0 ? -limited : limited);
}

/* Phone volume (0..100) combined with the player gain (0..3000 thousandths). */
static inline int tube_volume_total_milli(int phone_volume, int player_milli) {
    if (phone_volume < 0) phone_volume = 0;
    if (phone_volume > 100) phone_volume = 100;
    if (player_milli < 0) player_milli = 0;
    if (player_milli > TUBE_VOLUME_MAX_MILLI) player_milli = TUBE_VOLUME_MAX_MILLI;
    return phone_volume * player_milli / 100;
}

/* Up to unity this is the plain attenuation of tube_volume_pcm(); above it the
 * boosted signal goes through the limiter, so no sample can overflow. */
static inline void tube_volume_pcm_gain(int16_t *out, const int16_t *in, size_t count, int gain_milli) {
    if (gain_milli < 0) gain_milli = 0;
    if (gain_milli > TUBE_VOLUME_MAX_MILLI) gain_milli = TUBE_VOLUME_MAX_MILLI;
    if (gain_milli <= TUBE_VOLUME_UNITY) {
        for (size_t i = 0; i < count; ++i) out[i] = (int16_t)((int32_t)in[i] * gain_milli / TUBE_VOLUME_UNITY);
        return;
    }
    for (size_t i = 0; i < count; ++i) out[i] = tube_volume_limit((int32_t)in[i] * gain_milli / TUBE_VOLUME_UNITY);
}

/* Loudness normalisation as YouTube's own player applies it: a video louder
 * than the reference level (positive loudness_db) is attenuated by exactly
 * that many decibels, gain = 10^(-loudness_db / 20); a quieter video is never
 * amplified. Unknown or non-finite values leave the gain at unity. */
static inline int tube_volume_normalization_milli(double loudness_db) {
    if (!isfinite(loudness_db) || loudness_db <= 0) return TUBE_VOLUME_UNITY;
    if (loudness_db > 60) loudness_db = 60;
    const int gain = (int)(pow(10.0, -loudness_db / 20.0) * TUBE_VOLUME_UNITY + 0.5);
    return gain < 1 ? 1 : gain;
}
#endif
