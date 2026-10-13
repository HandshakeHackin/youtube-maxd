/* ProsperoTube interface preferences of one profile and what follows from them:
 * the browser's grid, the style rules added to native/youtube.rcss, themes and
 * classic colour schemes, the title marquee, the order of the video actions menu and the
 * Circle rules of the main menu and of search results. Pure: no I/O, no state.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_UI_STYLE_H
#define TUBE_UI_STYLE_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Rows of the video actions menu, in the order the menu had before it could be
 * arranged. The ids are saved in settings files: never renumber or rename. */
#define TUBE_MENU_KEYS 33
static const struct { const char *id, *label; } g_tube_menu_keys[TUBE_MENU_KEYS] = {
    {"like", "Like video"}, {"dislike", "Dislike video"}, {"unrate", "Remove video rating"},
    {"watch-later-add", "Add to YouTube Watch later"}, {"watch-later-remove", "Remove from YouTube Watch later"},
    {"subscribe", "Subscribe on YouTube"}, {"unsubscribe", "Unsubscribe on YouTube"},
    {"history-remove", "Remove from YouTube history"}, {"mark-watched", "Mark as watched on YouTube"},
    {"not-interested", "Not interested"}, {"dont-recommend", "Don't recommend channel"}, {"hide", "Hide from this list"},
    {"playlist-add", "Add to a YouTube playlist"}, {"playlist-remove", "Remove from a YouTube playlist"},
    {"playlist-create", "Create playlist with this video"}, {"recent-add", "Add to the last used playlist"},
    {"recent-remove", "Remove from the last used playlist"}, {"reminder", "Stream reminder"},
    {"notifications", "Subscription notifications"}, {"play-next", "Play next"},
    {"watched-local", "Mark as watched locally"}, {"forget-local", "Remove from local history"},
    {"related", "Related videos"}, {"block", "Block or unblock channel"}, {"description", "Open description"},
    {"details", "Video details"}, {"restart", "Play from start"}, {"incognito", "Play incognito"},
    {"open-channel", "Open channel"}, {"open-playlist", "Open playlist"}, {"share", "Share link"},
    {"sponsorblock-channel", "SponsorBlock on this channel"}, {"refresh", "Refresh section"}
};

/* One profile's preferences. Defaults reproduce the browser without this module. */
typedef struct TubeUiPrefs {
    int ui_scale;        /* percent, SmartTube's list 40..140 */
    int grid_scale;      /* percent, SmartTube's list 50..150 */
    int title_lines;     /* 1..4 lines of a card title */
    int title_scroll;    /* 0/1: the focused card's long title scrolls */
    int scroll_speed;    /* tenths, 10..80 step 5 */
    int subtitle_lines;  /* 1..2 lines of a card subtitle */
    int square;          /* 0/1: square card corners instead of rounded ones */
    int scheme;          /* index into g_tube_schemes: a theme, or a classic colour scheme */
    int app_exit;        /* TUBE_UI_EXIT_*: Circle on the main menu */
    int search_exit;     /* 0 = one press of Circle leaves search results, 1 = two */
    unsigned char menu_order[TUBE_MENU_KEYS]; /* a permutation of the menu keys */
    uint64_t menu_hidden;                     /* bit per menu key */
} TubeUiPrefs;

enum { TUBE_UI_EXIT_NONE = 0, TUBE_UI_EXIT_DOUBLE = 1, TUBE_UI_EXIT_SINGLE = 2 };
enum { TUBE_UI_PRESS_PASS = 0, TUBE_UI_PRESS_NOW = 1, TUBE_UI_PRESS_ARMED = 2 };
#define TUBE_UI_DOUBLE_MS 2000

static const int g_tube_ui_scales[] = {40, 50, 60, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110, 115, 120, 125, 130, 135, 140};
static const int g_tube_grid_scales[] = {50, 60, 70, 80, 90, 100, 110, 120, 125, 130, 135, 140, 150};
#define TUBE_UI_SCALES ((int)(sizeof g_tube_ui_scales / sizeof g_tube_ui_scales[0]))
#define TUBE_GRID_SCALES ((int)(sizeof g_tube_grid_scales / sizeof g_tube_grid_scales[0]))
#define TUBE_SCROLL_SPEEDS 15

/* Themes, then the classic colour schemes. The four themes are whole palettes:
 * youtube.rcss is Afterglow itself and names each token's colour in the other
 * three. The classic schemes are the look before the themes and SmartTube's
 * eight: a background and an accent, from which every token follows by its
 * role (tube_ui_scheme_color) starting at its classic colour. The ids are saved
 * in settings files: never rename one. "default", which every profile written
 * before the themes carries, names no scheme and so reads as the first theme. */
