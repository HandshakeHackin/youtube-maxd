/* Live-HLS DVR acceptance: exercise the pinned demuxer, not a playlist parser.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>

static double packet_position(AVFormatContext *format, int stream) {
    AVPacket *packet = av_packet_alloc();
    assert(packet);
    double result = NAN;
    for (int i = 0; i < 500 && av_read_frame(format, packet) >= 0; ++i) {
        if (packet->stream_index == stream && packet->pts != AV_NOPTS_VALUE) {
            result = packet->pts * av_q2d(format->streams[stream]->time_base);
            av_packet_unref(packet);
            break;
        }
        av_packet_unref(packet);
    }
    av_packet_free(&packet);
    return result;
}
int main(int argc, char **argv) {
    assert(argc == 4);
    assert(avformat_version() == AV_VERSION_INT(61, 7, 100));
    av_log_set_level(AV_LOG_ERROR);
    AVFormatContext *format = NULL;
    AVDictionary *options = NULL;
    /* Fix the initial live segment so a seek-before-open-origin has a known,
     * independently generated earlier segment in the same upstream window. */
    av_dict_set(&options, "live_start_index", "-2", 0);
    av_dict_set(&options, "rw_timeout", "3000000", 0);
    int opened = avformat_open_input(&format, argv[1], NULL, &options);
    av_dict_free(&options);
    if (opened < 0) {
        printf("{\"opened\":false,\"error\":%d}\n", opened);
        return 0;
    }
    assert(avformat_find_stream_info(format, NULL) >= 0);
    int video = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    assert(video >= 0 && !strcmp(format->iformat->name, "hls"));
    double origin = format->start_time == AV_NOPTS_VALUE ? NAN : format->start_time / (double)AV_TIME_BASE;
    double first = packet_position(format, video);
    assert(isfinite(origin) && isfinite(first));
    int unseekable = !!(format->ctx_flags & AVFMTCTX_UNSEEKABLE);
    /* Diagnostic only: demonstrate that bypassing the categorical gate is
     * insufficient. Shipping code must never clear this flag speculatively. */
    if (!strcmp(argv[3], "diagnose-clear-gate")) format->ctx_flags &= ~AVFMTCTX_UNSEEKABLE;
    double target = strtod(argv[2], NULL);
    int64_t timestamp = llround(target / av_q2d(format->streams[video]->time_base));
    int sought = av_seek_frame(format, video, timestamp, AVSEEK_FLAG_BACKWARD);
    double landed = sought < 0 ? NAN : packet_position(format, video);
    printf("{\"opened\":true,\"version\":%u,\"unseekable\":%s,\"origin\":%.6f,\"first\":%.6f,\"target\":%.6f,\"seek_result\":%d,\"landed\":",
           avformat_version(), unseekable ? "true" : "false", origin, first, target, sought);
    if (isfinite(landed)) printf("%.6f", landed); else fputs("null", stdout);
    fputs("}\n", stdout);
    avformat_close_input(&format);
    return 0;
}
