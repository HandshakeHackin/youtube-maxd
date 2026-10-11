/* Logical canvas -> physical display mapping for final video passes.
 * Rotation code: clockwise quarter-turns in bits 0..1, mirror-before-rotate in bit 2.
 * Intermediate upscaling/interpolation targets always use code 0.
 */
#ifndef TUBE_VIDEO_TRANSFORM_H
#define TUBE_VIDEO_TRANSFORM_H
static inline void tube_video_transform_canvas(unsigned code,int width,int height,int* logical_w,int* logical_h) {
    *logical_w=(code&1)?height:width;
    *logical_h=(code&1)?width:height;
}
static inline void tube_video_transform_rect(unsigned code,int width,int height,int* x,int* y,int* w,int* h) {
    int lw,lh;tube_video_transform_canvas(code,width,height,&lw,&lh);
    int px=*x,py=*y,pw=*w,ph=*h;
    if(code&4)px=lw-px-pw;
    switch(code&3) {
    case 1:*x=lh-py-ph;*y=px;*w=ph;*h=pw;break;
    case 2:*x=lw-px-pw;*y=lh-py-ph;break;
    case 3:*x=py;*y=lw-px-pw;*w=ph;*h=pw;break;
    default:*x=px;break;
    }
}
#endif
