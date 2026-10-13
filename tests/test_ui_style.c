/* Interface preferences without a screen: value lists, validation, the grid
 * and list geometry at every scale, the generated style rules, the title
 * marquee, colour schemes against the palette of native/youtube.rcss, the video
 * actions menu and the Circle rules. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_ui_style.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>

static char *slurp(const char *path) {
    FILE *file = fopen(path, "rb"); assert(file);
    assert(!fseek(file, 0, SEEK_END)); long size = ftell(file); assert(size > 0 && !fseek(file, 0, SEEK_SET));
    char *text = malloc((size_t)size + 1); assert(text && fread(text, 1, (size_t)size, file) == (size_t)size);
    text[size] = 0; fclose(file); return text;
}

static void test_defaults_and_validation(void) {
    TubeUiPrefs p, zero; tube_ui_defaults(&p);
    assert(p.ui_scale == 100 && p.grid_scale == 100 && p.title_lines == 1 && !p.title_scroll && p.scroll_speed == 10);
    assert(p.subtitle_lines == 1 && !p.square && !p.scheme && p.app_exit == TUBE_UI_EXIT_NONE && !p.search_exit && !p.menu_hidden);
    for (int i = 0; i < TUBE_MENU_KEYS; ++i) assert(p.menu_order[i] == i);
    assert(tube_ui_plain(&p) && !tube_ui_validate(&p));
    /* An all-zero block, as an older build leaves it, validates to the defaults. */
    memset(&zero, 0, sizeof zero); assert(tube_ui_validate(&zero) > 0 && !memcmp(&zero, &p, sizeof p));
    TubeUiPrefs bad = p;
    bad.ui_scale = 99; bad.grid_scale = 45; bad.title_lines = 5; bad.title_scroll = 2; bad.scroll_speed = 12; bad.subtitle_lines = 0;
    bad.square = -1; bad.scheme = TUBE_UI_SCHEMES; bad.app_exit = 3; bad.search_exit = 7; bad.menu_order[3] = 4;
    bad.menu_hidden = (uint64_t)1 << 40 | 2;
    assert(tube_ui_validate(&bad) == 12);
    p.menu_hidden = 2; assert(!memcmp(&bad, &p, sizeof p));
    /* SmartTube's value lists. */
    assert(TUBE_UI_SCALES == 19 && g_tube_ui_scales[0] == 40 && g_tube_ui_scales[18] == 140 && g_tube_ui_scales[3] == 65);
    assert(TUBE_GRID_SCALES == 13 && g_tube_grid_scales[0] == 50 && g_tube_grid_scales[8] == 125 && g_tube_grid_scales[12] == 150);
    assert(TUBE_UI_SCHEMES == 13 && TUBE_UI_THEMES == 4);
}

