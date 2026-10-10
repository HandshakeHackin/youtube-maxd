/* Shipping presentation clock with deterministic host/audio time.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "pp_clock.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
static uint64_t host=10000000, slept;
static int64_t audio=-1;
static double audio_rate=1;
int tube_test_gettimeofday(struct timeval *tv, void *tz) {
    (void)tz;tv->tv_sec=host/1000000;tv->tv_usec=host%1000000;return 0;
}
int tube_test_usleep(useconds_t us) {
    slept+=us;host+=us;if(audio>=0) audio+=(int64_t)(us*audio_rate);return 0;
}
static int64_t master(void *unused) { (void)unused;return audio; }
static void check(double rate) {
    pp_clock c;pp_clock_init(&c,120000,100000);
    assert(c.rate==1 && pp_clock_set_rate(&c,rate));
    pp_clock_start(&c,1000000);host+=1000000;
    int64_t expected=1000000+(int64_t)(rate*1000000);
    assert(pp_clock_media_us(&c)==expected);
    pp_clock_pause(&c);host+=25000000;
    assert(pp_clock_media_us(&c)==expected);
    assert(pp_clock_set_rate(&c,.5));host+=1000000;
    assert(pp_clock_media_us(&c)==expected);
    pp_clock_resume(&c);host+=1000000;expected+=500000;
    assert(pp_clock_media_us(&c)==expected);
    assert(pp_clock_set_rate(&c,4) && pp_clock_media_us(&c)==expected);
    host+=250000;assert(pp_clock_media_us(&c)==expected+1000000);
    pp_clock_reset(&c);assert(c.rate==4 && !c.started);
    pp_clock_start(&c,90000000);assert(pp_clock_media_us(&c)==90000000);
    pp_clock_reanchor(&c,120000000);assert(c.rate==4 && pp_clock_media_us(&c)==120000000);
    assert(!pp_clock_set_rate(&c,0) && !pp_clock_set_rate(&c,NAN) && !pp_clock_set_rate(&c,4.1));
    assert(c.rate==4);
    pp_clock_init(&c,120000,100000);assert(pp_clock_set_rate(&c,rate));
    pp_clock_start(&c,0);slept=0;
    assert(pp_clock_wait_or_drop(&c,40000)==PP_CLOCK_PRESENT);
    assert(slept==(uint64_t)ceil(40000/rate));
    assert(llabs(pp_clock_media_us(&c)-40000)<=4);
    host+=1000000;assert(pp_clock_wait_or_drop(&c,0)==PP_CLOCK_DROP);
    pp_clock_pause(&c);slept=0;assert(pp_clock_wait_or_drop(&c,1000000)==PP_CLOCK_DROP && !slept);
    /* External audio clocks are already in media units, including at 4x. */
    pp_clock_init(&c,120000,100000);assert(pp_clock_set_rate(&c,rate));
    pp_clock_set_clock_source(&c,master,NULL);pp_clock_start(&c,0);
    audio=2000000;audio_rate=rate;assert(pp_clock_media_us(&c)==audio);
    slept=0;assert(pp_clock_wait_or_drop(&c,2040000)==PP_CLOCK_PRESENT);
    assert(slept==(uint64_t)ceil(40000/rate) && llabs(audio-2040000)<=4);
    int64_t handoff=audio;audio=-1;host+=20000;
    assert(pp_clock_media_us(&c)==handoff+(int64_t)(20000*rate));
    pp_clock_pause(&c);int64_t frozen=pp_clock_media_us(&c);host+=1000000;
    pp_clock_resume(&c);assert(pp_clock_media_us(&c)==frozen);
}
int main(void) {
    const double rates[]={.25,.5,.75,1,1.25,1.5,1.75,2,2.5,3,3.5,4};
    for(unsigned i=0;i<sizeof rates/sizeof *rates;++i) check(rates[i]);
    puts("PASS: shipping rate clock, pause/change/resume, seek reset, silent pacing and unscaled audio master");
}
