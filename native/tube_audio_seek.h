/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_AUDIO_SEEK_H
#define TUBE_AUDIO_SEEK_H
#include <math.h>
#include <stdint.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>

typedef struct tube_audio_seek {
    double next_pts;
    unsigned untimed;
    int timed;
} tube_audio_seek;

/* Decode preroll normally. Only decoded PCM may be discarded, so codecs
 * retain their prediction/overlap state. Return keep=1, drop=0, error=-1.
 * Timestamp inference is limited to contiguous frames after a known PTS. */
static inline int tube_audio_seek_frame(tube_audio_seek *state, AVFrame *frame,
                                        double *pts, double origin, double *target)
{
    if (!frame || frame->sample_rate <= 0 || frame->nb_samples <= 0) return -1;
    if (!isfinite(*pts) && state->timed) *pts = state->next_pts;
    if (isfinite(*pts)) {
        state->next_pts = *pts + (double)frame->nb_samples / frame->sample_rate;
        state->timed = 1;
        state->untimed = 0;
    }
    if (*target < 0.0) return 1;
    if (!isfinite(*target) || !isfinite(origin)) return -1;
    if (!isfinite(*pts)) return ++state->untimed <= 512 ? 0 : -1;
    const double remaining = (*target + origin - *pts) * frame->sample_rate;
    if (!isfinite(remaining)) return -1;
    if (remaining >= frame->nb_samples) return 0;
    if (remaining > 0.0) {
        const int skip = (int)ceil(remaining);
        if (skip >= frame->nb_samples) return 0;
        const int channels = frame->ch_layout.nb_channels;
        const int bytes = av_get_bytes_per_sample((enum AVSampleFormat)frame->format);
        const int planar = av_sample_fmt_is_planar((enum AVSampleFormat)frame->format);
        if (channels <= 0 || channels > 64 || bytes <= 0 ||
            av_frame_make_writable(frame) < 0 || !frame->extended_data) return -1;
        const size_t stride = (size_t)bytes * (planar ? 1u : (unsigned)channels);
        if ((size_t)frame->nb_samples > SIZE_MAX / stride) return -1;
        const int planes = planar ? channels : 1;
        for (int i = 0; i < planes; ++i) if (!frame->extended_data[i]) return -1;
        for (int i = 0; i < planes; ++i)
            memmove(frame->extended_data[i], frame->extended_data[i] + skip * stride,
                    (frame->nb_samples - skip) * stride);
        frame->nb_samples -= skip;
        *pts += (double)skip / frame->sample_rate;
    }
    *target = -1.0;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif
