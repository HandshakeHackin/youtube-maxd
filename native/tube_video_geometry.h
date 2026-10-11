/* Centered video-quad sizing, shared by native and FFmpeg PS5 decode paths. */
#ifndef TUBE_VIDEO_GEOMETRY_H
#define TUBE_VIDEO_GEOMETRY_H
#include <stdio.h>
static inline int tube_zoom_valid(int percent) {
    return (percent>=50 && percent<=95 && percent%5==0) ||
        (percent>=96 && percent<=100) || (percent>=105 && percent<=300 && percent%5==0);
}
/* Bit 0 enables the YouTube transform; bits 1..3 hold resize mode, 4+ zoom. */
static inline void tube_video_geometry_scale(unsigned config,int vw,int vh,int sw,int sh,float* sx,float* sy) {
    if(!(config&1) || !sx || !sy || vw<=0 || vh<=0 || sw<=0 || sh<=0 ||
       vw>16384 || vh>16384 || sw>16384 || sh>16384)return;
    int mode=(config>>1)&7,zoom=config>>4;
    if(mode>4 || (zoom && (!tube_zoom_valid(zoom) || mode)))return;
    float va=(float)vw/vh,sa=(float)sw/sh;
    *sx=*sy=1;
    if(mode==0) {if(va>sa)*sy=sa/va;else *sx=va/sa;}
    else if(mode==1) {if(va>sa)*sx=va/sa;else *sy=sa/va;}
    else if(mode==3)*sy=sa/va;
    else if(mode==4)*sx=va/sa;
    if(zoom){*sx*=zoom/100.0f;*sy*=zoom/100.0f;}
}
static inline void tube_video_geometry_label(unsigned config,int fallback,char* out,size_t size) {
    const char* labels[]={"Fit","Fill","Stretch","Fit width","Fit height"};
    if(!out || !size)return;
    int mode=(config&1)?(int)((config>>1)&7):fallback,zoom=(config&1)?(int)(config>>4):0;
    if(zoom && tube_zoom_valid(zoom))snprintf(out,size,"Zoom %d%%",zoom);
    else snprintf(out,size,"%s",labels[mode>=0 && mode<=4?mode:0]);
}
#endif
