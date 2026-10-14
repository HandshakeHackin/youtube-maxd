/* Missing extension: a live source stays non-seekable with no SDK mutation. */
#include "tube_live_runtime.h"
#include <libavformat/avformat.h>
#include <libavutil/mem.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
int tube_live_owner_current(unsigned token) {return token==99;}
int64_t tube_live_restore(const tube_live_owner *owner,const int64_t window[7],const int64_t identity[3]) {(void)owner;(void)window;(void)identity;assert(0);return 0;}
int tube_live_remember(const tube_live_owner *owner,const int64_t window[7],int64_t utc,const int64_t identity[3]) {(void)owner;(void)window;(void)utc;(void)identity;assert(0);return 0;}
int main(void) {
    AVFormatContext *f=avformat_alloc_context();assert(f);
    f->iformat=av_find_input_format("hls");assert(f->iformat);
    f->url=av_strdup("http://127.0.0.1/media/session/h0.m3u8");assert(f->url);
    tube_live_owner owner={0};owner.token=99;snprintf(owner.url,sizeof owner.url,"%s",f->url);
    tube_live_context_open(f,&owner,f->url);
    double base,restore,start=123,end=456;
    assert(tube_live_context_matches(f));assert(!tube_live_context_calibrate(f,&base,&restore));
    assert(base==0 && restore==-1 && !tube_live_range(f,&start,&end) && start==123 && end==456);
    assert(!tube_live_seek_admit(f,3) && !tube_live_seek_prepare(f,3));
    tube_live_remember_position(f,3);
    tube_live_context_release(f);assert(!tube_live_context_matches(f));
    avformat_free_context(f);
    puts("PASS: missing HLS extension preserves non-seekable live behavior");return 0;
}
