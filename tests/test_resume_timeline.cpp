#include <cassert>
#include <cmath>
#include <cstdint>
#include "tube_seek_timeline.h"
extern "C" {
#include <libavcodec/avcodec.h>
}

static AVFormatContext *play_fmt;
static int video_stream_index, audio_stream_index;
static void *g_vdec, *g_adec;
static AVCodecContext *audio_ctx;
static int video_packet_queue, audio_packet_queue;
static double first_audio_pts_seconds, resume_base_offset_seconds;
static double first_video_pts_seconds = -1, video_clock_seconds;
static volatile double resume_base_anchor_pending;
static int seek_calls, flush_calls;
static int64_t requested_timestamp;
static bool force_refusal;
static void evo_vdec_flush(void *) { ++flush_calls; }
static void evo_adec_flush(void *) {}
static void prospero_audio_resampler_reset() {}
static void evo_audio_drain_reset() {}
static void prospero_embedded_subtitle_reset() {}
static void packet_queue_clear(int *) {}
static void evo_boot_log(const char *, ...) {}
static int tracked_seek(AVFormatContext *format, int stream, int64_t timestamp, int flags) {
    ++seek_calls; requested_timestamp = timestamp;
    return force_refusal ? AVERROR(EIO) : av_seek_frame(format, stream, timestamp, flags);
}
#define av_seek_frame tracked_seek
static double shipping_resume(double resumeOffset) {
    bool wantResume = true;
    double m_resumeBaseOffset = resumeOffset;
    resume_base_offset_seconds = resumeOffset;
#include "resume_seek.inc"
    return m_resumeBaseOffset;
}
#undef av_seek_frame
#include "resume_anchor.inc"
#include "resume_video_anchor.inc"
static void shipping_video_clock(int64_t pts_us, bool seek_discarding) {
    struct { int64_t pts_us; } pf = {pts_us};
#include "resume_video_clock.inc"
}

