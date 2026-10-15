/* Bounded offline VP9 reference: packet identity and sparse decoded luma only.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/sha.h>
#include "tube_luma_diag.h"

static void noop_flush(const void *p, size_t n) { (void)p; (void)n; }
static int64_t microseconds(int64_t pts, AVRational time_base) {
    return pts == AV_NOPTS_VALUE ? INT64_MIN : av_rescale_q(pts, time_base, AV_TIME_BASE_Q);
}
static int receive_frames(AVCodecContext *decoder, AVFrame *frame, FILE *file,
    AVRational time_base, uint64_t *sequence, int64_t end_us, uint64_t max_frames) {
    int rc;
    while ((rc = avcodec_receive_frame(decoder, frame)) >= 0) {
        if (*sequence >= max_frames) { av_frame_unref(frame); return AVERROR(EFBIG); }
        const int64_t pts = microseconds(frame->pts, time_base);
        if (frame->format != AV_PIX_FMT_YUV420P || frame->width < 64 ||
            frame->height <= 0 || frame->width > INT_MAX - 63 ||
            !frame->data[0] || frame->linesize[0] < frame->width) {
            fprintf(stderr, "Only positive-stride yuv420p VP9, width >=64, is supported\n");
            av_frame_unref(frame); return AVERROR(EINVAL);
        }
        if (pts == INT64_MIN || pts < end_us) {
            const int pitch = (frame->width + 63) & ~63;
            if ((size_t)frame->height > SIZE_MAX / (size_t)pitch) {
                av_frame_unref(frame); return AVERROR(ENOMEM);
            }
            const size_t bytes = (size_t)pitch * frame->height;
            uint8_t *y = aligned_alloc(64u, bytes);
            if (!y) { av_frame_unref(frame); return AVERROR(ENOMEM); }
            for (int row = 0; row < frame->height; ++row)
                memcpy(y + (size_t)row * pitch,
                    frame->data[0] + (size_t)row * frame->linesize[0], (size_t)frame->width);
            uint64_t hash = 0;
            const int valid = tube_luma_diag_sample(y, pitch, frame->width, frame->height,
                0, noop_flush, &hash);
            free(y);
            if (!valid || fprintf(file, "%" PRIu64 ",%" PRId64 ",%d,%d,%d,0,%d,%" PRIu64 "\n",
                ++*sequence, pts, frame->width, frame->height, pitch, valid, hash) < 0) {
                av_frame_unref(frame); return AVERROR(EIO);
            }
        }
        av_frame_unref(frame);
    }
    return rc == AVERROR(EAGAIN) || rc == AVERROR_EOF ? 0 : rc;
}

int main(int argc, char **argv) {
    if (argc < 4 || argc > 5) {
        fprintf(stderr, "usage: %s INPUT FRAMES.csv PACKETS.csv [SECONDS=36]\n", argv[0]);
        return 2;
    }
    char *tail = NULL;
    double seconds = argc == 5 ? strtod(argv[4], &tail) : 36.0;
    if ((tail && (*tail || tail == argv[4])) || !isfinite(seconds) || seconds <= 0 || seconds > 3600 ||
        !strcmp(argv[1], argv[2]) || !strcmp(argv[1], argv[3]) || !strcmp(argv[2], argv[3])) {
        fprintf(stderr, "Require distinct paths and a duration in (0,3600] seconds\n"); return 2;
    }
    AVFormatContext *input = NULL;
    AVCodecContext *decoder = NULL;
    AVPacket *packet = NULL;
    AVFrame *frame = NULL;
    struct AVSHA *sha = NULL;
    FILE *frames = NULL, *packets = NULL;
    uint64_t frame_sequence = 0, packet_sequence = 0;
    int rc = avformat_open_input(&input, argv[1], NULL, NULL);
    if (rc < 0) goto done;
    if ((rc = avformat_find_stream_info(input, NULL)) < 0) goto done;
    int stream = av_find_best_stream(input, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (stream < 0) { rc = stream; goto done; }
    AVStream *video = input->streams[stream];
    if (video->codecpar->codec_id != AV_CODEC_ID_VP9) { rc = AVERROR(EINVAL); goto done; }
    const AVCodec *codec = avcodec_find_decoder_by_name("vp9");
    if (!codec) { rc = AVERROR_DECODER_NOT_FOUND; goto done; }
    decoder = avcodec_alloc_context3(codec);
    packet = av_packet_alloc(); frame = av_frame_alloc(); sha = av_sha_alloc();
    if (!decoder || !packet || !frame || !sha) { rc = AVERROR(ENOMEM); goto done; }
    if ((rc = avcodec_parameters_to_context(decoder, video->codecpar)) < 0) goto done;
    decoder->thread_count = 1;
    decoder->pkt_timebase = video->time_base;
    if ((rc = avcodec_open2(decoder, codec, NULL)) < 0) goto done;
    /* Exclusive creation avoids overwriting evidence or the media via a path alias. */
    frames = fopen(argv[2], "wx"); packets = fopen(argv[3], "wx");
    if (!frames || !packets) { rc = AVERROR(errno); goto done; }
    if (fprintf(frames, "sequence,pts_us,width,height,pitch_bytes,ten_bit,hash_valid,luma_hash\n") < 0 ||
        fprintf(packets, "packet,pts_us,dts_us,size,sha256_valid,sha256\n") < 0) {
        rc = AVERROR(EIO); goto done;
    }
    int64_t end_us = INT64_MAX;
    int bounded = 0;
    const uint64_t max_packets = (uint64_t)(seconds * 240.0) + 1024u;
    uint64_t demux_packets = 0;
    while ((rc = av_read_frame(input, packet)) >= 0) {
        if (++demux_packets > max_packets * 16u || packet_sequence >= max_packets) { rc = AVERROR(EFBIG); goto done; }
        if (packet->stream_index != stream) { av_packet_unref(packet); continue; }
        const int64_t pts = microseconds(packet->pts, video->time_base);
        const int64_t dts = microseconds(packet->dts, video->time_base);
        const int64_t time = pts != INT64_MIN ? pts : dts;
        if (!packet_sequence) {
            if (time == INT64_MIN || time > INT64_MAX - (int64_t)(seconds * 1000000.0)) {
                rc = AVERROR(EINVAL); goto done;
            }
            end_us = time + (int64_t)(seconds * 1000000.0);
        }
        if (time != INT64_MIN && time >= end_us) { bounded = 1; break; }
        /* Hash the original demux packet before decoding or any superframe split. */
        uint8_t digest[32]; char hex[65];
        av_sha_init(sha, 256); av_sha_update(sha, packet->data, packet->size); av_sha_final(sha, digest);
        for (unsigned i = 0; i < 32; ++i) snprintf(hex + i * 2u, 3, "%02x", digest[i]);
        if ((rc = avcodec_send_packet(decoder, packet)) < 0) goto done;
        if (fprintf(packets, "%" PRIu64 ",%" PRId64 ",%" PRId64 ",%d,1,%s\n",
            ++packet_sequence, pts, dts, packet->size, hex) < 0) { rc = AVERROR(EIO); goto done; }
        av_packet_unref(packet);
        if ((rc = receive_frames(decoder, frame, frames, video->time_base, &frame_sequence, end_us, max_packets * 8u)) < 0) goto done;
    }
    if (!bounded && rc != AVERROR_EOF) goto done;
    rc = avcodec_send_packet(decoder, NULL);
    if (rc >= 0) rc = receive_frames(decoder, frame, frames, video->time_base, &frame_sequence, end_us, max_packets * 8u);
    if (rc >= 0 && (!packet_sequence || !frame_sequence)) rc = AVERROR_INVALIDDATA;
done:
    if (frames && fclose(frames) != 0) rc = AVERROR(EIO);
    if (packets && fclose(packets) != 0) rc = AVERROR(EIO);
    av_free(sha); av_frame_free(&frame); av_packet_free(&packet);
    avcodec_free_context(&decoder); avformat_close_input(&input);
    if (rc < 0) {
        char message[AV_ERROR_MAX_STRING_SIZE]; av_strerror(rc, message, sizeof message);
        fprintf(stderr, "VP9 reference failed: %s (partial CSVs may exist)\n", message); return 1;
    }
    fprintf(stderr, "VP9 reference: %" PRIu64 " packets, %" PRIu64 " frames\n", packet_sequence, frame_sequence);
    return 0;
}
