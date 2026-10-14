/* Live demux binding: source authority and the checked moving window.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_LIVE_RUNTIME_H
#define TUBE_LIVE_RUNTIME_H
#include "tube_provider.h"
#include "tube_hls_dvr.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Opening worker, before find_stream_info; calibrated after it. */
void tube_live_context_open(struct AVFormatContext *, const tube_live_owner *, const char *url);
int tube_live_context_calibrate(struct AVFormatContext *, double *initial_base, double *restore);
/* Close only after the demux owner stops; waits for snapshot readers. */
void tube_live_context_release(struct AVFormatContext *);
int tube_live_context_matches(struct AVFormatContext *);
int tube_live_range(struct AVFormatContext *, double *start, double *end);
/* UI snapshot without a borrowed demux pointer. */
int tube_live_active_range(double *start, double *end);
/* Demux owner only, when paused/scrubbing: at most one bounded metadata
 * request per two seconds, including while a packet queue is full. */
void tube_live_refresh_paused(struct AVFormatContext *);
/* Admission is a nonblocking snapshot check. Prepare is demux-owner only and
 * refreshes the upstream window before any playback clock/queue mutation. */
int tube_live_seek_admit(struct AVFormatContext *, double target);
int tube_live_seek_prepare(struct AVFormatContext *, double target);
/* UI thread: retain the actual played point; existing progress/stop flushes. */
void tube_live_remember_position(struct AVFormatContext *, double position);
#ifdef __cplusplus
}
#endif
#endif
