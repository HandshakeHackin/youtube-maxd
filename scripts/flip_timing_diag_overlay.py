"""Opt-in buffered all-present CPU timing; raw status has no assumed scanout ABI."""

DIAGNOSTIC_C = r'''
#include "tube_luma_diag.h"

/* Diagnostic records are render-thread-only; logging and file I/O happen after playback. */
#define TUBE_FLIP_DIAG_CAPACITY 8192u
#define TUBE_FLIP_DIAG_PATH "/data/prosperotube/flip_timing_diag.csv"
struct tube_flip_diag_record {
    uint64_t epoch, serial;
    int64_t pts, buffer_pts_us;
    uint64_t fetch_us, begin_us, suspend_enter_us, suspend_exit_us, gpu_done_us;
    uint64_t submit_us, observed_us, return_us, flip_arg, status[8];
    unsigned gpu_waits, vblank_waits;
    int backbuffer, gpu_retired, flip_rc, present_retired;
    int have_source, new_frame, held, blit_rc, smoothing_active;
    int refresh_mode, pacer_active, pacer_pending, pacer_drift_steps;
    unsigned pacer_misses;
    uint64_t present_enter_us, pacer_target_us;
    uint64_t source_y, source_luma_hash, sample_us;
    uint32_t source_width, source_height;
    int source_pitch, source_ten_bit, sample_valid;
};
static struct tube_flip_diag_record tube_flip_diag_records[TUBE_FLIP_DIAG_CAPACITY];
static struct tube_flip_diag_record *tube_flip_diag_current;
static struct tube_flip_diag_record tube_flip_diag_source_record;
static unsigned tube_flip_diag_count, tube_flip_diag_overflow, tube_flip_diag_session;
static int tube_flip_diag_playing;

uint64_t tube_flip_diag_now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

static void tube_flip_diag_dump(void)
{
    if (!tube_flip_diag_count) return;
    FILE *file = fopen(TUBE_FLIP_DIAG_PATH, "w");
    if (!file) {
        evo_boot_log("flip_diag: cannot write %s", TUBE_FLIP_DIAG_PATH);
        return;
    }
    int failed = fprintf(file, "session,index,epoch,serial,pts_us,fetch_us,begin_us,suspend_enter_us,suspend_exit_us,gpu_done_us,submit_us,observed_us,return_us,flip_arg,backbuffer,gpu_waits,vblank_waits,gpu_retired,flip_rc,present_retired,status0,status1,status2,status3,status4,status5,status6,status7,have_source,new_frame,held,blit_rc,smoothing_active,present_enter_us,refresh_mode,pacer_active,pacer_pending,pacer_target_us,pacer_misses,pacer_drift_steps,buffer_pts_us,source_y,source_pitch,source_width,source_height,source_ten_bit,sample_valid,source_luma_hash,sample_us\n") < 0;
    for (unsigned i = 0; i < tube_flip_diag_count && !failed; ++i) {
        const struct tube_flip_diag_record *r = &tube_flip_diag_records[i];
        failed = fprintf(file,
            "%u,%u,%llu,%llu,%lld,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%d,%u,%u,%d,%d,%d",
            tube_flip_diag_session, i, (unsigned long long)r->epoch, (unsigned long long)r->serial,
            (long long)r->pts, (unsigned long long)r->fetch_us, (unsigned long long)r->begin_us,
            (unsigned long long)r->suspend_enter_us, (unsigned long long)r->suspend_exit_us,
            (unsigned long long)r->gpu_done_us, (unsigned long long)r->submit_us,
            (unsigned long long)r->observed_us, (unsigned long long)r->return_us,
            (unsigned long long)r->flip_arg, r->backbuffer, r->gpu_waits, r->vblank_waits,
            r->gpu_retired, r->flip_rc, r->present_retired) < 0;
        for (unsigned j = 0; j < 8 && !failed; ++j)
            failed = fprintf(file, ",%llu", (unsigned long long)r->status[j]) < 0;
        if (!failed) failed = fprintf(file, ",%d,%d,%d,%d,%d,%llu,%d,%d,%d,%llu,%u,%d,%lld,%llu,%d,%u,%u,%d,%d,%llu,%llu\n",
            r->have_source, r->new_frame, r->held, r->blit_rc, r->smoothing_active,
            (unsigned long long)r->present_enter_us, r->refresh_mode,
            r->pacer_active, r->pacer_pending, (unsigned long long)r->pacer_target_us,
            r->pacer_misses, r->pacer_drift_steps, (long long)r->buffer_pts_us,
            (unsigned long long)r->source_y, r->source_pitch, r->source_width, r->source_height,
            r->source_ten_bit, r->sample_valid, (unsigned long long)r->source_luma_hash,
            (unsigned long long)r->sample_us) < 0;
    }
    if (fclose(file) != 0) failed = 1;
    evo_boot_log("flip_diag: CPU observations session=%u rows=%u omitted=%u file=%s result=%s; raw status ABI unspecified",
        tube_flip_diag_session, tube_flip_diag_count, tube_flip_diag_overflow,
        TUBE_FLIP_DIAG_PATH, failed ? "failed" : "ok");
}

void tube_flip_diag_active(int playing)
{
    playing = !!playing;
    if (playing == tube_flip_diag_playing) return;
    tube_flip_diag_current = NULL;
    if (playing) {
        ++tube_flip_diag_session;
        tube_flip_diag_count = tube_flip_diag_overflow = 0;
    } else {
        tube_flip_diag_dump();
    }
    tube_flip_diag_playing = playing;
}

void tube_flip_diag_source(uint64_t epoch, uint64_t serial, int64_t pts, uint64_t fetch_us,
    int have, int new_frame, int held, int smoothing_active)
{
    memset(&tube_flip_diag_source_record, 0, sizeof(tube_flip_diag_source_record));
    tube_flip_diag_source_record.epoch = epoch;
    tube_flip_diag_source_record.serial = serial;
    tube_flip_diag_source_record.pts = pts;
    tube_flip_diag_source_record.fetch_us = fetch_us;
    tube_flip_diag_source_record.have_source = !!have;
    tube_flip_diag_source_record.new_frame = !!new_frame;
    tube_flip_diag_source_record.held = !!held;
    tube_flip_diag_source_record.smoothing_active = !!smoothing_active;
    tube_flip_diag_source_record.blit_rc = INT32_MIN;
}

void tube_flip_diag_pixels(const void *y, int pitch, uint32_t width, uint32_t height, int ten_bit)
{
    struct tube_flip_diag_record *r = &tube_flip_diag_source_record;
    r->source_y = (uint64_t)(uintptr_t)y;
    r->source_pitch = pitch; r->source_width = width; r->source_height = height;
    r->source_ten_bit = !!ten_bit;
    if (!tube_flip_diag_playing || !r->have_source || tube_flip_diag_count >= TUBE_FLIP_DIAG_CAPACITY)
        return;
    const uint64_t begin = tube_flip_diag_now_us();
    r->sample_valid = tube_luma_diag_sample(y, pitch, width, height, ten_bit,
        evo_agc_runtime_cache_flush, &r->source_luma_hash);
    r->sample_us = tube_flip_diag_now_us() - begin;
}

void tube_flip_diag_blit(int rc)
{
    tube_flip_diag_source_record.blit_rc = rc;
    tube_flip_diag_source_record.begin_us = tube_flip_diag_now_us(); /* post-video, pre-UI */
}

void tube_flip_diag_begin(void)
{
    tube_flip_diag_current = NULL;
    if (!tube_flip_diag_playing) return;
    if (tube_flip_diag_count >= TUBE_FLIP_DIAG_CAPACITY) {
        ++tube_flip_diag_overflow;
        return;
    }
    struct tube_flip_diag_record *r = &tube_flip_diag_records[tube_flip_diag_count++];
    *r = tube_flip_diag_source_record;
    r->present_enter_us = tube_flip_diag_now_us();
    if (!r->begin_us) r->begin_us = r->present_enter_us;
    r->flip_rc = INT32_MIN; r->backbuffer = -1; r->buffer_pts_us = INT64_MIN;
    tube_flip_diag_current = r;
}

static void tube_flip_diag_pacer(int refresh_mode, int active, int pending,
    double target, unsigned misses, int drift_steps, int64_t buffer_pts_us)
{
    if (!tube_flip_diag_current) return;
    tube_flip_diag_current->buffer_pts_us = buffer_pts_us;
    tube_flip_diag_current->refresh_mode = refresh_mode;
    tube_flip_diag_current->pacer_active = active;
    tube_flip_diag_current->pacer_pending = pending;
    tube_flip_diag_current->pacer_target_us = target > 0.0 ? (uint64_t)target : 0;
    tube_flip_diag_current->pacer_misses = misses;
    tube_flip_diag_current->pacer_drift_steps = drift_steps;
}

static void tube_flip_diag_suspend(int entering)
{
    if (!tube_flip_diag_current) return;
    if (entering) tube_flip_diag_current->suspend_enter_us = tube_flip_diag_now_us();
    else tube_flip_diag_current->suspend_exit_us = tube_flip_diag_now_us();
}

static void tube_flip_diag_gpu(unsigned waits, int retired)
{
    if (!tube_flip_diag_current) return;
    tube_flip_diag_current->gpu_waits = waits;
    tube_flip_diag_current->gpu_retired = retired;
    tube_flip_diag_current->gpu_done_us = tube_flip_diag_now_us();
}

static void tube_flip_diag_submit(uint64_t arg, int backbuffer)
{
    if (!tube_flip_diag_current) return;
    tube_flip_diag_current->flip_arg = arg;
    tube_flip_diag_current->backbuffer = backbuffer;
    tube_flip_diag_current->submit_us = tube_flip_diag_now_us();
}

static void tube_flip_diag_submitted(int rc)
{
    if (tube_flip_diag_current) tube_flip_diag_current->flip_rc = rc;
}

static void tube_flip_diag_observed(const uint64_t *status, unsigned waits)
{
    if (!tube_flip_diag_current) return;
    tube_flip_diag_current->observed_us = tube_flip_diag_now_us();
    tube_flip_diag_current->vblank_waits = waits;
    memcpy(tube_flip_diag_current->status, status, sizeof(tube_flip_diag_current->status));
}

static void tube_flip_diag_waited(unsigned waits)
{
    if (tube_flip_diag_current) tube_flip_diag_current->vblank_waits = waits;
}

void tube_flip_diag_finish(int retired)
{
    if (!tube_flip_diag_current) return;
    tube_flip_diag_current->return_us = tube_flip_diag_now_us();
    tube_flip_diag_current->present_retired = retired;
    tube_flip_diag_current = NULL;
}
'''