static void test_layout(void) {
    TubeUiPrefs p; tube_ui_layout_t l; tube_ui_defaults(&p);
    tube_ui_layout(&p, &l);
    assert(l.columns == 4 && l.rows == 2 && l.list_rows == 8 && l.card == 1.0 && l.scale == 1.0 && !l.dw && !l.dh);
    static const struct { int grid, columns, rows; } unscaled[] = {{50, 8, 4}, {60, 6, 3}, {70, 5, 2}, {80, 5, 2}, {90, 4, 2},
        {100, 4, 2}, {110, 3, 1}, {120, 3, 1}, {125, 3, 1}, {130, 3, 1}, {135, 2, 1}, {140, 2, 1}, {150, 2, 1}};
    for (unsigned i = 0; i < sizeof unscaled / sizeof unscaled[0]; ++i) {
        p.grid_scale = unscaled[i].grid; tube_ui_layout(&p, &l);
        assert(l.columns == unscaled[i].columns && l.rows == unscaled[i].rows && fabs(l.card - unscaled[i].grid / 100.0) < 1e-3);
    }
    /* Every pair of scales: the page fits the grid box and is never empty or huge. */
    for (int u = 0; u < TUBE_UI_SCALES; ++u) for (int g = 0; g < TUBE_GRID_SCALES; ++g)
        for (int lines = 1; lines <= 4; ++lines) for (int subtitle = 1; subtitle <= 2; ++subtitle) {
            tube_ui_defaults(&p); p.ui_scale = g_tube_ui_scales[u]; p.grid_scale = g_tube_grid_scales[g];
            p.title_lines = lines; p.subtitle_lines = subtitle; tube_ui_layout(&p, &l);
            const double pitch = TUBE_UI_CARD_PITCH_Y + TUBE_UI_TITLE_LINE * (lines - 1) + TUBE_UI_SUBTITLE_LINE * (subtitle - 1);
            assert(l.columns >= 1 && l.columns <= TUBE_UI_MAX_COLUMNS && l.rows >= 1 && l.rows <= TUBE_UI_MAX_ROWS);
            assert(l.list_rows >= 1 && l.list_rows <= 16 && l.card > 0.05);
            assert(l.columns * TUBE_UI_CARD_PITCH_X * l.card <= 1792.0 + l.dw + 1e-6);
            assert((l.rows * pitch - 22.0) * l.card <= 702.0 + l.dh + 1e-6);
            assert(l.list_rows * TUBE_UI_LIST_PITCH <= 820.0 + l.dh + 1e-6 || l.list_rows == 1);
            assert(fabs(l.width * l.scale - 1920.0) < 1e-6 && fabs(l.height * l.scale - 1080.0) < 1e-6);
        }
    /* More title lines keep the rows: the cards shrink instead. */
    tube_ui_defaults(&p); p.title_lines = 2; tube_ui_layout(&p, &l);
    assert(l.columns == 4 && l.rows == 2 && l.card < 1.0 && l.card > 0.9);
    p.title_lines = 4; p.subtitle_lines = 2; tube_ui_layout(&p, &l);
    assert(l.columns == 5 && l.rows == 2 && l.card < 0.8 && l.card > 0.7);    /* smaller cards: a fifth column fits */
    /* A smaller interface shows more; a larger one less. */
    tube_ui_defaults(&p); p.ui_scale = 80; tube_ui_layout(&p, &l);
    assert(l.dw == 480 && l.dh == 270 && l.columns == 5 && l.rows == 2 && l.list_rows == 10);
    p.ui_scale = 140; tube_ui_layout(&p, &l);
    assert(l.dw < 0 && l.dh < 0 && l.columns == 2 && l.rows == 1 && l.list_rows == 5);
    p.ui_scale = 40; p.grid_scale = 50; tube_ui_layout(&p, &l);
    assert(l.columns == 8 && l.rows == 4 && l.card > 0.5);   /* capped at 8 x 4, enlarged to fill the row */
}

static void test_rules(void) {
    static char rules[8192]; char tiny[64];
    TubeUiPrefs p; tube_ui_defaults(&p);
    assert(tube_ui_rules(&p, rules, sizeof rules) == 0 && !*rules);
    p.grid_scale = 50;
    assert(tube_ui_rules(&p, rules, sizeof rules) > 0);
    assert(strstr(rules, ".tube-card { width: 215.88dp; height: 165.91dp; margin-right: 8.00dp;") && !strstr(rules, "#tube-root"));
    assert(strstr(rules, ".tube-image, .tube-image img { width: 205.89dp; height: 112.94dp; }"));
    assert(tube_ui_rules(&p, tiny, sizeof tiny) == -1 && !*tiny);
    tube_ui_defaults(&p); p.ui_scale = 80;
    assert(tube_ui_rules(&p, rules, sizeof rules) > 0);
    assert(strstr(rules, "#tube-root { width: 2272dp; height: 1274dp; }") && strstr(rules, "#tube-preview { top: 1080dp; width: 2260dp; }"));
    assert(strstr(rules, "#tube-grid { width: 2272dp; height: 990dp; }") && strstr(rules, "#tube-dim { width: 2401dp; height: 1351dp; }"));
    assert(!strstr(rules, ".tube-card {") && strstr(rules, "#tube-grid { align-content: flex-start; }"));
    tube_ui_defaults(&p); p.title_lines = 3; p.subtitle_lines = 2;
    assert(tube_ui_rules(&p, rules, sizeof rules) > 0);
    assert(strstr(rules, "white-space: normal;") && strstr(rules, "#tube-grid.discussion .tube-card-title { padding-top: 12dp; line-height: 28dp; white-space: nowrap; }"));
    tube_ui_defaults(&p); p.square = 1;
    assert(tube_ui_rules(&p, rules, sizeof rules) > 0 && !strcmp(rules,
        ".tube-card, .tube-image { border-radius: 0dp; }\n#tube-grid.discussion .tube-card { border-radius: 0dp; }\n"));
    for (int u = 0; u < TUBE_UI_SCALES; ++u) for (int g = 0; g < TUBE_GRID_SCALES; ++g) {
        tube_ui_defaults(&p); p.ui_scale = g_tube_ui_scales[u]; p.grid_scale = g_tube_grid_scales[g]; p.title_lines = 4; p.subtitle_lines = 2; p.square = 1;
        assert(tube_ui_rules(&p, rules, sizeof rules) > 0 && !strstr(rules, "nan") && !strstr(rules, "inf") && !strstr(rules, "-0."));
    }
}

