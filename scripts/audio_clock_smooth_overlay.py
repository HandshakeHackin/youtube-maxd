"""Interpolate the audio-master clock between completed 2048-sample outputs."""


def apply(replace, player):
    audio = player + "media/src/evo_audio_out.c"
    playback = player + "media/src/evo_playback.c"
    header = player + "media/include/evo_audio_out.h"
    demux = player + "media/src/evo_demux.c"
    controller = player + "core/src/services/PlaybackController.cpp"

    replace(audio, "#include <string.h>", "#include <string.h>\n#include <time.h>\n#include <pthread.h>")
    replace(audio, "static tube_audio_seek tube_seek_pcm;\nvoid evo_audio_drain_reset(void) {", """static tube_audio_seek tube_seek_pcm;
static void tube_audio_clock_hold(void);
void evo_audio_drain_reset(void) {""")
    replace(audio, "    tube_audio_reset(&tube_drain);\n    memset(&tube_seek_pcm, 0, sizeof(tube_seek_pcm));", """    tube_audio_reset(&tube_drain);
    tube_audio_clock_hold();
    memset(&tube_seek_pcm, 0, sizeof(tube_seek_pcm));""")
    replace(audio, "volatile double audio_clock_seconds = 0.0;", """volatile double audio_clock_seconds = 0.0;

/* AudioOut advances the old clock once per 42.7 ms block. The decoder and
 * render thread use this bounded interpolation of the last completed block. */
static pthread_mutex_t tube_audio_clock_mutex = PTHREAD_MUTEX_INITIALIZER;
static double tube_audio_clock_base_s, tube_audio_clock_rate, tube_audio_clock_span_s;
static uint64_t tube_audio_clock_stamp_us, tube_audio_clock_epoch;
static int tube_audio_clock_valid;
extern volatile int audio_thread_running;

static uint64_t tube_audio_clock_now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull;
}

static double tube_audio_clock_interpolate(double base_s, uint64_t stamp_us,
                                           double rate, double span_s, uint64_t now_us) {
    if (now_us <= stamp_us) return base_s;
    double elapsed = (double)(now_us - stamp_us) / 1000000.0;
    if (elapsed > (double)AUDIO_BLOCK_SAMPLES / 48000.0)
        elapsed = (double)AUDIO_BLOCK_SAMPLES / 48000.0;
    double advance = elapsed * rate;
    if (advance > span_s) advance = span_s;
    return base_s + advance;
}

static void tube_audio_clock_hold(void) {
    pthread_mutex_lock(&tube_audio_clock_mutex);
    tube_audio_clock_valid = 0;
    pthread_mutex_unlock(&tube_audio_clock_mutex);
}

void evo_audio_clock_smooth_reset(void) {
    pthread_mutex_lock(&tube_audio_clock_mutex);
    tube_audio_clock_valid = 0;
    audio_samples_played = 0;
    audio_clock_seconds = 0.0;
    pthread_mutex_unlock(&tube_audio_clock_mutex);
}

static void tube_audio_clock_advance(int samples, double rate, uint64_t epoch) {
    const int seeking = evo_demux_is_seeking();
    pthread_mutex_lock(&tube_audio_clock_mutex);
    if (epoch == tube_audio_epoch(&tube_drain) && !seeking) {
        audio_samples_played += samples;
        tube_audio_clock_span_s = (double)samples * rate / 48000.0;
        audio_clock_seconds += tube_audio_clock_span_s;
        tube_audio_clock_base_s = audio_clock_seconds;
        tube_audio_clock_rate = rate;
        tube_audio_clock_stamp_us = tube_audio_clock_now_us();
        tube_audio_clock_epoch = epoch;
        tube_audio_clock_valid = 1;
    } else {
        tube_audio_clock_valid = 0;
    }
    pthread_mutex_unlock(&tube_audio_clock_mutex);
}

double evo_audio_clock_smooth_seconds(void) {
    const int running = audio_thread_running && !player_paused && !pb_scrub_hold &&
        !evo_demux_is_seeking() && !evo_audio_is_drained();
    pthread_mutex_lock(&tube_audio_clock_mutex);
    double value = audio_clock_seconds;
    if (tube_audio_clock_valid && running &&
        tube_audio_clock_epoch == tube_audio_epoch(&tube_drain)) {
        value = tube_audio_clock_interpolate(tube_audio_clock_base_s,
            tube_audio_clock_stamp_us, tube_audio_clock_rate,
            tube_audio_clock_span_s, tube_audio_clock_now_us());
    }
    pthread_mutex_unlock(&tube_audio_clock_mutex);
    return value;
}""")
    replace(header, "extern volatile double    audio_clock_seconds;", """extern volatile double    audio_clock_seconds;
double evo_audio_clock_smooth_seconds(void);
void evo_audio_clock_smooth_reset(void);""")

    replace(audio, "            usleep(2000);\n            continue;\n        }\n        if (screen == 2", """            tube_audio_clock_hold();
            usleep(2000);
            continue;
        }
        if (screen == 2""")
    replace(audio, "                video_stuck_iters++;\n                usleep(2000);\n                continue;", """                video_stuck_iters++;
                tube_audio_clock_hold();
                usleep(2000);
                continue;""")
    replace(audio, "                if (submitted < 0) tube_audio_mark(&tube_drain, drain_epoch, TUBE_AUDIO_FAILED);\n                else {\n                    audio_samples_played += audio_queue_samples[audio_queue_read];\n                    audio_clock_seconds += audio_queue_samples[audio_queue_read] * audio_queue_rate[audio_queue_read] / 48000.0;\n                }", """                if (submitted < 0) {
                    tube_audio_clock_hold();
                    tube_audio_mark(&tube_drain, drain_epoch, TUBE_AUDIO_FAILED);
                } else {
                    tube_audio_clock_advance(audio_queue_samples[audio_queue_read],
                                             audio_queue_rate[audio_queue_read], epoch);
                }""")
    replace(audio, "                const int tube_delay_pending = tube_audio_delay_tail(&tube_delay_line) != 0;", """                tube_audio_clock_hold();
                const int tube_delay_pending = tube_audio_delay_tail(&tube_delay_line) != 0;""")
    replace(audio, "            /* Reset throttle tracker while paused or inactive so resume starts clean */", """            tube_audio_clock_hold();
            /* Reset throttle tracker while paused or inactive so resume starts clean */""")

    replace(playback, "(int64_t)((audio_clock_seconds - evo_pb_video_hold_s()) * 1000000.0)",
            "(int64_t)((evo_audio_clock_smooth_seconds() - evo_pb_video_hold_s()) * 1000000.0)")
    replace(playback, "audio_rel = audio_clock_seconds - evo_pb_video_hold_s();",
            "audio_rel = evo_audio_clock_smooth_seconds() - evo_pb_video_hold_s();", count=2)

    replace(demux, "        audio_samples_played = 0;\n        audio_samples_decoded = 0;\n\n        audio_clock_seconds = 0.0;", """        evo_audio_clock_smooth_reset();
        audio_samples_decoded = 0;""")
    replace(controller, "    audio_samples_played = 0;\n    audio_samples_decoded = 0;\n    audio_clock_seconds = 0.0;", """    evo_audio_clock_smooth_reset();
    audio_samples_decoded = 0;""")
