/* Seek-preview storyboard arithmetic: validation, level choice, sheets and tiles. */
#include "tube_storyboard.h"
#include <assert.h>
#include <stdio.h>

static tube_storyboard_level level(int width, int height, int columns, int rows, int count, int sheets, int interval) {
    tube_storyboard_level out = { width, height, columns, rows, count, sheets, interval, "http://192.168.1.2:8765/media/abc/sb1-$M.jpg" };
    return out;
}

int main(void) {
    tube_storyboard_level small = level(160, 90, 5, 5, 120, 5, 1000);
    assert(tube_storyboard_valid(&small));
    assert(!tube_storyboard_valid(NULL));

    /* Every bound of the service contract is enforced. */
    tube_storyboard_level bad = small; bad.width = 0; assert(!tube_storyboard_valid(&bad));
    bad = small; bad.height = 1025; assert(!tube_storyboard_valid(&bad));
    bad = small; bad.columns = 33; assert(!tube_storyboard_valid(&bad));
    bad = small; bad.rows = 0; assert(!tube_storyboard_valid(&bad));
    bad = small; bad.count = 0; assert(!tube_storyboard_valid(&bad));
    bad = small; bad.sheets = 513; assert(!tube_storyboard_valid(&bad));
    bad = small; bad.sheets = 4; assert(!tube_storyboard_valid(&bad));      /* 120 thumbnails need five sheets */
    bad = small; bad.sheets = 6; assert(!tube_storyboard_valid(&bad));      /* a sixth sheet would be empty */
    bad = small; bad.interval_ms = -1; assert(!tube_storyboard_valid(&bad));
    bad = small; snprintf(bad.url, sizeof bad.url, "http://host/sb.jpg"); assert(!tube_storyboard_valid(&bad));
    bad = small; snprintf(bad.url, sizeof bad.url, "http://host/$M/$M.jpg"); assert(!tube_storyboard_valid(&bad));
    bad = small; snprintf(bad.url, sizeof bad.url, "file:///data/$M.jpg"); assert(!tube_storyboard_valid(&bad));
    bad = small; snprintf(bad.url, sizeof bad.url, "http://host/a b$M.jpg"); assert(!tube_storyboard_valid(&bad));
    bad = small; memset(bad.url, 'a', sizeof bad.url); assert(!tube_storyboard_valid(&bad)); /* unterminated */

    /* Thumbnail n is in sheet n / 25, row cell / 5, column cell % 5. */
    tube_storyboard_tile tile;
    assert(tube_storyboard_tile_at(&small, 0, 120, &tile) && tile.index == 0 && tile.sheet == 0 && tile.x == 0 && tile.y == 0);
    assert(tile.width == 160 && tile.height == 90);
    assert(tube_storyboard_tile_at(&small, 7.999, 120, &tile) && tile.index == 7 && tile.sheet == 0 && tile.x == 320 && tile.y == 90);
    assert(tube_storyboard_tile_at(&small, 25, 120, &tile) && tile.index == 25 && tile.sheet == 1 && tile.x == 0 && tile.y == 0);
    assert(tube_storyboard_tile_at(&small, 119.5, 120, &tile) && tile.index == 119 && tile.sheet == 4 && tile.x == 640 && tile.y == 270);
    /* Out-of-range and non-finite times clamp to the ends. */
    assert(tube_storyboard_tile_at(&small, 5000, 120, &tile) && tile.index == 119);
    assert(tube_storyboard_tile_at(&small, -3, 120, &tile) && tile.index == 0);
    assert(tube_storyboard_tile_at(&small, NAN, 120, &tile) && tile.index == 0);
    assert(tube_storyboard_tile_at(&small, INFINITY, 120, &tile) && tile.index == 0);
    assert(!tube_storyboard_tile_at(&small, 1, 120, NULL));
    bad = small; bad.rows = 0; assert(!tube_storyboard_tile_at(&bad, 1, 120, &tile));

    /* Without an interval the thumbnails spread evenly over the duration. */
    tube_storyboard_level spread = level(80, 45, 10, 10, 100, 1, 0);
    assert(tube_storyboard_valid(&spread));
    assert(tube_storyboard_tile_at(&spread, 300, 600, &tile) && tile.index == 50 && tile.x == 0 && tile.y == 225);
    assert(tube_storyboard_tile_at(&spread, 600, 600, &tile) && tile.index == 99);
    assert(!tube_storyboard_tile_at(&spread, 10, 0, &tile));
    assert(!tube_storyboard_tile_at(&spread, 10, NAN, &tile));

    /* The last sheet holds only the rows it needs. */
    int width, height;
    assert(tube_storyboard_sheet_size(&small, 0, &width, &height) && width == 800 && height == 450);
    tube_storyboard_level partial = level(160, 90, 5, 5, 57, 3, 2000);
    assert(tube_storyboard_valid(&partial));
    assert(tube_storyboard_sheet_size(&partial, 2, &width, &height) && width == 800 && height == 180);
    assert(!tube_storyboard_sheet_size(&partial, 3, &width, &height) && !tube_storyboard_sheet_size(&partial, -1, &width, &height));
    assert(tube_storyboard_tile_at(&partial, 113, 114, &tile) && tile.index == 56 && tile.sheet == 2 && tile.x == 160 && tile.y == 90);

    /* A texture decoded at half size keeps the tile's proportions. */
    tube_storyboard_tile scaled;
    assert(tube_storyboard_tile_at(&small, 7, 120, &tile));
    assert(tube_storyboard_tile_scaled(&small, &tile, 400, 225, &scaled));
    assert(scaled.x == 160 && scaled.y == 45 && scaled.width == 80 && scaled.height == 45 && scaled.sheet == 0);
    assert(!tube_storyboard_tile_scaled(&small, &tile, 0, 225, &scaled));

    /* Sheet addresses replace the one "$M"; a short buffer fails cleanly. */
    char url[TUBE_STORYBOARD_URL];
    assert(tube_storyboard_sheet_url(&small, 0, url, sizeof url) && !strcmp(url, "http://192.168.1.2:8765/media/abc/sb1-0.jpg"));
    assert(tube_storyboard_sheet_url(&small, 4, url, sizeof url) && !strcmp(url, "http://192.168.1.2:8765/media/abc/sb1-4.jpg"));
    tube_storyboard_level many = level(160, 90, 1, 1, 512, 512, 1000);
    assert(tube_storyboard_sheet_url(&many, 511, url, sizeof url) && !strcmp(url, "http://192.168.1.2:8765/media/abc/sb1-511.jpg"));
    assert(!tube_storyboard_sheet_url(&small, 5, url, sizeof url) && !url[0]);
    assert(!tube_storyboard_sheet_url(&small, -1, url, sizeof url));
    char tiny[12];
    assert(!tube_storyboard_sheet_url(&small, 0, tiny, sizeof tiny) && !tiny[0]);
    assert(!tube_storyboard_sheet_url(&small, 0, NULL, 0));

    /* Level choice: the smallest that covers the target, else the largest. */
    tube_storyboard_level levels[4] = { level(48, 27, 10, 10, 100, 1, 0), small, level(320, 180, 3, 3, 120, 14, 1000), level(0, 0, 1, 1, 1, 1, 0) };
    assert(tube_storyboard_choose(levels, 4, 160, 90) == 1);
    assert(tube_storyboard_choose(levels, 4, 161, 90) == 2);
    assert(tube_storyboard_choose(levels, 4, 40, 20) == 0);
    assert(tube_storyboard_choose(levels, 4, 1920, 1080) == 2);
    assert(tube_storyboard_choose(levels + 3, 1, 160, 90) == -1);
    assert(tube_storyboard_choose(levels, 0, 160, 90) == -1 && tube_storyboard_choose(NULL, 4, 160, 90) == -1);
    puts("PASS: storyboard validation, level choice, sheet sizes, tiles, scaling and sheet addresses");
    return 0;
}
