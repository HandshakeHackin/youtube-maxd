#ifndef TUBE_SEEK_TIMELINE_H
#define TUBE_SEEK_TIMELINE_H

#include <math.h>
#include <stdint.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>

/* UI positions share the container origin; audio/video stream starts can
 * differ. Packet PTS, demux seeks and the video discard gate remain absolute. */
static inline int64_t tube_media_origin_us(const AVFormatContext *format, int stream)
{
    if (format->start_time != AV_NOPTS_VALUE) return format->start_time;
    const AVStream *selected = format->streams[stream];
    if (selected->start_time != AV_NOPTS_VALUE &&
        selected->time_base.num > 0 && selected->time_base.den > 0) {
        int64_t origin = av_rescale_q(selected->start_time, selected->time_base, AV_TIME_BASE_Q);
        if (origin != INT64_MIN) return origin;
    }
    return 0;
}

static inline int tube_seek_times(const AVFormatContext *format, int stream,
                                  double seconds, int64_t *absolute_us,
                                  int64_t *stream_timestamp)
{
    if (!format || stream < 0 || (unsigned)stream >= format->nb_streams ||
        !isfinite(seconds) || seconds < 0 || seconds > 1000000.0) return 0;
    AVRational base = format->streams[stream]->time_base;
    if (base.num <= 0 || base.den <= 0) return 0;
    int64_t relative = (int64_t)llround(seconds * AV_TIME_BASE);
    int64_t origin = tube_media_origin_us(format, stream);
    if (origin > INT64_MAX - relative) return 0;
    int64_t absolute = origin + relative;
    int64_t timestamp = av_rescale_q(absolute, AV_TIME_BASE_Q, base);
    if (timestamp == INT64_MIN) return 0;
    *absolute_us = absolute;
    *stream_timestamp = timestamp;
    return 1;
}
#endif
