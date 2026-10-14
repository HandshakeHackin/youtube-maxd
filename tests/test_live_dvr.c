/* Live-HLS DVR acceptance: exercise the pinned demuxer, not a playlist parser.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
static int64_t probe_deadline;
static int interrupted(void *unused) {(void)unused;return av_gettime_relative()>=probe_deadline;}
#ifdef TUBE_DVR_RUNTIME
#include "tube_live_runtime.h"
static unsigned owner_token=77;
static int64_t remembered;
int tube_live_owner_current(unsigned token) { return token && token==owner_token; }
int64_t tube_live_restore(const tube_live_owner *owner,const int64_t window[7],const int64_t identity[3]) {
    return identity && identity[0]==owner->saved_sequence && identity[1]==owner->saved_segment_utc && identity[2]==owner->saved_duration_us &&
        tube_live_owner_current(owner->token) && owner->saved_utc>=window[5] &&
        owner->saved_utc-window[5]<window[4]-window[3] ? owner->saved_utc : 0;
}
int tube_live_remember(const tube_live_owner *owner,const int64_t window[7],int64_t utc,const int64_t identity[3]) {
    assert(tube_live_owner_current(owner->token) && !strcmp(owner->profile,"guest") && !strcmp(owner->video,"abcdefghijk"));
    assert(identity && identity[0]==3 && identity[1]==1760000006000000LL && identity[2]==2000000);
    assert(utc>=window[5] && utc-window[5]<window[4]-window[3]);remembered=utc;return 1;
}
#endif
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
            int64_t identity[3];
            if (av_hls_dvr_point_identity(opaque,window[5]+2000000,identity))
                assert(identity[0]==window[1]+1 && identity[1]==window[5]+2000000 && identity[2]==2000000);
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
    AVFormatContext *format = avformat_alloc_context();assert(format);
    probe_deadline=av_gettime_relative()+(!strcmp(argv[3],"split-runtime")?10000000:3000000);
    format->interrupt_callback=(AVIOInterruptCB){interrupted,NULL};
    AVDictionary *options = NULL;
    /* Fix the initial live segment so a seek-before-open-origin has a known,
     * independently generated earlier segment in the same upstream window. */
    av_dict_set(&options, "live_start_index", "-2", 0);
#ifdef TUBE_DVR_CANDIDATE
    if (strcmp(argv[3],"runtime") && strcmp(argv[3],"runtime-shifted")) av_dict_set(&options, "prosperotube_dvr", "1", 0);
#endif
    av_dict_set(&options, "rw_timeout", "3000000", 0);
    int opened = avformat_open_input(&format, argv[1], NULL, &options);
    av_dict_free(&options);
    if (opened < 0) {
        printf("{\"opened\":false,\"error\":%d}\n", opened);
        return 0;
    }
#ifdef TUBE_DVR_RUNTIME
    tube_live_owner owner={0};
    if (!strcmp(argv[3],"runtime") || !strcmp(argv[3],"runtime-shifted") || !strcmp(argv[3],"split-runtime")) {
        owner.token=owner_token;owner.saved_utc=1760000006000000LL;owner.saved_sequence=3;owner.saved_segment_utc=1760000006000000LL;owner.saved_duration_us=2000000;
        snprintf(owner.profile,sizeof owner.profile,"guest");snprintf(owner.video,sizeof owner.video,"abcdefghijk");
        snprintf(owner.url,sizeof owner.url,"%s",argv[1]);tube_live_context_open(format,&owner,argv[1]);
    }
#endif
    int stream_info=avformat_find_stream_info(format,NULL);
    if (stream_info<0) {
        printf("{\"opened\":false,\"stage\":\"stream_info\",\"error\":%d}\n",stream_info);
        avformat_close_input(&format);return 0;
    }
#ifdef TUBE_DVR_RUNTIME
    if (!strcmp(argv[3],"runtime") || !strcmp(argv[3],"runtime-shifted") || !strcmp(argv[3],"split-runtime")) {
        double base,restore;
        assert(tube_live_context_calibrate(format,&base,&restore));
        assert(base>3.9 && base<4.1 && fabs(restore-6.0)<.001);
    }
#endif
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
#ifdef TUBE_DVR_CANDIDATE
    if (!strncmp(argv[3], "split", 5)) {
        int64_t split_window[7];
        assert(av_hls_get_dvr_window(format, split_window));
        target = (split_window[3] + AV_TIME_BASE / 4) / (double)AV_TIME_BASE;
    }
