/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <cassert>
#include <cmath>
#include "tube_audio_seek.h"
#include "tube_seek_timeline.h"
extern "C" {
#include <libavcodec/avcodec.h>
}
static AVFormatContext *play_fmt;
static int video_stream_index = -1, audio_stream_index;
static tube_audio_seek tube_seek_pcm;
/* Observe the existing drain failure boundary; its atomic implementation has
 * independent C tests. The shipping acceptance helper and PCM gate are real. */
static struct { unsigned flags; } tube_drain;
static constexpr unsigned TUBE_AUDIO_FAILED = 4;
static uint64_t tube_audio_epoch(decltype(tube_drain) *) { return 0; }
static unsigned tube_audio_flags(decltype(tube_drain) *d, uint64_t) { return d->flags; }
static void tube_audio_mark(decltype(tube_drain) *d, uint64_t, unsigned f) { d->flags |= f; }
static void tube_audio_reset(decltype(tube_drain) *d) { d->flags = 0; }
static double audio_seek_discard_until, audio_pts_seconds, first_audio_pts_seconds;
static double first_kept;
static unsigned kept;
static void anchor_resume_base() {}
static void mix_audio_frame_to_queue(AVFrame *frame) {
    assert(frame->nb_samples > 0);
    if (!kept++) first_kept = audio_pts_seconds;
}
static int evo_audio_has_failed() { return !!(tube_audio_flags(&tube_drain, tube_audio_epoch(&tube_drain)) & TUBE_AUDIO_FAILED); }
#include "audio_accept.inc"

static void pcm(enum AVSampleFormat format) {
    AVFrame *frame = av_frame_alloc(); assert(frame);
    frame->format = format; frame->sample_rate = 48000; frame->nb_samples = 1024;
    av_channel_layout_default(&frame->ch_layout, 2);
    assert(av_frame_get_buffer(frame, 0) == 0);
    const bool planar = av_sample_fmt_is_planar(format);
    assert(av_get_bytes_per_sample(format) == 2);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 1024; ++i)
            ((int16_t *)frame->extended_data[planar ? ch : 0])[planar ? i : i * 2 + ch] = i + ch;
    AVFrame *shared = av_frame_clone(frame); assert(shared);
    tube_audio_seek state = {};
    double pts = 7, target = 1.010001;
    assert(tube_audio_seek_frame(&state, frame, &pts, 6, &target) == 1);
    assert(frame->nb_samples == 543 && target == -1);
    assert(pts >= 7.010001 && pts < 7.010001 + 1.0 / 48000);
    for (int ch = 0; ch < 2; ++ch) {
        assert(((int16_t *)frame->extended_data[planar ? ch : 0])[planar ? 0 : ch] == 481 + ch);
        assert(((int16_t *)shared->extended_data[planar ? ch : 0])[planar ? 0 : ch] == ch);
    }
    target = 10; pts = NAN;
    assert(tube_audio_seek_frame(&state, frame, &pts, 6, &target) == 0 && std::isfinite(pts));
    state = {}; pts = NAN;
    for (unsigned i = 0; i < 512; ++i)
        assert(tube_audio_seek_frame(&state, frame, &pts, 0, &target) == 0);
    assert(tube_audio_seek_frame(&state, frame, &pts, 0, &target) == -1);
    av_frame_free(&shared); av_frame_free(&frame);
}

static void decoded(const char *path) {
    assert(avformat_open_input(&play_fmt, path, nullptr, nullptr) == 0);
    assert(avformat_find_stream_info(play_fmt, nullptr) >= 0);
    audio_stream_index = av_find_best_stream(play_fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    assert(audio_stream_index >= 0);
    AVStream *stream = play_fmt->streams[audio_stream_index];
    AVCodecContext *codec = avcodec_alloc_context3(avcodec_find_decoder(stream->codecpar->codec_id));
    assert(codec && avcodec_parameters_to_context(codec, stream->codecpar) == 0);
    codec->pkt_timebase = stream->time_base;
    assert(avcodec_open2(codec, nullptr, nullptr) == 0);
    int64_t absolute, timestamp;
    assert(tube_seek_times(play_fmt, audio_stream_index, 3.25, &absolute, &timestamp));
    assert(av_seek_frame(play_fmt, audio_stream_index, timestamp, AVSEEK_FLAG_BACKWARD) >= 0);
    tube_seek_pcm = {}; tube_audio_reset(&tube_drain);
    audio_seek_discard_until = 4.250001; first_audio_pts_seconds = -1; kept = 0;
    AVPacket *packet = av_packet_alloc(); AVFrame *frame = av_frame_alloc(); assert(packet && frame);
    unsigned dropped = 0;
    while (av_read_frame(play_fmt, packet) >= 0) {
        if (packet->stream_index == audio_stream_index) {
            assert(avcodec_send_packet(codec, packet) == 0);
            int result;
            while ((result = avcodec_receive_frame(codec, frame)) == 0) {
                const unsigned before = kept;
                assert(tube_audio_accept_frame(frame));
                if (kept == before) ++dropped;
                av_frame_unref(frame);
            }
            assert(result == AVERROR(EAGAIN));
        }
        av_packet_unref(packet);
    }
    assert(avcodec_send_packet(codec, nullptr) == 0);
    while (avcodec_receive_frame(codec, frame) == 0) {
        assert(tube_audio_accept_frame(frame)); av_frame_unref(frame);
    }
    const double origin = (double)tube_media_origin_us(play_fmt, audio_stream_index) / AV_TIME_BASE;
    assert(dropped && kept && first_kept - origin >= 4.250001 - 1e-9);
    assert(first_kept - origin < 4.250001 + 1.0 / codec->sample_rate);
    assert(first_audio_pts_seconds == first_kept && audio_seek_discard_until == -1);
    /* A malformed decoded frame is a terminal audio failure, never output. */
    frame->sample_rate = 0;
    assert(!tube_audio_accept_frame(frame) && evo_audio_has_failed());
    av_frame_free(&frame); av_packet_free(&packet); avcodec_free_context(&codec); avformat_close_input(&play_fmt);
}
int main(int argc, char **argv) {
    assert(argc == 3); pcm(AV_SAMPLE_FMT_S16); pcm(AV_SAMPLE_FMT_S16P);
    decoded(argv[1]); decoded(argv[2]);
}