def apply(replace, player):
    header = player + "media/include/evo_agc_runtime.h"
    runtime = player + "media/src/evo_agc_runtime.c"
    app = player + "core/src/Application.cpp"
    replace(header, 'extern "C" {\n#endif', '''extern "C" {
#endif

/* Opt-in CPU observations, not authenticated scanout timestamps. */
uint64_t tube_flip_diag_now_us(void);
void tube_flip_diag_active(int playing);
void tube_flip_diag_source(uint64_t epoch, uint64_t serial, int64_t pts, uint64_t fetch_us,
    int have, int new_frame, int held, int smoothing_active);
void tube_flip_diag_pixels(const void *y, int pitch, uint32_t width, uint32_t height, int ten_bit);
void tube_flip_diag_blit(int rc);
void tube_flip_diag_begin(void);
void tube_flip_diag_finish(int retired);''')
    replace(runtime, '#include <unistd.h>', DIAGNOSTIC_C + '\n#include <unistd.h>')
    replace(runtime, 'void evo_agc_runtime_shutdown(void)\n{',
            'void evo_agc_runtime_shutdown(void)\n{\n    tube_flip_diag_active(0);')
    replace(runtime, '        int32_t sp_rc = sceAgcSuspendPoint();', '''        tube_flip_diag_suspend(1);
        int32_t sp_rc = sceAgcSuspendPoint();
        tube_flip_diag_suspend(0);''')
    replace(runtime, '''            const int retired = agc_wait_slot_fence(slot, marker,
                500000u / step_us, step_us, &waits);''', '''            const int retired = agc_wait_slot_fence(slot, marker,
                500000u / step_us, step_us, &waits);
            tube_flip_diag_gpu(waits, retired);''')
    replace(runtime, '            int32_t fliprc = sceVideoOutSubmitFlip(', '''            tube_flip_diag_submit((uint64_t)g_agc_dev.flip_arg, g_agc_dev.active_backbuffer);
            tube_flip_diag_pacer(evo_agc_runtime_get_refresh_rate(), tube_pacer.active,
                tube_pacer.pending, tube_pacer.target, tube_pacer.misses, tube_pacer.drift_steps,
                g_agc_dev.video_pts[g_agc_dev.active_backbuffer]);
            int32_t fliprc = sceVideoOutSubmitFlip(''')
    replace(runtime, '''                                                   (int64_t)g_agc_dev.flip_arg);

            /*''', '''                                                   (int64_t)g_agc_dev.flip_arg);
            tube_flip_diag_submitted(fliprc);

            /*''')
    replace(runtime, '''                        status[3] == (uint64_t)g_agc_dev.flip_arg)
                        break;''', '''                        status[3] == (uint64_t)g_agc_dev.flip_arg) {
                        tube_flip_diag_observed(status, fwaits);
                        break;
                    }''')
    replace(runtime, '                g_agc_dev.flip_waits += fwaits;',
            '                tube_flip_diag_waited(fwaits);\n                g_agc_dev.flip_waits += fwaits;')
    replace(app, '        bool playbackActive = m_playbackController && m_playbackController->isActive();',
            '        bool playbackActive = m_playbackController && m_playbackController->isActive();\n        tube_flip_diag_active(playbackActive);\n        tube_flip_diag_source(0, 0, INT64_MIN, 0, 0, 0, 0, 0);')
    replace(app, '            bool new_frame = have && (f.tube_epoch != tube_last_epoch || f.tube_serial != tube_last_serial);',
            '            bool new_frame = have && (f.tube_epoch != tube_last_epoch || f.tube_serial != tube_last_serial);\n            const uint64_t tube_diag_fetch_us = have ? tube_flip_diag_now_us() : 0;')
    replace(app, '            bool smoothing_active = evo_agc_motion_smoothing_is_active() && !is_paused && !is_scrubbing && !g_pp_pb.seek_discarding;',
            '            bool smoothing_active = evo_agc_motion_smoothing_is_active() && !is_paused && !is_scrubbing && !g_pp_pb.seek_discarding;\n            tube_flip_diag_source(f.tube_epoch, f.tube_serial, f.pts_us, tube_diag_fetch_us,\n                have, new_frame, f.held, smoothing_active);')
    replace(app, '                int tube_blit_result = evo_agc_blit_yuv(f.y, f.y_pitch, f.uv, f.uv_pitch,',
            '                tube_flip_diag_pixels(f.y, f.y_pitch,\n                    f.disp_w > 0 && f.disp_w <= f.coded_w ? f.disp_w : f.coded_w,\n                    f.disp_h > 0 && f.disp_h <= f.coded_h ? f.disp_h : f.coded_h, f.ten_bit);\n                int tube_blit_result = evo_agc_blit_yuv(f.y, f.y_pitch, f.uv, f.uv_pitch,')
    replace(app, '                if (tube_blit_result == 0 && !f.held) {',
            '                tube_flip_diag_blit(tube_blit_result);\n                if (tube_blit_result == 0 && !f.held) {')
    replace(app, '            evo_agc_runtime_present();',
            '            tube_flip_diag_begin();\n            evo_agc_runtime_present();\n            tube_flip_diag_finish(evo_agc_runtime_last_present_retired());')
