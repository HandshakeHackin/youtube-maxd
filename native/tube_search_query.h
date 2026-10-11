/* UTF-8 search input bounds. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_SEARCH_QUERY_H
#define TUBE_SEARCH_QUERY_H
static inline int tube_search_query_valid(const char *value) {
    if (!value) return 0;
    const unsigned char *p = (const unsigned char *)value;
    unsigned characters = 0; int visible = 0;
    while (*p) {
        unsigned c = *p++, tails = 0, minimum = 0;
        if (c >= 0xc2 && c <= 0xdf) { tails = 1; c &= 31; minimum = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { tails = 2; c &= 15; minimum = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { tails = 3; c &= 7; minimum = 0x10000; }
        else if (c >= 0x80) return 0;
        for (unsigned i = 0; i < tails; ++i) {
            if ((*p & 0xc0) != 0x80) return 0;
            c = (c << 6) | (*p++ & 63);
        }
        if (c < minimum || c < 32 || c == 127 || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) return 0;
        if (++characters > 200) return 0;
        if (c != 32) visible = 1;
    }
    return visible;
}
#endif
