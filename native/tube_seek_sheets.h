/* Seek-preview thumbnails: which storyboard sheets to hold while the viewer
 * scrubs, when to fetch them and where the tiles are drawn. A sheet is one
 * image with a grid of thumbnails (tube_storyboard.h); the player draws the
 * tile under the seek target, cut out of its sheet.
 *
 * The cache holds at most TUBE_SHEET_SLOTS decoded sheets of at most
 * TUBE_SHEET_MAX_PIXELS each (32 MB of RGBA at the very most, about 8 MB with
 * YouTube's usual sheets), least recently used first out. Scrubbing fetches
 * only the sheet under the target, once the target has rested on it for a
 * moment, and then the sheet after it; never more than two requests at a time.
 *
 * The first part of this file is pure and tested on its own; the second is the
 * player's side (tube_seek_sheets.c): fetch through EVO's HTTP client, decode
 * with the FFmpeg image decoders at full size, hand pixels to the controls.
 * Main/UI thread only.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_SEEK_SHEETS_H
#define TUBE_SEEK_SHEETS_H
#include <stddef.h>
#include <stdint.h>
#include "tube_storyboard.h"
#ifdef __cplusplus
extern "C" {
#endif

#define TUBE_SHEET_SLOTS 4
#define TUBE_SHEET_MAX_PIXELS 2097152L   /* 8 MB of RGBA */
#define TUBE_SHEET_MAX_BYTES 4194304u    /* an encoded sheet larger than this is refused */
#define TUBE_SHEET_FETCHES 2
#define TUBE_SEEK_TILES 5

enum { TUBE_SHEET_EMPTY = 0, TUBE_SHEET_FETCHING = 1, TUBE_SHEET_READY = 2, TUBE_SHEET_FAILED = 3 };
typedef struct {
    int state, sheet;
    unsigned ticket;      /* names the fetch that fills the slot */
    uint64_t used;        /* cache clock at the last use */
} tube_sheet_slot;
typedef struct {
    tube_sheet_slot slots[TUBE_SHEET_SLOTS];
    unsigned ticket;      /* grows with every fetch and every reset */
    uint64_t clock;
    int wanted;           /* sheet under the seek target, -1 = none yet */
    uint64_t wanted_since;
} tube_sheet_cache;

/* Forget every sheet. Fetches still under way no longer match their slot. */
static inline void tube_sheets_reset(tube_sheet_cache *cache) {
    for (int i = 0; i < TUBE_SHEET_SLOTS; ++i) { cache->slots[i].state = TUBE_SHEET_EMPTY; cache->slots[i].sheet = -1; cache->slots[i].ticket = 0; }
    cache->wanted = -1; cache->wanted_since = 0;
    if (!++cache->ticket) ++cache->ticket;
}
static inline int tube_sheets_find(const tube_sheet_cache *cache, int sheet) {
    for (int i = 0; i < TUBE_SHEET_SLOTS; ++i)
        if (cache->slots[i].state != TUBE_SHEET_EMPTY && cache->slots[i].sheet == sheet) return i;
    return -1;
}
/* The slot of a decoded sheet, marked as used now; -1 while it is not there. */
static inline int tube_sheets_ready(tube_sheet_cache *cache, int sheet) {
    const int slot = tube_sheets_find(cache, sheet);
    if (slot < 0 || cache->slots[slot].state != TUBE_SHEET_READY) return -1;
    cache->slots[slot].used = ++cache->clock;
    return slot;
}
static inline int tube_sheets_fetching(const tube_sheet_cache *cache) {
    int count = 0;
    for (int i = 0; i < TUBE_SHEET_SLOTS; ++i) count += cache->slots[i].state == TUBE_SHEET_FETCHING;
    return count;
}
/* A slot for another sheet: an empty one, else the least recently used one that
 * is neither being fetched nor holding the sheet under the target. */
