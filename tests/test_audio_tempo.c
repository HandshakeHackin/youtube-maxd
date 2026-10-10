/* Real FFmpeg streaming PCM, duration/pitch/channel and lifecycle checks.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_audio_tempo.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>

typedef struct { int16_t *data; size_t samples, capacity; int channels, calls; } Capture;
static int collect(void *user, const int16_t *data, int samples) {
    Capture *out = user;
    assert(samples > 0 && out->samples + samples <= out->capacity);
    memcpy(out->data + out->samples * out->channels, data,
           (size_t)samples * out->channels * sizeof *data);
    out->samples += samples; ++out->calls;
    return 1; /* Any nonnegative callback result means success. */
}
static int cancel(void *user, const int16_t *data, int samples) {
    (void)data; (void)samples; ++*(int *)user; return AVERROR_EXIT;
}
static double frequency(const Capture *out, int channel) {
    size_t begin=out->samples/4, end=out->samples*3/4, crosses=0;
    for (size_t i=begin+1; i<end; ++i)
        if (out->data[(i-1)*out->channels+channel] <= 0 && out->data[i*out->channels+channel] > 0) ++crosses;
    return (double)crosses * 48000 / (end-begin);
}
static void run(int channels, double rate, int chunk) {
    const int count=48000*4;
    int16_t *input=malloc((size_t)count*channels*sizeof *input);assert(input);
    for (int i=0;i<count;++i) for (int c=0;c<channels;++c)
        input[i*channels+c]=(int16_t)(12000*sin(2*3.141592653589793*(440+110*c)*i/48000));
    Capture out={.capacity=(size_t)count*5,.channels=channels};
    out.data=calloc(out.capacity*channels,sizeof *out.data);assert(out.data);
    tube_audio_tempo *tempo=tube_tempo_create(channels,rate);assert(tempo);
    assert(!tube_tempo_feed(tempo,NULL,0,collect,&out));
    for(int pos=0;pos<count;pos+=chunk) {
        int length=count-pos<chunk?count-pos:chunk;
        assert(!tube_tempo_feed(tempo,input+pos*channels,length,collect,&out));
    }
    assert(!tube_tempo_finish(tempo,collect,&out));
    size_t final=out.samples;int calls=out.calls;
    assert(!tube_tempo_finish(tempo,collect,&out) && out.samples==final && out.calls==calls);
    assert(tube_tempo_feed(tempo,input,1,collect,&out)==AVERROR_EOF);
    double actual=(double)out.samples/48000, expected=4/rate;
    if(fabs(actual-expected)>.10) fprintf(stderr,"duration channels=%d rate=%g actual=%g expected=%g\n",channels,rate,actual,expected);
    assert(fabs(actual-expected)<.10);
    for(int c=0;c<channels;++c) {
        double pitch=frequency(&out,c),expected_pitch=440+110*c;
        if(fabs(pitch-expected_pitch)>expected_pitch*.015) fprintf(stderr,"pitch channels=%d rate=%g channel=%d actual=%g\n",channels,rate,c,pitch);
        assert(fabs(pitch-expected_pitch)<expected_pitch*.015);
    }
    if(rate==1) assert(out.samples==count && !memcmp(input,out.data,(size_t)count*channels*sizeof *input));
    tube_tempo_destroy(tempo);free(out.data);free(input);
}
int main(void) {
    const double rates[]={.25,.5,.75,.8,.85,.9,.95,1,1.05,1.1,1.15,1.2,1.25,1.3,1.35,1.4,1.45,1.5,1.6,1.75,1.8,2,2.25,2.5,2.75,3,3.25,3.5,3.75,4};
    for(size_t i=0;i<sizeof rates/sizeof *rates;++i) run(2,rates[i],i%2?1024:4093);
    for(int ch=1;ch<=8;++ch) { run(ch,.5,137);run(ch,2,65536); }
    assert(!tube_tempo_create(0,1) && !tube_tempo_create(9,1));
    assert(!tube_tempo_create(2,NAN) && !tube_tempo_create(2,INFINITY));
    assert(!tube_tempo_create(2,.24) && !tube_tempo_create(2,4.01));
    int16_t zeros[4096]={0};
    for(int mode=0;mode<2;++mode) {
        tube_audio_tempo *tempo=tube_tempo_create(2,mode?1.5:1);assert(tempo);
        int calls=0,rc=0;
        assert(tube_tempo_feed(tempo,NULL,1,cancel,&calls)==AVERROR(EINVAL));
        assert(tube_tempo_feed(tempo,zeros,65537,cancel,&calls)==AVERROR(EINVAL));
        for(int i=0;i<20 && !rc;++i) rc=tube_tempo_feed(tempo,zeros,2048,cancel,&calls);
        assert(rc==AVERROR_EXIT && calls==1);
        assert(tube_tempo_finish(tempo,cancel,&calls)==AVERROR_EXIT && calls==1);
        assert(tube_tempo_feed(tempo,zeros,1,cancel,&calls)==AVERROR_EXIT && calls==1);
        tube_tempo_destroy(tempo);
    }
    /* A seek discards delayed old-epoch samples by destroying the old graph. */
    for(int i=0;i<20;++i) {
        tube_audio_tempo *tempo=tube_tempo_create(2,.25);assert(tempo);
        int calls=0;assert(!tube_tempo_feed(tempo,zeros,8,cancel,&calls) && !calls);
        tube_tempo_destroy(tempo);
    }
    tube_audio_tempo *empty=tube_tempo_create(2,4);assert(empty);
    int empty_calls=0;assert(!tube_tempo_finish(empty,cancel,&empty_calls) && !empty_calls);
    tube_tempo_destroy(empty);
    tube_tempo_destroy(NULL);
    printf("PASS: FFmpeg %s tempo range, duration/pitch, 1..8 channels, 1x bytes, EOF, cancellation and epoch disposal\n",av_version_info());
}
