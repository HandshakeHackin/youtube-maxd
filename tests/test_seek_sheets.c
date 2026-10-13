/* Seek-preview sheets: the cache rules on their own, then the player's side
 * with a recorded fetch, a stepped clock and FFmpeg's real image decoders on
 * JPEG, WebP and PNG sheets (WebP under a .jpg name, as YouTube serves them).
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_seek_sheets.h"
#include "tube_player_extra.h"
#include "cJSON.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- The pure rules. ---- */
static void test_cache(void) {
    tube_sheet_cache c; memset(&c, 0, sizeof c); tube_sheets_reset(&c);
    int start[TUBE_SHEET_FETCHES];
    assert(c.wanted == -1 && tube_sheets_ready(&c, 0) == -1 && !tube_sheets_fetching(&c));
    /* The target must rest on a sheet before anything is fetched. */
    assert(tube_sheets_plan(&c, 3, 10, 1000, 120, start) == 0 && tube_sheets_plan(&c, 3, 10, 1119, 120, start) == 0);
    assert(tube_sheets_plan(&c, 3, 10, 1120, 120, start) == 2);
    assert(c.slots[start[0]].sheet == 3 && c.slots[start[1]].sheet == 4 && start[0] != start[1]);
    assert(c.slots[start[0]].state == TUBE_SHEET_FETCHING && c.slots[start[0]].ticket != c.slots[start[1]].ticket);
    const int s3 = start[0], s4 = start[1]; const unsigned t3 = c.slots[s3].ticket, t4 = c.slots[s4].ticket;
    /* Nothing is asked for twice, and never more than two requests at a time. */
    assert(tube_sheets_plan(&c, 3, 10, 2000, 120, start) == 0 && tube_sheets_fetching(&c) == 2);
    assert(tube_sheets_plan(&c, 7, 10, 2000, 120, start) == 0 && tube_sheets_plan(&c, 7, 10, 5000, 120, start) == 0);
    assert(tube_sheets_ready(&c, 3) == -1);
    assert(!tube_sheets_complete(&c, s3, t3 + 100, 1) && !tube_sheets_complete(&c, -1, t3, 1) && !tube_sheets_complete(&c, TUBE_SHEET_SLOTS, t3, 1));
    assert(tube_sheets_complete(&c, s3, t3, 1) && !tube_sheets_complete(&c, s3, t3, 1) && tube_sheets_ready(&c, 3) == s3);
    /* With one request back, the rested target (7) is fetched; its neighbour waits for the second. */
    assert(tube_sheets_plan(&c, 7, 10, 5001, 120, start) == 1 && c.slots[start[0]].sheet == 7);
    const int s7 = start[0]; const unsigned t7 = c.slots[s7].ticket;
    assert(tube_sheets_complete(&c, s4, t4, 0) && c.slots[s4].state == TUBE_SHEET_FAILED && tube_sheets_ready(&c, 4) == -1);
    assert(tube_sheets_plan(&c, 7, 10, 5002, 120, start) == 1 && c.slots[start[0]].sheet == 8);
    const int s8 = start[0]; const unsigned t8 = c.slots[s8].ticket;
    assert(tube_sheets_complete(&c, s7, t7, 1) && tube_sheets_complete(&c, s8, t8, 1));
    /* A failed sheet is not asked for again while it is remembered. */
    assert(tube_sheets_plan(&c, 4, 10, 6000, 120, start) == 0 && tube_sheets_plan(&c, 4, 10, 9000, 120, start) == 1 && c.slots[start[0]].sheet == 5);
    assert(tube_sheets_complete(&c, start[0], c.slots[start[0]].ticket, 1));
    /* Four slots: the least recently used sheet gives way, never the one under the target. */
    int held = 0; for (int i = 0; i < TUBE_SHEET_SLOTS; ++i) held += c.slots[i].state != TUBE_SHEET_EMPTY;
    assert(held == TUBE_SHEET_SLOTS);
    assert(tube_sheets_ready(&c, 7) >= 0 && tube_sheets_ready(&c, 8) >= 0 && tube_sheets_ready(&c, 5) >= 0);   /* 4 (failed) is the oldest */
    assert(tube_sheets_plan(&c, 0, 10, 10000, 120, start) == 0 && tube_sheets_plan(&c, 0, 10, 10200, 120, start) == 2);
    assert(c.slots[start[0]].sheet == 0 && c.slots[start[1]].sheet == 1 && tube_sheets_find(&c, 4) < 0 && tube_sheets_find(&c, 7) < 0);
    assert(tube_sheets_find(&c, 8) >= 0 && tube_sheets_find(&c, 5) >= 0);
    /* The last sheet has no neighbour to fetch; sheets outside the level are nothing. */
    tube_sheets_reset(&c);
    assert(tube_sheets_plan(&c, 9, 10, 0, 0, start) == 1 && c.slots[start[0]].sheet == 9);
    assert(tube_sheets_plan(&c, 10, 10, 0, 0, start) == 0 && tube_sheets_plan(&c, -1, 10, 0, 0, start) == 0);
    /* A reset orphans the requests under way. */
    const int s9 = start[0]; const unsigned t9 = c.slots[s9].ticket;
    tube_sheets_reset(&c); assert(!tube_sheets_complete(&c, s9, t9, 1) && c.slots[s9].state == TUBE_SHEET_EMPTY);
    /* A target that never rests fetches nothing, however long it moves. */
    for (int frame = 0; frame < 2000; ++frame) assert(tube_sheets_plan(&c, frame % 10, 10, (uint64_t)frame * 16, 120, start) == 0);
    assert(!tube_sheets_fetching(&c));
    /* A clock that went back starts the rest over instead of fetching at once. */
    assert(tube_sheets_plan(&c, 2, 10, 50000, 120, start) == 0 && tube_sheets_plan(&c, 2, 10, 40000, 120, start) == 0);
    assert(tube_sheets_plan(&c, 2, 10, 40119, 120, start) == 0 && tube_sheets_plan(&c, 2, 10, 40120, 120, start) == 2);
    assert(tube_sheets_debounce_ms(1) == 120 && tube_sheets_debounce_ms(2) == 250 && tube_sheets_debounce_ms(3) == 60);
}

