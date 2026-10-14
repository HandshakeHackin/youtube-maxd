/* Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_video_destination.h"
static float box[4]={0,0,1920,1080};
void tube_video_destination_reset(void) { box[0]=box[1]=0;box[2]=1920;box[3]=1080; }
void tube_video_destination_set(float x,float y,float w,float h) {
    tube_video_destination_reset();
    if (!(x>=0 && y>=0 && w>0 && h>0 && x+w<=1920 && y+h<=1080)) return;
    box[0]=x;box[1]=y;box[2]=w;box[3]=h;
}
void tube_video_destination_get(int width,int height,tube_video_destination *out) {
    if (!out) return;
    *out=(tube_video_destination){0,0,width,height};
    if(width<=0||height<=0||width>16384||height>16384) return;
    const int x=(int)(box[0]*width/1920+0.5f),y=(int)(box[1]*height/1080+0.5f);
    const int right=(int)((box[0]+box[2])*width/1920+0.5f),bottom=(int)((box[1]+box[3])*height/1080+0.5f);
    if(right>x&&bottom>y) *out=(tube_video_destination){x,y,right-x,bottom-y};
}