static void test_marquee(void) {
    assert(tube_ui_marquee(5, 0, 10, 1) == 0 && tube_ui_marquee(5, 0.4, 10, 1) == 0 && tube_ui_marquee(0, 100, 10, 1) == 0);
    assert(tube_ui_marquee(1.0, 100, 10, 1) == 0);                          /* rests at the start */
    assert(fabs(tube_ui_marquee(2.2, 100, 10, 1) - 40) < 1e-6);             /* 40 dp per second at 1x */
    assert(fabs(tube_ui_marquee(1.7, 100, 20, 1) - 40) < 1e-6);             /* twice as fast at 2x */
    assert(fabs(tube_ui_marquee(2.2, 100, 10, 2) - 80) < 1e-6);             /* dp, not pixels */
    assert(tube_ui_marquee(3.8, 100, 10, 1) == 100 && tube_ui_marquee(4.8, 100, 10, 1) == 100); /* rests at the end */
    assert(tube_ui_marquee(4.95, 100, 10, 1) == 0 && fabs(tube_ui_marquee(4.9 + 2.2, 100, 10, 1) - 40) < 1e-6);
    for (double t = 0; t < 60; t += 0.013) { const double x = tube_ui_marquee(t, 333, 80, 1.5); assert(x >= 0 && x <= 333); }
    assert(tube_ui_marquee(2, 100, 0, 1) == 0 && tube_ui_marquee(2, 100, 10, 0) == 0 && tube_ui_marquee(NAN, 100, 10, 1) == 0);
}

/* WCAG relative luminance and contrast of two colours. */
static double luminance(unsigned rgb) {
    double sum = 0; static const double weight[3] = {0.2126, 0.7152, 0.0722};
    for (int i = 0; i < 3; ++i) {
        const double c = (rgb >> (16 - 8 * i) & 255) / 255.0;
        sum += weight[i] * (c <= 0.03928 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4));
    }
    return sum;
}
static double contrast(unsigned a, unsigned b) {
    const double x = luminance(a), y = luminance(b);
    return (fmax(x, y) + 0.05) / (fmin(x, y) + 0.05);
}
/* Every text of the sheet on the surface it is drawn on reaches 4.5:1 in a theme or the classic look. */
static void check_contrast(const tube_ui_token_t *tokens, int count, int scheme) {
    static const char *const pairs[][2] = {
        {"text", "background"}, {"text-muted", "background"}, {"overview", "background"}, {"footer", "background"}, {"key", "background"},
        {"text", "card"}, {"text", "card-focus"}, {"author", "card"}, {"author", "card-focus"}, {"watched", "card"}, {"watched", "card-focus"},
        {"text", "panel"}, {"error-text", "error-surface"}, {"text", "reader"}, {"placeholder", "image"}, {"folder-mark", "folder-image"},
        {"on-button", "button"}, {"on-live", "live"}, {"on-scrim", "scrim"}};
    for (unsigned i = 0; i < sizeof pairs / sizeof pairs[0]; ++i) {
        unsigned ink = 0, ground = 0; int found = 0;
        for (int k = 0; k < count; ++k) {
            if (!strcmp(tokens[k].name, pairs[i][0])) { ink = tube_ui_token_color(&tokens[k], scheme); ++found; }
            if (!strcmp(tokens[k].name, pairs[i][1])) { ground = tube_ui_token_color(&tokens[k], scheme); ++found; }
        }
        assert(found == 2);
        /* The classic look keeps the colours it shipped with; its letter tiles are large type (3:1). */
        const double least = scheme == TUBE_UI_CLASSIC && (!strcmp(pairs[i][0], "watched") || !strcmp(pairs[i][0], "folder-mark") || !strcmp(pairs[i][0], "on-button") || !strcmp(pairs[i][0], "on-live")) ? 3.0 : 4.5;
        if (contrast(ink, ground) < least) fprintf(stderr, "scheme %d: %s on %s is %.2f:1\n", scheme, pairs[i][0], pairs[i][1], contrast(ink, ground));
        assert(contrast(ink, ground) >= least);
    }
}