static inline int tube_sheets_victim(const tube_sheet_cache *cache, int keep) {
    int victim = -1;
    for (int i = 0; i < TUBE_SHEET_SLOTS; ++i) {
        const tube_sheet_slot *slot = &cache->slots[i];
        if (slot->state == TUBE_SHEET_EMPTY) return i;
        if (slot->state == TUBE_SHEET_FETCHING || slot->sheet == keep) continue;
        if (victim < 0 || slot->used < cache->slots[victim].used) victim = i;
    }
    return victim;
}
static inline int tube_sheets_begin(tube_sheet_cache *cache, int sheet, int keep) {
    const int slot = tube_sheets_victim(cache, keep);
    if (slot < 0) return -1;
    if (!++cache->ticket) ++cache->ticket;
    cache->slots[slot].state = TUBE_SHEET_FETCHING; cache->slots[slot].sheet = sheet;
    cache->slots[slot].ticket = cache->ticket; cache->slots[slot].used = ++cache->clock;
    return slot;
}
/* One frame of scrubbing with `sheet` (0..sheets-1) under the target. Writes
 * the slots whose sheets are to be fetched now to start[] and returns how many:
 * the sheet under the target once the target stayed on it for debounce_ms, then
 * the one after it. A slot named here loses whatever it held. A failed sheet is
 * not asked for again until it leaves the cache. */
static inline int tube_sheets_plan(tube_sheet_cache *cache, int sheet, int sheets, uint64_t now_ms, unsigned debounce_ms,
                                   int start[TUBE_SHEET_FETCHES]) {
    int count = 0;
    if (sheet < 0 || sheet >= sheets) return 0;
    if (cache->wanted != sheet || now_ms < cache->wanted_since) { cache->wanted = sheet; cache->wanted_since = now_ms; }
    const int have = tube_sheets_find(cache, sheet);
    if (have >= 0) cache->slots[have].used = ++cache->clock;
    /* A target on the move asks for nothing, not even the sheet after a held one. */
    if (now_ms - cache->wanted_since < debounce_ms) return 0;
    if (have < 0) {
        if (tube_sheets_fetching(cache) >= TUBE_SHEET_FETCHES) return 0;
        const int slot = tube_sheets_begin(cache, sheet, sheet);
        if (slot < 0) return 0;
        start[count++] = slot;
    }
    if (sheet + 1 < sheets && tube_sheets_find(cache, sheet + 1) < 0 && tube_sheets_fetching(cache) < TUBE_SHEET_FETCHES) {
        const int slot = tube_sheets_begin(cache, sheet + 1, sheet);
        if (slot >= 0) start[count++] = slot;
    }
    return count;
}
/* A fetch ended. 1 = it was the one its slot waits for; 0 = stale, drop it. */
static inline int tube_sheets_complete(tube_sheet_cache *cache, int slot, unsigned ticket, int ok) {
    if (slot < 0 || slot >= TUBE_SHEET_SLOTS || cache->slots[slot].state != TUBE_SHEET_FETCHING ||
        cache->slots[slot].ticket != ticket) return 0;
    cache->slots[slot].state = ok ? TUBE_SHEET_READY : TUBE_SHEET_FAILED;
    return 1;
}
/* How long the target rests on a sheet before it is fetched, by the "Preview
 * while seeking" choice: single frame, carousel (slow), carousel (fast). */
static inline unsigned tube_sheets_debounce_ms(int mode) { return mode == 3 ? 60u : mode == 2 ? 250u : 120u; }

/* Level for a preview: among the levels whose sheets fit the pixel bound, the
 * smallest one at least as large as the target, else the largest. -1 = none. */
static inline int tube_sheets_level(const tube_storyboard_level *levels, int count, int target_width, int target_height,
                                    long max_pixels) {
    int enough = -1, largest = -1;
    for (int i = 0; levels && i < count; ++i) {
        if (!tube_storyboard_valid(&levels[i])) continue;
        const long area = (long)levels[i].width * levels[i].height;
        if (area * levels[i].columns * levels[i].rows > max_pixels) continue;
        if (largest < 0 || area > (long)levels[largest].width * levels[largest].height) largest = i;
        if (levels[i].width >= target_width && levels[i].height >= target_height &&
            (enough < 0 || area < (long)levels[enough].width * levels[enough].height)) enough = i;
    }
    return enough >= 0 ? enough : largest;
}
/* The tile of a thumbnail number. 1 = it exists. */
static inline int tube_sheets_tile(const tube_storyboard_level *level, int index, tube_storyboard_tile *tile) {
    if (!tube_storyboard_valid(level) || index < 0 || index >= level->count || !tile) return 0;
    const int per_sheet = level->columns * level->rows, cell = index % per_sheet;
    tile->index = index; tile->sheet = index / per_sheet;
    tile->x = cell % level->columns * level->width; tile->y = cell / level->columns * level->height;
    tile->width = level->width; tile->height = level->height;
    return 1;
}