#define TUBE_UI_THEMES 4
#define TUBE_UI_CLASSIC TUBE_UI_THEMES      /* the first classic scheme: the look before the themes */
#define TUBE_UI_SCHEMES 13
static const struct { const char *id, *label; unsigned background, accent; int mono; } g_tube_schemes[TUBE_UI_SCHEMES] = {
    {"afterglow", "Afterglow", 0x06070c, 0xff5a47, 0}, {"onyx", "Onyx", 0x000000, 0xf2c14e, 0},
    {"daybreak", "Daybreak", 0xf3ede4, 0xe2533d, 0}, {"high-contrast", "High contrast", 0x000000, 0xffd60a, 0},
    {"classic", "Classic ProsperoTube", 0x101116, 0xef3340, 0}, {"teal", "Teal", 0x16262b, 0x1fb5a3, 0},
    {"dark-grey", "Dark grey", 0x212121, 0xef3340, 0}, {"red", "Red", 0x1f1414, 0xe53935, 0},
    {"dark-grey-oled", "Dark grey (true black)", 0x000000, 0xef3340, 0}, {"teal-oled", "Teal (true black)", 0x000000, 0x1fb5a3, 0},
    {"dark-grey-mono", "Dark grey monochrome", 0x212121, 0x8a8a8a, 1}, {"dark-blue", "Dark blue", 0x0d1626, 0x3d8bff, 0},
    {"dark-blue-oled", "Dark blue (true black)", 0x000000, 0x3d8bff, 0}
};

static inline void tube_ui_menu_defaults(unsigned char *order) {
    for (int i = 0; i < TUBE_MENU_KEYS; ++i) order[i] = (unsigned char)i;
}
static inline void tube_ui_defaults(TubeUiPrefs *prefs) {
    memset(prefs, 0, sizeof *prefs);
    prefs->ui_scale = prefs->grid_scale = 100; prefs->title_lines = prefs->subtitle_lines = 1;
    prefs->scroll_speed = 10;
    tube_ui_menu_defaults(prefs->menu_order);
}
static inline int tube_ui_listed(const int *values, int count, int value) {
    for (int i = 0; i < count; ++i) if (values[i] == value) return i;
    return -1;
}
/* Replace every out-of-range field with its default; returns how many changed. */
static inline int tube_ui_validate(TubeUiPrefs *prefs) {
    int changed = 0; unsigned seen[2] = {0, 0};
    if (tube_ui_listed(g_tube_ui_scales, TUBE_UI_SCALES, prefs->ui_scale) < 0) { prefs->ui_scale = 100; ++changed; }
    if (tube_ui_listed(g_tube_grid_scales, TUBE_GRID_SCALES, prefs->grid_scale) < 0) { prefs->grid_scale = 100; ++changed; }
    if (prefs->title_lines < 1 || prefs->title_lines > 4) { prefs->title_lines = 1; ++changed; }
    if (prefs->title_scroll < 0 || prefs->title_scroll > 1) { prefs->title_scroll = 0; ++changed; }
    if (prefs->scroll_speed < 10 || prefs->scroll_speed > 80 || prefs->scroll_speed % 5) { prefs->scroll_speed = 10; ++changed; }
    if (prefs->subtitle_lines < 1 || prefs->subtitle_lines > 2) { prefs->subtitle_lines = 1; ++changed; }
    if (prefs->square < 0 || prefs->square > 1) { prefs->square = 0; ++changed; }
    if (prefs->scheme < 0 || prefs->scheme >= TUBE_UI_SCHEMES) { prefs->scheme = 0; ++changed; }
    if (prefs->app_exit < TUBE_UI_EXIT_NONE || prefs->app_exit > TUBE_UI_EXIT_SINGLE) { prefs->app_exit = TUBE_UI_EXIT_NONE; ++changed; }
    if (prefs->search_exit < 0 || prefs->search_exit > 1) { prefs->search_exit = 0; ++changed; }
    for (int i = 0; i < TUBE_MENU_KEYS; ++i) {
        const unsigned key = prefs->menu_order[i];
        if (key >= TUBE_MENU_KEYS || (seen[key / 32] >> key % 32 & 1)) { tube_ui_menu_defaults(prefs->menu_order); ++changed; break; }
        seen[key / 32] |= 1u << key % 32;
    }
    if (prefs->menu_hidden >> TUBE_MENU_KEYS) { prefs->menu_hidden &= ((uint64_t)1 << TUBE_MENU_KEYS) - 1; ++changed; }
    return changed;
}
/* 1 = the browser needs no added rule, scale or scheme. */
static inline int tube_ui_plain(const TubeUiPrefs *p) {
    return p->ui_scale == 100 && p->grid_scale == 100 && p->title_lines == 1 && p->subtitle_lines == 1 && !p->square && !p->scheme;
}

