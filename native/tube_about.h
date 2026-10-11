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

/* "ProsperoTube 0.1.0". Returns 0, or -1 when it does not fit. */
static inline int tube_about_title(char *out, size_t size) {
    int written = out && size ? snprintf(out, size, "%s %s", TUBE_ABOUT_NAME, TUBE_ABOUT_VERSION) : -1;
    if (written < 0 || (size_t)written >= size) { if (out && size) out[0] = 0; return -1; }
    return 0;
}

/* Project links first (SmartTube's Releases and Sources), then feedback. */
static inline const tube_about_link *tube_about_link_at(int index) {
    static const tube_about_link links[] = {
        {"releases", "Releases", "Download new versions and read their notes", TUBE_ABOUT_PROJECT_URL "/releases", 0},
        {"source", "Source code", "ProsperoTube is free software under GPL-3.0-or-later", TUBE_ABOUT_PROJECT_URL, 0},
        {"notices", "Third-party notices", "Licenses and sources of everything ProsperoTube builds on",
         TUBE_ABOUT_PROJECT_URL "/blob/main/THIRD_PARTY_NOTICES.md", 0},
        {"issues", "Report a problem", "Open the issue tracker to report a bug or ask for a feature",
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

/* Short credit lines; THIRD_PARTY_NOTICES.md (shipped in the package under
 * assets/prosperotube-notices) stays the complete and binding text. */
static inline const tube_about_credit *tube_about_credit_at(int index) {
    static const tube_about_credit credits[] = {
        {"EVO player", "Playback, decoding and interface base, derived from ProsperoPlayer. GPL-3.0-or-later.", "ProsperoPlayer"},
        {"ProsperoTV and ProsperoLight", "Elevation client, entropy reader and self-exit usage. GPL-3.0-or-later.", "ProsperoLight"},
        {"PS5-Lapy-JB-Daemon", "Packaged one-shot storage helper. MIT.", "PS5-Lapy-JB-Daemon"},
        {"cJSON 1.7.19", "JSON reading and writing. MIT.", "cJSON 1.7.19"},
        {"FFmpeg 7.1.1 and dav1d 1.5.1", "Demuxing and software decoding, under their upstream licenses.", "FFmpeg 7.1.1"},
        {"RmlUi 6.2, SDL2, FreeType, OpenSSL and zlib", "Interface, fonts, TLS and compression, under their upstream licenses.", "RmlUi 6.2"},
        {"PS5 Payload SDK, Pacbrew and MkPFS", "Toolchain, runtime libraries and package image.", "PS5 Payload SDK"},
        {"yt-dlp, FastAPI, Uvicorn and HTTPX", "The companion service on your computer.", "yt-dlp"},
        {"YouTube.js 18.1.0", "Optional companion library for account features. MIT.", "YouTube.js 18.1.0"},
        {"SponsorBlock", "Segment data by Ajay Ramachandran and contributors. CC BY-NC-SA 4.0.", "SponsorBlock community database"},
        {"DeArrow", "Community titles and thumbnails by Ajay Ramachandran and contributors. CC BY-NC-SA 4.0.", "DeArrow title/thumbnail metadata"},
        {"whisper.cpp", "Optional local speech recognition on the companion host. MIT.", "whisper.cpp (MIT)"},
        {"Unicode Character Database 15.0.0", "Case mapping for channel names. Unicode License V3.", "UnicodeData 15.0.0"},
        {"SmartTube", "Behaviour reference only; none of its code or assets is included.", "SmartTube/MediaServiceCore"},
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