/* Where the tiles go, in the controls' 1920 x 1080 space: the target's tile
 * centred over the seek mark with its lower edge at `bottom`, kept inside the
 * side margins; the carousel choices add two smaller neighbours on each side.
 * boxes[0] is the target's. Returns how many boxes. */
typedef struct { double x, y, w, h; int offset; } tube_seek_box;
#define TUBE_SEEK_WIDTH 320.0
#define TUBE_SEEK_SIDE 0.6
#define TUBE_SEEK_GAP 8.0
#define TUBE_SEEK_MARGIN 96.0
static inline int tube_seek_layout(int mode, double mark_x, double bottom, int tile_width, int tile_height, tube_seek_box *boxes) {
    if (mode < 1 || mode > 3 || tile_width < 1 || tile_height < 1 || !boxes) return 0;
    double height = TUBE_SEEK_WIDTH * tile_height / tile_width, width = TUBE_SEEK_WIDTH;
    if (height > 240.0) { width *= 240.0 / height; height = 240.0; }
    const int sides = mode == 1 ? 0 : 2;
    const double side_w = width * TUBE_SEEK_SIDE, side_h = height * TUBE_SEEK_SIDE;
    const double reach = width / 2 + sides * (side_w + TUBE_SEEK_GAP);
    double centre = mark_x;
    if (!(centre >= TUBE_SEEK_MARGIN + reach)) centre = TUBE_SEEK_MARGIN + reach;
    if (centre > 1920.0 - TUBE_SEEK_MARGIN - reach) centre = 1920.0 - TUBE_SEEK_MARGIN - reach;
    int count = 0;
    boxes[count].x = centre - width / 2; boxes[count].y = bottom - height; boxes[count].w = width; boxes[count].h = height;
    boxes[count++].offset = 0;
    for (int k = 1; k <= sides; ++k) for (int sign = -1; sign <= 1; sign += 2) {
        const double edge = width / 2 + TUBE_SEEK_GAP + (k - 1) * (side_w + TUBE_SEEK_GAP);
        boxes[count].x = sign < 0 ? centre - edge - side_w : centre + edge;
        boxes[count].y = bottom - side_h; boxes[count].w = side_w; boxes[count].h = side_h;
        boxes[count++].offset = sign * k;
    }
    return count;
}

/* ---- The player's side (tube_seek_sheets.c). ---- */
/* One tile to draw: its sheet's pixels (R,G,B,A bytes, top row first, valid
 * until the next call into this module), the tile's rectangle inside them and
 * where it goes. */
typedef struct {
    const uint32_t *pixels;
    int sheet_width, sheet_height;
    int x, y, width, height;
    tube_seek_box box;
} tube_seek_tile_view;
typedef struct { int count; tube_seek_tile_view tiles[TUBE_SEEK_TILES]; } tube_seek_view;
/* Call once per drawn frame while a seek target is shown. mark_x and bottom
 * place the tiles (tube_seek_layout). 1 = the target's tile is in view->tiles[0]
 * (neighbours follow when their sheets are held); 0 = nothing to draw yet, or
 * previews are off or this video has none. Starts the fetches it needs. */
int tube_seek_preview_frame(double seconds, double duration, double mark_x, double bottom, tube_seek_view *view);
/* Release every sheet (the video ended). */
void tube_seek_preview_drop(void);
/* Decoded bytes held now; never above TUBE_SHEET_SLOTS * TUBE_SHEET_MAX_PIXELS * 4. */
size_t tube_seek_preview_bytes(void);
/* A fetch ended: the completion path of the built-in fetch, also for tests. */
void tube_seek_preview_deliver(int slot, unsigned ticket, int ok, const char *bytes, size_t size);
/* An encoded sheet to RGBA at its own size, refused above max_pixels. The
 * caller frees *pixels. 1 = decoded. */
int tube_seek_sheet_decode(const char *bytes, size_t size, uint32_t **pixels, int *width, int *height, long max_pixels);
/* Stand-ins for tests; NULL restores the built-in one. fetch returns 0 when
 * the request went out; now returns milliseconds on a steady clock. */
typedef int (*tube_seek_fetch_fn)(const char *url, int slot, unsigned ticket);
typedef int (*tube_seek_decode_fn)(const char *bytes, size_t size, uint32_t **pixels, int *width, int *height, long max_pixels);
typedef uint64_t (*tube_seek_now_fn)(void);
void tube_seek_preview_hooks(tube_seek_fetch_fn fetch, tube_seek_decode_fn decode, tube_seek_now_fn now);
#ifdef __cplusplus
}
#endif
#endif
