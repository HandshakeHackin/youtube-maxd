/* A pending seek must wake a live HLS read that is waiting for a new segment. */
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
#include "tube_hls_dvr.h"
/* Compile the generated SIO source: the callback is otherwise private. */
#include "evo_stream_io.c"

static _Atomic int seek_pending;
static void *request_seek(void *unused) {
    (void)unused;
    usleep(1000000);
    __atomic_store_n(&seek_pending, 1, __ATOMIC_RELEASE);
    return NULL;
}

int main(int argc, char **argv) {
    assert(argc == 2);
    av_log_set_level(AV_LOG_ERROR);
    evo_stream_io_ctx_t io = {0};
    AVFormatContext *format = avformat_alloc_context();
    assert(format);
    format->interrupt_callback = (AVIOInterruptCB){sio_interrupt_cb, &io};
    AVDictionary *options = NULL;
    av_dict_set(&options, "live_start_index", "-3", 0);
    av_dict_set(&options, "prosperotube_dvr", "1", 0);
    assert(avformat_open_input(&format, argv[1], NULL, &options) >= 0);
    av_dict_free(&options);
    assert(avformat_find_stream_info(format, NULL) >= 0);
    evo_stream_io_interrupt_on_seek(format, &seek_pending);

    pthread_t requester;
    assert(pthread_create(&requester, NULL, request_seek, NULL) == 0);
    AVPacket *packet = av_packet_alloc();
    assert(packet);
    int packets = 0, result;
    int64_t started = av_gettime_relative();
    while ((result = av_read_frame(format, packet)) >= 0) {
        ++packets;
        av_packet_unref(packet);
    }
    int64_t elapsed_us = av_gettime_relative() - started;
    pthread_join(requester, NULL);
    assert(packets > 0 && result < 0 && elapsed_us < 3000000);
    assert(evo_stream_io_take_seek_interrupt(format));
    __atomic_store_n(&seek_pending, 0, __ATOMIC_RELEASE);

    int64_t window[7];
    assert(av_hls_get_dvr_window(format, window));
    int video = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    assert(video >= 0);
    int64_t timestamp = av_rescale_q(window[3] + AV_TIME_BASE / 4,
                                     AV_TIME_BASE_Q, format->streams[video]->time_base);
    assert(av_seek_frame(format, video, timestamp, AVSEEK_FLAG_BACKWARD) >= 0);
    assert(av_read_frame(format, packet) >= 0);
    __atomic_store_n(&io.seek_interrupted, 1, __ATOMIC_RELEASE);
    io.aborted = 1;
    assert(!evo_stream_io_take_seek_interrupt(format));
    io.aborted = 0;
    evo_stream_io_cancel_pending_start();
    assert(!evo_stream_io_take_seek_interrupt(format));
    evo_stream_io_reset_start_cancel();
    evo_stream_io_interrupt_on_seek(format, NULL);
    av_packet_free(&packet);
    avformat_close_input(&format);
    printf("PASS: pending live DVR seek interrupted stalled HLS read in %lld ms and sought backward\n",
           (long long)(elapsed_us / 1000));
    return 0;
}
