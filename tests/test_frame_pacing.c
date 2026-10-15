#include "tube_frame_pacing.h"
#include <assert.h>
#include <stdio.h>

static const double refresh = 1001000.0 / 120.0;

static double simulate(double fps, double ppm, int stall_frame, unsigned *misses, int *drift)
{
    tube_frame_pacing p = {0};
    double previous = 0.0, busy_until = 0.0, worst_late = 0.0;
    const double interval = 1000000.0 / fps;
    int frames = ppm == 0.0 ? 900 : 24 * 180;
    unsigned unusual = 0;
    for (int i = 0; i < frames; ++i) {
        double due = 1000000.0 + i * interval * (1.0 + ppm / 1000000.0);
        double fetch = due + (i * 613u % 1000u);
        if (fetch < busy_until) fetch = busy_until;
        int64_t pts = (int64_t)llround((i * interval) / 1000.0) * 1000;
        assert(tube_frame_pacing_begin(&p, 1, (uint64_t)i + 1, pts,
            (uint64_t)ceil(fetch), 1.0, fps, refresh));
        /* Sampled ready-time envelope: median 1.1 ms, occasional 4.4-7.8 ms fence waits. */
        double gpu = fetch + (i % 11 == 0 ? 7800.0 : (i % 3 == 0 ? 4400.0 : 1110.0));
        if (i == stall_frame) gpu += 100000.0;
        uint64_t wake = tube_frame_pacing_wait_until(&p, (uint64_t)ceil(gpu));
        double submit = fmax(gpu, (double)wake);
        double event = ceil(submit / refresh) * refresh;
        double observed = event + 50.0 + (i * 17u % 40u);
        tube_frame_pacing_observe(&p, (uint64_t)llround(observed), 1);
        if (i > 10 && ppm == 0.0 && stall_frame < 0) {
            double ticks = round((event - previous) / refresh);
            if (ticks < floor(interval / refresh) || ticks > ceil(interval / refresh))
                ++unusual;
        }
        /* Publication can only advance after the first flip; duplicate redraw also occupies renderer. */
        busy_until = event + refresh + 200.0;
        tube_frame_pacing_observe(&p, (uint64_t)llround(event + refresh + 70.0), 1);
        if (i > 10 && (stall_frame < 0 || i > stall_frame + 10))
            worst_late = fmax(worst_late, event - due);
        previous = event;
    }
    printf("fps=%.0f drift=%.0fppm stall=%d unusual=%u misses=%u steps=%d max steady latency=%.2fms\n",
        fps, ppm, stall_frame, unusual, p.misses, p.drift_steps, worst_late / 1000.0);
    assert(unusual == 0);
    *misses = p.misses; *drift = p.drift_steps;
    return worst_late;
}

static void earliest_submit_control(void)
{
    double previous = 0.0, busy_until = 0.0;
    unsigned sixes = 0, fours = 0;
    for (int i=0; i<900; ++i) {
        double fetch = 1000000.0 + i * (1000000.0/24.0) + (i*613u%1000u);
        fetch = fmax(fetch, busy_until);
        double gpu = fetch + (i%11==0 ? 7800.0 : (i%3==0 ? 4400.0 : 1110.0));
        double event = ceil(gpu/refresh)*refresh;
        if(i>10) {
            int ticks=(int)llround((event-previous)/refresh);
            sixes += ticks==6; fours += ticks==4;
        }
        previous=event; busy_until=event+refresh+200.0;
    }
    printf("Earliest-submit negative control: %u four-refresh and %u six-refresh gaps\n",fours,sixes);
    assert(sixes>20 && fours>20);
}

