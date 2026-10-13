// ProsperoTube Afterglow - the pictures the frame's own decoder does not read.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/app/picture_decode.hpp"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

#include <cstring>

namespace afterglow
{

namespace
{

constexpr std::size_t kLargestFile = 16u * 1024u * 1024u;
constexpr int kLargestSide = 4096;

} // namespace

bool is_webp(const std::string &bytes)
{
    return bytes.size() > 12 && std::memcmp(bytes.data(), "RIFF", 4) == 0 && std::memcmp(bytes.data() + 8, "WEBP", 4) == 0;
}

bool decode_webp(const std::string &bytes, int *width, int *height, std::vector<std::uint8_t> *rgba)
{
    if (rgba != nullptr)
        rgba->clear();
    if (width == nullptr || height == nullptr || rgba == nullptr || !is_webp(bytes) || bytes.size() > kLargestFile)
        return false;
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_WEBP);
    if (codec == nullptr)
        return false;
    AVCodecContext *context = avcodec_alloc_context3(codec);
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    SwsContext *scale = nullptr;
    bool ok = false;
    // The decoder takes the whole file as one packet and reads the container itself.
    if (context != nullptr && packet != nullptr && frame != nullptr)
    {
        context->thread_count = 1; // one picture, on the thread that draws: no worker threads for it
        if (avcodec_open2(context, codec, nullptr) == 0 && av_new_packet(packet, static_cast<int>(bytes.size())) == 0)
        {
            std::memcpy(packet->data, bytes.data(), bytes.size());
            int got = avcodec_send_packet(context, packet);
            if (got == 0)
            {
                got = avcodec_receive_frame(context, frame);
                if (got == AVERROR(EAGAIN) && avcodec_send_packet(context, nullptr) == 0)
                    got = avcodec_receive_frame(context, frame);
            }
            const int w = frame->width;
            const int h = frame->height;
            if (got == 0 && w > 0 && h > 0 && w <= kLargestSide && h <= kLargestSide)
            {
                scale = sws_getContext(w, h, static_cast<AVPixelFormat>(frame->format), w, h, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
                if (scale != nullptr)
                {
                    rgba->assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u, 0);
                    std::uint8_t *planes[4] = {rgba->data(), nullptr, nullptr, nullptr};
                    const int strides[4] = {w * 4, 0, 0, 0};
                    ok = sws_scale(scale, frame->data, frame->linesize, 0, h, planes, strides) == h;
                    *width = w;
                    *height = h;
                }
            }
        }
    }
    sws_freeContext(scale);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&context);
    if (!ok)
        rgba->clear();
    return ok;
}

} // namespace afterglow
