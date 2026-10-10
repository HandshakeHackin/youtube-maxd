#include <assert.h>
#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include "tube_seek_timeline.h"

static double first_kept(const char *file, double seconds, int adjusted, enum AVMediaType type) {
    AVFormatContext *f = NULL;
    assert(avformat_open_input(&f, file, NULL, NULL) == 0);
    assert(avformat_find_stream_info(f, NULL) >= 0);
    int stream = av_find_best_stream(f, type, -1, -1, NULL, 0);
    assert(stream >= 0 && f->start_time != AV_NOPTS_VALUE);
    int64_t target = (int64_t)(seconds * AV_TIME_BASE);
    int64_t timestamp = av_rescale_q(target, AV_TIME_BASE_Q, f->streams[stream]->time_base);
    if (adjusted) assert(tube_seek_times(f, stream, seconds, &target, &timestamp));
    assert(av_seek_frame(f, stream, timestamp, AVSEEK_FLAG_BACKWARD) >= 0);
    AVPacket *packet = av_packet_alloc();
    assert(packet);
    double position = -1;
    while (av_read_frame(f, packet) >= 0) {
        if (packet->stream_index == stream && packet->pts != AV_NOPTS_VALUE) {
            int64_t pts = av_rescale_q(packet->pts, f->streams[stream]->time_base, AV_TIME_BASE_Q);
            if (pts >= target) {
                position = (double)(pts - f->start_time) / AV_TIME_BASE;
                av_packet_unref(packet);
                break;
            }
        }
        av_packet_unref(packet);
    }
    av_packet_free(&packet);
    avformat_close_input(&f);
    return position;
}
static void boundaries(void) {
    AVFormatContext *f = avformat_alloc_context();
    assert(f);
    AVStream *video = avformat_new_stream(f, NULL);
    AVStream *audio = avformat_new_stream(f, NULL);
    assert(video && audio);
    video->time_base = (AVRational){1, 90000};
    video->start_time = 132000;
    audio->time_base = (AVRational){1, 48000};
    audio->start_time = 69376;
    int64_t absolute = 777, timestamp = 888;
    f->start_time = 1445333;
    assert(tube_seek_times(f, 0, 3.25, &absolute, &timestamp));
    assert(absolute == 4695333 && timestamp == 422580);
    /* Audio/video keep one common origin despite their distinct start PTS. */
    assert(tube_media_origin_us(f, 1) == tube_media_origin_us(f, 0));
    assert(fabs(225376.0 / 48000 - (double)tube_media_origin_us(f, 0) / AV_TIME_BASE - 3.25) < .001);
    f->start_time = -1500000;
    assert(tube_seek_times(f, 0, 3.25, &absolute, &timestamp));
    assert(absolute == 1750000 && timestamp == 157500);
    f->start_time = AV_NOPTS_VALUE;
    assert(tube_seek_times(f, 0, 3.25, &absolute, &timestamp));
    assert(absolute == 4716667);
    video->start_time = AV_NOPTS_VALUE;
    assert(tube_seek_times(f, 0, 3.25, &absolute, &timestamp));
    assert(absolute == 3250000);
    const double invalid[] = {NAN, INFINITY, -1, 1000001};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        absolute = 777; timestamp = 888;
        assert(!tube_seek_times(f, 0, invalid[i], &absolute, &timestamp));
        assert(absolute == 777 && timestamp == 888);
    }
    f->start_time = INT64_MAX - 100;
    assert(!tube_seek_times(f, 0, 3.25, &absolute, &timestamp));
    f->start_time = 0; video->time_base.num = 0;
    assert(!tube_seek_times(f, 0, 3.25, &absolute, &timestamp));
    assert(!tube_seek_times(f, 2, 3.25, &absolute, &timestamp));
    assert(!tube_seek_times(NULL, 0, 3.25, &absolute, &timestamp));
    avformat_free_context(f);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    boundaries();
    const double target = 3.25;
    double old = first_kept(argv[1], target, 0, AVMEDIA_TYPE_VIDEO);
    double corrected = first_kept(argv[1], target, 1, AVMEDIA_TYPE_VIDEO);
    double audio = first_kept(argv[1], target, 1, AVMEDIA_TYPE_AUDIO);
    printf("{\"target_seconds\":%.6f,\"current_first_video_position\":%.6f,\"adjusted_first_video_position\":%.6f}\n", target, old, corrected);
    fflush(stdout);
    assert(old < target - 1);
    assert(corrected >= target && corrected < target + 0.2);
    assert(audio >= target && audio < target + 0.1);
}