static void test_schemes(const char *path) {
    char *sheet = slurp(path); size_t body = 0; static tube_ui_token_t tokens[TUBE_UI_TOKENS];
    const int count = tube_ui_tokens(sheet, tokens, TUBE_UI_TOKENS, &body);
    assert(count >= 12 && count < TUBE_UI_TOKENS && body > 0 && !strncmp(sheet + body - 2, "*/", 2));
    /* The palette is defined once: unique names, unique colours, and no colour in the rules that it does not name. */
    int background = -1, accent = -1;
    for (int i = 0; i < count; ++i) {
        for (int k = 0; k < i; ++k) assert(strcmp(tokens[i].name, tokens[k].name) && tokens[i].rgb != tokens[k].rgb);
        if (!strcmp(tokens[i].name, "background")) background = i;
        if (!strcmp(tokens[i].name, "accent")) accent = i;
    }
    assert(background >= 0 && tokens[background].rgb == TUBE_UI_DEFAULT_BACKGROUND && tokens[background].role == TUBE_UI_ROLE_BASE);
    assert(accent >= 0 && tokens[accent].rgb == TUBE_UI_DEFAULT_ACCENT && tokens[accent].role == TUBE_UI_ROLE_ACCENT);
    int colours = 0;
    for (const char *c = sheet + body; (c = strchr(c, '#')) != NULL; ++c) {
        unsigned rgb; int known = 0;
        if (!tube_ui_hex(c + 1, 6, &rgb) || (tube_ui_hex_digit(c[7]) && !(tube_ui_hex_digit(c[8]) && !tube_ui_hex_digit(c[9])))) continue;
        for (int i = 0; i < count; ++i) known |= tokens[i].rgb == rgb;
        if (!known) fprintf(stderr, "colour without a token: %.9s\n", c);
        assert(known); ++colours;
    }
    assert(colours > 30);
    /* The first theme is the sheet itself, and it passes the same contrast rule as the other three. */
    check_contrast(tokens, count, 0);
    assert(TUBE_UI_THEMES == 4 && TUBE_UI_CLASSIC == 4 && TUBE_UI_SCHEMES == 13 && !strcmp(g_tube_schemes[0].id, "afterglow") &&
           !strcmp(g_tube_schemes[TUBE_UI_CLASSIC].id, "classic"));
    for (int i = 0; i < TUBE_UI_SCHEMES; ++i) assert(strcmp(g_tube_schemes[i].id, "default"));   /* what older files carry reads as the first theme */
    const size_t size = strlen(sheet) + 1; char *out = malloc(size), *small = malloc(size - 1);
    assert(tube_ui_scheme_sheet(sheet, 0, out, size) == (int)strlen(sheet) && !strcmp(out, sheet));
    assert(tube_ui_scheme_sheet(sheet, 0, small, size - 1) == -1 && tube_ui_scheme_sheet(NULL, 0, out, size) == -1);
    for (int scheme = 1; scheme < TUBE_UI_SCHEMES; ++scheme) {
        char body_rule[64];
        assert(tube_ui_scheme_sheet(sheet, scheme, out, size) == (int)strlen(sheet) && strcmp(out, sheet));
        assert(!strncmp(out, sheet, body));                               /* the palette comment keeps the default values */
        snprintf(body_rule, sizeof body_rule, "background: #%06x; }", g_tube_schemes[scheme].background);
        assert(strstr(out + body, "body {") && strstr(strstr(out + body, "body {"), body_rule));
        /* A theme and the classic look are columns of the palette; the scheme table names their background and accent. */
        assert(tube_ui_token_color(&tokens[background], scheme) == g_tube_schemes[scheme].background);
        assert(tube_ui_token_color(&tokens[accent], scheme) == g_tube_schemes[scheme].accent);
        if (scheme <= TUBE_UI_CLASSIC) {
            assert(tokens[background].look[scheme - 1] == g_tube_schemes[scheme].background && tokens[accent].look[scheme - 1] == g_tube_schemes[scheme].accent);
            assert(tube_ui_scheme_color(scheme, TUBE_UI_ROLE_BASE, 0x181a22) == 0x181a22);
            check_contrast(tokens, count, scheme);
            for (size_t i = 0; i < strlen(sheet); ++i) assert((out[i] == '#') == (sheet[i] == '#'));
            continue;
        }
        assert(tube_ui_scheme_color(scheme, TUBE_UI_ROLE_BASE, TUBE_UI_CLASSIC_BACKGROUND) == g_tube_schemes[scheme].background);
        assert(tube_ui_scheme_color(scheme, TUBE_UI_ROLE_ACCENT, TUBE_UI_CLASSIC_ACCENT) == g_tube_schemes[scheme].accent);
        assert(tube_ui_scheme_color(scheme, TUBE_UI_ROLE_TEXT, 0xf6f7fb) == 0xf6f7fb);
        /* Surfaces stay lighter than the background, by what they were lighter in the classic look. */
        const unsigned card = tube_ui_scheme_color(scheme, TUBE_UI_ROLE_BASE, 0x181a22);
        assert((card >> 16 & 255) == (g_tube_schemes[scheme].background >> 16 & 255) + (g_tube_schemes[scheme].mono ? 9 : 8));
        for (size_t i = 0; i < strlen(sheet); ++i) assert((out[i] == '#') == (sheet[i] == '#'));
    }
    /* True black: the OLED schemes paint the page #000000. */
    int black = 0;
    for (int scheme = 1; scheme < TUBE_UI_SCHEMES; ++scheme) if (strstr(g_tube_schemes[scheme].label, "true black")) {
        assert(!g_tube_schemes[scheme].background); ++black;
    }
    assert(black == 3);
    /* Monochrome: no tint in surfaces or accent. */
    assert(g_tube_schemes[10].mono);
    const unsigned mono = tube_ui_scheme_color(10, TUBE_UI_ROLE_BASE, 0x181a22), grey = tube_ui_scheme_color(10, TUBE_UI_ROLE_ACCENT, 0xee5360);
    assert((mono >> 16 & 255) == (mono >> 8 & 255) && (mono >> 8 & 255) == (mono & 255));
    assert((grey >> 16 & 255) == (grey >> 8 & 255) && (grey >> 8 & 255) == (grey & 255));
    assert(tube_ui_scheme_color(99, TUBE_UI_ROLE_BASE, 0x123456) == 0x123456 && tube_ui_scheme_color(-1, TUBE_UI_ROLE_ACCENT, 0x123456) == 0x123456);
    free(out); free(small); free(sheet);
}

