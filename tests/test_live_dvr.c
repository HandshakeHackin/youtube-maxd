/* Live-HLS DVR acceptance: exercise the pinned demuxer, not a playlist parser.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavformat/avformat.h>
#ifdef TUBE_DVR_CANDIDATE
#include <pthread.h>
#include <stdatomic.h>
#include "tube_hls_dvr.h"
#include <libavutil/time.h>
static _Atomic int stop_reader;
static unsigned snapshots;
static int64_t expected_wall_offset;
static void *read_snapshots(void *opaque) {
    int64_t window[7];
    while (!atomic_load(&stop_reader)) {
        if (av_hls_get_dvr_window(opaque, window)) {
            assert(window[2] - window[1] == 4);
            assert(window[4] - window[3] == 8 * AV_TIME_BASE);
            assert(window[5] - window[3] == expected_wall_offset);
            ++snapshots;
        }
    }
    return NULL;
}
#endif
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
#ifdef TUBE_DVR_CANDIDATE
    av_dict_set(&options, "prosperotube_dvr", "1", 0);
#endif
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
    fputs("}", stdout);
#ifdef TUBE_DVR_CANDIDATE
    if (!strcmp(argv[3], "invalidate-gap") || !strcmp(argv[3], "invalidate-time") ||
        !strcmp(argv[3], "invalidate-sequence") || !strcmp(argv[3], "invalidate-jump") ||
        !strcmp(argv[3], "invalidate-expiry")) {
        int64_t window[7];
        assert(av_hls_get_dvr_window(format, window));
        char control[1024];
        snprintf(control, sizeof(control), "%.*s/%s", (int)(strrchr(argv[1], '/') - argv[1]), argv[1], argv[3]);
        AVIOContext *io = NULL;
        assert(avio_open2(&io, control, AVIO_FLAG_READ, NULL, NULL) >= 0);
        unsigned char byte;
        assert(avio_read(io, &byte, 1) == 1);
        avio_closep(&io);
        assert(av_seek_frame(format, video, timestamp, AVSEEK_FLAG_BACKWARD) < 0);
        assert(!av_hls_get_dvr_window(format, window));
        printf("\n{\"invalidated\":true}");
    }
    if (!strcmp(argv[3], "prepare")) {
        int64_t window[7];
        assert(!av_hls_dvr_prepare_seek(NULL, 0));
        assert(!av_hls_dvr_prepare_seek(format, -1));
        assert(!av_hls_dvr_prepare_seek(format, INT64_MAX));
        assert(av_hls_dvr_prepare_seek(format, llround(target * AV_TIME_BASE)));
        char control[1024];
        snprintf(control, sizeof(control), "%.*s/advance", (int)(strrchr(argv[1], '/') - argv[1]), argv[1]);
        AVIOContext *io = NULL;
        assert(avio_open2(&io, control, AVIO_FLAG_READ, NULL, NULL) >= 0);
        unsigned char byte;
        assert(avio_read(io, &byte, 1) == 1);avio_closep(&io);
        assert(!av_hls_dvr_prepare_seek(format, llround(target * AV_TIME_BASE)));
        assert(av_hls_get_dvr_window(format, window));
        assert(av_hls_dvr_prepare_seek(format, window[3] + AV_TIME_BASE / 4));
        printf("\n{\"prepare_rejects_expired_target\":true,\"fresh_target_accepted\":true}");
    }
    if (!strcmp(argv[3], "deadline")) {
        int64_t window[7], unchanged = -1;
        assert(av_hls_get_dvr_window(format, window));
        av_usleep(7000000);
        assert(!av_hls_get_dvr_window(format, window));
        for (int i = 0; i < 7; ++i) assert(window[i] == 0);
        assert(!av_hls_dvr_wall_to_pts(format, 1760000000000000LL, &unchanged) && unchanged == -1);
        printf("\n{\"stale_snapshot_refused\":true}");
    }
    if (!strcmp(argv[3], "transport-reset")) {
        int64_t window[7];
        int retired = !av_hls_get_dvr_window(format, window);
        for (int i = 0; !retired && i < 1000; ++i) {
            if (!isfinite(packet_position(format, video))) break;
            if (!av_hls_get_dvr_window(format, window)) { retired = 1; break; }
        }
        assert(retired);
        printf("\n{\"transport_reset_refused\":true}");
    }
    if (!strcmp(argv[3], "reopen") || !strcmp(argv[3], "reopen-other-timeline")) {
        int64_t before[7], after[7], saved, restored = -1;
        assert(av_hls_get_dvr_window(format, before));
        assert(av_hls_dvr_pts_to_wall(format, before[3] + 6 * AV_TIME_BASE, &saved));
        assert(!av_hls_dvr_pts_to_wall(format, before[3] - 1, &restored) && restored == -1);
        char control[1024];
        snprintf(control, sizeof(control), "%.*s/%s", (int)(strrchr(argv[1], '/') - argv[1]), argv[1], !strcmp(argv[3], "reopen") ? "advance" : "invalidate-time");
        AVIOContext *io = NULL;
        assert(avio_open2(&io, control, AVIO_FLAG_READ, NULL, NULL) >= 0);
        unsigned char byte;
        assert(avio_read(io, &byte, 1) == 1);
        avio_closep(&io);
        avformat_close_input(&format);
        av_dict_set(&options, "prosperotube_dvr", "1", 0);
        av_dict_set(&options, "live_start_index", "-2", 0);
        assert(avformat_open_input(&format, argv[1], NULL, &options) >= 0);
        av_dict_free(&options);
        assert(avformat_find_stream_info(format, NULL) >= 0);
        assert(av_hls_get_dvr_window(format, after));
        if (!strcmp(argv[3], "reopen-other-timeline")) {
            assert(!av_hls_dvr_wall_to_pts(format, saved, &restored) && restored == -1);
            printf("\n{\"different_timeline_refused\":true}");
        } else {
        assert(av_hls_dvr_wall_to_pts(format, saved, &restored));
        assert(restored == before[3] + 6 * AV_TIME_BASE);
        assert(after[4] - restored == before[4] - restored + 4 * AV_TIME_BASE);
        int64_t rejected = -1;
        assert(!av_hls_dvr_wall_to_pts(format, before[5], &rejected) && rejected == -1);
        assert(!av_hls_dvr_wall_to_pts(format, INT64_MAX, &rejected) && rejected == -1);
        video = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        assert(video >= 0);
        int64_t converted = av_rescale_q(restored, AV_TIME_BASE_Q, format->streams[video]->time_base);
        assert(av_seek_frame(format, video, converted, AVSEEK_FLAG_BACKWARD) >= 0);
        assert(fabs(packet_position(format, video) - restored / (double)AV_TIME_BASE) < .2);
        printf("\n{\"reopened_same_point\":true,\"lag_increased_seconds\":4,\"expired_point_refused\":true}");
        }
    }
    if (!strcmp(argv[3], "rollover")) {
        int64_t before[7], after[7];
        assert(av_hls_get_dvr_window(format, before));
        expected_wall_offset = before[5] - before[3];
        pthread_t reader;
        assert(pthread_create(&reader, NULL, read_snapshots, format) == 0);
        char control[1024];
        snprintf(control, sizeof(control), "%.*s/advance", (int)(strrchr(argv[1], '/') - argv[1]), argv[1]);
        AVIOContext *io = NULL;
        assert(avio_open2(&io, control, AVIO_FLAG_READ, NULL, NULL) >= 0);
        unsigned char byte;
        assert(avio_read(io, &byte, 1) == 1);
        avio_closep(&io);
        assert(av_seek_frame(format, video, timestamp, AVSEEK_FLAG_BACKWARD) < 0);
        assert(av_hls_get_dvr_window(format, after));
        assert(after[1] == before[1] + 2 && after[3] == before[3] + 4 * AV_TIME_BASE);
        for (int i = 0; i < 12; ++i) {
            int64_t current = av_rescale_q(after[3] + AV_TIME_BASE / 4, AV_TIME_BASE_Q, format->streams[video]->time_base);
            assert(av_seek_frame(format, video, current, AVSEEK_FLAG_BACKWARD) >= 0);
            assert(fabs(packet_position(format, video) - after[3] / (double)AV_TIME_BASE) < .2);
        }
        atomic_store(&stop_reader, 1);
        assert(pthread_join(reader, NULL) == 0 && snapshots > 0);
        printf("\n{\"rollover\":true,\"concurrent_snapshots\":%u,\"start_before\":%.6f,\"start_after\":%.6f}",
               snapshots, before[3] / (double)AV_TIME_BASE, after[3] / (double)AV_TIME_BASE);
    }
#endif
    fputc('\n', stdout);
    avformat_close_input(&format);
    return 0;
}
