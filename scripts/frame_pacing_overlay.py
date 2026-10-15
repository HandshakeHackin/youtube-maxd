"""Schedule low-rate pictures in a stable VideoOut interval after GPU completion."""

RUNTIME_C = r'''
static tube_frame_pacing tube_pacer;

uint64_t evo_agc_frame_pacing_now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

void evo_agc_frame_pacing_reset(void)
{
    tube_frame_pacing_reset(&tube_pacer);
}

void evo_agc_frame_pacing_begin(uint64_t epoch, uint64_t serial, int64_t pts,
    uint64_t fetch_us, double rate, double fps)
{
    /* Only refresh token 13 has a verified 119.88 Hz contract on this path. */
    double refresh = evo_agc_runtime_get_refresh_rate() == 13 ? 1001000.0 / 120.0 : 0.0;
    tube_frame_pacing_begin(&tube_pacer, epoch, serial, pts, fetch_us, rate, fps, refresh);
}

static void evo_agc_frame_pacing_wait(void)
{
    uint64_t now = evo_agc_frame_pacing_now_us();
    uint64_t until = tube_frame_pacing_wait_until(&tube_pacer, now);
    while (until > now) {
        uint64_t remaining = until - now;
        sceKernelUsleep((unsigned)(remaining > 20000u ? 20000u : remaining));
        now = evo_agc_frame_pacing_now_us();
    }
}

void evo_agc_frame_pacing_observe(int retired)
{
    tube_frame_pacing_observe(&tube_pacer, evo_agc_frame_pacing_now_us(), retired);
}
'''


def apply(replace, player, work):
    header = player + "media/include/evo_agc_runtime.h"
    runtime = player + "media/src/evo_agc_runtime.c"
    app = player + "core/src/Application.cpp"
    replace(header, 'extern "C" {\n#endif', '''extern "C" {
#endif

uint64_t evo_agc_frame_pacing_now_us(void);
void evo_agc_frame_pacing_reset(void);
void evo_agc_frame_pacing_begin(uint64_t epoch, uint64_t serial, int64_t pts,
    uint64_t fetch_us, double rate, double fps);
void evo_agc_frame_pacing_observe(int retired);''')
    replace(runtime, '#include "evo_agc_runtime.h"', '#include "evo_agc_runtime.h"\n#include "tube_frame_pacing.h"')
    # sceKernelUsleep is declared below the platform includes.
    replace(runtime, 'static int tube_present_retired;', RUNTIME_C + '\nstatic int tube_present_retired;')
    source = (work / runtime).read_text()
    anchor = ('            tube_flip_diag_submit((uint64_t)g_agc_dev.flip_arg, g_agc_dev.active_backbuffer);'
              if 'tube_flip_diag_submit((uint64_t)' in source
              else '            int32_t fliprc = sceVideoOutSubmitFlip(')
    replace(runtime, anchor, '            evo_agc_frame_pacing_wait();\n' + anchor)
    replace(app, '            tube_last_epoch = tube_last_serial = 0;',
            '            tube_last_epoch = tube_last_serial = 0;\n            evo_agc_frame_pacing_reset();')
    replace(app, '            bool new_frame = have && (f.tube_epoch != tube_last_epoch || f.tube_serial != tube_last_serial);',
            '            bool new_frame = have && (f.tube_epoch != tube_last_epoch || f.tube_serial != tube_last_serial);\n            const uint64_t tube_pacing_fetch_us = new_frame ? evo_agc_frame_pacing_now_us() : 0;')
    replace(app, '            bool is_scrubbing = m_playbackController && m_playbackController->isScrubbing();',
            '            bool is_scrubbing = m_playbackController && m_playbackController->isScrubbing();\n            if (is_paused || is_scrubbing) evo_agc_frame_pacing_reset();')
    replace(app, '            bool smoothing_active = evo_agc_motion_smoothing_is_active() && !is_paused && !is_scrubbing && !g_pp_pb.seek_discarding;',
            '            bool smoothing_active = evo_agc_motion_smoothing_is_active() && !is_paused && !is_scrubbing && !g_pp_pb.seek_discarding;\n            if (smoothing_active) evo_agc_frame_pacing_reset();')
    replace(app, '                    tube_frame_epoch = f.tube_epoch; tube_frame_serial = f.tube_serial;',
            '''                    tube_frame_epoch = f.tube_epoch; tube_frame_serial = f.tube_serial;
                    if (new_frame && !is_paused && !is_scrubbing && !smoothing_active)
                        evo_agc_frame_pacing_begin(f.tube_epoch, f.tube_serial, f.pts_us,
                            tube_pacing_fetch_us, evo_audio_rate(), video_fps);''')
    replace(runtime, '                agc_mode_switch_presented();',
            '                evo_agc_frame_pacing_observe(tube_present_retired);\n                agc_mode_switch_presented();')
    replace(app, '            evo_agc_runtime_present();',
            '            evo_agc_runtime_present();\n            if (!evo_agc_runtime_last_present_retired()) evo_agc_frame_pacing_reset();')