/* ---- Video actions menu. ---- */
static inline int tube_ui_menu_find(const char *id) {
    for (int i = 0; id && i < TUBE_MENU_KEYS; ++i) if (!strcmp(id, g_tube_menu_keys[i].id)) return i;
    return -1;
}
/* The menu key of a catalog row by its id; -1 = a row that is always listed first. */
static inline int tube_ui_menu_key(const char *row) {
    static const char *const actions[] = {"like", "dislike", "unrate", "watch_later_add", "watch_later_remove", "subscribe",
        "unsubscribe", "history_remove", "mark_watched", "not_interested", "dont_recommend_channel", "hide"};
    static const struct { const char *prefix; int key; } prefixes[] = {
        {"playlist:pick:add:", 12}, {"playlist:pick:remove:", 13}, {"playlist:prompt:create:", 14},
        {"playlist:confirm:add:", 15}, {"playlist:confirm:remove:", 16}, {"reminder:", 17}, {"notify:", 18},
        {"library:suggestions:", 22}, {"description:", 24}, {"details:", 25}, {"channel:", 28},
        {"library:playlist:", 29}, {"playlist:", 29}, {"link:video:", 30}, {"settings:refresh", 32}};
    static const struct { const char *verb; int key; } verbs[] = {{"next", 19}, {"watched", 20}, {"forget", 21},
        {"block", 23}, {"unblock", 23}, {"restart", 26}, {"incognito", 27}, {"sbexclude", 31}, {"sbinclude", 31}};
    if (!row) return -1;
    if (!strncmp(row, "account:confirm:", 16)) {
        const char *action = row + 16, *end = strchr(action, ':');
        for (unsigned i = 0; end && i < sizeof actions / sizeof actions[0]; ++i)
            if (strlen(actions[i]) == (size_t)(end - action) && !strncmp(action, actions[i], (size_t)(end - action))) return (int)i;
        return -1;
    }
    if (!strncmp(row, "local:video:", 12)) {
        const char *verb = strchr(row + 12, ':'), *end = verb ? strchr(verb + 1, ':') : NULL;
        for (unsigned i = 0; end && i < sizeof verbs / sizeof verbs[0]; ++i)
            if (strlen(verbs[i].verb) == (size_t)(end - verb - 1) && !strncmp(verb + 1, verbs[i].verb, (size_t)(end - verb - 1))) return verbs[i].key;
        return -1;
    }
    for (unsigned i = 0; i < sizeof prefixes / sizeof prefixes[0]; ++i)
        if (!strncmp(row, prefixes[i].prefix, strlen(prefixes[i].prefix))) return prefixes[i].key;
    return -1;
}
static inline int tube_ui_menu_rank(const TubeUiPrefs *prefs, int key) {
    for (int i = 0; i < TUBE_MENU_KEYS; ++i) if (prefs->menu_order[i] == key) return i;
    return -1;
}
static inline int tube_ui_menu_shown(const TubeUiPrefs *prefs, int key) {
    return key < 0 || key >= TUBE_MENU_KEYS || !(prefs->menu_hidden >> key & 1);
}
/* Move a key one place earlier (-1) or later (+1). 1 = moved. */
static inline int tube_ui_menu_move(TubeUiPrefs *prefs, int key, int direction) {
    const int rank = tube_ui_menu_rank(prefs, key), other = rank + (direction < 0 ? -1 : 1);
    if (rank < 0 || other < 0 || other >= TUBE_MENU_KEYS) return 0;
    prefs->menu_order[rank] = prefs->menu_order[other]; prefs->menu_order[other] = (unsigned char)key;
    return 1;
}
/* Arrange menu rows given by their keys: hidden rows leave, the rest follow the
 * profile's order, rows of one key keep their order and rows without a key stay
 * first. Fills index[] with the rows to list; returns how many. */
static inline int tube_ui_menu_arrange(const TubeUiPrefs *prefs, const int *keys, int count, int *index) {
    int kept = 0;
    for (int i = 0; i < count; ++i) if (tube_ui_menu_shown(prefs, keys[i])) index[kept++] = i;
    for (int i = 1; i < kept; ++i) {
        const int row = index[i], rank = tube_ui_menu_rank(prefs, keys[row]);
        int k = i;
        while (k > 0 && tube_ui_menu_rank(prefs, keys[index[k - 1]]) > rank) { index[k] = index[k - 1]; --k; }
        index[k] = row;
    }
    return kept;
}

/* ---- Circle on the main menu and on search results. ---- */
/* mode: TUBE_UI_EXIT_*. NONE passes the press on, SINGLE acts at once, DOUBLE
 * arms on the first press and acts on a second one within TUBE_UI_DOUBLE_MS. */
static inline int tube_ui_press(int mode, uint64_t now_ms, uint64_t *armed_ms) {
    if (mode == TUBE_UI_EXIT_SINGLE) { *armed_ms = 0; return TUBE_UI_PRESS_NOW; }
    if (mode != TUBE_UI_EXIT_DOUBLE) { *armed_ms = 0; return TUBE_UI_PRESS_PASS; }
    if (*armed_ms && now_ms >= *armed_ms && now_ms - *armed_ms <= TUBE_UI_DOUBLE_MS) { *armed_ms = 0; return TUBE_UI_PRESS_NOW; }
    *armed_ms = now_ms ? now_ms : 1;
    return TUBE_UI_PRESS_ARMED;
}

