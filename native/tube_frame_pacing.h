#ifndef TUBE_FRAME_PACING_H
#define TUBE_FRAME_PACING_H

#include <math.h>
#include <stdint.h>
#include <string.h>

/* Render-thread state. Keep a fixed media deadline; GPU completion is not the clock. */
typedef struct tube_frame_pacing {
    uint64_t epoch, serial, last_fetch;
    int64_t anchor_pts, last_pts;
    double rate, fps, refresh_us, anchor_fetch, anchor_target, grid;
    double target, window_start, window_min, window_max;
    int active, pending, cfr;
    unsigned misses;
    int drift_steps;
} tube_frame_pacing;

static inline void tube_frame_pacing_reset(tube_frame_pacing *p)
{
    memset(p, 0, sizeof(*p));
}

static inline double tube_frame_pacing_delta(tube_frame_pacing *p, int64_t pts)
{
    double delta = (double)pts - (double)p->anchor_pts;
    if (p->cfr) {
        double normalized = round(delta * p->fps / 1000000.0) * 1000000.0 / p->fps;
        /* WebM commonly quantizes 24 fps timestamps to alternating 41/42 ms. */
        if (fabs(normalized - delta) <= 1000.0) delta = normalized;
        else p->cfr = 0;
    }
    return delta / p->rate;
}

static inline int tube_frame_pacing_begin(tube_frame_pacing *p, uint64_t epoch,
    uint64_t serial, int64_t pts, uint64_t fetch_us, double rate, double fps, double refresh_us)
{
    /* Publication waits for presentation: the added lead must fit inside one source interval. */
    if (!serial || pts == INT64_MIN || !isfinite(rate) || rate <= 0.0 ||
        !isfinite(fps) || fps <= 0.0 || !isfinite(refresh_us) || refresh_us <= 0.0 ||
        1000000.0 / (fps * rate) <= 3.0 * refresh_us) {
        tube_frame_pacing_reset(p);
        return 0;
    }
    int restart = !p->active || p->epoch != epoch || p->rate != rate ||
        p->fps != fps || p->refresh_us != refresh_us || pts < p->last_pts ||
        fetch_us < p->last_fetch || fetch_us - p->last_fetch > 500000u;
    if (!restart && serial == p->serial) return 0;
    if (!restart && pts == p->last_pts) {
        p->serial = serial; p->last_fetch = fetch_us; p->pending = 0;
        return 0;
    }
    if (!restart) {
        double residual = (double)fetch_us - p->anchor_fetch - tube_frame_pacing_delta(p, pts);
        if (fabs(residual) > 250000.0) restart = 1;
    }
    if (restart) {
        tube_frame_pacing_reset(p);
        p->active = p->cfr = 1;
        p->epoch = epoch; p->rate = rate; p->fps = fps; p->refresh_us = refresh_us;
        p->anchor_pts = pts; p->anchor_fetch = (double)fetch_us;
        /* The ideal 2.5R deadline rounds to a display event between 2R and 3R. */
        p->anchor_target = (double)fetch_us + 2.5 * refresh_us;
        p->window_start = (double)fetch_us;
        p->window_min = HUGE_VAL; p->window_max = -HUGE_VAL;
    }
    double delta = tube_frame_pacing_delta(p, pts);
    double residual = (double)fetch_us - p->anchor_fetch - delta;
    if (residual < p->window_min) p->window_min = residual;
    if (residual > p->window_max) p->window_max = residual;
    if ((double)fetch_us - p->window_start >= 1000000.0) {
        /* Whole-window evidence rejects scheduling jitter; one refresh bounds gradual audio drift. */
        int step = p->window_min > refresh_us ? 1 : (p->window_max < -refresh_us ? -1 : 0);
        p->anchor_fetch += step * refresh_us;
        p->anchor_target += step * refresh_us;
        p->drift_steps += step;
        p->window_start = (double)fetch_us;
        p->window_min = HUGE_VAL; p->window_max = -HUGE_VAL;
    }
    p->target = p->anchor_target + delta;
    if (p->grid > 0.0)
        p->target = p->grid + round((p->target - p->grid) / refresh_us) * refresh_us;
    p->serial = serial; p->last_pts = pts; p->last_fetch = fetch_us; p->pending = 1;
    return 1;
}

static inline uint64_t tube_frame_pacing_wait_until(tube_frame_pacing *p, uint64_t gpu_ready_us)
{
    if (!p->pending) return 0;
    if ((double)gpu_ready_us >= p->target) {
        ++p->misses;
        return 0;
    }
    double middle = p->target - p->refresh_us * 0.5;
    if (middle - (double)gpu_ready_us > 3.0 * p->refresh_us) {
        /* Bound UI latency while a faster audio clock outruns the one-second drift window. */
        p->anchor_fetch -= p->refresh_us;
        p->anchor_target -= p->refresh_us;
        p->target -= p->refresh_us;
        middle -= p->refresh_us;
        --p->drift_steps;
        p->window_start = (double)p->last_fetch;
        p->window_min = HUGE_VAL; p->window_max = -HUGE_VAL;
        if (middle - (double)gpu_ready_us > 3.0 * p->refresh_us) {
            tube_frame_pacing_reset(p);
            return 0;
        }
    }
    return middle > (double)gpu_ready_us ? (uint64_t)ceil(middle) : 0;
}

static inline void tube_frame_pacing_observe(tube_frame_pacing *p, uint64_t observed_us, int retired)
{
    if (!p->active) return;
    if (!retired) { tube_frame_pacing_reset(p); return; }
    double observed = (double)observed_us;
    if (p->grid <= 0.0) p->grid = observed;
    else {
        double predicted = p->grid + round((observed - p->grid) / p->refresh_us) * p->refresh_us;
        double residual = observed - predicted;
        /* Filter CPU wake noise without moving the media anchor on every frame. */
        double limit = p->refresh_us * 0.25;
        if (residual > limit) residual = limit;
        if (residual < -limit) residual = -limit;
        p->grid = predicted + residual * 0.125;
    }
    p->pending = 0;
}
#endif

