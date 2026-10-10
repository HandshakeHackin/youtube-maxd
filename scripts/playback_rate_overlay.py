"""Rate-aware presentation, audio output and stopped-session selection."""


def apply_playback_rate_overlay(replace, player):
    header = player + 'pp/include/pp_clock.h'
    source = player + 'pp/src/pp_clock.c'
    replace(header, '    int started;', '    double rate; /* media time per host second; owner-thread changes only */\n    int started;')
    replace(header, 'void pp_clock_start(pp_clock *c, int64_t first_pts_us);', '''/* Owner-thread operation. Preserves current media position, including pause.
 * An external audio master already reports media time and is never multiplied. */
int pp_clock_set_rate(pp_clock *c, double rate);
void pp_clock_start(pp_clock *c, int64_t first_pts_us);''')
    replace(source, '#include <stdint.h>', '#include <stdint.h>\n#include <math.h>')
    replace(source, '    c->max_late_us = max_late_us > 0 ? max_late_us : 80000;',
            '    c->rate = 1.0;\n    c->max_late_us = max_late_us > 0 ? max_late_us : 80000;')
    replace(source, 'void pp_clock_start(pp_clock *c, int64_t first_pts_us)', '''int pp_clock_set_rate(pp_clock *c, double rate)
{
    if (!c || !isfinite(rate) || rate < .25 || rate > 4.0) return 0;
    if (c->rate == rate) return 1;
    if (c->started) {
        const int64_t media = pp_clock_media_us(c);
        c->host_start_us = now_us();
        c->media_start_pts_us = media;
        if (c->paused) {
            c->media_at_pause_us = media;
            c->pause_host_us = c->host_start_us;
        }
    }
    c->rate = rate;
    return 1;
}

void pp_clock_start(pp_clock *c, int64_t first_pts_us)''')
    replace(source, '''    elapsed = now_us() - c->host_start_us;
    return c->media_start_pts_us + (int64_t)elapsed;''', '''    const uint64_t now = now_us();
    elapsed = now >= c->host_start_us ? now - c->host_start_us : 0;
    return c->media_start_pts_us + (int64_t)((double)elapsed * c->rate);''')
    replace(source, '    int64_t max_late, max_early;', '    int64_t max_late, max_early;\n    double rate;')
    replace(source, '    max_late = c->max_late_us;', '    rate = c->rate;\n    max_late = c->max_late_us;')
    replace(source, '    c->max_late_us = max_late;', '    c->rate = rate;\n    c->max_late_us = max_late;')
    replace(source, '    c->pause_host_us = now_us();', '''    c->pause_host_us = now_us();
    c->host_start_us = c->pause_host_us;
    c->media_start_pts_us = c->media_at_pause_us;''')
    replace(source, 'int pp_clock_wait_or_drop(pp_clock *c, int64_t pts_us)', '''/* Keep a fallback anchor at each observed audio-master position. When the
 * audio tail ends before video, host pacing continues from that position. */
static void pp_clock_sync_master(pp_clock *c)
{
    if (c->clock_source) {
        const int64_t media = c->clock_source(c->clock_source_user);
        if (media >= 0) {
            c->media_start_pts_us = media;
            c->host_start_us = now_us();
        }
    }
}

int pp_clock_wait_or_drop(pp_clock *c, int64_t pts_us)''')
    replace(source, '    media = pp_clock_media_us(c);',
            '    pp_clock_sync_master(c);\n    media = pp_clock_media_us(c);', count=2)
    replace(source, '''        if (early > 500) {''', '''        /* PTS lag is media time; the sleep API takes host time. */
        early = (int64_t)ceil((double)early / c->rate);
        if (early > 500) {''')


