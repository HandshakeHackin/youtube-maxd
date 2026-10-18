/* Bounded playback adaptation. GPL-3.0-or-later. */
#ifndef MAXD_AUTO_MAX_H
#define MAXD_AUTO_MAX_H
#include <stdint.h>
#include <string.h>
typedef struct {
    uint64_t last, frames, dropped, elapsed, buffering, stable, cooldown;
    uint64_t window_frames, window_dropped, warmup;
    unsigned changes;
    int primed;
} MaxdAutoMax;
enum { MAXD_AUTO_NONE, MAXD_AUTO_BUFFER, MAXD_AUTO_DROP, MAXD_AUTO_UP };
typedef struct { int height, fps, codec; int64_t bitrate; } MaxdAutoFormat;
static inline uint64_t maxd_auto_cost(MaxdAutoFormat f) {
    return (uint64_t)f.height*f.height*(f.fps?f.fps:30)*(f.codec==3?4:1);
}
static inline int maxd_auto_rank(MaxdAutoFormat f) {
    return f.height*10000+f.fps*10+(f.codec==3?0:1000);
}
static inline int maxd_auto_pick(const MaxdAutoFormat *formats,int count,int current,int reason,
                                 int height,int fps,int codec,uint64_t failed_cost,int failed_height) {
    if(!formats || count<1 || count>16 || current<0 || current>=count)return -1;
    int best=-1;MaxdAutoFormat old=formats[current];
    for(int i=0;i<count;++i) {
        MaxdAutoFormat f=formats[i];
        if(i==current || f.height<144 || f.height>height || f.fps<0 || f.fps>120 ||
           (fps && f.fps>fps) || f.codec<1 || f.codec>4 || (codec && f.codec!=codec) ||
           (failed_cost && maxd_auto_cost(f)>=failed_cost) || (failed_height && f.height>=failed_height))continue;
        if(reason==MAXD_AUTO_UP ? maxd_auto_rank(f)<=maxd_auto_rank(old) :
           reason==MAXD_AUTO_DROP ? maxd_auto_cost(f)>=maxd_auto_cost(old) :
           !(f.height<old.height || (f.height==old.height && f.fps<old.fps) ||
             (f.bitrate>0 && old.bitrate>0 && f.bitrate<old.bitrate*7/10)))continue;
        if(best<0 || maxd_auto_rank(f)>maxd_auto_rank(formats[best]) ||
           (maxd_auto_rank(f)==maxd_auto_rank(formats[best]) && f.bitrate>0 && formats[best].bitrate>f.bitrate))best=i;
    }
    return best;
}
static inline void maxd_auto_reset(MaxdAutoMax *s) { memset(s,0,sizeof *s);s->warmup=10000; }
/* Suppressed time (pause, seek, live, cast, manual pin) never counts as a
 * performance sample. Counter resets and stalls in the app loop are discarded. */
static inline int maxd_auto_tick(MaxdAutoMax *s, uint64_t now, uint64_t frames,
                                uint64_t dropped, int suppressed, int buffering) {
    if (suppressed || !s->primed || now<s->last || now-s->last>1500 || frames<s->frames || dropped<s->dropped) {
        s->last=now; s->frames=frames; s->dropped=dropped;
        s->elapsed=s->buffering=s->stable=s->window_frames=s->window_dropped=0; s->primed=!suppressed;
        s->warmup=10000;
        return MAXD_AUTO_NONE;
    }
    const uint64_t dt=now-s->last, df=frames-s->frames, dd=dropped-s->dropped;
    s->last=now; s->frames=frames; s->dropped=dropped;
    if(s->warmup) { s->warmup=dt<s->warmup?s->warmup-dt:0;return MAXD_AUTO_NONE; }
    s->elapsed+=dt; s->buffering+=buffering?dt:0;
    s->window_frames+=df;s->window_dropped+=dd;
    /* Accumulate deltas over eight seconds, rather than measuring one frame. */
    static const uint64_t window_ms=8000;
    if (buffering || (df && dd*100>df*2)) s->stable=0;
    else if (df) s->stable+=dt;
    if (s->cooldown) { s->cooldown=dt<s->cooldown?s->cooldown-dt:0; s->elapsed=s->buffering=s->window_frames=s->window_dropped=0; return MAXD_AUTO_NONE; }
    if (s->changes>=6) return MAXD_AUTO_NONE;
    if (s->buffering>=3000) return MAXD_AUTO_BUFFER;
    if (s->elapsed>=window_ms) {
        int drop=s->window_frames>=48 && s->window_dropped*100>=s->window_frames*8;
        s->elapsed=s->buffering=s->window_frames=s->window_dropped=0;
        if(drop)return MAXD_AUTO_DROP;
    }
    if (s->stable>=90000) return MAXD_AUTO_UP;
    return MAXD_AUTO_NONE;
}
static inline void maxd_auto_changed(MaxdAutoMax *s) {
    ++s->changes; s->cooldown=30000; s->elapsed=s->buffering=s->stable=s->window_frames=s->window_dropped=0; s->primed=0;
}
#endif
