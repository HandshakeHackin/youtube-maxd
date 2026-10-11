/* Seek-preview storyboards: level choice, sheet number and tile rectangle for a
 * time. A level is a set of JPEG sheets, each a grid of thumbnails filled row
 * by row (docs/PROTOCOL.md, "Second-batch request and response fields").
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_STORYBOARD_H
#define TUBE_STORYBOARD_H
#include <math.h>
#include <stddef.h>
#include <string.h>

#define TUBE_STORYBOARD_LEVELS 4
#define TUBE_STORYBOARD_URL 512
#define TUBE_STORYBOARD_MAX_INTERVAL_MS 3600000

typedef struct {
    int width, height;     /* one thumbnail, 1..1024 pixels */
    int columns, rows;     /* thumbnails per sheet, 1..32 each */
    int count;             /* thumbnails in the level */
    int sheets;            /* sheet images, 1..512 */
    int interval_ms;       /* time between thumbnails; 0 = spread over the duration */
    char url[TUBE_STORYBOARD_URL]; /* http(s) address with one literal "$M" */
} tube_storyboard_level;

typedef struct {
    int index;             /* thumbnail number in the level */
    int sheet;             /* sheet number, 0..sheets-1 */
    int x, y, width, height; /* rectangle inside that sheet, in pixels */
} tube_storyboard_tile;

static inline int tube_storyboard_valid(const tube_storyboard_level *level) {
    if (!level) return 0;
    if (level->width < 1 || level->width > 1024 || level->height < 1 || level->height > 1024) return 0;
    if (level->columns < 1 || level->columns > 32 || level->rows < 1 || level->rows > 32) return 0;
    if (level->sheets < 1 || level->sheets > 512 || level->count < 1) return 0;
    if (level->interval_ms < 0 || level->interval_ms > TUBE_STORYBOARD_MAX_INTERVAL_MS) return 0;
    const int per_sheet = level->columns * level->rows;
    /* The sheet count must be exactly what the thumbnail count needs. */
    if (level->count > per_sheet * level->sheets || level->count <= per_sheet * (level->sheets - 1)) return 0;
    if (!memchr(level->url, 0, sizeof level->url)) return 0;
    if (strncmp(level->url, "http://", 7) && strncmp(level->url, "https://", 8)) return 0;
    const char *mark = strstr(level->url, "$M");
    if (!mark || strstr(mark + 2, "$M")) return 0;
    for (const unsigned char *c = (const unsigned char *)level->url; *c; ++c)
        if (*c <= 0x20 || *c == 0x7f || *c == '"' || *c == '<' || *c == '>') return 0;
    return 1;
}

/* Smallest valid level at least as large as the target in both directions;
 * the largest valid level when none reaches it. -1 = no usable level. */
static inline int tube_storyboard_choose(const tube_storyboard_level *levels, int count,
                                         int target_width, int target_height) {
    int enough = -1, largest = -1;
    if (!levels) return -1;
    for (int i = 0; i < count; ++i) {
        if (!tube_storyboard_valid(&levels[i])) continue;
        const long area = (long)levels[i].width * levels[i].height;
        if (largest < 0 || area > (long)levels[largest].width * levels[largest].height) largest = i;
        if (levels[i].width >= target_width && levels[i].height >= target_height &&
            (enough < 0 || area < (long)levels[enough].width * levels[enough].height)) enough = i;
    }
    return enough >= 0 ? enough : largest;
}

/* Thumbnail number for a time. Times outside the video clamp to its ends. */
static inline int tube_storyboard_index(const tube_storyboard_level *level, double seconds, double duration) {
    if (!tube_storyboard_valid(level)) return -1;
    if (!isfinite(seconds) || seconds < 0) seconds = 0;
    double index;
    if (level->interval_ms > 0) index = floor(seconds * 1000.0 / level->interval_ms);
    else if (isfinite(duration) && duration > 0) index = floor(seconds / duration * level->count);
    else return -1;
    if (!(index < level->count)) index = level->count - 1;
    return (int)index;
}

/* Pixel size of a sheet; the last one holds only the remaining rows. */
static inline int tube_storyboard_sheet_size(const tube_storyboard_level *level, int sheet, int *width, int *height) {
    if (!tube_storyboard_valid(level) || sheet < 0 || sheet >= level->sheets) return 0;
    const int per_sheet = level->columns * level->rows;
    const int on_sheet = sheet == level->sheets - 1 ? level->count - per_sheet * sheet : per_sheet;
    const int rows = (on_sheet + level->columns - 1) / level->columns;
    if (width) *width = level->columns * level->width;
    if (height) *height = rows * level->height;
    return 1;
}

static inline int tube_storyboard_tile_at(const tube_storyboard_level *level, double seconds, double duration,
                                          tube_storyboard_tile *tile) {
    const int index = tube_storyboard_index(level, seconds, duration);
    if (index < 0 || !tile) return 0;
    const int per_sheet = level->columns * level->rows;
    const int cell = index % per_sheet;
    tile->index = index;
    tile->sheet = index / per_sheet;
    tile->x = cell % level->columns * level->width;
    tile->y = cell / level->columns * level->height;
    tile->width = level->width;
    tile->height = level->height;
    return 1;
}

/* Address of one sheet: the literal "$M" replaced by the sheet number. */
static inline int tube_storyboard_sheet_url(const tube_storyboard_level *level, int sheet, char *out, size_t size) {
    if (!out || !size) return 0;
    out[0] = 0;
    if (!tube_storyboard_valid(level) || sheet < 0 || sheet >= level->sheets) return 0;
    const char *mark = strstr(level->url, "$M");
    char number[12];
    int digits = 0, value = sheet;
    do { number[digits++] = (char)('0' + value % 10); value /= 10; } while (value);
    const size_t head = (size_t)(mark - level->url), tail = strlen(mark + 2);
    if (head + (size_t)digits + tail + 1 > size) return 0;
    memcpy(out, level->url, head);
    for (int i = 0; i < digits; ++i) out[head + (size_t)i] = number[digits - 1 - i];
    memcpy(out + head + (size_t)digits, mark + 2, tail + 1);
    return 1;
}

/* Scale a tile into a texture that was decoded at another size than the sheet. */
static inline int tube_storyboard_tile_scaled(const tube_storyboard_level *level, const tube_storyboard_tile *tile,
                                              int texture_width, int texture_height, tube_storyboard_tile *scaled) {
    int width, height;
    if (!tile || !scaled || texture_width < 1 || texture_height < 1 ||
        !tube_storyboard_sheet_size(level, tile->sheet, &width, &height)) return 0;
    *scaled = *tile;
    scaled->x = (int)((long long)tile->x * texture_width / width);
    scaled->y = (int)((long long)tile->y * texture_height / height);
    scaled->width = (int)((long long)(tile->x + tile->width) * texture_width / width) - scaled->x;
    scaled->height = (int)((long long)(tile->y + tile->height) * texture_height / height) - scaled->y;
    return scaled->width > 0 && scaled->height > 0;
}
#endif