def apply_audio_rate_overlay(replace, player, work):
    source = player + 'media/src/evo_audio_out.c'
    header = player + 'media/include/evo_audio_out.h'
    replace(source, '#include "tube_audio_drain.h"', '#include "tube_audio_drain.h"\n#include "tube_audio_tempo.h"')
    replace(source, 'static int audio_queue_samples[AUDIO_QUEUE_BLOCKS];', '''static int audio_queue_samples[AUDIO_QUEUE_BLOCKS];
static double audio_queue_rate[AUDIO_QUEUE_BLOCKS];
static _Atomic int tube_rate_percent = 100;
double evo_audio_rate(void) { return atomic_load_explicit(&tube_rate_percent, memory_order_acquire) / 100.0; }
int evo_audio_set_rate(int percent) {
    if (percent < 25 || percent > 400 || audio_decode_thread_running || audio_thread_running) return 0;
    atomic_store_explicit(&tube_rate_percent, percent, memory_order_release);
    return 1;
}
static tube_audio_tempo *tube_tempo_pcm;
static uint64_t tube_tempo_epoch;
static int tube_tempo_channels;
static void tube_tempo_dispose(void) {
    tube_tempo_destroy(tube_tempo_pcm); tube_tempo_pcm = NULL;
    tube_tempo_epoch = 0; tube_tempo_channels = 0;
}''')
    replace(header, 'int evo_audio_is_drained(void);', '''int evo_audio_is_drained(void);
/* Change only while both audio workers are stopped, before opening a session.
 * Runtime changes must stop/reopen at the saved media position. */
int evo_audio_set_rate(int percent);
double evo_audio_rate(void);''')
    replace(source, '    audio_queue_samples[audio_queue_write] = samples;',
            '    audio_queue_samples[audio_queue_write] = samples;\n    audio_queue_rate[audio_queue_write] = evo_audio_rate();')
    replace(source, '''                else audio_samples_played += audio_queue_samples[audio_queue_read];
                audio_clock_seconds = (double)audio_samples_played / 48000.0;''', '''                else {
                    audio_samples_played += audio_queue_samples[audio_queue_read];
                    audio_clock_seconds += audio_queue_samples[audio_queue_read] * audio_queue_rate[audio_queue_read] / 48000.0;
                }''')
    replace(source, '''static void audio_queue_push(int16_t *buf) {
    (void)audio_queue_push_counted(buf, AUDIO_BLOCK_SAMPLES);
}''', '')
    helpers = '''/* PCM stage and queue accumulator share one decoder owner/seek epoch. */
static int tube_tempo_accumulate(void *user, const int16_t *samples, int count) {
    const uint64_t epoch = *(const uint64_t *)user;
    const int ch = evo_audio_channels;
    int offset = 0;
    while (offset < count) {
        if (!audio_decode_thread_running || evo_demux_is_seeking() || tube_audio_epoch(&tube_drain) != epoch)
            return AVERROR_EXIT;
        int run = AUDIO_BLOCK_SAMPLES - audio_accum_pos;
        if (run > count - offset) run = count - offset;
        memcpy(audio_accum + audio_accum_pos * ch, samples + offset * ch,
               (size_t)run * ch * sizeof(int16_t));
        audio_accum_pos += run; audio_samples_decoded += run; offset += run;
        if (audio_accum_pos == AUDIO_BLOCK_SAMPLES) {
            while (audio_decode_thread_running && !evo_demux_is_seeking() &&
                   tube_audio_epoch(&tube_drain) == epoch && audio_queue_count >= AUDIO_QUEUE_BLOCKS - 2)
                usleep(1000);
            if (!audio_decode_thread_running || evo_demux_is_seeking() || tube_audio_epoch(&tube_drain) != epoch)
                return AVERROR_EXIT;
            if (!audio_queue_push_counted(audio_accum, AUDIO_BLOCK_SAMPLES)) return AVERROR(EIO);
            audio_accum_pos = 0;
        }
    }
    return 0;
}
static int tube_tempo_process(const int16_t *samples, int count, uint64_t epoch, int finish) {
    if (!audio_decode_thread_running || evo_demux_is_seeking() || tube_audio_epoch(&tube_drain) != epoch) return 2;
    if (!tube_tempo_pcm || tube_tempo_epoch != epoch || tube_tempo_channels != evo_audio_channels) {
        tube_tempo_dispose();
        tube_tempo_pcm = tube_tempo_create(evo_audio_channels, evo_audio_rate());
        tube_tempo_epoch = epoch; tube_tempo_channels = evo_audio_channels;
    }
    int rc = AVERROR(ENOMEM);
    if (tube_tempo_pcm) {
        rc = 0;
        if (finish) rc = tube_tempo_finish(tube_tempo_pcm, tube_tempo_accumulate, &epoch);
        else for (int offset = 0; offset < count && rc >= 0;) {
            int run = count - offset; if (run > 65536) run = 65536;
            rc = tube_tempo_feed(tube_tempo_pcm, samples + (size_t)offset * evo_audio_channels,
                                 run, tube_tempo_accumulate, &epoch);
            offset += run;
        }
    }
    if (rc >= 0) return 1;
    if (!audio_decode_thread_running || evo_demux_is_seeking() || tube_audio_epoch(&tube_drain) != epoch) {
        tube_tempo_dispose(); return 2;
    }
    tube_audio_mark(&tube_drain, epoch, TUBE_AUDIO_FAILED); return 0;
}

'''
    replace(source, 'static void mix_audio_frame_to_queue(', helpers + 'static void mix_audio_frame_to_queue(')
    text = (work / source).read_text()
    start = text.index('    if (converted > 0) {', text.index('static void mix_audio_frame_to_queue('))
    end = text.index('\n\n}\n\n\n/* The live native audio decoder', start)
    replace(source, text[start:end], '''    if (converted > 0)
        (void)tube_tempo_process((const int16_t *)output_buffer, converted, tube_audio_epoch(&tube_drain), 0);''')
    text = (work / source).read_text()
    start = text.index('        int offset = 0;', text.index('static int tube_audio_flush_tail('))
    end = text.index('\n    }\n    if (audio_accum_pos)', start)
    replace(source, text[start:end], '''        int staged = tube_tempo_process(tail, count, epoch, 0);
        if (staged != 1) return staged;''')
    replace(source, '    if (audio_accum_pos) {', '''    int staged = tube_tempo_process(NULL, 0, epoch, 1);
    if (staged != 1) return staged;
    if (audio_accum_pos) {''')
    replace(source, '    av_frame_free(&af);', '    tube_tempo_dispose();\n    av_frame_free(&af);')
    replace(player + 'media/src/evo_playback.c', '        pp_playback_set_clock_source(&g_pp_pb, evo_audio_master_media_us, NULL);',
            '        pp_clock_set_rate(&g_pp_pb.clock, evo_audio_rate());\n        pp_playback_set_clock_source(&g_pp_pb, evo_audio_master_media_us, NULL);')
    replace(player + 'core/src/services/PlaybackController.cpp', '    m_playbackFsm.postEvent(PlaybackEvent::Open);', '''    if (!evo_audio_set_rate(source.provider == "youtube" && !source.is_live ? tube_playback_speed() : 100)) {
        evo_error_set("Playback speed could not be applied while audio is active.");
        return false;
    }
    m_playbackFsm.postEvent(PlaybackEvent::Open);''')
    replace(player + 'core/src/screens/PlayerScreen.cpp',
            'tube_skip_target(playback->getPositionSeconds())',
            'tube_skip_target_at_rate(playback->getPositionSeconds(), evo_audio_rate())')
    replace(player + 'ui_rml/src/evo_rmlui_provider.cpp',
            'id != "settings:languages" && id != "settings:audio-languages"',
            'id != "settings:languages" && id != "settings:audio-languages" && id != "settings:speed"')
