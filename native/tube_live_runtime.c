/* Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_live_runtime.h"
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <math.h>
#include <pthread.h>
#include <string.h>
/* Old SDKs keep unsupported live streams at the edge until the checked
 * extension is built. Weak imports never cause a speculative gate lift. */
int av_hls_get_dvr_window(AVFormatContext *, int64_t out[7]) __attribute__((weak));
int av_hls_dvr_prepare_seek(AVFormatContext *, int64_t) __attribute__((weak));
int av_hls_dvr_point_identity(AVFormatContext *, int64_t, int64_t out[3]) __attribute__((weak));
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static AVFormatContext *context;
static tube_live_owner binding;
static int enabled, calibrated;
static int64_t origin;
static int snapshot(AVFormatContext *f, int64_t window[7]) {
    return f && f == context && enabled && tube_live_owner_current(binding.token) &&
        av_hls_get_dvr_window && av_hls_get_dvr_window(f,window);
}
void tube_live_context_open(AVFormatContext *f, const tube_live_owner *owner, const char *url) {
    pthread_mutex_lock(&mutex);
    context=f;enabled=calibrated=0;origin=0;memset(&binding,0,sizeof binding);
    if (f && owner && url && f->url && !strcmp(f->url,url) && !strcmp(owner->url,url) &&
        tube_live_owner_current(owner->token) && av_hls_get_dvr_window && av_hls_dvr_prepare_seek && av_hls_dvr_point_identity &&
        f->iformat && !strcmp(f->iformat->name,"hls") &&
        av_opt_set_int(f->priv_data,"prosperotube_dvr",1,0)==0) {binding=*owner;enabled=1;}
    pthread_mutex_unlock(&mutex);
}
int tube_live_context_calibrate(AVFormatContext *f, double *initial_base, double *restore) {
    int ok=0;int64_t window[7];
    if (!initial_base || !restore) return 0;
    *initial_base=0;*restore=-1;
    pthread_mutex_lock(&mutex);
    if (!calibrated && snapshot(f,window) && f->start_time != AV_NOPTS_VALUE &&
        f->start_time >= window[3] - AV_TIME_BASE && f->start_time < window[4]) {
        origin=window[3];
        *initial_base=fmax(0.0,(f->start_time-origin)/(double)AV_TIME_BASE);
        int64_t identity[3],saved=0;
        if (binding.saved_utc && av_hls_dvr_point_identity(f,binding.saved_utc,identity)) saved=tube_live_restore(&binding,window,identity);
        if (saved) *restore=(saved-window[5])/(double)AV_TIME_BASE;
        f->start_time=origin;calibrated=1;ok=1;
    }
    pthread_mutex_unlock(&mutex);return ok;
}
void tube_live_context_release(AVFormatContext *f) {
    pthread_mutex_lock(&mutex);
    if (f==context) {context=NULL;enabled=calibrated=0;origin=0;memset(&binding,0,sizeof binding);}
    pthread_mutex_unlock(&mutex);
}
int tube_live_context_matches(AVFormatContext *f) {
    pthread_mutex_lock(&mutex);int result=f && f==context;pthread_mutex_unlock(&mutex);return result;
}
int tube_live_range(AVFormatContext *f, double *start, double *end) {
    int ok=0;int64_t window[7];
    pthread_mutex_lock(&mutex);
    if (calibrated && snapshot(f,window) && window[3]>=origin) {
        if(start)*start=(window[3]-origin)/(double)AV_TIME_BASE;
        if(end)*end=(window[4]-origin)/(double)AV_TIME_BASE;
        ok=1;
    }
    pthread_mutex_unlock(&mutex);return ok;
}
int tube_live_seek_admit(AVFormatContext *f, double target) {
    double start,end;
    if (!isfinite(target) || target<0 || target>1000000.0) return 0;
    if (!tube_live_context_matches(f)) return 1;
    return tube_live_range(f,&start,&end) && target>=start && target<end;
}
int tube_live_seek_prepare(AVFormatContext *f, double target) {
    unsigned token;int64_t absolute;
    if (!isfinite(target) || target<0 || target>1000000.0) return 0;
    pthread_mutex_lock(&mutex);
    if (!f || f!=context) {pthread_mutex_unlock(&mutex);return 1;}
    if (!enabled || !calibrated || !tube_live_owner_current(binding.token)) {pthread_mutex_unlock(&mutex);return 0;}
    token=binding.token;absolute=origin+(int64_t)llround(target*AV_TIME_BASE);
    pthread_mutex_unlock(&mutex);
    /* The demux owner keeps f alive while this bounded network operation runs.
     * Do not hold the snapshot mutex across it: the UI must remain responsive. */
    return tube_live_owner_current(token) && av_hls_dvr_prepare_seek &&
        av_hls_dvr_prepare_seek(f,absolute) && tube_live_owner_current(token);
}
void tube_live_remember_position(AVFormatContext *f, double position) {
    tube_live_owner owner;int64_t window[7],identity[3],utc=0;
    if (!isfinite(position) || position<0 || position>1000000.0) return;
    pthread_mutex_lock(&mutex);
    if (calibrated && snapshot(f,window)) {
        int64_t pts=origin+(int64_t)llround(position*AV_TIME_BASE);
        if (pts>=window[3] && pts<window[4]) {
            utc=window[5]+pts-window[3];
            if (av_hls_dvr_point_identity(f,utc,identity)) owner=binding;else utc=0;
        }
    }
    pthread_mutex_unlock(&mutex);
    if (utc) tube_live_remember(&owner,window,utc,identity);
}