static void decoded_video_only(const char *file) {
    assert(avformat_open_input(&play_fmt, file, nullptr, nullptr) == 0);
    assert(avformat_find_stream_info(play_fmt, nullptr) >= 0);
    video_stream_index = av_find_best_stream(play_fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    audio_stream_index = av_find_best_stream(play_fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    assert(video_stream_index >= 0 && audio_stream_index < 0);
    AVStream *stream = play_fmt->streams[video_stream_index];
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    assert(codec);
    AVCodecContext *decoder = avcodec_alloc_context3(codec);
    assert(decoder && avcodec_parameters_to_context(decoder, stream->codecpar) == 0);
    assert(avcodec_open2(decoder, codec, nullptr) == 0);
    resume_base_anchor_pending = -1; force_refusal = false;
    assert(shipping_resume(4.25) == 4.25 && resume_base_anchor_pending == 4.25);
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    assert(packet && frame);
    bool decoded = false;
    for (unsigned count = 0; count < 1000 && av_read_frame(play_fmt, packet) >= 0; ++count) {
        if (packet->stream_index == video_stream_index) {
            assert(avcodec_send_packet(decoder, packet) >= 0);
            if (avcodec_receive_frame(decoder, frame) == 0) {
                assert(frame->best_effort_timestamp != AV_NOPTS_VALUE);
                const int64_t pts_us = av_rescale_q(frame->best_effort_timestamp, stream->time_base, AV_TIME_BASE_Q);
                const double landing = (double)(pts_us - tube_media_origin_us(play_fmt, video_stream_index)) / AV_TIME_BASE;
                shipping_video_clock(pts_us, false);
                assert(landing >= 1.8 && landing <= 3.3);
                assert(std::fabs(resume_base_offset_seconds - landing) < 1e-6);
                assert(resume_base_anchor_pending == -1);
                decoded = true; av_packet_unref(packet); break;
            }
        }
        av_packet_unref(packet);
    }
    assert(decoded);
    av_frame_free(&frame); av_packet_free(&packet);
    avcodec_free_context(&decoder); avformat_close_input(&play_fmt);
}

int main(int argc, char **argv) {
    assert(argc == 3);
    assert(avformat_open_input(&play_fmt, argv[1], nullptr, nullptr) == 0);
    assert(avformat_find_stream_info(play_fmt, nullptr) >= 0);
    video_stream_index = av_find_best_stream(play_fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    audio_stream_index = av_find_best_stream(play_fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    assert(video_stream_index >= 0 && audio_stream_index >= 0);
    const double origin = (double)play_fmt->start_time / AV_TIME_BASE;
    assert(origin > 1);
    assert(shipping_resume(4.25) == 4.25);
    assert(seek_calls == 1 && flush_calls == 1 && resume_base_anchor_pending == 4.25);
    const int64_t expected = av_rescale_q(3250000 + play_fmt->start_time, AV_TIME_BASE_Q,
                                        play_fmt->streams[video_stream_index]->time_base);
    assert(requested_timestamp == expected);
    AVPacket *packet = av_packet_alloc();
    assert(packet);
    bool found = false;
    while (av_read_frame(play_fmt, packet) >= 0) {
        if (packet->stream_index == audio_stream_index && packet->pts != AV_NOPTS_VALUE) {
            first_audio_pts_seconds = packet->pts * av_q2d(play_fmt->streams[audio_stream_index]->time_base);
            found = true; av_packet_unref(packet); break;
        }
        av_packet_unref(packet);
    }
    assert(found);
    const double relative_landing = first_audio_pts_seconds - origin;
    assert(relative_landing >= 1.8 && relative_landing <= 3.3);
    anchor_resume_base();
    assert(std::fabs(resume_base_offset_seconds - relative_landing) < 1e-6);
    assert(resume_base_anchor_pending == -1);
    const double base = resume_base_offset_seconds;
    anchor_resume_base(); assert(resume_base_offset_seconds == base);
    /* A/V playback keeps audio ownership even when video arrives first. */
    resume_base_anchor_pending = 4.25;
    shipping_video_clock((int64_t)((origin + 2.1) * AV_TIME_BASE), false);
    assert(resume_base_anchor_pending == 4.25 && resume_base_offset_seconds == base);
    audio_stream_index = -1;
    assert(shipping_resume(4.25) == 4.25 && resume_base_anchor_pending == 4.25);
    shipping_video_clock(INT64_MIN, false);
    shipping_video_clock((int64_t)((origin + 2.1) * AV_TIME_BASE), true);
    assert(resume_base_anchor_pending == 4.25 && resume_base_offset_seconds == 4.25);
    shipping_video_clock((int64_t)((origin + 2.1) * AV_TIME_BASE), false);
    assert(std::fabs(resume_base_offset_seconds - 2.1) < 1e-6 && resume_base_anchor_pending == -1);
    shipping_video_clock((int64_t)((origin + 3.1) * AV_TIME_BASE), false);
    assert(std::fabs(resume_base_offset_seconds - 2.1) < 1e-6);
    resume_base_offset_seconds = resume_base_anchor_pending = 4.25;
    shipping_video_clock((int64_t)((origin + 40) * AV_TIME_BASE), false);
    assert(resume_base_offset_seconds == 4.25 && resume_base_anchor_pending == -1);
    resume_base_anchor_pending = NAN;
    shipping_video_clock((int64_t)((origin + 2.1) * AV_TIME_BASE), false);
    assert(resume_base_offset_seconds == 4.25);
    resume_base_anchor_pending = -1;
    force_refusal = true;
    assert(shipping_resume(4.25) == 0 && resume_base_offset_seconds == 0 && flush_calls == 2);
    force_refusal = false;
    const int calls = seek_calls;
    assert(shipping_resume(NAN) == 0 && seek_calls == calls);
    play_fmt->start_time = INT64_MAX - 100;
    assert(shipping_resume(4.25) == 0 && seek_calls == calls);
    av_packet_free(&packet);
    avformat_close_input(&play_fmt);
    decoded_video_only(argv[2]);
}