static void test_level_and_layout(void) {
    tube_storyboard_level levels[4]; memset(levels, 0, sizeof levels);
    const struct { int w, h, columns, rows, count, sheets; } specs[4] = {
        {48, 27, 10, 10, 100, 1}, {160, 90, 5, 5, 120, 5}, {320, 180, 3, 3, 120, 14}, {1024, 576, 4, 4, 32, 2}};
    for (int i = 0; i < 4; ++i) {
        levels[i].width = specs[i].w; levels[i].height = specs[i].h; levels[i].columns = specs[i].columns; levels[i].rows = specs[i].rows;
        levels[i].count = specs[i].count; levels[i].sheets = specs[i].sheets; levels[i].interval_ms = 1000;
        snprintf(levels[i].url, sizeof levels[i].url, "http://127.0.0.1:8765/media/cap/sb%d-$M.jpg", i);
        assert(tube_storyboard_valid(&levels[i]));
    }
    assert(tube_sheets_level(levels, 4, 320, 180, TUBE_SHEET_MAX_PIXELS) == 2);
    assert(tube_sheets_level(levels, 2, 320, 180, TUBE_SHEET_MAX_PIXELS) == 1);          /* none large enough: the largest */
    assert(tube_sheets_level(levels + 3, 1, 320, 180, TUBE_SHEET_MAX_PIXELS) == -1);     /* 4096 x 2304 sheets are past the bound */
    assert(tube_sheets_level(levels, 4, 2000, 2000, TUBE_SHEET_MAX_PIXELS) == 2 && tube_sheets_level(levels, 4, 320, 180, 150000) == 0);
    assert(tube_sheets_level(NULL, 4, 320, 180, TUBE_SHEET_MAX_PIXELS) == -1 && tube_sheets_level(levels, 0, 320, 180, TUBE_SHEET_MAX_PIXELS) == -1);
    tube_storyboard_tile tile, by_time;
    assert(tube_sheets_tile(&levels[1], 37, &tile) && tile.sheet == 1 && tile.x == 2 * 160 && tile.y == 2 * 90 && tile.width == 160 && tile.height == 90);
    assert(tube_storyboard_tile_at(&levels[1], 37.5, 120, &by_time) && !memcmp(&tile, &by_time, sizeof tile));
    assert(!tube_sheets_tile(&levels[1], -1, &tile) && !tube_sheets_tile(&levels[1], 120, &tile) && !tube_sheets_tile(&levels[1], 3, NULL));
    /* One tile over the mark; five for the carousel; always inside the margins. */
    tube_seek_box boxes[TUBE_SEEK_TILES];
    assert(tube_seek_layout(1, 960, 868, 160, 90, boxes) == 1 && boxes[0].x == 800 && boxes[0].y == 688 && boxes[0].w == 320 && boxes[0].h == 180 && !boxes[0].offset);
    assert(tube_seek_layout(1, 0, 868, 160, 90, boxes) == 1 && boxes[0].x == 96);
    assert(tube_seek_layout(1, 1920, 868, 160, 90, boxes) == 1 && boxes[0].x + boxes[0].w == 1824);
    assert(tube_seek_layout(1, NAN, 868, 160, 90, boxes) == 1 && boxes[0].x == 96);
    for (int mode = 2; mode <= 3; ++mode) for (double mark = -50; mark <= 2000; mark += 37) {
        assert(tube_seek_layout(mode, mark, 868, 160, 90, boxes) == 5 && !boxes[0].offset);
        int offsets = 0;
        for (int i = 0; i < 5; ++i) {
            assert(boxes[i].x >= 96 - 1e-9 && boxes[i].x + boxes[i].w <= 1824 + 1e-9 && fabs(boxes[i].y + boxes[i].h - 868) < 1e-9);
            assert(boxes[i].offset >= -2 && boxes[i].offset <= 2); offsets |= 1 << (boxes[i].offset + 2);
            for (int k = 0; k < i; ++k) assert(boxes[i].x >= boxes[k].x + boxes[k].w || boxes[k].x >= boxes[i].x + boxes[i].w);
            if (i) assert(boxes[i].w == 192 && (boxes[i].offset < 0) == (boxes[i].x < boxes[0].x));
        }
        assert(offsets == 31);
    }
    assert(tube_seek_layout(1, 960, 868, 90, 160, boxes) == 1 && boxes[0].h == 240 && boxes[0].w == 135);   /* a tall tile is not taller than 240 */
    assert(!tube_seek_layout(0, 960, 868, 160, 90, boxes) && !tube_seek_layout(4, 960, 868, 160, 90, boxes) &&
           !tube_seek_layout(1, 960, 868, 0, 90, boxes) && !tube_seek_layout(1, 960, 868, 160, 90, NULL));
}