#endif
    int64_t timestamp = llround(target / av_q2d(format->streams[video]->time_base));
    int sought = av_seek_frame(format, video, timestamp, AVSEEK_FLAG_BACKWARD);
    double landed = sought < 0 ? NAN : packet_position(format, video);
    printf("{\"opened\":true,\"version\":%u,\"unseekable\":%s,\"origin\":%.6f,\"first\":%.6f,\"target\":%.6f,\"seek_result\":%d,\"landed\":",
           avformat_version(), unseekable ? "true" : "false", origin, first, target, sought);
    if (isfinite(landed)) printf("%.6f", landed); else fputs("null", stdout);
    fputs("}", stdout);
#ifdef TUBE_DVR_CANDIDATE
    if (!strcmp(argv[3], "split") || !strcmp(argv[3], "split-invalidate-audio") || !strcmp(argv[3], "split-runtime")) {
        int64_t window[7];
        int audio = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
        assert(audio >= 0 && !unseekable && sought >= 0 && av_hls_get_dvr_window(format, window));
        assert(window[4] - window[3] == 8 * AV_TIME_BASE);
        assert(av_seek_frame(format, video, timestamp, AVSEEK_FLAG_BACKWARD) >= 0);
        AVPacket *packet = av_packet_alloc();
        double video_pts = NAN, audio_pts = NAN;
        for (int i = 0; i < 500 && (!isfinite(video_pts) || !isfinite(audio_pts)); ++i) {
            assert(av_read_frame(format, packet) >= 0);
            if (packet->pts != AV_NOPTS_VALUE) {
                double pts = packet->pts * av_q2d(format->streams[packet->stream_index]->time_base);
                if (packet->stream_index == video && !isfinite(video_pts)) video_pts = pts;
                if (packet->stream_index == audio && !isfinite(audio_pts)) audio_pts = pts;
            }
            av_packet_unref(packet);
        }
        av_packet_free(&packet);
        assert(isfinite(video_pts) && isfinite(audio_pts) && fabs(video_pts - audio_pts) < .5);
        if (!strcmp(argv[3], "split-invalidate-audio")) {
            char control[1024];
            snprintf(control, sizeof(control), "%.*s/invalidate-audio-time",
                     (int)(strrchr(argv[1], '/') - argv[1]), argv[1]);
            AVIOContext *io = NULL; unsigned char byte;
            assert(avio_open2(&io, control, AVIO_FLAG_READ, NULL, NULL) >= 0);
            assert(avio_read(io, &byte, 1) == 1); avio_closep(&io);
            assert(!av_hls_dvr_prepare_seek(format, llround(target * AV_TIME_BASE)));
            assert(!av_hls_get_dvr_window(format, window));
        }
        printf("\n{\"split_audio_video_calibrated\":true,\"audio_shift_refused\":%s}",
               !strcmp(argv[3], "split-invalidate-audio") ? "true" : "false");
    }
