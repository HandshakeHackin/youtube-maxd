// About section data: name, version, project links, feedback, credits and the
// changelog rows generated from CHANGELOG.md.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Header-only and read-only. The version is not written here: the console
// build passes the packaged VERSION file as EVO_PLAYER_VERSION, and host
// builds fall back to the newest CHANGELOG.md section, which
// `scripts/generate_about.py --check` keeps equal to the packaging scripts.
#ifndef TUBE_ABOUT_H
#define TUBE_ABOUT_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "tube_about_data.h"

#define TUBE_ABOUT_NAME "ProsperoTube"
#define TUBE_ABOUT_TAGLINE "A native YouTube client for PS5 homebrew"
#define TUBE_ABOUT_LICENSE "GPL-3.0-or-later"
#define TUBE_ABOUT_AUTHOR "BlackBearReloaded"
#define TUBE_ABOUT_BROUGHT "Based on ProsperoTube by " TUBE_ABOUT_AUTHOR
#define TUBE_ABOUT_UNOFFICIAL "Not affiliated with or endorsed by YouTube or Google"
#define TUBE_ABOUT_PROJECT_URL "https://github.com/blackbearreloaded/ProsperoTube"
#ifdef EVO_PLAYER_VERSION
#define TUBE_ABOUT_VERSION EVO_PLAYER_VERSION
#else
#define TUBE_ABOUT_VERSION TUBE_ABOUT_CHANGELOG_VERSION
#endif

typedef struct {
    const char *id;     /* stable row suffix: "releases", "source", ... */
    const char *title;
    const char *detail; /* one line under the title */
    const char *url;    /* shown as text and as a QR code */
    int feedback;       /* 1 for a feedback channel, 0 for a project link */
} tube_about_link;

typedef struct {
    const char *name;
    const char *detail;
    const char *notice; /* words that locate it in THIRD_PARTY_NOTICES.md */
} tube_about_credit;

typedef struct {
    const char *version;
    const char *text;
} tube_about_change;

static inline const char *tube_about_version(void) { return TUBE_ABOUT_VERSION; }

/* "ProsperoTube 1.000.000". Returns 0, or -1 when it does not fit. */
static inline int tube_about_title(char *out, size_t size) {
    int written = out && size ? snprintf(out, size, "%s %s", TUBE_ABOUT_NAME, TUBE_ABOUT_VERSION) : -1;
    if (written < 0 || (size_t)written >= size) { if (out && size) out[0] = 0; return -1; }
    return 0;
}

/* Project links first (SmartTube's Releases and Sources), then feedback. */
static inline const tube_about_link *tube_about_link_at(int index) {
    static const tube_about_link links[] = {
        {"releases", "Upstream releases", "ProsperoTube releases; Max’d updates are distributed separately", TUBE_ABOUT_PROJECT_URL "/releases", 0},
        {"source", "Upstream source", "Max’d changes and matching source are included with its release", TUBE_ABOUT_PROJECT_URL, 0},
        {"notices", "Third-party notices", "Licenses and sources of everything ProsperoTube builds on",
         TUBE_ABOUT_PROJECT_URL "/blob/main/THIRD_PARTY_NOTICES.md", 0},
        {"issues", "Upstream issue tracker", "Report Max’d-specific problems to its distributor",
         TUBE_ABOUT_PROJECT_URL "/issues", 1},
    };
    return index >= 0 && (size_t)index < sizeof links / sizeof *links ? &links[index] : NULL;
}

static inline int tube_about_link_count(void) {
    int count = 0;
    while (tube_about_link_at(count)) count++;
    return count;
}

static inline const tube_about_link *tube_about_link_find(const char *id) {
    const tube_about_link *link;
    for (int i = 0; id && (link = tube_about_link_at(i)) != NULL; i++)
        if (!strcmp(link->id, id)) return link;
    return NULL;
}

/* Who made it, what it is not, whose approach it follows, what it is built on and what it carries, each
 * with its licence; THIRD_PARTY_NOTICES.md (shipped in the package under assets/prosperotube-notices) stays
 * the complete and binding text. At most one page of rows (24): the Afterglow reader shows one page. */
