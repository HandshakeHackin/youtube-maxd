/* UTF-8 channel-name comparison; no locale or multi-character case expansion.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_CHANNEL_NAME_H
#define TUBE_CHANNEL_NAME_H
#include <stdint.h>
#include <stddef.h>
#include "tube_name_case_data.h"
static inline uint32_t tube_name_point(const unsigned char **text) {
    const unsigned char *p=*text;uint32_t c=*p++;unsigned tails=0,minimum=0;
    if(c>=0xc2 && c<=0xdf){tails=1;c&=31;minimum=0x80;}
    else if(c>=0xe0 && c<=0xef){tails=2;c&=15;minimum=0x800;}
    else if(c>=0xf0 && c<=0xf4){tails=3;c&=7;minimum=0x10000;}
    else if(c>=0x80)return UINT32_MAX;
    for(unsigned i=0;i<tails;++i){if((*p&0xc0)!=0x80)return UINT32_MAX;c=(c<<6)|(*p++&63);}
    if(c<minimum || c<32 || c==127 || c>0x10ffff || (c>=0xd800 && c<=0xdfff))return UINT32_MAX;
    *text=p;return c;
}
static inline uint32_t tube_name_fold(uint32_t point) {
    size_t lo=0,hi=sizeof tube_name_case/sizeof tube_name_case[0];
    while(lo<hi){size_t mid=lo+(hi-lo)/2;if(tube_name_case[mid][0]<point)lo=mid+1;else hi=mid;}
    return lo<sizeof tube_name_case/sizeof tube_name_case[0] && tube_name_case[lo][0]==point?tube_name_case[lo][1]:point;
}
static inline int tube_channel_name_equal(const char *left,const char *right) {
    if(!left || !right || !*left || !*right)return 0;
    const unsigned char *a=(const unsigned char*)left,*b=(const unsigned char*)right;
    while(*a && *b){uint32_t x=tube_name_point(&a),y=tube_name_point(&b);
        if(x==UINT32_MAX || y==UINT32_MAX || tube_name_fold(x)!=tube_name_fold(y))return 0;}
    return !*a && !*b;
}
#endif