/* ---- Grid and list geometry, in dp of the scaled interface. ---- */
typedef struct {
    double scale;        /* factor on the dp ratio */
    double width, height;/* the screen in dp: 1920 x 1080 divided by the scale */
    int dw, dh;          /* what the design's 1920 x 1080 box gains (or loses) */
    int columns, rows;   /* cards of one grid page */
    int list_rows;       /* rows of one list page (comments, chat, review) */
    double card;         /* factor on every card measure */
} tube_ui_layout_t;
#define TUBE_UI_MAX_COLUMNS 8
#define TUBE_UI_MAX_ROWS 4
#define TUBE_UI_CARD_PITCH_X 448.0   /* card 432 + margin 16 */
#define TUBE_UI_CARD_PITCH_Y 354.0   /* card 332 + margin 22 */
#define TUBE_UI_TITLE_LINE 28.0
#define TUBE_UI_SUBTITLE_LINE 23.0
#define TUBE_UI_LIST_PITCH 102.0     /* list row 90 + margins 12 */
static inline void tube_ui_layout(const TubeUiPrefs *prefs, tube_ui_layout_t *layout) {
    TubeUiPrefs p = *prefs; tube_ui_validate(&p);
    layout->scale = p.ui_scale / 100.0;
    layout->width = 1920.0 / layout->scale; layout->height = 1080.0 / layout->scale;
    layout->dw = (int)(layout->width + 1e-6) - 1920; layout->dh = (int)(layout->height + 1e-6) - 1080;
    /* The grid box inside its padding. A row wraps by the cards with their
     * margins; the margin under the last row may leave the box. */
    const double wide = 1792.0 + layout->dw, high = 720.0 + layout->dh - 18.0;
    const double pitch = TUBE_UI_CARD_PITCH_Y + TUBE_UI_TITLE_LINE * (p.title_lines - 1) + TUBE_UI_SUBTITLE_LINE * (p.subtitle_lines - 1);
    double grid = p.grid_scale / 100.0;
    /* At most eight columns: smaller cards than that are enlarged to fill the row. */
    if (wide / (TUBE_UI_CARD_PITCH_X * grid) >= TUBE_UI_MAX_COLUMNS + 1) grid = wide / (TUBE_UI_CARD_PITCH_X * (TUBE_UI_MAX_COLUMNS + 0.999));
    layout->columns = (int)(wide / (TUBE_UI_CARD_PITCH_X * grid) + 1e-3);
    layout->rows = (int)((high + 22.0 * grid) / (TUBE_UI_CARD_PITCH_Y * grid) + 1e-3);
    if (layout->columns < 1) layout->columns = 1;
    if (layout->columns > TUBE_UI_MAX_COLUMNS) layout->columns = TUBE_UI_MAX_COLUMNS;
    if (layout->rows < 1) layout->rows = 1;
    if (layout->rows > TUBE_UI_MAX_ROWS) layout->rows = TUBE_UI_MAX_ROWS;
    /* Extra text lines shrink the cards instead of costing a row, which can
     * make room for another column; a card larger than the grid shrinks to fit it. */
    layout->card = grid;
    if ((layout->rows * pitch - 22.0) * layout->card > high) {
        layout->card = high / (layout->rows * pitch - 22.0);
        layout->columns = (int)(wide / (TUBE_UI_CARD_PITCH_X * layout->card) + 1e-3);
        if (layout->columns < 1) layout->columns = 1;
        if (layout->columns > TUBE_UI_MAX_COLUMNS) layout->columns = TUBE_UI_MAX_COLUMNS;
    }
    /* A row that fills the box exactly, as the unscaled one does, keeps a dp
     * of slack at any other scale so that rounding cannot wrap its last card. */
    const double fit = p.ui_scale == 100 && p.grid_scale == 100 ? wide : wide - 1.0;
    if (layout->columns * TUBE_UI_CARD_PITCH_X * layout->card > fit) layout->card = fit / (layout->columns * TUBE_UI_CARD_PITCH_X);
    layout->list_rows = (int)((820.0 + layout->dh) / TUBE_UI_LIST_PITCH + 1e-3);
    if (layout->list_rows < 1) layout->list_rows = 1;
    if (layout->list_rows > 16) layout->list_rows = 16;
}

/* Rules appended to youtube.rcss for these preferences; "" for the defaults.
 * Returns the length, or -1 when the buffer is too small. */
