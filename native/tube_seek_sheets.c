/* Seek-preview sheets on the player's side: see tube_seek_sheets.h.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_seek_sheets.h"
#include "tube_player_extra.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef TUBE_SEEK_SHEETS_NO_NET
#include "tube_provider.h"
#include "evo_net.h"
#endif
#ifndef TUBE_SEEK_SHEETS_NO_FFMPEG
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#endif

static struct {
    tube_sheet_cache cache;
    tube_storyboard_level level;          /* the level the held sheets belong to */
    int has_level;
    uint32_t *pixels[TUBE_SHEET_SLOTS];
    int width[TUBE_SHEET_SLOTS], height[TUBE_SHEET_SLOTS];
    tube_seek_fetch_fn fetch;
    tube_seek_decode_fn decode;
    tube_seek_now_fn now;
} g;

static void release_slot(int slot) {
    free(g.pixels[slot]); g.pixels[slot] = NULL; g.width[slot] = g.height[slot] = 0;
}
void tube_seek_preview_drop(void) {
    for (int i = 0; i < TUBE_SHEET_SLOTS; ++i) release_slot(i);
    tube_sheets_reset(&g.cache);
    g.has_level = 0;
}
size_t tube_seek_preview_bytes(void) {
    size_t bytes = 0;
    for (int i = 0; i < TUBE_SHEET_SLOTS; ++i) bytes += (size_t)g.width[i] * (size_t)g.height[i] * 4;
    return bytes;
}

static uint64_t steady_ms(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) return 0;
    return (uint64_t)t.tv_sec * 1000u + (uint64_t)t.tv_nsec / 1000000u;
}

#ifndef TUBE_SEEK_SHEETS_NO_FFMPEG
/* The whole file is one packet for FFmpeg's image decoders. The type comes
 * from the bytes: YouTube serves WebP sheets under a .jpg name. */
int tube_seek_sheet_decode(const char *bytes, size_t size, uint32_t **pixels, int *width, int *height, long max_pixels) {
    enum AVCodecID id = AV_CODEC_ID_NONE;
    int ok = 0;
    if (!bytes || !pixels || !width || !height || size < 12 || size > TUBE_SHEET_MAX_BYTES || max_pixels < 1) return 0;
    *pixels = NULL; *width = *height = 0;
    if (!memcmp(bytes, "\xff\xd8\xff", 3)) id = AV_CODEC_ID_MJPEG;
    else if (!memcmp(bytes, "RIFF", 4) && !memcmp(bytes + 8, "WEBP", 4)) id = AV_CODEC_ID_WEBP;
    else if (!memcmp(bytes, "\x89PNG\r\n\x1a\n", 8)) id = AV_CODEC_ID_PNG;
    const AVCodec *codec = id == AV_CODEC_ID_NONE ? NULL : avcodec_find_decoder(id);
    if (!codec) return 0;
    AVCodecContext *context = avcodec_alloc_context3(codec);
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    struct SwsContext *scale = NULL;
    uint32_t *out = NULL;
    if (!context || !packet || !frame) goto done;
    context->max_pixels = max_pixels;      /* refused before any picture is allocated */
    context->thread_count = 1;
    context->err_recognition |= AV_EF_EXPLODE;   /* a damaged sheet is no sheet, where the decoder can tell */
    if (avcodec_open2(context, codec, NULL) < 0 || av_new_packet(packet, (int)size) < 0) goto done;
    memcpy(packet->data, bytes, size);
    if (avcodec_send_packet(context, packet) < 0) goto done;
    avcodec_send_packet(context, NULL);
    if (avcodec_receive_frame(context, frame) < 0 || frame->width < 1 || frame->height < 1 ||
        (long)frame->width * frame->height > max_pixels) goto done;
    scale = sws_getContext(frame->width, frame->height, (enum AVPixelFormat)frame->format,
                           frame->width, frame->height, AV_PIX_FMT_RGBA, SWS_POINT, NULL, NULL, NULL);
    out = malloc((size_t)frame->width * (size_t)frame->height * 4);
    if (!scale || !out) goto done;
    {
        uint8_t *planes[4] = {(uint8_t *)out, NULL, NULL, NULL};
        int strides[4] = {frame->width * 4, 0, 0, 0};
        if (sws_scale(scale, (const uint8_t *const *)frame->data, frame->linesize, 0, frame->height, planes, strides) != frame->height) goto done;
    }
    *pixels = out; out = NULL; *width = frame->width; *height = frame->height; ok = 1;
done:
    free(out);
    if (scale) sws_freeContext(scale);
    av_frame_free(&frame); av_packet_free(&packet); avcodec_free_context(&context);
    return ok;
}
#else
int tube_seek_sheet_decode(const char *bytes, size_t size, uint32_t **pixels, int *width, int *height, long max_pixels) {
    (void)bytes; (void)size; (void)pixels; (void)width; (void)height; (void)max_pixels;
    return 0;
}
#endif