static inline const tube_about_credit *tube_about_credit_at(int index) {
    static const tube_about_credit credits[] = {
        {TUBE_ABOUT_NAME, TUBE_ABOUT_BROUGHT ". Free software under " TUBE_ABOUT_LICENSE ".", "(C) 2026 BlackBearReloaded"},
        {"An unofficial client", TUBE_ABOUT_UNOFFICIAL ". YouTube is a trademark of Google LLC.", "not affiliated with or endorsed by YouTube or Google"},
        {"SmartTube", "The Android TV client whose behaviour ProsperoTube follows. MIT. None of its code or assets is included.", "SmartTube (MIT"},
        {"FreeTube", "The desktop client whose way of reading YouTube without an account the built-in service follows. AGPL-3.0-or-later. No code included.", "FreeTube (AGPL-3.0-or-later"},
        {"EVO Player", "The player ProsperoTube is built on, by sainsaji; derived from ProsperoPlayer by KINGDKAK. GPL-3.0-or-later.", "ProsperoPlayer"},
        {"YouTube.js (youtubei.js) 18.1.0", "YouTube access of the built-in service. MIT.", "YouTube.js 18.1.0"},
        {"QuickJS-NG 0.17.0", "JavaScript engine of the built-in service. MIT.", "QuickJS-NG v0.17.0"},
        {"FFmpeg 7.1.1", "Demuxing and software decoding. LGPL-2.1-or-later, with GPL parts as configured.", "FFmpeg: LGPL-2.1-or-later"},
        {"dav1d 1.5.1", "AV1 software decoding. BSD-2-Clause.", "dav1d: BSD-2-Clause"},
        {"curl 8.18.0, libpsl 0.21.5 and Mozilla's root certificates", "HTTPS of the built-in service and its trusted roots. curl license, MIT and MPL-2.0.", "libcurl 8.18.0"},
        {"OpenSSL 3.5.2", "TLS. Apache-2.0.", "OpenSSL: Apache-2.0"},
        {"SponsorBlock and DeArrow", "Community segment data, titles and thumbnails by Ajay Ramachandran and contributors. CC BY-NC-SA 4.0.", "DeArrow title/thumbnail metadata"},
        {"RmlUi 6.2, SDL2 and zlib", "The classic interface, the platform layer and compression. MIT and zlib license.", "RmlUi: MIT"},
        {"ps5-homebrew-ui", "The interface kit Afterglow and the player's controls are drawn with, by the same author. GPL-3.0-or-later.", "ps5-homebrew-ui"},
        {"HarfBuzz 9.0.0, FreeType 2.13.2 and GNU FriBidi 1.0.15", "Text shaping, glyphs and text direction. Old MIT, FreeType Project License and LGPL-2.1-or-later.", "HarfBuzz 9.0.0"},
        {"Inter, Montserrat and DejaVu Sans Mono", "The typefaces. SIL Open Font License 1.1; Bitstream Vera and DejaVu licences.", "Montserrat Medium (SIL Open Font License 1.1)"},
        {"Interface sounds", "Original procedural synthesis by BlackBearReloaded. GPL-3.0-or-later.", "original procedural synthesis by this project"},
        {"cJSON 1.7.19 and PS5-Lapy-JB-Daemon", "JSON reading and writing; the packaged one-shot storage helper. MIT.", "PS5-Lapy-JB-Daemon"},
        {"PS5 Payload SDK 0.42, Pacbrew 0.39 and MkPFS", "Toolchain, runtime libraries and package image. The SDK is GPL-3.0-or-later; the others keep their upstream licenses.", "PS5 Payload SDK v0.42"},
        {"ProsperoTV, ProsperoLight and ps5-native-app-boilerplate", "Elevation client, entropy reader, self-exit and the update kit, by the same author. GPL-3.0-or-later.", "ProsperoLight"},
        {"Unicode Character Database 15.0.0", "Case mapping for channel names. Unicode License V3.", "UnicodeData 15.0.0"},
        {"yt-dlp, FastAPI, Uvicorn, HTTPX and whisper.cpp", "The optional development tool on a computer; not part of the app.", "yt-dlp"},
        {"Full notices", "THIRD_PARTY_NOTICES.md in the package and in the source repository.", "Third-party notices and corresponding source"},
    };
    return index >= 0 && (size_t)index < sizeof credits / sizeof *credits ? &credits[index] : NULL;
}

static inline int tube_about_credit_count(void) {
    int count = 0;
    while (tube_about_credit_at(count)) count++;
    return count;
}

/* Changelog rows, newest version first. `version` selects one version's rows
 * (SmartTube lists the installed version's changes); NULL selects all. */
static inline const tube_about_change *tube_about_change_at(const char *version, int index) {
    static const tube_about_change changes[TUBE_ABOUT_CHANGELOG_ROWS] = {TUBE_ABOUT_CHANGELOG_DATA};
    for (int i = 0; index >= 0 && i < TUBE_ABOUT_CHANGELOG_ROWS; i++) {
        if (version && strcmp(changes[i].version, version)) continue;
        if (index-- == 0) return &changes[i];
    }
    return NULL;
}

static inline int tube_about_change_count(const char *version) {
    int count = 0;
    while (tube_about_change_at(version, count)) count++;
    return count;
}

#endif