static inline int tube_ui_rules(const TubeUiPrefs *prefs, char *out, size_t size) {
    TubeUiPrefs p = *prefs; tube_ui_validate(&p);
    tube_ui_layout_t l; tube_ui_layout(&p, &l);
    int used = 0;
    if (!out || !size) return -1;
    out[0] = 0;
#define TUBE_UI_RULE(...) do { \
        const int wrote = snprintf(out + used, size - (size_t)used, __VA_ARGS__); \
        if (wrote < 0 || (size_t)wrote >= size - (size_t)used) { out[0] = 0; return -1; } \
        used += wrote; \
    } while (0)
    if (p.ui_scale != 100) {
        const int w = 1792 + l.dw, h = 1004 + l.dh;
        TUBE_UI_RULE("#tube-root { width: %ddp; height: %ddp; }\n", w, h);
        TUBE_UI_RULE("#tube-location { width: %ddp; }\n", 1100 + l.dw);
        TUBE_UI_RULE("#tube-status, #tube-footer, #tube-empty, #tube-discussion-footer, #tube-chat-footer, #tube-review-footer, "
                     "#tube-auth-footer, #tube-description-footer, #tube-link-footer { width: %ddp; }\n", w);
        TUBE_UI_RULE("#tube-empty { top: %ddp; }\n", 290 + l.dh / 2);
        TUBE_UI_RULE("#tube-grid { width: %ddp; height: %ddp; }\n", w, 720 + l.dh);
        TUBE_UI_RULE("#tube-preview { top: %ddp; width: %ddp; }\n", 810 + l.dh, 1780 + l.dw);
        TUBE_UI_RULE("#tube-grid.discussion { width: 640dp; height: %ddp; }\n", 820 + l.dh);
        TUBE_UI_RULE("#tube-preview.discussion { top: 140dp; width: %ddp; height: %ddp; }\n", 1100 + l.dw, 780 + l.dh);
        TUBE_UI_RULE("#tube-discussion-text { width: %ddp; height: %ddp; }\n", 1100 + l.dw, 690 + l.dh);
        TUBE_UI_RULE("#tube-discussion-text.with-qr { height: 150dp; }\n");
        TUBE_UI_RULE("#tube-qr { width: %ddp; }\n", 1100 + l.dw);
        TUBE_UI_RULE("#tube-qr-caption { width: %ddp; }\n", 640 + l.dw < 80 ? 80 : 640 + l.dw);
        TUBE_UI_RULE("#tube-dim { width: %ddp; height: %ddp; }\n", 1921 + l.dw, 1081 + l.dh);
        /* The body paints the page ground: it follows the screen. */
        TUBE_UI_RULE("body { width: %ddp; height: %ddp; }\n", 1921 + l.dw, 1081 + l.dh);
    }
    const double c = l.card;
    const int sized = c != 1.0 || p.title_lines != 1 || p.subtitle_lines != 1;
    /* Rows of cards stay together at the top when the grid has room to spare. */
    if (sized || p.ui_scale != 100) TUBE_UI_RULE("#tube-grid { align-content: flex-start; }\n");
    if (sized) {
        const double title = 12.0 + TUBE_UI_TITLE_LINE * p.title_lines, author = TUBE_UI_SUBTITLE_LINE * p.subtitle_lines;
        const double height = 332.0 + TUBE_UI_TITLE_LINE * (p.title_lines - 1) + TUBE_UI_SUBTITLE_LINE * (p.subtitle_lines - 1);
        TUBE_UI_RULE(".tube-card { width: %.2fdp; height: %.2fdp; margin-right: %.2fdp; margin-bottom: %.2fdp; padding: %.2fdp; border-radius: %.2fdp; }\n",
                     432 * c, height * c, 16 * c, 22 * c, 7 * c, 13 * c);
        TUBE_UI_RULE(".tube-image, .tube-image img { width: %.2fdp; height: %.2fdp; }\n", 412 * c, 226 * c);
        TUBE_UI_RULE(".tube-image { border-radius: %.2fdp; }\n", 7 * c);
        TUBE_UI_RULE(".tube-placeholder { padding-top: %.2fdp; font-size: %.2fdp; }\n", 70 * c, 64 * c);
        TUBE_UI_RULE(".tube-card-title { padding-top: %.2fdp; font-size: %.2fdp; line-height: %.2fdp; height: %.2fdp;%s }\n",
                     12 * c, 23 * c, TUBE_UI_TITLE_LINE * c, title * c, p.title_lines > 1 ? " white-space: normal;" : "");
        TUBE_UI_RULE(".tube-card-author { font-size: %.2fdp; line-height: %.2fdp; height: %.2fdp;%s }\n",
                     19 * c, TUBE_UI_SUBTITLE_LINE * c, author * c, p.subtitle_lines > 1 ? " white-space: normal;" : "");
        TUBE_UI_RULE(".tube-badge, .tube-duration { bottom: %.2fdp; right: %.2fdp; padding: %.2fdp %.2fdp; font-size: %.2fdp; }\n",
                     10 * c, 10 * c, 4 * c, 8 * c, 18 * c);
        TUBE_UI_RULE(".tube-badge { left: %.2fdp; right: auto; }\n", 10 * c);
        TUBE_UI_RULE(".tube-progress { width: %.2fdp; }\n", 414 * c);
        TUBE_UI_RULE(".tube-watched { font-size: %.2fdp; }\n", 15 * c);
        /* Lists keep their own rows whatever the cards do. */
        TUBE_UI_RULE("#tube-grid.discussion .tube-card { border-radius: 13dp; }\n");
        TUBE_UI_RULE("#tube-grid.discussion .tube-card-title { padding-top: 12dp; line-height: 28dp; white-space: nowrap; }\n");
        TUBE_UI_RULE("#tube-grid.discussion .tube-card-author { line-height: 23dp; height: 23dp; white-space: nowrap; }\n");
    }
    if (p.square) {
        TUBE_UI_RULE(".tube-card, .tube-image { border-radius: 0dp; }\n");
        TUBE_UI_RULE("#tube-grid.discussion .tube-card { border-radius: 0dp; }\n");
    }