/* ---- The player's side. ---- */
static struct { char url[600]; int slot; unsigned ticket; } fetched[64];
static int fetch_count, fetch_refuse;
static uint64_t clock_ms;
static char *files[3]; static size_t file_sizes[3];
static int record_fetch(const char *url, int slot, unsigned ticket) {
    assert(fetch_count < 64);
    snprintf(fetched[fetch_count].url, sizeof fetched[fetch_count].url, "%s", url);
    fetched[fetch_count].slot = slot; fetched[fetch_count].ticket = ticket; ++fetch_count;
    return fetch_refuse ? -1 : 0;
}
static uint64_t stepped_clock(void) { return clock_ms; }
static int sheet_of(const char *url) { const char *dash = strrchr(url, '-'); assert(dash && strstr(url, ".jpg")); return atoi(dash + 1); }
/* Answer request n with a real sheet; the format rotates, the name stays .jpg. */
static void deliver(int n) {
    const int format = sheet_of(fetched[n].url) % 3;
    tube_seek_preview_deliver(fetched[n].slot, fetched[n].ticket, 1, files[format], file_sizes[format]);
}
static unsigned expected_rgb(int cell) { return (unsigned)(40 + cell * 37 % 200) << 16 | (unsigned)(40 + cell * 59 % 200) << 8 | (unsigned)(40 + cell * 83 % 200); }
static int near_rgb(uint32_t pixel, unsigned rgb, int tolerance) {
    return abs((int)(pixel & 255) - (int)(rgb >> 16 & 255)) <= tolerance && abs((int)(pixel >> 8 & 255) - (int)(rgb >> 8 & 255)) <= tolerance &&
           abs((int)(pixel >> 16 & 255) - (int)(rgb & 255)) <= tolerance && (pixel >> 24) == 255;
}
static void storyboards(const char *json) {
    cJSON *list = cJSON_Parse(json); assert(list);
    tube_extra_set_storyboards(list); cJSON_Delete(list);
}
static const char *kLevels = "["
    "{\"width\":48,\"height\":27,\"columns\":10,\"rows\":10,\"count\":120,\"sheets\":2,\"interval_ms\":1000,\"url\":\"http://127.0.0.1:8765/media/cap/sb0-$M.jpg\"},"
    "{\"width\":160,\"height\":90,\"columns\":5,\"rows\":5,\"count\":250,\"sheets\":10,\"interval_ms\":1000,\"url\":\"http://127.0.0.1:8765/media/cap/sb2-$M.jpg\"},"
    "{\"width\":1024,\"height\":576,\"columns\":4,\"rows\":4,\"count\":32,\"sheets\":2,\"interval_ms\":8000,\"url\":\"http://127.0.0.1:8765/media/cap/sb9-$M.jpg\"}]";
