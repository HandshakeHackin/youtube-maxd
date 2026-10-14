/* ProsperoTube's opt-in FFmpeg HLS DVR extension (not a stock FFmpeg API).
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_HLS_DVR_H
#define TUBE_HLS_DVR_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct AVFormatContext;
enum tube_hls_dvr_field {
    TUBE_HLS_DVR_VALID, TUBE_HLS_DVR_FIRST_SEQUENCE, TUBE_HLS_DVR_END_SEQUENCE,
    TUBE_HLS_DVR_START_PTS_US, TUBE_HLS_DVR_END_PTS_US, TUBE_HLS_DVR_START_UTC_US,
    TUBE_HLS_DVR_DEADLINE_US, TUBE_HLS_DVR_FIELDS
};
/* Enable with the demuxer option prosperotube_dvr=1. Defaults off.
 * Snapshot/UTC conversions may run concurrently with the demux thread, but the
 * caller owns the context lifetime and must join readers before closing it.
 * End bounds are exclusive. No caller may mutate the returned snapshot.
 * A zero result means unsupported, expired or invalidated; conversion outputs
 * remain untouched. The snapshot output is cleared on invalid/expired state.
 * A saved UTC point belongs to its video/profile, which the caller must verify;
 * a timestamp by itself grants no authority to restore into another source. */
int av_hls_get_dvr_window(struct AVFormatContext *context, int64_t out[TUBE_HLS_DVR_FIELDS]);
/* Demux owner only: refresh and validate before pausing/flushing. */
int av_hls_dvr_prepare_seek(struct AVFormatContext *context, int64_t pts_us);
/* Exact segment sequence, program UTC start and duration for a saved point. */
int av_hls_dvr_point_identity(struct AVFormatContext *context, int64_t utc_us, int64_t out[3]);
int av_hls_dvr_pts_to_wall(struct AVFormatContext *context, int64_t pts_us, int64_t *utc_us);
int av_hls_dvr_wall_to_pts(struct AVFormatContext *context, int64_t utc_us, int64_t *pts_us);
#ifdef __cplusplus
}
#endif
#endif