#undef TUBE_UI_RULE
    return used;
}

/* ---- Title marquee. ---- */
#define TUBE_UI_MARQUEE_REST 1.2      /* seconds at each end */
#define TUBE_UI_MARQUEE_SPEED 40.0    /* dp per second at 1x */
/* Offset, in the same unit as overflow, of a title that overflows its box:
 * rest, travel to the end, rest, start over. 0 without overflow. */
static inline double tube_ui_marquee(double elapsed_s, double overflow, int speed_tenths, double dp_ratio) {
    if (!(overflow > 0.5) || !(elapsed_s > 0) || speed_tenths < 1 || !(dp_ratio > 0)) return 0;
    const double speed = TUBE_UI_MARQUEE_SPEED * dp_ratio * speed_tenths / 10.0, travel = overflow / speed;
    const double cycle = TUBE_UI_MARQUEE_REST * 2 + travel;
    double t = elapsed_s - (double)(long long)(elapsed_s / cycle) * cycle;
    if (t < TUBE_UI_MARQUEE_REST) return 0;
    t -= TUBE_UI_MARQUEE_REST;
    return t >= travel ? overflow : t * speed;
}

/* ---- Themes and colour schemes. ---- */
enum { TUBE_UI_ROLE_BASE = 0, TUBE_UI_ROLE_ACCENT = 1, TUBE_UI_ROLE_TEXT = 2 };
#define TUBE_UI_DEFAULT_BACKGROUND 0x06070cu     /* the sheet's own: Afterglow */
#define TUBE_UI_DEFAULT_ACCENT 0xff5a47u
#define TUBE_UI_CLASSIC_BACKGROUND 0x101116u     /* what the classic schemes are measured from */
#define TUBE_UI_CLASSIC_ACCENT 0xef3340u
static inline unsigned tube_ui_shift(unsigned from, unsigned reference, unsigned target, int grey) {
    unsigned result = 0; int offsets[3], mean = 0;
    for (int i = 0; i < 3; ++i) {
        offsets[i] = (int)(from >> (16 - 8 * i) & 255) - (int)(reference >> (16 - 8 * i) & 255);
        mean += offsets[i];
    }
    for (int i = 0; i < 3; ++i) {
        int value = (int)(target >> (16 - 8 * i) & 255) + (grey ? mean / 3 : offsets[i]);
        if (value < 0) value = 0;
        if (value > 255) value = 255;
        result |= (unsigned)value << (16 - 8 * i);
    }
    return result;
}
/* A classic colour in a classic scheme: surfaces keep their distance from the
 * background, accent shades their distance from the accent, text stays. */
static inline unsigned tube_ui_scheme_color(int scheme, int role, unsigned rgb) {
    if (scheme <= TUBE_UI_CLASSIC || scheme >= TUBE_UI_SCHEMES || role == TUBE_UI_ROLE_TEXT) return rgb;
    if (role == TUBE_UI_ROLE_ACCENT) return tube_ui_shift(rgb, TUBE_UI_CLASSIC_ACCENT, g_tube_schemes[scheme].accent, g_tube_schemes[scheme].mono);
    return tube_ui_shift(rgb, TUBE_UI_CLASSIC_BACKGROUND, g_tube_schemes[scheme].background, g_tube_schemes[scheme].mono);
}
static inline int tube_ui_hex(const char *text, int digits, unsigned *value) {
    *value = 0;
    for (int i = 0; i < digits; ++i) {
        const char c = text[i];
        const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (digit < 0) return 0;
        *value = *value << 4 | (unsigned)digit;
    }
    return 1;
}
static inline int tube_ui_hex_digit(char c) { unsigned v; return tube_ui_hex(&c, 1, &v); }
#define TUBE_UI_TOKENS 48
/* look[]: the token in Onyx, Daybreak, High contrast and the classic look. */
typedef struct { char name[24]; unsigned rgb; int role; unsigned look[TUBE_UI_THEMES]; } tube_ui_token_t;
/* A token's colour in a theme or scheme; the sheet's own for the first theme. */
static inline unsigned tube_ui_token_color(const tube_ui_token_t *token, int scheme) {
    if (scheme <= 0 || scheme >= TUBE_UI_SCHEMES) return token->rgb;
    if (scheme < TUBE_UI_THEMES) return token->look[scheme - 1];
    return tube_ui_scheme_color(scheme, token->role, token->look[TUBE_UI_THEMES - 1]);
}
/* The "@token name #rrggbb role #onyx #daybreak #contrast #classic" lines of a
 * style sheet's palette comment. Returns the count; *body receives the offset
 * just after that comment. */