static int oversize_decode(const char *bytes, size_t size, uint32_t **pixels, int *width, int *height, long max_pixels) {
    (void)bytes; (void)size; (void)max_pixels;
    *width = 2048; *height = 1025; *pixels = calloc((size_t)*width * (size_t)*height, 4); return *pixels != NULL;
}

static void test_decode(void) {
    static const char *const names[] = {"JPEG", "WebP", "PNG"};
    for (int format = 0; format < 3; ++format) {
        uint32_t *pixels = NULL; int width = 0, height = 0;
        if (!tube_seek_sheet_decode(files[format], file_sizes[format], &pixels, &width, &height, TUBE_SHEET_MAX_PIXELS)) {
            fprintf(stderr, "%s sheet did not decode\n", names[format]); assert(0);
        }
        assert(width == 800 && height == 450 && pixels);           /* its own size: no downscale */
        for (int cell = 0; cell < 25; ++cell) {
            const uint32_t pixel = pixels[(size_t)(cell / 5 * 90 + 45) * 800 + (size_t)(cell % 5 * 160 + 80)];
            if (!near_rgb(pixel, expected_rgb(cell), format == 2 ? 0 : 14)) {
                fprintf(stderr, "%s cell %d: %08x, expected %06x\n", names[format], cell, pixel, expected_rgb(cell)); assert(0);
            }
        }
        free(pixels);
        /* The pixel bound refuses the picture; a cut file and other bytes do not decode. */
        assert(!tube_seek_sheet_decode(files[format], file_sizes[format], &pixels, &width, &height, 800L * 450 - 1) && !pixels);
        assert(!tube_seek_sheet_decode(files[format], 11, &pixels, &width, &height, TUBE_SHEET_MAX_PIXELS) && !pixels);
        /* A cut file is refused, or comes out as a whole picture of the right size: never anything else. */
        if (tube_seek_sheet_decode(files[format], file_sizes[format] / 8, &pixels, &width, &height, TUBE_SHEET_MAX_PIXELS)) {
            assert(pixels && width == 800 && height == 450); free(pixels); pixels = NULL;
        } else assert(!pixels);
    }
    uint32_t *pixels = NULL; int width, height;
    static char garbage[4096];
    for (size_t i = 0; i < sizeof garbage; ++i) garbage[i] = (char)(i * 31 + 7);
    assert(!tube_seek_sheet_decode(garbage, sizeof garbage, &pixels, &width, &height, TUBE_SHEET_MAX_PIXELS) && !pixels);
    memcpy(garbage, "\xff\xd8\xff\xe0", 4);
    assert(!tube_seek_sheet_decode(garbage, sizeof garbage, &pixels, &width, &height, TUBE_SHEET_MAX_PIXELS) && !pixels);
    assert(!tube_seek_sheet_decode(NULL, 100, &pixels, &width, &height, TUBE_SHEET_MAX_PIXELS));
    assert(!tube_seek_sheet_decode(files[0], TUBE_SHEET_MAX_BYTES + 1, &pixels, &width, &height, TUBE_SHEET_MAX_PIXELS));
}