void tube_seek_preview_deliver(int slot, unsigned ticket, int ok, const char *bytes, size_t size) {
    uint32_t *pixels = NULL; int width = 0, height = 0;
    if (slot < 0 || slot >= TUBE_SHEET_SLOTS || g.cache.slots[slot].state != TUBE_SHEET_FETCHING ||
        g.cache.slots[slot].ticket != ticket) return;                     /* a sheet nobody waits for any more */
    if (ok) ok = (g.decode ? g.decode : tube_seek_sheet_decode)(bytes, size, &pixels, &width, &height, TUBE_SHEET_MAX_PIXELS);
    if (ok && (width < 1 || height < 1 || (long)width * height > TUBE_SHEET_MAX_PIXELS || !pixels)) { free(pixels); pixels = NULL; ok = 0; }
    if (!tube_sheets_complete(&g.cache, slot, ticket, ok)) { free(pixels); return; }
    release_slot(slot);
    if (ok) { g.pixels[slot] = pixels; g.width[slot] = width; g.height[slot] = height; }
}

#ifndef TUBE_SEEK_SHEETS_NO_NET
typedef struct { int slot; unsigned ticket; } sheet_request;
static void on_sheet(int success, int status, const char *body, size_t size, void *user) {
    sheet_request *request = user;
    tube_seek_preview_deliver(request->slot, request->ticket, success && status == 200 && body && size, body, size);
    free(request);
}
/* Through EVO's HTTP client, as the artwork is: completions arrive on the
 * main thread from evo_net_poll(). Only addresses artwork may come from. */
static int net_fetch(const char *url, int slot, unsigned ticket) {
    if (!tube_art_url_allowed(url)) return -1;
    sheet_request *request = malloc(sizeof *request);
    if (!request) return -1;
    request->slot = slot; request->ticket = ticket;
    if (evo_net_request_async("GET", url, NULL, NULL, 0, on_sheet, request) != 0) { free(request); return -1; }
    return 0;
}
#else
static int net_fetch(const char *url, int slot, unsigned ticket) { (void)url; (void)slot; (void)ticket; return -1; }
#endif

void tube_seek_preview_hooks(tube_seek_fetch_fn fetch, tube_seek_decode_fn decode, tube_seek_now_fn now) {
    g.fetch = fetch; g.decode = decode; g.now = now;
}

int tube_seek_preview_frame(double seconds, double duration, double mark_x, double bottom, tube_seek_view *view) {
    tube_storyboard_level levels[TUBE_STORYBOARD_LEVELS];
    tube_storyboard_tile target;
    tube_seek_box boxes[TUBE_SEEK_TILES];
    int start[TUBE_SHEET_FETCHES], count = 0;
    const int mode = tube_extra_seek_preview();
    if (view) view->count = 0;
    if (!view || !mode) return 0;
    for (int i = 0; i < TUBE_STORYBOARD_LEVELS && i < tube_extra_storyboard_count(); ++i) levels[count++] = *tube_extra_storyboard(i);
    const int chosen = tube_sheets_level(levels, count, (int)TUBE_SEEK_WIDTH, 180, TUBE_SHEET_MAX_PIXELS);
    if (chosen < 0) { if (g.has_level) tube_seek_preview_drop(); return 0; }
    /* Another video, or another list for this one: its sheets are not these. */
    if (!g.has_level || memcmp(&g.level, &levels[chosen], sizeof g.level)) {
        tube_seek_preview_drop();
        memcpy(&g.level, &levels[chosen], sizeof g.level); g.has_level = 1;
    }
    if (!tube_storyboard_tile_at(&g.level, seconds, duration, &target)) return 0;
    const int fetches = tube_sheets_plan(&g.cache, target.sheet, g.level.sheets, (g.now ? g.now : steady_ms)(),
                                         tube_sheets_debounce_ms(mode), start);
    for (int i = 0; i < fetches; ++i) {
        char url[TUBE_STORYBOARD_URL + 16];
        const int slot = start[i]; const unsigned ticket = g.cache.slots[slot].ticket;
        release_slot(slot);
        if (!tube_storyboard_sheet_url(&g.level, g.cache.slots[slot].sheet, url, sizeof url) ||
            (g.fetch ? g.fetch : net_fetch)(url, slot, ticket) != 0)
            tube_sheets_complete(&g.cache, slot, ticket, 0);
    }
    const int shown = tube_seek_layout(mode, mark_x, bottom, g.level.width, g.level.height, boxes);
    for (int i = 0; i < shown; ++i) {
        tube_storyboard_tile tile, scaled;
        if (!tube_sheets_tile(&g.level, target.index + boxes[i].offset, &tile)) continue;
        const int slot = tube_sheets_ready(&g.cache, tile.sheet);
        if (slot < 0 || !tube_storyboard_tile_scaled(&g.level, &tile, g.width[slot], g.height[slot], &scaled)) {
            if (!i) { view->count = 0; return 0; }           /* without the target's tile there is no preview */
            continue;
        }
        tube_seek_tile_view *out = &view->tiles[view->count++];
        out->pixels = g.pixels[slot]; out->sheet_width = g.width[slot]; out->sheet_height = g.height[slot];
        out->x = scaled.x; out->y = scaled.y; out->width = scaled.width; out->height = scaled.height;
        out->box = boxes[i];
    }
    return view->count > 0;
}
