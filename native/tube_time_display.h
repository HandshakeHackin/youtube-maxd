/* Speed-corrected player time labels, shared by both PS5 HUD renderers. */
#ifndef TUBE_TIME_DISPLAY_H
#define TUBE_TIME_DISPLAY_H
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static inline void tube_duration_text(char *out, size_t size, double seconds) {
    if (!out || !size) return;
    if (!isfinite(seconds) || seconds < 0 || seconds > 4000000000.0) {
        snprintf(out,size,"--:--"); return;
    }
    int64_t value=(int64_t)seconds;
    if(value>=3600) snprintf(out,size,"%lld:%02lld:%02lld",(long long)(value/3600),
        (long long)((value/60)%60),(long long)(value%60));
    else snprintf(out,size,"%02lld:%02lld",(long long)(value/60),(long long)(value%60));
}
static inline void tube_time_display(char *out, size_t size, int mode, double position,
                                    double duration, double rate, int live, time_t now) {
    if (!out || !size) return;
    if (live) { snprintf(out,size,"LIVE"); return; }
    if (!isfinite(duration) || duration<=0 || duration>1000000000.0) {
        snprintf(out,size,"--:--"); return;
    }
    char total[40];tube_duration_text(total,sizeof total,duration);
    if(mode==0) { snprintf(out,size,"%s",total);return; }
    if(!isfinite(position) || position<0)position=0;
    if(position>duration)position=duration;
    if(!isfinite(rate) || rate<.25 || rate>4)rate=1;
    double remaining=ceil((duration-position)/rate);
    if(mode==2) {
        long double end=(long double)now+remaining;
        const long double max_time=sizeof(time_t)>=8 ? (long double)INT64_MAX : (long double)INT32_MAX;
        if(now>=0 && end<=max_time) {
            time_t finish=(time_t)end;struct tm local;
            if(localtime_r(&finish,&local)) {
                snprintf(out,size,"Ends %02d:%02d / %s",local.tm_hour,local.tm_min,total);return;
            }
        }
    }
    char left[40];tube_duration_text(left,sizeof left,remaining);
    snprintf(out,size,"-%s / %s",left,total);
}
#endif