static void test_menu(void) {
    TubeUiPrefs p; tube_ui_defaults(&p);
    static const struct { const char *row; const char *key; } rows[] = {
        {"account:confirm:like:dQw4w9WgXcQ", "like"}, {"account:confirm:watch_later_add:dQw4w9WgXcQ", "watch-later-add"},
        {"account:confirm:subscribe:UCabcdefghijklmnopqrstuv", "subscribe"}, {"account:confirm:hide:dQw4w9WgXcQ", "hide"},
        {"account:confirm:dont_recommend_channel:dQw4w9WgXcQ", "dont-recommend"},
        {"playlist:pick:add:dQw4w9WgXcQ", "playlist-add"}, {"playlist:pick:remove:dQw4w9WgXcQ", "playlist-remove"},
        {"playlist:prompt:create:dQw4w9WgXcQ", "playlist-create"}, {"playlist:confirm:add:dQw4w9WgXcQ:PL1", "recent-add"},
        {"playlist:confirm:remove:dQw4w9WgXcQ:PL1", "recent-remove"}, {"reminder:dQw4w9WgXcQ", "reminder"},
        {"notify:UCabcdefghijklmnopqrstuv", "notifications"}, {"local:video:7:next:dQw4w9WgXcQ", "play-next"},
        {"local:video:7:watched:dQw4w9WgXcQ", "watched-local"}, {"local:video:7:forget:dQw4w9WgXcQ", "forget-local"},
        {"library:suggestions:dQw4w9WgXcQ", "related"}, {"local:video:7:block:dQw4w9WgXcQ", "block"},
        {"local:video:7:unblock:dQw4w9WgXcQ", "block"}, {"description:dQw4w9WgXcQ", "description"}, {"details:dQw4w9WgXcQ", "details"},
        {"local:video:7:restart:dQw4w9WgXcQ", "restart"}, {"local:video:12:incognito:dQw4w9WgXcQ", "incognito"},
        {"channel:UCabcdefghijklmnopqrstuv", "open-channel"}, {"library:playlist:PL1", "open-playlist"}, {"playlist:PL1234567890", "open-playlist"},
        {"link:video:dQw4w9WgXcQ", "share"}, {"local:video:7:sbexclude:dQw4w9WgXcQ", "sponsorblock-channel"},
        {"local:video:7:sbinclude:dQw4w9WgXcQ", "sponsorblock-channel"}, {"settings:refresh", "refresh"}};
    for (unsigned i = 0; i < sizeof rows / sizeof rows[0]; ++i)
        assert(tube_ui_menu_key(rows[i].row) == tube_ui_menu_find(rows[i].key) && tube_ui_menu_find(rows[i].key) >= 0);
    assert(tube_ui_menu_key("account:notice") == -1 && tube_ui_menu_key("account:confirm:unknown:x") == -1 && tube_ui_menu_key(NULL) == -1);
    assert(tube_ui_menu_key("local:video:7:nextish:x") == -1 && tube_ui_menu_key("local:video:7") == -1 && tube_ui_menu_key("account:confirm:like") == -1);
    for (int i = 0; i < TUBE_MENU_KEYS; ++i) {
        assert(tube_ui_menu_find(g_tube_menu_keys[i].id) == i && *g_tube_menu_keys[i].label);
        for (int k = 0; k < i; ++k) assert(strcmp(g_tube_menu_keys[i].id, g_tube_menu_keys[k].id));
    }
    assert(tube_ui_menu_find("nope") == -1 && tube_ui_menu_find(NULL) == -1);
    /* Defaults keep the rows as they are. */
    const int keys[] = {-1, 0, 1, 12, 19, 19, 22, 30, 32}; int index[9];
    assert(tube_ui_menu_arrange(&p, keys, 9, index) == 9);
    for (int i = 0; i < 9; ++i) assert(index[i] == i);
    /* Hidden rows leave; moved rows move; rows of one key keep their order; the notice stays first. */
    p.menu_hidden = (uint64_t)1 << 12 | (uint64_t)1 << 32;
    for (int i = 0; i < 30; ++i) assert(tube_ui_menu_move(&p, 30, -1));
    assert(!tube_ui_menu_move(&p, 30, -1) && tube_ui_menu_rank(&p, 30) == 0 && tube_ui_menu_rank(&p, 0) == 1);
    assert(tube_ui_menu_move(&p, 0, 1) && tube_ui_menu_rank(&p, 1) == 1 && tube_ui_menu_rank(&p, 0) == 2);
    assert(tube_ui_menu_arrange(&p, keys, 9, index) == 7);
    const int expected[] = {0, 7, 2, 1, 4, 5, 6};
    for (int i = 0; i < 7; ++i) assert(index[i] == expected[i]);
    assert(!tube_ui_validate(&p));
    assert(!tube_ui_menu_move(&p, 99, 1) && !tube_ui_menu_move(&p, -1, 1));
    p.menu_hidden = ((uint64_t)1 << TUBE_MENU_KEYS) - 1;
    assert(tube_ui_menu_arrange(&p, keys, 9, index) == 1 && index[0] == 0);
}