#endif
#ifdef TUBE_DVR_RUNTIME
    if (!strcmp(argv[3],"split-runtime")) {
        int64_t window[7];
        assert(av_hls_get_dvr_window(format,window));
        int64_t played=window[3]+6*AV_TIME_BASE;
        int stream=av_find_best_stream(format,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0);
        assert(tube_live_seek_prepare(format,6.0));
        assert(av_seek_frame(format,stream,av_rescale_q(played,AV_TIME_BASE_Q,format->streams[stream]->time_base),AVSEEK_FLAG_BACKWARD)>=0);
        assert(fabs(packet_position(format,stream)-played/(double)AV_TIME_BASE)<.2);
        tube_live_remember_position(format,6.0);assert(remembered==1760000006000000LL);
        char control[1024];AVIOContext *io=NULL;unsigned char byte;
        snprintf(control,sizeof(control),"%.*s/advance",(int)(strrchr(argv[1],'/')-argv[1]),argv[1]);
        assert(avio_open2(&io,control,AVIO_FLAG_READ,NULL,NULL)>=0 && avio_read(io,&byte,1)==1);avio_closep(&io);
        tube_live_context_release(format);avformat_close_input(&format);
        av_dict_set(&options,"live_start_index","-2",0);av_dict_set(&options,"prosperotube_dvr","1",0);
        assert(avformat_open_input(&format,argv[1],NULL,&options)>=0);av_dict_free(&options);
        tube_live_context_open(format,&owner,argv[1]);assert(avformat_find_stream_info(format,NULL)>=0);
        double base,restore;
        assert(tube_live_context_calibrate(format,&base,&restore) && base>3.9 && base<4.1 && fabs(restore-2.0)<.001);
        assert(tube_live_range(format,NULL,NULL));
        stream=av_find_best_stream(format,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0);
        played=format->start_time+2*AV_TIME_BASE;
        assert(av_seek_frame(format,stream,av_rescale_q(played,AV_TIME_BASE_Q,format->streams[stream]->time_base),AVSEEK_FLAG_BACKWARD)>=0);
        assert(fabs(packet_position(format,stream)-played/(double)AV_TIME_BASE)<.2);
        snprintf(control,sizeof(control),"%.*s/evict",(int)(strrchr(argv[1],'/')-argv[1]),argv[1]);
        assert(avio_open2(&io,control,AVIO_FLAG_READ,NULL,NULL)>=0 && avio_read(io,&byte,1)==1);avio_closep(&io);
        tube_live_context_release(format);avformat_close_input(&format);
        av_dict_set(&options,"live_start_index","-2",0);av_dict_set(&options,"prosperotube_dvr","1",0);
        assert(avformat_open_input(&format,argv[1],NULL,&options)>=0);av_dict_free(&options);
        tube_live_context_open(format,&owner,argv[1]);assert(avformat_find_stream_info(format,NULL)>=0);
        assert(tube_live_context_calibrate(format,&base,&restore) && restore<0);
        tube_live_context_release(format);
        printf("\n{\"same_point_restored\":true,\"evicted_point_refused\":true}");
    }
    if (!strcmp(argv[3],"runtime-shifted")) {
        char control[1024];
        snprintf(control,sizeof(control),"%.*s/invalidate-small-time",(int)(strrchr(argv[1],'/')-argv[1]),argv[1]);
        AVIOContext *io=NULL;unsigned char byte;
        assert(avio_open2(&io,control,AVIO_FLAG_READ,NULL,NULL)>=0 && avio_read(io,&byte,1)==1);avio_closep(&io);
        tube_live_context_release(format);avformat_close_input(&format);
        av_dict_set(&options,"live_start_index","-2",0);
        assert(avformat_open_input(&format,argv[1],NULL,&options)>=0);av_dict_free(&options);
        tube_live_owner owner={0};owner.token=owner_token;owner.saved_utc=1760000006000000LL;
        owner.saved_sequence=3;owner.saved_segment_utc=1760000006000000LL;owner.saved_duration_us=2000000;
        snprintf(owner.profile,sizeof owner.profile,"guest");snprintf(owner.video,sizeof owner.video,"abcdefghijk");snprintf(owner.url,sizeof owner.url,"%s",argv[1]);
        tube_live_context_open(format,&owner,argv[1]);assert(avformat_find_stream_info(format,NULL)>=0);
        double base,restore;
        assert(tube_live_context_calibrate(format,&base,&restore) && restore==-1 && base>3.9 && base<4.1);
        tube_live_context_release(format);
        printf("\n{\"contained_UTC_with_changed_segment_identity_refused\":true}");
    }
    if (!strcmp(argv[3],"runtime")) {
        double start,end;
        assert(tube_live_context_matches(format) && tube_live_range(format,&start,&end));
        assert(start==0 && end==8);
        assert(!tube_live_seek_admit(format,-1) && !tube_live_seek_admit(format,end));
        assert(tube_live_seek_admit(format,6) && tube_live_seek_prepare(format,6));
        tube_live_remember_position(format,6);assert(remembered==1760000006000000LL);
        char control[1024];
        snprintf(control, sizeof(control), "%.*s/advance", (int)(strrchr(argv[1], '/') - argv[1]), argv[1]);
        AVIOContext *io=NULL;unsigned char byte;
        assert(avio_open2(&io,control,AVIO_FLAG_READ,NULL,NULL)>=0 && avio_read(io,&byte,1)==1);avio_closep(&io);
        tube_live_refresh_paused(format);
        assert(tube_live_active_range(&start,&end) && start==4 && end==12);
        /* Repeated queue-cap calls use the same bounded refresh interval. */
        for (int i=0;i<1000;++i) tube_live_refresh_paused(format);
        assert(!tube_live_seek_prepare(format,.25));
        assert(tube_live_range(format,&start,&end) && start==4 && end==12);
        assert(!tube_live_seek_admit(format,.25) && tube_live_seek_admit(format,6));
        owner_token=78;
        assert(!tube_live_range(format,&start,&end) && !tube_live_seek_admit(format,6) && !tube_live_seek_prepare(format,6));
        remembered=0;tube_live_remember_position(format,6);assert(!remembered);
        tube_live_context_release(format);assert(!tube_live_context_matches(format));
        owner_token=77;
        printf("\n{\"runtime_owner_and_fixed_origin\":true,\"preflight_expiry\":true,\"revoked_owner_refused\":true}");
    }
#endif
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
        int64_t before[7], after[7], saved, restored = -1, before_identity[3], after_identity[3];
        assert(av_hls_get_dvr_window(format, before));
        assert(av_hls_dvr_pts_to_wall(format, before[3] + 6 * AV_TIME_BASE, &saved));
        assert(av_hls_dvr_point_identity(format,saved,before_identity));
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
        assert(av_hls_dvr_point_identity(format,saved,after_identity) && !memcmp(before_identity,after_identity,sizeof before_identity));
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
