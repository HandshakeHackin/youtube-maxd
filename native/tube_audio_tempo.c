/* Original bounded FFmpeg tempo adapter. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_audio_tempo.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>

struct tube_audio_tempo {
    AVFilterGraph *graph;
    AVFilterContext *source, *sink;
    AVFrame *frame;
    int channels, terminal;
    int64_t input_samples;
};

void tube_tempo_destroy(tube_audio_tempo *tempo) {
    if (!tempo) return;
    av_frame_free(&tempo->frame);
    avfilter_graph_free(&tempo->graph);
    free(tempo);
}

tube_audio_tempo *tube_tempo_create(int channels, double rate) {
    if (channels < 1 || channels > 8 || !isfinite(rate) || rate < .25 || rate > 4) return NULL;
    tube_audio_tempo *tempo = calloc(1, sizeof *tempo);
    if (!tempo) return NULL;
    tempo->channels = channels;
    /* Preserve the existing 1x path exactly, without allocation/filter latency. */
    if (rate == 1) return tempo;
    const AVFilter *source = avfilter_get_by_name("abuffer"), *sink = avfilter_get_by_name("abuffersink");
    const AVFilter *atempo = avfilter_get_by_name("atempo");
    if (!source || !sink || !atempo) goto fail;
    tempo->graph = avfilter_graph_alloc();
    tempo->frame = av_frame_alloc();
    if (!tempo->graph || !tempo->frame) goto fail;
    tempo->graph->nb_threads = 1;
    AVChannelLayout layout;
    av_channel_layout_default(&layout, channels);
    char description[128], args[256];
    int described = av_channel_layout_describe(&layout, description, sizeof description);
    av_channel_layout_uninit(&layout);
    if (described < 0 || described >= (int)sizeof description) goto fail;
    snprintf(args, sizeof args, "time_base=1/48000:sample_rate=48000:sample_fmt=s16:channel_layout=%s", description);
    if (avfilter_graph_create_filter(&tempo->source, source,
                                    "input", args, NULL, tempo->graph) < 0) goto fail;
    if (avfilter_graph_create_filter(&tempo->sink, sink,
                                    "output", NULL, NULL, tempo->graph) < 0) goto fail;
    AVFilterContext *last = tempo->source;
    /* atempo above 2 skips samples. Cascade factors within [0.5,2] instead. */
    double factors[2] = {rate, 1};
    int stages = 1;
    if (rate < .5) { factors[0] = .5; factors[1] = rate / .5; stages = 2; }
    if (rate > 2) { factors[0] = 2; factors[1] = rate / 2; stages = 2; }
    for (int i = 0; i < stages; ++i) {
        AVFilterContext *filter = NULL;
        const char *name = i ? "tempo1" : "tempo0";
        snprintf(args, sizeof args, "tempo=%.12g", factors[i]);
        if (avfilter_graph_create_filter(&filter, atempo, name,
                                        args, NULL, tempo->graph) < 0 || avfilter_link(last, 0, filter, 0) < 0) goto fail;
        last = filter;
    }
    if (avfilter_link(last, 0, tempo->sink, 0) < 0 || avfilter_graph_config(tempo->graph, NULL) < 0) goto fail;
    return tempo;
fail:
    tube_tempo_destroy(tempo);
    return NULL;
}

static int drain(tube_audio_tempo *tempo, tube_tempo_output output, void *user, int finishing) {
    for (;;) {
        int rc = av_buffersink_get_frame(tempo->sink, tempo->frame);
        if (rc == AVERROR_EOF) return 0;
        if (rc == AVERROR(EAGAIN) && !finishing) return 0;
        if (rc < 0) { tempo->terminal = rc; return rc; }
        AVFrame *frame = tempo->frame;
        if (frame->format != AV_SAMPLE_FMT_S16 || frame->sample_rate != 48000 ||
            frame->ch_layout.nb_channels != tempo->channels || frame->nb_samples <= 0) {
            av_frame_unref(frame); tempo->terminal = AVERROR_INVALIDDATA; return tempo->terminal;
        }
        rc = output(user, (const int16_t *)frame->data[0], frame->nb_samples);
        av_frame_unref(frame);
        if (rc < 0) { tempo->terminal = rc; return rc; }
    }
}

int tube_tempo_feed(tube_audio_tempo *tempo, const int16_t *pcm, int samples,
                    tube_tempo_output output, void *user) {
    if (!tempo || !output || samples < 0 || samples > 65536 || (samples && !pcm)) return AVERROR(EINVAL);
    if (tempo->terminal) return tempo->terminal < 0 ? tempo->terminal : AVERROR_EOF;
    if (!samples) return 0;
    if (tempo->input_samples > INT64_MAX - samples) return AVERROR(EOVERFLOW);
    if (!tempo->graph) {
        int rc = output(user, pcm, samples);
        if (rc < 0) tempo->terminal = rc;
        else tempo->input_samples += samples;
        return rc < 0 ? rc : 0;
    }
    AVFrame *frame = tempo->frame;
    frame->format = AV_SAMPLE_FMT_S16;
    frame->sample_rate = 48000;
    av_channel_layout_default(&frame->ch_layout, tempo->channels);
    frame->nb_samples = samples;
    frame->pts = tempo->input_samples;
    int rc = av_frame_get_buffer(frame, 0);
    if (rc >= 0) {
        memcpy(frame->data[0], pcm, (size_t)samples * tempo->channels * sizeof *pcm);
        rc = av_buffersrc_add_frame_flags(tempo->source, frame, 0);
    }
    av_frame_unref(frame);
    if (rc < 0) { tempo->terminal = rc; return rc; }
    tempo->input_samples += samples;
    return drain(tempo, output, user, 0);
}

int tube_tempo_finish(tube_audio_tempo *tempo, tube_tempo_output output, void *user) {
    if (!tempo || !output) return AVERROR(EINVAL);
    if (tempo->terminal) return tempo->terminal < 0 ? tempo->terminal : 0;
    if (tempo->graph) {
        int rc = av_buffersrc_add_frame_flags(tempo->source, NULL, 0);
        if (rc >= 0) rc = drain(tempo, output, user, 1);
        if (rc < 0) { tempo->terminal = rc; return rc; }
    }
    tempo->terminal = 1;
    return 0;
}