static void test_press(void) {
    uint64_t armed = 0;
    assert(tube_ui_press(TUBE_UI_EXIT_NONE, 1000, &armed) == TUBE_UI_PRESS_PASS && !armed);
    assert(tube_ui_press(TUBE_UI_EXIT_SINGLE, 1000, &armed) == TUBE_UI_PRESS_NOW && !armed);
    assert(tube_ui_press(TUBE_UI_EXIT_DOUBLE, 1000, &armed) == TUBE_UI_PRESS_ARMED && armed == 1000);
    assert(tube_ui_press(TUBE_UI_EXIT_DOUBLE, 3000, &armed) == TUBE_UI_PRESS_NOW && !armed);
    assert(tube_ui_press(TUBE_UI_EXIT_DOUBLE, 5000, &armed) == TUBE_UI_PRESS_ARMED);
    assert(tube_ui_press(TUBE_UI_EXIT_DOUBLE, 7001, &armed) == TUBE_UI_PRESS_ARMED && armed == 7001); /* too late: arms again */
    assert(tube_ui_press(TUBE_UI_EXIT_DOUBLE, 7000, &armed) == TUBE_UI_PRESS_ARMED);                   /* a clock that went back */
    assert(tube_ui_press(TUBE_UI_EXIT_NONE, 7001, &armed) == TUBE_UI_PRESS_PASS && !armed);            /* a changed mode disarms */
    assert(tube_ui_press(TUBE_UI_EXIT_DOUBLE, 0, &armed) == TUBE_UI_PRESS_ARMED && armed == 1);
    assert(tube_ui_press(7, 10, &armed) == TUBE_UI_PRESS_PASS);
}