int main(void)
{
    tube_frame_pacing p = {0};
    assert(!tube_frame_pacing_begin(&p,1,1,0,1000,1.0,60.0,refresh));
    assert(!p.active && !tube_frame_pacing_wait_until(&p,9999));
    assert(!tube_frame_pacing_begin(&p,1,1,0,1000,1.0,24.0,1001000.0/60.0));
    assert(!tube_frame_pacing_begin(&p,1,1,0,1000,2.0,24.0,refresh));
    assert(!tube_frame_pacing_begin(&p,1,1,INT64_MIN,1000,1.0,24.0,refresh));
    assert(tube_frame_pacing_begin(&p,1,1,0,1000000,1.0,24.0,refresh));
    uint64_t wake = tube_frame_pacing_wait_until(&p,1001000);
    assert(fabs((double)wake - (1000000.0+2.0*refresh)) < 1.0);
    tube_frame_pacing_observe(&p,1025000,1);
    double anchor = p.anchor_target;
    tube_frame_pacing_observe(&p,1033342,1); /* duplicate does not advance media origin */
    assert(p.anchor_target == anchor);
    assert(!tube_frame_pacing_begin(&p,1,1,0,1034000,1.0,24.0,refresh));
    assert(!tube_frame_pacing_begin(&p,1,2,0,1034000,1.0,24.0,refresh));
    assert(!p.pending && p.active);
    assert(tube_frame_pacing_begin(&p,1,3,42000,1042000,1.0,24.0,refresh));
    assert(fabs(tube_frame_pacing_delta(&p,42000) - 1000000.0/24.0) < 0.001);
    assert(tube_frame_pacing_begin(&p,2,1,0,1200000,1.0,24.0,refresh));
    assert(p.anchor_pts==0 && p.anchor_fetch==1200000 && !p.grid);
    tube_frame_pacing_reset(&p); /* pause/stop */
    assert(!p.active && !p.pending);
    assert(tube_frame_pacing_begin(&p,2,1,500000,2000000,1.0,24.0,refresh));
    tube_frame_pacing_observe(&p,2025000,1);
    assert(tube_frame_pacing_begin(&p,2,2,541000,2041000,0.5,24.0,refresh));
    assert(p.rate==0.5 && p.anchor_pts==541000);
    assert(tube_frame_pacing_begin(&p,2,3,582000,3000000,0.5,24.0,refresh)); /* stall */
    assert(p.anchor_pts==582000 && p.anchor_fetch==3000000);
    assert(!tube_frame_pacing_begin(&p,2,4,625000,3100000,1.0,24.0,0.0)); /* output mode */
    assert(tube_frame_pacing_begin(&p,3,1,0,4000000,1.0,24.0,refresh));
    tube_frame_pacing_observe(&p,4025000,1);
    assert(tube_frame_pacing_begin(&p,3,2,47000,4047000,1.0,24.0,refresh));
    assert(!p.cfr && tube_frame_pacing_delta(&p,47000)==47000);
    anchor = p.anchor_target;
    assert(!tube_frame_pacing_wait_until(&p,(uint64_t)(p.target+10000)));
    tube_frame_pacing_observe(&p,(uint64_t)(p.target+20000),1);
    assert(p.anchor_target==anchor && p.misses==1);
    tube_frame_pacing_observe(&p,4100000,0);
    assert(!p.active);
    assert(tube_frame_pacing_begin(&p,5,1,0,5000000,1.0,24.0,refresh));
    tube_frame_pacing_observe(&p,5025000,1);
    assert(tube_frame_pacing_begin(&p,5,2,150000,5042000,1.0,24.0,refresh));
    assert(!tube_frame_pacing_wait_until(&p,5044000) && !p.active);
    earliest_submit_control();
    unsigned misses; int drift;
    assert(simulate(24.0,0.0,-1,&misses,&drift) < 4.0*refresh && !misses && !drift);
    assert(simulate(30.0,0.0,-1,&misses,&drift) < 4.0*refresh && !misses && !drift);
    assert(simulate(24.0,0.0,0,&misses,&drift) < 4.0*refresh && misses>0 && misses<=4 && !drift);
    assert(simulate(24.0,0.0,100,&misses,&drift) < 4.0*refresh && misses>0 && misses<=4 && !drift);
    assert(simulate(24.0,200.0,-1,&misses,&drift) < 5.0*refresh && drift>0 && drift<=5 && misses<=1);
    assert(simulate(24.0,-200.0,-1,&misses,&drift) < 5.0*refresh && drift<0 && drift>=-5 && !misses);
    puts("Frame pacing deadlines, quantized PTS, reset, drift and backpressure: PASS");
    return 0;
}