static inline int tube_ui_tokens(const char *sheet, tube_ui_token_t *tokens, int capacity, size_t *body) {
    int count = 0; const char *at = sheet, *last = NULL;
    while (sheet && (at = strstr(at, "@token ")) != NULL) {
        char name[24], role[12]; unsigned rgb, look[TUBE_UI_THEMES];
        at += 7; last = at;
        if (sscanf(at, "%23[a-z-] #%6x %11[a-z] #%6x #%6x #%6x #%6x", name, &rgb, role, &look[0], &look[1], &look[2], &look[3]) != 7 ||
            count >= capacity) continue;
        memcpy(tokens[count].name, name, sizeof name); tokens[count].rgb = rgb;
        memcpy(tokens[count].look, look, sizeof look);
        tokens[count].role = !strcmp(role, "accent") ? TUBE_UI_ROLE_ACCENT : !strcmp(role, "text") ? TUBE_UI_ROLE_TEXT : TUBE_UI_ROLE_BASE;
        ++count;
    }
    if (body) { const char *end = last ? strstr(last, "*/") : NULL; *body = end ? (size_t)(end + 2 - sheet) : 0; }
    return count;
}
/* A copy of the sheet with every token colour after the palette comment
 * replaced by the scheme's. Returns the length, or -1 when it does not fit. */
static inline int tube_ui_scheme_sheet(const char *sheet, int scheme, char *out, size_t size) {
    tube_ui_token_t tokens[TUBE_UI_TOKENS]; size_t body = 0, used = 0;
    const int count = tube_ui_tokens(sheet, tokens, TUBE_UI_TOKENS, &body);
    const size_t length = sheet ? strlen(sheet) : 0;
    if (!sheet || !out || length + 1 > size) return -1;
    for (size_t i = 0; i < length; ++i) {
        unsigned rgb; int replaced = 0;
        if (i >= body && sheet[i] == '#' && i + 7 <= length && tube_ui_hex(sheet + i + 1, 6, &rgb) &&
            (!tube_ui_hex_digit(sheet[i + 7]) || (i + 9 <= length && tube_ui_hex_digit(sheet[i + 8]) && !tube_ui_hex_digit(sheet[i + 9]))))
            for (int k = 0; k < count && !replaced; ++k) if (tokens[k].rgb == rgb) {
                snprintf(out + used, 8, "#%06x", tube_ui_token_color(&tokens[k], scheme));
                used += 7; i += 6; replaced = 1;
            }
        if (!replaced) out[used++] = sheet[i];
    }
    out[used] = 0;
    return (int)used;
}

/* ---- Preference groups: one table for the settings screens. ---- */
enum { TUBE_UI_GROUP_SCALE, TUBE_UI_GROUP_GRID, TUBE_UI_GROUP_TITLE_LINES, TUBE_UI_GROUP_TITLE_SCROLL, TUBE_UI_GROUP_SCROLL_SPEED,
       TUBE_UI_GROUP_SUBTITLE_LINES, TUBE_UI_GROUP_ROUNDED, TUBE_UI_GROUP_SCHEME, TUBE_UI_GROUP_APP_EXIT, TUBE_UI_GROUP_SEARCH_EXIT,
       TUBE_UI_GROUPS };
