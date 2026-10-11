/* Centered video-quad sizing, shared by native and FFmpeg PS5 decode paths. */
#ifndef TUBE_VIDEO_GEOMETRY_H
#define TUBE_VIDEO_GEOMETRY_H
#include <stdio.h>
#include <string.h>
#define TUBE_PIXEL_MASK (7u<<17)
static const float tube_aspect_values[]={0,1,1.33f,1.25f,1.77f,1.6f,2.33f,2.37f,2.21f,2.35f,2.39f};
static const char* const tube_aspect_labels[]={"Original","1:1","4:3","5:4","16:9","16:10","21:9","64:27","2.21:1","2.35:1","2.39:1"};
static const float tube_pixel_values[]={1,1.11111f,1.3333f,.75f,.7442f,.7407f};
static const char* const tube_pixel_labels[]={"1:1 (16:9 display)","1.11111:1 (16:10 display)","1.3333:1 (4:3 display)","0.75:1 (64:27 display)","0.7442:1 (43:18 display)","0.7407:1 (12:5 display)"};
static const char* const tube_pixel_short[]={"1","1.11111","1.3333","0.75","0.7442","0.7407"};
static inline int tube_zoom_valid(int percent) {
    return (percent>=50 && percent<=95 && percent%5==0) ||
        (percent>=96 && percent<=100) || (percent>=105 && percent<=300 && percent%5==0);
}
/* Bit 0 enables the YouTube transform; bits 1..3 resize, 4..12 zoom, 13..16 aspect, 17..19 pixel ratio. */
static inline void tube_video_geometry_scale(unsigned config,int vw,int vh,int sw,int sh,float* sx,float* sy) {
    if(!(config&1) || !sx || !sy || vw<=0 || vh<=0 || sw<=0 || sh<=0 ||
       vw>16384 || vh>16384 || sw>16384 || sh>16384)return;
    int mode=(config>>1)&7,zoom=(config>>4)&511,aspect=(config>>13)&15,pixel=(config>>17)&7;
    if(aspect>10 || pixel>5 || mode>4 || (zoom && (!tube_zoom_valid(zoom) || mode)))return;
    float va=(aspect?tube_aspect_values[aspect]:(float)vw/vh)*tube_pixel_values[pixel],sa=(float)sw/sh;
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
    int mode=(config&1)?(int)((config>>1)&7):fallback,zoom=(config&1)?(int)((config>>4)&511):0;
    if(zoom && tube_zoom_valid(zoom))snprintf(out,size,"Zoom %d%%",zoom);
    else snprintf(out,size,"%s",labels[mode>=0 && mode<=4?mode:0]);
    int aspect=(config&1)?(int)((config>>13)&15):0,pixel=(config&1)?(int)((config>>17)&7):0;
    size_t used=strlen(out);
    if(aspect>0 && aspect<=10 && used<size)snprintf(out+used,size-used," / %s",tube_aspect_labels[aspect]);
    used=strlen(out);
    if(pixel>0 && pixel<=5 && used<size)snprintf(out+used,size-used," / PAR %s",tube_pixel_short[pixel]);
}
#endif
