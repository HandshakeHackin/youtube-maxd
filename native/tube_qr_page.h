// QR codes on catalog pages: which rows carry one, the links they hold, and
// the markup that draws a symbol with plain boxes.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Page contract (provider side):
//   * Sign-in page, parent id "auth:...": the row "auth:info:code" has the
//     device code as its title. Its QR opens the activation address with the
//     code already filled in, as SmartTube's sign-in screen does.
//   * Link page, any parent id starting with "link:": every row whose id also
//     starts with "link:" is a link; its `overview` is the address (http or
//     https, printable ASCII, at most TUBE_QR_MAX_PAYLOAD bytes), its title
//     the label. The screen shows the focused link's address as text and as
//     a QR code, and falls back to the page's first link.
//
// The symbol is drawn as absolutely positioned dark boxes on a light square,
// so it needs no texture, stays sharp at any scale and behaves the same on
// the CPU and GPU renderers.
#ifndef TUBE_QR_PAGE_H
#define TUBE_QR_PAGE_H

#include <stdio.h>

#include "tube_qr.h"

#define TUBE_QR_PAGE_NONE 0
#define TUBE_QR_PAGE_SIGN_IN 1
#define TUBE_QR_PAGE_LINK 2
#define TUBE_QR_PAYLOAD_SIZE (TUBE_QR_MAX_PAYLOAD + 1)
#define TUBE_QR_QUIET_MODULES 4
/* Side of the light square in dp; the module size is the largest whole dp
 * that fits, so module edges land on pixels. */
#define TUBE_QR_TARGET_DP 416
/* Enough for any 57x57 bitmap: at most 1625 boxes of at most 61 bytes. */
#define TUBE_QR_MARKUP_SIZE (104 * 1024)
#define TUBE_QR_ACTIVATE_URL "https://youtube.com/qr/activate/"

static inline int tube_qr_page_kind(const char *parent) {
    if (!parent) return TUBE_QR_PAGE_NONE;
    if (!strncmp(parent, "auth:", 5)) return TUBE_QR_PAGE_SIGN_IN;
    if (!strncmp(parent, "link:", 5)) return TUBE_QR_PAGE_LINK;
    return TUBE_QR_PAGE_NONE;
}

/* 1 for an address a phone can open: http:// or https:// followed by a host,
 * printable ASCII only, short enough for a version 10 symbol. */
static inline int tube_qr_link_valid(const char *url) {
    size_t length = 0, scheme;
    if (!url) return 0;
    if (!strncmp(url, "https://", 8)) scheme = 8;
    else if (!strncmp(url, "http://", 7)) scheme = 7;
    else return 0;
    for (; url[length]; length++) {
        unsigned char c = (unsigned char)url[length];
        if (c <= 0x20 || c >= 0x7F || c == '"' || c == '<' || c == '>' || c == '\\' || length >= TUBE_QR_MAX_PAYLOAD) return 0;
    }
    return length > scheme && url[scheme] != '/' && url[scheme] != '.' && url[scheme] != ':';
}

