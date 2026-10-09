/* Actual generated EVO mixer/output + host FFmpeg, with only AudioOut mocked.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <assert.h>
#include <stdio.h>
#include "evo_audio_out.c"

int screen = 2, player_paused, video_stream_index = -1, audio_stream_index = -1;
int evo_audio_channels = 2;
volatile int pb_prebuffer_hold, pb_scrub_hold;
pp_playback g_pp_pb;
AVFormatContext *play_fmt;
AVCodecContext *audio_ctx;
static _Atomic int seeking;
static int submissions, stop_after, fail_at;
PacketQueue audio_packet_queue;
double resume_base_offset_seconds;

int evo_demux_is_seeking(void) { return seeking; }
AVPacket *packet_queue_pop(PacketQueue *queue) { (void)queue; return NULL; }
void evo_boot_log(const char *format, ...) { (void)format; }
evo_adec_backend evo_adec_active(const evo_adec *decoder)
{ return decoder == (evo_adec *)(uintptr_t)1 ? EVO_ADEC_BACKEND_NATIVE : EVO_ADEC_BACKEND_FFMPEG; }
int evo_adec_decode(evo_adec *decoder, const uint8_t *data, int size, int16_t *pcm, int capacity)
{ (void)decoder; (void)data; (void)size; (void)pcm; (void)capacity; assert(0); return -1; }
int evo_adec_channels(const evo_adec *decoder) { (void)decoder; return 2; }
int evo_adec_rate(const evo_adec *decoder) { (void)decoder; return 48000; }
void evo_adec_close(evo_adec *decoder) { (void)decoder; }
void evo_speaker_cal_process(int16_t *pcm, int frames, int channels)
{ (void)pcm; (void)frames; (void)channels; }
int sceAudioOutOutput(int handle, const void *pcm)
{
    assert(handle == 1 && pcm);
    ++submissions;
    if (submissions >= stop_after) audio_thread_running = 0;
    return submissions == fail_at ? -1 : 0;
}

static void reset(void)
{
    prospero_audio_resampler_destroy();
    audio_queue_read = audio_queue_write = audio_queue_count = audio_accum_pos = 0;
    audio_samples_played = audio_samples_decoded = 0;
    audio_clock_seconds = 0;
    audio_handle = 1;
    video_decode_ready = video_decode_done = 1;
    video_clock_seconds = 0; first_video_pts_seconds = -1;
    video_stream_index = audio_stream_index = -1;
    audio_decode_thread_running = 1;
    seeking = submissions = fail_at = 0;
    audio_ctx = NULL; g_adec = NULL;
    evo_audio_drain_reset();
}

static AVFrame *frame(int samples, int rate)
{
    AVFrame *af = av_frame_alloc();
    assert(af);
    af->format = AV_SAMPLE_FMT_S16; af->sample_rate = rate; af->nb_samples = samples;
    av_channel_layout_default(&af->ch_layout, 2);
    assert(av_frame_get_buffer(af, 0) == 0);
    for (int i = 0; i < samples * 2; ++i) ((int16_t *)af->data[0])[i] = 1234;
    return af;
}

static long long collect(void)
{
    long long samples = 0;
    while (audio_queue_count) {
        int count = audio_queue_samples[audio_queue_read];
        assert(count > 0 && count <= AUDIO_BLOCK_SAMPLES);
        for (int i = count * 2; i < AUDIO_BLOCK_SAMPLES * 2; ++i)
            assert(audio_queue[audio_queue_read][i] == 0);
        samples += count;
        audio_queue_read = (audio_queue_read + 1) % AUDIO_QUEUE_BLOCKS;
        --audio_queue_count;
    }
    return samples;
}

static void partial_and_resampler_tail(void)
{
    reset();
    AVFrame *af = frame(257, 44100);
    mix_audio_frame_to_queue(af);
    assert(audio_queue_count == 0 && audio_accum_pos > 0 && audio_accum_pos < 280);
    long long before = audio_samples_decoded;
    uint64_t epoch = tube_audio_epoch(&tube_drain);
    assert(tube_audio_flush_tail(af, epoch) == 1);
    assert(audio_samples_decoded > before && audio_samples_decoded >= 279 && audio_samples_decoded <= 281);
    assert(audio_accum_pos == 0 && audio_queue_count == 1);
    const long long expected = audio_samples_decoded;
    tube_audio_mark(&tube_drain, epoch, TUBE_AUDIO_DECODED);
    assert(!evo_audio_is_drained());
    stop_after = 2; audio_thread_running = 1;
    audio_output_thread(NULL);
    assert(!evo_audio_is_drained()); // One silence block is insufficient.
    assert(audio_samples_played == expected);
    stop_after = submissions + 2; audio_thread_running = 1;
    audio_output_thread(NULL);
    assert(evo_audio_is_drained());
    assert(audio_samples_played == expected); // Padding/silence are not media.
    assert(fabs(audio_clock_seconds - (double)expected / 48000) < 1e-12);
    audio_stream_index = 0; audio_clock_seconds = 2;
    first_video_pts_seconds = 10; video_clock_seconds = 14;
    assert(fabs(evo_pb_position_s() - 4) < 1e-12); // Video outlasts the audio tail.
    assert(evo_audio_master_media_us(NULL) == -1); // Resume independent video pacing.
    evo_audio_drain_reset();
    tube_audio_mark(&tube_drain, epoch, TUBE_AUDIO_DECODED | TUBE_AUDIO_OUTPUT);
    assert(!evo_audio_is_drained()); // An old EOF cannot finish a new seek.
    assert(evo_pb_position_s() == 2); // Active audio remains the master.
    av_frame_free(&af);
}

static void output_error_and_native_pcm(void)
{
    reset();
    audio_stream_index = 0;
    AVFrame *af = frame(100, 48000);
    mix_audio_frame_to_queue(af);
    g_adec = (evo_adec *)(uintptr_t)1; // Native AU PCM already mixed synchronously.
    uint64_t epoch = tube_audio_epoch(&tube_drain);
    assert(tube_audio_flush_tail(af, epoch) == 1);
    tube_audio_mark(&tube_drain, epoch, TUBE_AUDIO_DECODED);
    fail_at = 1; stop_after = 3; audio_thread_running = 1;
    audio_output_thread(NULL);
    assert(!evo_audio_is_drained() && audio_samples_played == 0);
    assert(!evo_pb_audio_drained());
    assert(tube_audio_flags(&tube_drain, epoch) & TUBE_AUDIO_FAILED);
    g_adec = NULL;
    av_frame_free(&af);
}

static void actual_aac_decoder(const char *path)
{
    reset();
    assert(avformat_open_input(&play_fmt, path, NULL, NULL) == 0);
    assert(avformat_find_stream_info(play_fmt, NULL) >= 0);
    int index = av_find_best_stream(play_fmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    assert(index >= 0);
    const AVCodec *codec = avcodec_find_decoder(play_fmt->streams[index]->codecpar->codec_id);
    assert(codec);
    audio_ctx = avcodec_alloc_context3(codec);
    assert(audio_ctx && avcodec_parameters_to_context(audio_ctx, play_fmt->streams[index]->codecpar) == 0);
    assert(avcodec_open2(audio_ctx, codec, NULL) == 0);
    AVFrame *af = av_frame_alloc(); AVPacket *pkt = av_packet_alloc();
    assert(af && pkt);
    long long submitted = 0;
    while (av_read_frame(play_fmt, pkt) >= 0) {
        if (pkt->stream_index == index) {
            assert(avcodec_send_packet(audio_ctx, pkt) == 0);
            while (avcodec_receive_frame(audio_ctx, af) == 0) {
                mix_audio_frame_to_queue(af); submitted += collect(); av_frame_unref(af);
            }
        }
        av_packet_unref(pkt);
    }
    assert(audio_accum_pos > 0); // This real AAC stream has an incomplete block.
    uint64_t epoch = tube_audio_epoch(&tube_drain);
    assert(tube_audio_flush_tail(af, epoch) == 1);
    submitted += collect();
    assert(audio_accum_pos == 0 && submitted == audio_samples_decoded && submitted > 0);
    assert(avcodec_receive_frame(audio_ctx, af) == AVERROR_EOF);
    assert(tube_audio_flush_tail(af, epoch) == 1 && collect() == 0); // No tail replay.
    av_packet_free(&pkt); av_frame_free(&af);
    avcodec_free_context(&audio_ctx); avformat_close_input(&play_fmt);
}

static void seek_cancels_before_flush(void)
{
    reset();
    AVFrame *af = frame(100, 48000);
    mix_audio_frame_to_queue(af);
    int accumulated = audio_accum_pos;
    uint64_t epoch = tube_audio_epoch(&tube_drain);
    seeking = 1;
    assert(tube_audio_flush_tail(af, epoch) == 2);
    assert(audio_accum_pos == accumulated && audio_queue_count == 0);
    seeking = 0; evo_audio_drain_reset();
    assert(tube_audio_flush_tail(af, epoch) == 2);
    assert(!evo_audio_is_drained());
    av_frame_free(&af);
}

static void seek_parks_a_queue_blocked_producer(void)
{
    reset();
    audio_queue_count = 11; // Producer's steady-state high-water wait.
    pthread_t thread;
    assert(pthread_create(&thread, NULL, audio_decode_thread_func, NULL) == 0);
    unsigned wait = 0;
    while (evo_audio_decode_is_parked() && ++wait < 1000) usleep(1000);
    assert(!evo_audio_decode_is_parked());
    seeking = 1;
    wait = 0;
    while (!evo_audio_decode_is_parked() && ++wait < 1000) usleep(1000);
    assert(evo_audio_decode_is_parked());
    assert(audio_queue_count == 11 && !evo_audio_is_drained());
    audio_decode_thread_running = 0;
    assert(pthread_join(thread, NULL) == 0);
    assert(evo_audio_decode_is_parked());
    audio_queue_count = 0; seeking = 0;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    partial_and_resampler_tail(); output_error_and_native_pcm();
    actual_aac_decoder(argv[1]); seek_cancels_before_flush();
    seek_parks_a_queue_blocked_producer();
    prospero_audio_resampler_destroy();
    puts("PASS: actual EVO audio tail, resampler drain, AAC decoder EOF, counted padding, output failure and stale seek epochs");
    return 0;
}