static void test_groups(void) {
    TubeUiPrefs p; char label[96]; tube_ui_defaults(&p);
    for (int group = 0; group < TUBE_UI_GROUPS; ++group) {
        assert(tube_ui_group_find(g_tube_ui_groups[group].id) == group && tube_ui_group_count(group) >= 2);
        const int selected = tube_ui_group_selected(&p, group);
        assert(selected >= 0 && selected < tube_ui_group_count(group));
        for (int choice = 0; choice < tube_ui_group_count(group); ++choice) {
            TubeUiPrefs next = p;
            assert(tube_ui_group_label(group, choice, label, sizeof label) && *label);
            assert(tube_ui_group_select(&next, group, choice) && tube_ui_group_selected(&next, group) == choice && !tube_ui_validate(&next));
            assert((choice == selected) == !memcmp(&next, &p, sizeof p));
        }
        assert(!tube_ui_group_select(&p, group, -1) && !tube_ui_group_select(&p, group, tube_ui_group_count(group)));
        assert(!tube_ui_group_label(group, tube_ui_group_count(group), label, sizeof label) && !*label);
    }
    assert(tube_ui_group_find("settings:ui") == -1 && tube_ui_group_find(NULL) == -1 && !tube_ui_group_count(-1) && !tube_ui_group_count(TUBE_UI_GROUPS));
    assert(tube_ui_group_label(TUBE_UI_GROUP_SCALE, 3, label, sizeof label) && !strcmp(label, "0.65x"));
    assert(tube_ui_group_label(TUBE_UI_GROUP_GRID, 8, label, sizeof label) && !strcmp(label, "1.25x"));
    assert(tube_ui_group_label(TUBE_UI_GROUP_SCROLL_SPEED, 14, label, sizeof label) && !strcmp(label, "8x"));
    assert(tube_ui_group_label(TUBE_UI_GROUP_SCROLL_SPEED, 1, label, sizeof label) && !strcmp(label, "1.5x"));
    assert(tube_ui_group_label(TUBE_UI_GROUP_TITLE_LINES, 3, label, sizeof label) && !strcmp(label, "4"));
    assert(tube_ui_group_selected(&p, TUBE_UI_GROUP_ROUNDED) == 1 && tube_ui_group_selected(&p, TUBE_UI_GROUP_APP_EXIT) == 0);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    test_defaults_and_validation(); test_layout(); test_rules(); test_marquee(); test_schemes(argv[1]);
    test_menu(); test_press(); test_groups();
    puts("PASS: interface value lists, validation, geometry at every scale, style rules, marquee, schemes and palette, menu arrangement, Circle rules");
    return 0;
}
