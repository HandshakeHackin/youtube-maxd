/* Main-thread video destination in the interface's 1920 x 1080 canvas.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_VIDEO_DESTINATION_H
#define TUBE_VIDEO_DESTINATION_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct tube_video_destination { int x,y,w,h; } tube_video_destination;
void tube_video_destination_reset(void);
void tube_video_destination_set(float x,float y,float w,float h);
void tube_video_destination_get(int width,int height,tube_video_destination *out);
void tube_afterglow_video_prepare(int eligible,float dt);
#ifdef __cplusplus
}
#endif
#endif
