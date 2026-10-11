// QR page helpers: row contract, link forms and the box markup, which is
// painted back into a bitmap and compared with the symbol.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "tube_qr_page.h"

#include <assert.h>
#include <stdlib.h>

static char g_markup[TUBE_QR_MARKUP_SIZE];

/* Paint the markup's boxes; every dark module must be covered exactly once
 * and nothing else may be dark. Returns the number of boxes. */
static int painted_matches(const tube_qr_code *code, const char *markup) {
    static uint8_t canvas[512][512];
    int module = tube_qr_module_dp(code), side = tube_qr_square_dp(code), width = 0, height = 0, used = 0, boxes = 0;
    memset(canvas, 0, sizeof canvas);
    assert(sscanf(markup, "<div id=\"tube-qr-symbol\" style=\"width:%ddp;height:%ddp;\">%n", &width, &height, &used) == 2 && used > 0);
    assert(width == side && height == side && side <= TUBE_QR_TARGET_DP && side > TUBE_QR_TARGET_DP - 2 * 65 && module >= 6);
    const char *at = markup + used;
    while (strncmp(at, "</div>", 6)) {
        int left, top, w, h;
        used = 0;
        assert(sscanf(at, "<div style=\"left:%ddp;top:%ddp;width:%ddp;height:%ddp;\"/>%n", &left, &top, &w, &h, &used) == 4 && used > 0);
        assert(left % module == 0 && top % module == 0 && w % module == 0 && h % module == 0 && w > 0 && h > 0);
        assert(left >= TUBE_QR_QUIET_MODULES * module && top >= TUBE_QR_QUIET_MODULES * module);
        assert(left + w <= side - TUBE_QR_QUIET_MODULES * module && top + h <= side - TUBE_QR_QUIET_MODULES * module);
        for (int y = top; y < top + h; y++) for (int x = left; x < left + w; x++) { assert(!canvas[y][x]); canvas[y][x] = 1; }
        at += used;
        boxes++;
    }
    assert(!at[6]);
    for (int y = 0; y < side; y++) for (int x = 0; x < side; x++)
        assert(canvas[y][x] == tube_qr_module(code, x / module - TUBE_QR_QUIET_MODULES, y / module - TUBE_QR_QUIET_MODULES));
    return boxes;
}

