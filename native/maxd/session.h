/* Bounded subset sum: closest session at or below the budget, stable feed order.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MAXD_SESSION_H
#define MAXD_SESSION_H
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#define MAXD_SESSION_SECONDS 7200
static inline int maxd_session_pick(const int64_t *durations,int count,int budget,unsigned char *chosen) {
    if(!durations || !chosen || count<0 || count>128 || budget<1 || budget>MAXD_SESSION_SECONDS)return -1;
    memset(chosen,0,(size_t)count);
    int16_t *item=malloc((size_t)(budget+1)*sizeof *item);
    int16_t *previous=malloc((size_t)(budget+1)*sizeof *previous);
    if(!item || !previous){free(item);free(previous);return -1;}
    for(int s=0;s<=budget;++s)item[s]=-1;
    item[0]=-2;
    for(int i=0;i<count;++i) {
        if(durations[i]<1 || durations[i]>budget)continue;
        int duration=(int)durations[i];
        for(int s=budget;s>=duration;--s)if(item[s]==-1 && item[s-duration]!=-1) {
            item[s]=(int16_t)i;previous[s]=(int16_t)(s-duration);
        }
    }
    int total=budget;while(total && item[total]==-1)--total;
    for(int s=total;s>0;s=previous[s])chosen[item[s]]=1;
    free(item);free(previous);return total;
}
#endif