static const struct { const char *id, *title, *detail; int count; } g_tube_ui_groups[TUBE_UI_GROUPS] = {
    {"settings:ui:scale", "Interface scale", "Size of everything in the browser; smaller shows more", TUBE_UI_SCALES},
    {"settings:ui:grid", "Video grid scale", "Size of the video cards; smaller shows more cards on a page", TUBE_GRID_SCALES},
    {"settings:ui:title-lines", "Card title lines", "Lines a card keeps for its title", 4},
    {"settings:ui:title-scroll", "Scroll long titles", "The selected card's title moves when it does not fit on one line", 2},
    {"settings:ui:scroll-speed", "Title scroll speed", "How fast a long title moves", TUBE_SCROLL_SPEEDS},
    {"settings:ui:subtitle-lines", "Multiline subtitles", "Two lines for the channel and details of a card", 2},
    {"settings:ui:rounded", "Rounded card corners", "Round or square corners for cards and thumbnails", 2},
    {"settings:ui:scheme", "Theme", "Colours of the browser; the true black ones suit OLED screens", TUBE_UI_SCHEMES},
    {"settings:ui:exit", "Circle on the main menu", "Whether Circle closes ProsperoTube from the main menu", 3},
    {"settings:ui:search-exit", "Leaving search results", "One press of Circle, or two", 2}
};
static inline int tube_ui_group_find(const char *id) {
    for (int i = 0; id && i < TUBE_UI_GROUPS; ++i) if (!strcmp(id, g_tube_ui_groups[i].id)) return i;
    return -1;
}
static inline int tube_ui_group_count(int group) { return group >= 0 && group < TUBE_UI_GROUPS ? g_tube_ui_groups[group].count : 0; }
static inline int tube_ui_group_label(int group, int choice, char *out, size_t size) {
    static const char *const exits[] = {"Does nothing", "Press twice to close ProsperoTube", "Closes ProsperoTube"};
    if (!out || !size) return 0;
    out[0] = 0;
    if (choice < 0 || choice >= tube_ui_group_count(group)) return 0;
    switch (group) {
    case TUBE_UI_GROUP_SCALE: snprintf(out, size, "%.4gx", g_tube_ui_scales[choice] / 100.0); break;
    case TUBE_UI_GROUP_GRID: snprintf(out, size, "%.4gx", g_tube_grid_scales[choice] / 100.0); break;
    case TUBE_UI_GROUP_TITLE_LINES: snprintf(out, size, "%d", choice + 1); break;
    case TUBE_UI_GROUP_SCROLL_SPEED: snprintf(out, size, "%.4gx", 1.0 + choice * 0.5); break;
    case TUBE_UI_GROUP_SCHEME: snprintf(out, size, "%s", g_tube_schemes[choice].label); break;
    case TUBE_UI_GROUP_APP_EXIT: snprintf(out, size, "%s", exits[choice]); break;
    case TUBE_UI_GROUP_SEARCH_EXIT: snprintf(out, size, "%s", choice ? "Press Circle twice" : "Press Circle once"); break;
    default: snprintf(out, size, "%s", choice ? "On" : "Off"); break;
    }
    return 1;
}
static inline int tube_ui_group_selected(const TubeUiPrefs *p, int group) {
    switch (group) {
    case TUBE_UI_GROUP_SCALE: return tube_ui_listed(g_tube_ui_scales, TUBE_UI_SCALES, p->ui_scale);
    case TUBE_UI_GROUP_GRID: return tube_ui_listed(g_tube_grid_scales, TUBE_GRID_SCALES, p->grid_scale);
    case TUBE_UI_GROUP_TITLE_LINES: return p->title_lines - 1;
    case TUBE_UI_GROUP_TITLE_SCROLL: return p->title_scroll;
    case TUBE_UI_GROUP_SCROLL_SPEED: return (p->scroll_speed - 10) / 5;
    case TUBE_UI_GROUP_SUBTITLE_LINES: return p->subtitle_lines - 1;
    case TUBE_UI_GROUP_ROUNDED: return !p->square;
    case TUBE_UI_GROUP_SCHEME: return p->scheme;
    case TUBE_UI_GROUP_APP_EXIT: return p->app_exit;
    case TUBE_UI_GROUP_SEARCH_EXIT: return p->search_exit;
    default: return -1;
    }
}
/* 1 = the choice exists and is now the preference. */
static inline int tube_ui_group_select(TubeUiPrefs *p, int group, int choice) {
    if (choice < 0 || choice >= tube_ui_group_count(group)) return 0;
    switch (group) {
    case TUBE_UI_GROUP_SCALE: p->ui_scale = g_tube_ui_scales[choice]; break;
    case TUBE_UI_GROUP_GRID: p->grid_scale = g_tube_grid_scales[choice]; break;
    case TUBE_UI_GROUP_TITLE_LINES: p->title_lines = choice + 1; break;
    case TUBE_UI_GROUP_TITLE_SCROLL: p->title_scroll = choice; break;
    case TUBE_UI_GROUP_SCROLL_SPEED: p->scroll_speed = 10 + choice * 5; break;
    case TUBE_UI_GROUP_SUBTITLE_LINES: p->subtitle_lines = choice + 1; break;
    case TUBE_UI_GROUP_ROUNDED: p->square = !choice; break;
    case TUBE_UI_GROUP_SCHEME: p->scheme = choice; break;
    case TUBE_UI_GROUP_APP_EXIT: p->app_exit = choice; break;
    case TUBE_UI_GROUP_SEARCH_EXIT: p->search_exit = choice; break;
    default: return 0;
    }
    return 1;
}
#endif
