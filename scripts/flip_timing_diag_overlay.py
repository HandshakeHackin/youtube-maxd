"""Opt-in buffered CPU-side flip timing; raw status words have no assumed timestamp ABI."""

DIAGNOSTIC_C = r'''
/* Diagnostic records are render-thread-only; logging and file I/O happen after playback. */
#define TUBE_FLIP_DIAG_CAPACITY 2048u
#define TUBE_FLIP_DIAG_PATH "/data/prosperotube/flip_timing_diag.csv"
struct tube_flip_diag_record {
    uint64_t epoch, serial;
    int64_t pts;
    uint64_t fetch_us, begin_us, suspend_enter_us, suspend_exit_us, gpu_done_us;
    uint64_t submit_us, observed_us, return_us, flip_arg, status[8];
    unsigned gpu_waits, vblank_waits;
    int backbuffer, gpu_retired, flip_rc, present_retired;
};
static struct tube_flip_diag_record tube_flip_diag_records[TUBE_FLIP_DIAG_CAPACITY];
static struct tube_flip_diag_record *tube_flip_diag_current;
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
    int failed = fprintf(file, "session,index,epoch,serial,pts_us,fetch_us,begin_us,suspend_enter_us,suspend_exit_us,gpu_done_us,submit_us,observed_us,return_us,flip_arg,backbuffer,gpu_waits,vblank_waits,gpu_retired,flip_rc,present_retired,status0,status1,status2,status3,status4,status5,status6,status7\n") < 0;
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
        if (!failed) failed = fputc('\n', file) == EOF;
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

void tube_flip_diag_begin(uint64_t epoch, uint64_t serial, int64_t pts, uint64_t fetch_us)
{
    tube_flip_diag_current = NULL;
    if (!tube_flip_diag_playing || !serial) return;
    if (tube_flip_diag_count >= TUBE_FLIP_DIAG_CAPACITY) {
        ++tube_flip_diag_overflow;
        return;
    }
    struct tube_flip_diag_record *r = &tube_flip_diag_records[tube_flip_diag_count++];
    memset(r, 0, sizeof(*r));
    r->epoch = epoch; r->serial = serial; r->pts = pts;
    r->fetch_us = fetch_us; r->begin_us = tube_flip_diag_now_us();
    r->flip_rc = INT32_MIN; r->backbuffer = -1;
    tube_flip_diag_current = r;
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
void tube_flip_diag_begin(uint64_t epoch, uint64_t serial, int64_t pts, uint64_t fetch_us);
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
            '        bool playbackActive = m_playbackController && m_playbackController->isActive();\n        tube_flip_diag_active(playbackActive);')
    replace(app, '            bool new_frame = have && (f.tube_epoch != tube_last_epoch || f.tube_serial != tube_last_serial);',
            '            bool new_frame = have && (f.tube_epoch != tube_last_epoch || f.tube_serial != tube_last_serial);\n            const uint64_t tube_diag_fetch_us = new_frame ? tube_flip_diag_now_us() : 0;')
    replace(app, '                    tube_frame_epoch = f.tube_epoch; tube_frame_serial = f.tube_serial;',
            '                    tube_frame_epoch = f.tube_epoch; tube_frame_serial = f.tube_serial;\n                    if (new_frame) tube_flip_diag_begin(f.tube_epoch, f.tube_serial, f.pts_us, tube_diag_fetch_us);')
    replace(app, '            evo_agc_runtime_present();',
            '            evo_agc_runtime_present();\n            tube_flip_diag_finish(evo_agc_runtime_last_present_retired());')