static void test_player(void) {
    tube_seek_view view;
    TubePlayerExtra prefs; tube_player_extra_defaults(&prefs); tube_player_extra_select(&prefs);
    tube_seek_preview_hooks(record_fetch, NULL, stepped_clock);
    storyboards(kLevels);
    assert(tube_extra_storyboard_count() == 3);
    /* The preference decides: off draws and fetches nothing. */
    assert(tube_extra_set_seek_preview(0));
    clock_ms = 1000; assert(!tube_seek_preview_frame(30, 250, 960, 868, &view)); clock_ms = 5000;
    assert(!tube_seek_preview_frame(30, 250, 960, 868, &view) && !fetch_count && !view.count);
    assert(tube_extra_set_seek_preview(1));
    /* The target rests on second 30: tile 30 of the 160 x 90 level, sheet 1; the huge level is never used. */
    clock_ms = 10000; assert(!tube_seek_preview_frame(30, 250, 960, 868, &view) && !fetch_count);
    clock_ms = 10119; assert(!tube_seek_preview_frame(30, 250, 960, 868, &view) && !fetch_count);
    clock_ms = 10120; assert(!tube_seek_preview_frame(30, 250, 960, 868, &view) && fetch_count == 2);
    assert(!strcmp(fetched[0].url, "http://127.0.0.1:8765/media/cap/sb2-1.jpg") && !strcmp(fetched[1].url, "http://127.0.0.1:8765/media/cap/sb2-2.jpg"));
    for (int frame = 0; frame < 300; ++frame) { clock_ms += 16; assert(!tube_seek_preview_frame(30, 250, 960, 868, &view)); }
    assert(fetch_count == 2 && !tube_seek_preview_bytes());             /* waiting costs no further request */
    deliver(0);                                                           /* a WebP sheet named .jpg */
    assert(tube_seek_preview_bytes() == 800u * 450 * 4);
    assert(tube_seek_preview_frame(30, 250, 960, 868, &view) && view.count == 1);
    assert(view.tiles[0].sheet_width == 800 && view.tiles[0].sheet_height == 450 && view.tiles[0].pixels);
    assert(view.tiles[0].x == 0 && view.tiles[0].y == 90 && view.tiles[0].width == 160 && view.tiles[0].height == 90);   /* cell 5 of its sheet */
    assert(view.tiles[0].box.x == 800 && view.tiles[0].box.y == 688 && view.tiles[0].box.w == 320 && view.tiles[0].box.h == 180);
    assert(near_rgb(view.tiles[0].pixels[(size_t)(view.tiles[0].y + 45) * 800 + (size_t)(view.tiles[0].x + 80)], expected_rgb(5), 14));
    /* Moving inside the sheet draws at once and asks for nothing. */
    for (int second = 25; second < 50; ++second) {
        clock_ms += 16; assert(tube_seek_preview_frame(second, 250, 300 + second, 868, &view) && view.count == 1);
        assert(view.tiles[0].x == (second - 25) % 5 * 160 && view.tiles[0].y == (second - 25) / 5 * 90);
    }
    assert(fetch_count == 2);
    /* Scrubbing across the whole video without resting: no request at all. */
    for (int frame = 0; frame < 1000; ++frame) { clock_ms += 16; tube_seek_preview_frame(frame * 7 % 250, 250, 960, 868, &view); }
    assert(fetch_count == 2);
    /* The neighbour that was fetched with it is there when the target arrives. */
    deliver(1);
    clock_ms += 16; assert(tube_seek_preview_frame(55, 250, 960, 868, &view) && view.tiles[0].x == 0 && view.tiles[0].y == 90 && fetch_count == 2);
    assert(near_rgb(view.tiles[0].pixels[(size_t)(view.tiles[0].y + 45) * 800 + 80], expected_rgb(5), 14));
    clock_ms += 200; assert(tube_seek_preview_frame(55, 250, 960, 868, &view) && fetch_count == 3 && sheet_of(fetched[2].url) == 3);
    deliver(2);
    /* Four sheets at most: 8 MB each at the very most, these 1.44 MB each. */
    int next = fetch_count;
    for (int sheet = 4; sheet < 10; ++sheet) {
        clock_ms += 1000; tube_seek_preview_frame(sheet * 25 + 3, 250, 960, 868, &view);
        clock_ms += 200; tube_seek_preview_frame(sheet * 25 + 3, 250, 960, 868, &view);
        while (next < fetch_count) deliver(next++);
        assert(tube_seek_preview_frame(sheet * 25 + 3, 250, 960, 868, &view) && view.tiles[0].x == 3 * 160 && view.tiles[0].y == 0);
        assert(tube_seek_preview_bytes() <= (size_t)TUBE_SHEET_SLOTS * 800 * 450 * 4);
        assert(tube_seek_preview_bytes() <= (size_t)TUBE_SHEET_SLOTS * TUBE_SHEET_MAX_PIXELS * 4);
    }
    assert(tube_seek_preview_bytes() == (size_t)TUBE_SHEET_SLOTS * 800 * 450 * 4);
    /* A sheet that left the cache is fetched again when the target rests on it. */
    const int before = fetch_count;
    clock_ms += 1000; assert(!tube_seek_preview_frame(30, 250, 960, 868, &view));
    clock_ms += 120; assert(!tube_seek_preview_frame(30, 250, 960, 868, &view) && fetch_count > before && sheet_of(fetched[before].url) == 1);
    /* The player closes with requests under way: their answers are dropped, nothing stays. */
    tube_seek_preview_drop(); assert(!tube_seek_preview_bytes());
    for (int n = before; n < fetch_count; ++n) deliver(n);
    assert(!tube_seek_preview_bytes());
    clock_ms += 16; assert(!tube_seek_preview_frame(30, 250, 960, 868, &view));

    /* The carousel: the target's tile and the neighbours whose sheets are held. */
    assert(tube_extra_set_seek_preview(2));
    next = fetch_count;
    clock_ms += 1000; tube_seek_preview_frame(62, 250, 960, 868, &view);
    clock_ms += 249; tube_seek_preview_frame(62, 250, 960, 868, &view); assert(fetch_count == next);    /* the slow carousel rests longer */
    clock_ms += 1; tube_seek_preview_frame(62, 250, 960, 868, &view); assert(fetch_count == next + 2);
    while (next < fetch_count) deliver(next++);
    assert(tube_seek_preview_frame(62, 250, 960, 868, &view) && view.count == 5 && !view.tiles[0].box.offset);
    for (int i = 0; i < 5; ++i) {
        const int cell = 62 - 50 + view.tiles[i].box.offset;
        assert(view.tiles[i].x == cell % 5 * 160 && view.tiles[i].y == cell / 5 * 90);
        assert(near_rgb(view.tiles[i].pixels[(size_t)(view.tiles[i].y + 45) * 800 + (size_t)(view.tiles[i].x + 80)], expected_rgb(cell), 14));
    }
    /* At the start of a sheet the earlier neighbours belong to a sheet that is not held: they are left out. */
    assert(tube_seek_preview_frame(50, 250, 960, 868, &view) && view.count == 3);
    for (int i = 0; i < view.count; ++i) assert(view.tiles[i].box.offset >= 0);
    /* The first and the last second of the video have no tile beyond them. */
    assert(tube_extra_set_seek_preview(3));
    next = fetch_count;
    clock_ms += 1000; tube_seek_preview_frame(0, 250, 960, 868, &view); clock_ms += 60; tube_seek_preview_frame(0, 250, 960, 868, &view);
    while (next < fetch_count) deliver(next++);
    assert(tube_seek_preview_frame(0, 250, 960, 868, &view) && view.count == 3);
    clock_ms += 1000; tube_seek_preview_frame(1e9, 250, 960, 868, &view); clock_ms += 60; tube_seek_preview_frame(1e9, 250, 960, 868, &view);
    while (next < fetch_count) deliver(next++);
    assert(tube_seek_preview_frame(1e9, 250, 960, 868, &view) && view.count == 3 && view.tiles[0].x == 4 * 160 && view.tiles[0].y == 4 * 90);
    assert(tube_extra_set_seek_preview(1));

    /* A request that cannot go out, an answer that failed and bytes that are no picture leave no preview and no retry storm. */
    tube_seek_preview_drop(); fetch_refuse = 1; next = fetch_count;
    clock_ms += 1000; tube_seek_preview_frame(130, 250, 960, 868, &view);
    for (int frame = 0; frame < 200; ++frame) { clock_ms += 16; assert(!tube_seek_preview_frame(130, 250, 960, 868, &view)); }
    assert(fetch_count == next + 2);
    fetch_refuse = 0; tube_seek_preview_drop(); next = fetch_count;
    clock_ms += 1000; tube_seek_preview_frame(130, 250, 960, 868, &view); clock_ms += 120; tube_seek_preview_frame(130, 250, 960, 868, &view);
    assert(fetch_count == next + 2);
    tube_seek_preview_deliver(fetched[next].slot, fetched[next].ticket, 0, NULL, 0);
    tube_seek_preview_deliver(fetched[next + 1].slot, fetched[next + 1].ticket, 1, "not a picture at all", 20);
    for (int frame = 0; frame < 200; ++frame) { clock_ms += 16; assert(!tube_seek_preview_frame(130, 250, 960, 868, &view)); }
    assert(fetch_count == next + 2 && !tube_seek_preview_bytes());
    /* An answer for a slot that is not waiting, or with another ticket, is ignored. */
    tube_seek_preview_deliver(fetched[next].slot, fetched[next].ticket, 1, files[0], file_sizes[0]);
    tube_seek_preview_deliver(-1, 1, 1, files[0], file_sizes[0]); tube_seek_preview_deliver(TUBE_SHEET_SLOTS, 1, 1, files[0], file_sizes[0]);
    assert(!tube_seek_preview_bytes());
    /* A decoder that returns more pixels than the bound is refused all the same. */
    tube_seek_preview_drop(); tube_seek_preview_hooks(record_fetch, oversize_decode, stepped_clock); next = fetch_count;
    clock_ms += 1000; tube_seek_preview_frame(130, 250, 960, 868, &view); clock_ms += 120; tube_seek_preview_frame(130, 250, 960, 868, &view);
    while (next < fetch_count) deliver(next++);
    assert(!tube_seek_preview_frame(130, 250, 960, 868, &view) && !tube_seek_preview_bytes());
    tube_seek_preview_hooks(record_fetch, NULL, stepped_clock);

    /* Another video: its sheets are fetched afresh, the earlier ones are gone. */
    tube_seek_preview_drop(); next = fetch_count;
    clock_ms += 1000; tube_seek_preview_frame(30, 250, 960, 868, &view); clock_ms += 120; tube_seek_preview_frame(30, 250, 960, 868, &view);
    while (next < fetch_count) deliver(next++);
    assert(tube_seek_preview_frame(30, 250, 960, 868, &view) && tube_seek_preview_bytes() == 2u * 800 * 450 * 4);
    storyboards("[{\"width\":160,\"height\":90,\"columns\":5,\"rows\":5,\"count\":250,\"sheets\":10,\"interval_ms\":1000,\"url\":\"http://127.0.0.1:8765/media/other/sb2-$M.jpg\"}]");
    clock_ms += 16; assert(!tube_seek_preview_frame(30, 250, 960, 868, &view) && !tube_seek_preview_bytes());
    clock_ms += 120; tube_seek_preview_frame(30, 250, 960, 868, &view);
    assert(fetch_count == next + 2 && !strcmp(fetched[next].url, "http://127.0.0.1:8765/media/other/sb2-1.jpg"));
    while (next < fetch_count) deliver(next++);
    assert(tube_seek_preview_frame(30, 250, 960, 868, &view));
    /* A level that spreads its thumbnails over the duration, and a video without storyboards. */
    storyboards("[{\"width\":160,\"height\":90,\"columns\":5,\"rows\":5,\"count\":100,\"sheets\":4,\"interval_ms\":0,\"url\":\"http://127.0.0.1:8765/media/third/sb2-$M.jpg\"}]");
    next = fetch_count;
    clock_ms += 16; tube_seek_preview_frame(500, 1000, 960, 868, &view); clock_ms += 120; tube_seek_preview_frame(500, 1000, 960, 868, &view);
    assert(fetch_count == next + 2 && sheet_of(fetched[next].url) == 2);
    assert(!tube_seek_preview_frame(500, 0, 960, 868, &view) && !tube_seek_preview_frame(500, NAN, 960, 868, &view));   /* no duration: no tile */
    while (next < fetch_count) deliver(next++);
    assert(tube_seek_preview_frame(500, 1000, 960, 868, &view) && view.tiles[0].x == 0 && view.tiles[0].y == 0);
    tube_extra_video_end();
    clock_ms += 16; assert(!tube_seek_preview_frame(500, 1000, 960, 868, &view) && !tube_seek_preview_bytes());
    assert(!tube_seek_preview_frame(30, 250, 960, 868, NULL));
    tube_seek_preview_drop(); tube_seek_preview_hooks(NULL, NULL, NULL);
}

static void slurp(const char *path, int index) {
    FILE *file = fopen(path, "rb"); assert(file);
    assert(!fseek(file, 0, SEEK_END)); const long size = ftell(file); assert(size > 100 && !fseek(file, 0, SEEK_SET));
    files[index] = malloc((size_t)size); assert(files[index] && fread(files[index], 1, (size_t)size, file) == (size_t)size);
    file_sizes[index] = (size_t)size; fclose(file);
}

int main(int argc, char **argv) {
    assert(argc == 4);                                     /* sheet.jpg sheet.webp sheet.png */
    for (int i = 0; i < 3; ++i) slurp(argv[i + 1], i);
    assert(!memcmp(files[0], "\xff\xd8\xff", 3) && !memcmp(files[1], "RIFF", 4) && !memcmp(files[2], "\x89PNG", 4));
    test_cache(); test_level_and_layout(); test_decode(); test_player();
    for (int i = 0; i < 3; ++i) free(files[i]);
    puts("PASS: seek-preview sheets: rest before fetching, two requests at most, four sheets held, stale and failed answers, full-size JPEG/WebP/PNG decode, tiles and carousel");
    return 0;
}