/* Device code as shown to the user: letters, digits and dashes. */
static inline int tube_qr_activation_url(const char *code, char *out, size_t size) {
    size_t length = code ? strlen(code) : 0;
    if (out && size) out[0] = 0;
    if (!out || length < 4 || length > 32 || sizeof TUBE_QR_ACTIVATE_URL + length > size) return -1;
    for (size_t i = 0; i < length; i++) {
        char c = code[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return -1;
    }
    memcpy(out, TUBE_QR_ACTIVATE_URL, sizeof TUBE_QR_ACTIVATE_URL - 1);
    memcpy(out + sizeof TUBE_QR_ACTIVATE_URL - 1, code, length + 1);
    return 0;
}

static inline int tube_qr_id_text(const char *id, size_t minimum, size_t maximum) {
    size_t length = id ? strlen(id) : 0;
    if (length < minimum || length > maximum) return 0;
    for (size_t i = 0; i < length; i++) {
        char c = id[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return 0;
    }
    return 1;
}

/* Share addresses in the forms SmartTube shares. `position_sec` above zero
 * adds a start time to a video link. Each returns 0, or -1 for a malformed
 * id or a small buffer (the output is then empty). */
static inline int tube_qr_video_url(const char *video_id, long long position_sec, char *out, size_t size) {
    int written = -1;
    if (out && size) out[0] = 0;
    if (!out || !tube_qr_id_text(video_id, 11, 11)) return -1;
    if (position_sec > 0) written = snprintf(out, size, "https://youtu.be/%s?t=%lld", video_id, position_sec);
    else written = snprintf(out, size, "https://youtu.be/%s", video_id);
    if (written < 0 || (size_t)written >= size) { if (size) out[0] = 0; return -1; }
    return 0;
}

static inline int tube_qr_channel_url(const char *channel_id, char *out, size_t size) {
    int written = -1;
    if (out && size) out[0] = 0;
    if (!out || !tube_qr_id_text(channel_id, 24, 24) || strncmp(channel_id, "UC", 2)) return -1;
    written = snprintf(out, size, "https://www.youtube.com/channel/%s", channel_id);
    if (written < 0 || (size_t)written >= size) { if (size) out[0] = 0; return -1; }
    return 0;
}

static inline int tube_qr_playlist_url(const char *playlist_id, char *out, size_t size) {
    int written = -1;
    if (out && size) out[0] = 0;
    if (!out || !tube_qr_id_text(playlist_id, 2, 64)) return -1;
    written = snprintf(out, size, "https://www.youtube.com/playlist?list=%s", playlist_id);
    if (written < 0 || (size_t)written >= size) { if (size) out[0] = 0; return -1; }
    return 0;
}

/* The QR payload a row carries on its page, per the contract above. Returns
 * 1 and fills `out`, or 0 (empty `out`) when the row has none. */
static inline int tube_qr_row_payload(const char *parent, const char *id, const char *title, const char *overview,
                                      char *out, size_t size) {
    int kind = tube_qr_page_kind(parent);
    if (out && size) out[0] = 0;
    if (!out || !id) return 0;
    if (kind == TUBE_QR_PAGE_SIGN_IN)
        return !strcmp(id, "auth:info:code") && tube_qr_activation_url(title, out, size) == 0;
    if (kind == TUBE_QR_PAGE_LINK && !strncmp(id, "link:", 5) && tube_qr_link_valid(overview) && strlen(overview) < size) {
        memcpy(out, overview, strlen(overview) + 1);
        return 1;
    }
    return 0;
}

/* Module size in dp for a symbol on the TUBE_QR_TARGET_DP square. */
static inline int tube_qr_module_dp(const tube_qr_code *code) {
    return code && code->size ? TUBE_QR_TARGET_DP / (code->size + 2 * TUBE_QR_QUIET_MODULES) : 0;
}

/* Side of the light square, quiet zone included. */
static inline int tube_qr_square_dp(const tube_qr_code *code) {
    return code && code->size ? (code->size + 2 * TUBE_QR_QUIET_MODULES) * tube_qr_module_dp(code) : 0;
}

/* RML for the symbol: one sized container holding a box per dark rectangle
 * (runs merged across and then down). Returns the length written, or -1 when
 * `size` is too small or the symbol is empty; TUBE_QR_MARKUP_SIZE always
 * suffices. Style the container light and its children dark and absolute. */
static inline int tube_qr_markup(const tube_qr_code *code, char *out, size_t size) {
    uint8_t covered[TUBE_QR_MAX_SIZE * TUBE_QR_MAX_SIZE];
    int module = tube_qr_module_dp(code), side = tube_qr_square_dp(code), written;
    size_t used;
    if (out && size) out[0] = 0;
    if (!out || !module || code->size > TUBE_QR_MAX_SIZE) return -1;
    written = snprintf(out, size, "<div id=\"tube-qr-symbol\" style=\"width:%ddp;height:%ddp;\">", side, side);
    if (written < 0 || (size_t)written >= size) { if (size) out[0] = 0; return -1; }
    used = (size_t)written;
    memset(covered, 0, sizeof covered);
    for (int y = 0; y < code->size; y++) for (int x = 0; x < code->size; x++) {
        int width = 0, height = 1, grows = 1;
        if (!code->modules[y * code->size + x] || covered[y * code->size + x]) continue;
        while (x + width < code->size && code->modules[y * code->size + x + width] && !covered[y * code->size + x + width]) width++;
        while (y + height < code->size && grows) {
            for (int i = 0; i < width && grows; i++)
                grows = code->modules[(y + height) * code->size + x + i] && !covered[(y + height) * code->size + x + i];
            height += grows;
        }
        for (int j = 0; j < height; j++) memset(covered + (y + j) * code->size + x, 1, (size_t)width);
        written = snprintf(out + used, size - used, "<div style=\"left:%ddp;top:%ddp;width:%ddp;height:%ddp;\"/>",
                           (x + TUBE_QR_QUIET_MODULES) * module, (y + TUBE_QR_QUIET_MODULES) * module, width * module, height * module);
        if (written < 0 || (size_t)written >= size - used) { out[0] = 0; return -1; }
        used += (size_t)written;
    }
    written = snprintf(out + used, size - used, "</div>");
    if (written < 0 || (size_t)written >= size - used) { out[0] = 0; return -1; }
    return (int)(used + (size_t)written);
}

#endif
