/* S16 gain after mixing/tempo, before PS5 AudioOut. */
#ifndef TUBE_VOLUME_H
#define TUBE_VOLUME_H
#include <stddef.h>
#include <stdint.h>
static inline void tube_volume_pcm(int16_t *out, const int16_t *in, size_t count, int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    for (size_t i = 0; i < count; ++i) out[i] = (int16_t)((int32_t)in[i] * volume / 100);
}
#endif