int main(void) {
    char out[TUBE_QR_PAYLOAD_SIZE], tiny[20], text[TUBE_QR_MAX_PAYLOAD + 8];
    tube_qr_code code;

    assert(tube_qr_page_kind("auth:session") == TUBE_QR_PAGE_SIGN_IN && tube_qr_page_kind("auth:") == TUBE_QR_PAGE_SIGN_IN);
    assert(tube_qr_page_kind("link:about") == TUBE_QR_PAGE_LINK && tube_qr_page_kind("link:") == TUBE_QR_PAGE_LINK);
    assert(!tube_qr_page_kind(NULL) && !tube_qr_page_kind("") && !tube_qr_page_kind("settings") && !tube_qr_page_kind("auth") &&
           !tube_qr_page_kind("links") && !tube_qr_page_kind("account:link:x") && !tube_qr_page_kind("Link:x"));

    /* Addresses. */
    static const char *good[] = {"https://github.com/blackbearreloaded/ProsperoTube", "http://example.org", "https://youtu.be/abcdefghijk?t=42",
                                 "https://www.youtube.com/playlist?list=PLabc&index=2#frag", "https://a"};
    static const char *bad[] = {"", "https://", "http://", "ftp://example.org", "javascript:alert(1)", "https:///path", "https://.", "https://:80",
                                "https://example.org/a b", "https://example.org/\"", "https://example.org/<b>", "https://example.org/\\x",
                                "https://example.org/\n", "https://example.org/\t", "HTTPS://example.org", " https://example.org",
                                "https://example.org/caf\xC3\xA9", "example.org", "//example.org", "https://example.org/\x7F"};
    for (size_t i = 0; i < sizeof good / sizeof *good; i++) assert(tube_qr_link_valid(good[i]));
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) assert(!tube_qr_link_valid(bad[i]));
    assert(!tube_qr_link_valid(NULL));
    memset(text, 'a', sizeof text);
    memcpy(text, "https://", 8);
    text[TUBE_QR_MAX_PAYLOAD] = 0;
    assert(tube_qr_link_valid(text) && tube_qr_encode_text(text, &code) == 0 && code.version == 10 && code.level == TUBE_QR_LEVEL_L);
    text[TUBE_QR_MAX_PAYLOAD] = 'a'; text[TUBE_QR_MAX_PAYLOAD + 1] = 0;
    assert(!tube_qr_link_valid(text));

    /* Sign-in address. */
    assert(tube_qr_activation_url("ABC-DEF-GHI", out, sizeof out) == 0 && !strcmp(out, "https://youtube.com/qr/activate/ABC-DEF-GHI"));
    assert(tube_qr_activation_url("abcd1234", out, sizeof out) == 0 && !strcmp(out, "https://youtube.com/qr/activate/abcd1234"));
    static const char *codes[] = {"", "ABC", "ABC DEF", "ABC/DEF", "ABC?x=1", "ABC-DEF-GHI\n", "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456", "AB%2F", "../..", "c\xC3\xB3" "digo"};
    for (size_t i = 0; i < sizeof codes / sizeof *codes; i++) { strcpy(out, "x"); assert(tube_qr_activation_url(codes[i], out, sizeof out) == -1 && !*out); }
    assert(tube_qr_activation_url(NULL, out, sizeof out) == -1 && tube_qr_activation_url("ABCD", NULL, 0) == -1);
    assert(tube_qr_activation_url("ABC-DEF-GHI", tiny, sizeof tiny) == -1 && !*tiny);
    assert(tube_qr_activation_url("ABCDEFGHIJKLMNOPQRSTUVWXYZ012345", out, 65) == 0 && strlen(out) == 64);
    assert(tube_qr_activation_url("ABCDEFGHIJKLMNOPQRSTUVWXYZ012345", out, 64) == -1 && !*out);

    /* Share addresses. */
    assert(tube_qr_video_url("abcdefghijk", 0, out, sizeof out) == 0 && !strcmp(out, "https://youtu.be/abcdefghijk"));
    assert(tube_qr_video_url("abc-_fghijk", 193, out, sizeof out) == 0 && !strcmp(out, "https://youtu.be/abc-_fghijk?t=193"));
    assert(tube_qr_video_url("abcdefghijk", -5, out, sizeof out) == 0 && !strcmp(out, "https://youtu.be/abcdefghijk"));
    assert(tube_qr_video_url("abcdefghij", 0, out, sizeof out) == -1 && !*out && tube_qr_video_url("abcdefghijkl", 0, out, sizeof out) == -1);
    assert(tube_qr_video_url("abcde/ghijk", 0, out, sizeof out) == -1 && tube_qr_video_url("abcde&ghijk", 0, out, sizeof out) == -1);
    assert(tube_qr_video_url(NULL, 0, out, sizeof out) == -1 && tube_qr_video_url("abcdefghijk", 0, NULL, 0) == -1);
    assert(tube_qr_video_url("abcdefghijk", 0, tiny, sizeof tiny) == -1 && !*tiny);
    assert(tube_qr_channel_url("UCaaaaaaaaaaaaaaaaaaaaaa", out, sizeof out) == 0 && !strcmp(out, "https://www.youtube.com/channel/UCaaaaaaaaaaaaaaaaaaaaaa"));
    assert(tube_qr_channel_url("XXaaaaaaaaaaaaaaaaaaaaaa", out, sizeof out) == -1 && tube_qr_channel_url("UCaaaa", out, sizeof out) == -1 && !*out);
    assert(tube_qr_channel_url("UCaaaaaaaaaaaaaaaaaaaaaa", tiny, sizeof tiny) == -1 && tube_qr_channel_url(NULL, out, sizeof out) == -1);
    assert(tube_qr_playlist_url("PLabcdefghijk", out, sizeof out) == 0 && !strcmp(out, "https://www.youtube.com/playlist?list=PLabcdefghijk"));
    assert(tube_qr_playlist_url("P", out, sizeof out) == -1 && tube_qr_playlist_url("PL abc", out, sizeof out) == -1 && !*out);
    assert(tube_qr_playlist_url("PLabc&x=1", out, sizeof out) == -1 && tube_qr_playlist_url("PLabcdefghijk", tiny, sizeof tiny) == -1);
    assert(tube_qr_video_url("abcdefghijk", 9, out, sizeof out) == 0 && tube_qr_link_valid(out));
    assert(tube_qr_playlist_url("PLabcdefghijk", out, sizeof out) == 0 && tube_qr_link_valid(out));

    /* Row contract. */
    assert(tube_qr_row_payload("auth:session", "auth:info:code", "ABC-DEF-GHI", "", out, sizeof out) == 1);
    assert(!strcmp(out, "https://youtube.com/qr/activate/ABC-DEF-GHI"));
    assert(!tube_qr_row_payload("auth:session", "auth:info:url", "https://yt.be/activate", "", out, sizeof out) && !*out);
    assert(!tube_qr_row_payload("auth:session", "auth:info", "Waiting for sign-in", "", out, sizeof out));
    assert(!tube_qr_row_payload("auth:session", "auth:info:code", "not a code!", "", out, sizeof out) && !*out);
    assert(!tube_qr_row_payload("auth:session", "auth:info:code", NULL, NULL, out, sizeof out));
    assert(!tube_qr_row_payload("auth:session", "link:x", "Label", "https://example.org", out, sizeof out));
    assert(!tube_qr_row_payload("settings", "auth:info:code", "ABC-DEF-GHI", "", out, sizeof out));
    assert(tube_qr_row_payload("link:about", "link:about:source", "Source code", "https://github.com/blackbearreloaded/ProsperoTube", out, sizeof out) == 1);
    assert(!strcmp(out, "https://github.com/blackbearreloaded/ProsperoTube"));
    assert(!tube_qr_row_payload("link:about", "about:back", "Back", "https://example.org", out, sizeof out) && !*out);
    assert(!tube_qr_row_payload("link:about", "link:about:source", "https://example.org", "Open on a phone", out, sizeof out));
    assert(!tube_qr_row_payload("link:about", "link:x", "Label", NULL, out, sizeof out) && !tube_qr_row_payload("link:about", NULL, "", "", out, sizeof out));
    assert(!tube_qr_row_payload("about", "link:about:source", "Source", "https://example.org", out, sizeof out));
    assert(!tube_qr_row_payload(NULL, "link:x", "Label", "https://example.org", out, sizeof out));
    assert(!tube_qr_row_payload("link:about", "link:x", "Label", "https://example.org/x", tiny, sizeof tiny) && !*tiny);
    assert(tube_qr_row_payload("link:about", "link:x", "Label", "https://example.org", tiny, sizeof tiny) == 1);
    assert(!tube_qr_row_payload("link:about", "link:x", "Label", "https://example.org", NULL, 0));

    /* Markup for every version, and the densest bitmap of each size. */
    assert(tube_qr_module_dp(NULL) == 0 && tube_qr_square_dp(NULL) == 0);
    memset(&code, 0, sizeof code);
    assert(tube_qr_module_dp(&code) == 0 && tube_qr_markup(&code, g_markup, sizeof g_markup) == -1);
    unsigned seed = 12345;
    for (int version = 1; version <= 10; version++) {
        uint8_t bytes[TUBE_QR_MAX_PAYLOAD];
        size_t length = tube_qr_capacity(version, TUBE_QR_LEVEL_M);
        for (size_t i = 0; i < length; i++) { seed = seed * 1103515245u + 12345u; bytes[i] = (uint8_t)(seed >> 16); }
        assert(tube_qr_encode_ex(bytes, length, TUBE_QR_LEVEL_M, version, TUBE_QR_MASK_AUTO, &code) == 0 && code.version == version);
        assert(tube_qr_module_dp(&code) == 416 / (code.size + 8) && tube_qr_square_dp(&code) == tube_qr_module_dp(&code) * (code.size + 8));
        int length_written = tube_qr_markup(&code, g_markup, sizeof g_markup);
        assert(length_written > 0 && (size_t)length_written == strlen(g_markup));
        int boxes = painted_matches(&code, g_markup), dark = 0;
        for (int i = 0; i < code.size * code.size; i++) dark += code.modules[i];
        /* Merging keeps the element count well under one per dark module. */
        assert(boxes > 0 && boxes < dark * 2 / 3 && boxes < 1000);
        /* A buffer one byte short is refused whole. */
        char *exact = (char *)malloc((size_t)length_written + 1);
        assert(exact && tube_qr_markup(&code, exact, (size_t)length_written + 1) == length_written && !strcmp(exact, g_markup));
        exact[0] = 'x';
        assert(tube_qr_markup(&code, exact, (size_t)length_written) == -1 && !*exact);
        free(exact);
        for (int i = 0; i < code.size * code.size; i++) code.modules[i] = (uint8_t)((i % code.size + i / code.size) % 2 == 0);
        length_written = tube_qr_markup(&code, g_markup, sizeof g_markup);
        assert(length_written > 0 && painted_matches(&code, g_markup) == (code.size * code.size + 1) / 2);
    }
    assert(code.size == TUBE_QR_MAX_SIZE && strlen(g_markup) < TUBE_QR_MARKUP_SIZE - 1024);
    assert(tube_qr_markup(&code, g_markup, 10) == -1 && !*g_markup && tube_qr_markup(&code, NULL, 0) == -1 && tube_qr_markup(NULL, g_markup, sizeof g_markup) == -1);

    puts("PASS: QR page rows, sign-in and share addresses, and box markup that paints back to the symbol");
    return 0;
}
