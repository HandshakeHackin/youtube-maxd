/* Update check and in-place self-update.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The check asks the project's GitHub releases for the newest version, compares
 * content versions (sce_sys/param.json contentVersion, "NN.NNN.NNN") and keeps
 * the answer for a day. The update downloads the release ZIP in pieces (so it
 * can be cancelled and resumed), requires the size and SHA-256 the release
 * lists, unpacks it beside the app's data, verifies every file, and then swaps
 * the top-level entries of the installed folder for the new ones. The previous
 * entries are kept until the new version has started properly; a journal makes
 * every step either complete or undone at the next start.
 *
 * The engine is plain C and owns one worker thread. It reaches the network
 * through a transport (tube_update_http.c puts tube_js_http behind it), so it
 * does not depend on the JavaScript service being healthy.
 *
 * The design follows the update kit of ps5-native-app-boilerplate (signed
 * catalog check, loader-started helper); see docs/UPDATES.md for what was kept
 * and what differs, and THIRD_PARTY_NOTICES.md. */
#ifndef TUBE_UPDATE_H
#define TUBE_UPDATE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define TUBE_UPDATE_TITLE "PPSA99142"
/* The one place the update source is named. The repository must be public for
 * this to answer without credentials; the app sends none. */
#define TUBE_UPDATE_MANIFEST_URL "https://api.github.com/repos/blackbearreloaded/ProsperoTube/releases?per_page=10"
/* A release file is taken only from here (and GitHub's own file hosts after a redirect). */
#define TUBE_UPDATE_DOWNLOAD_PREFIX "https://github.com/blackbearreloaded/ProsperoTube/releases/download/"
#define TUBE_UPDATE_ASSET TUBE_UPDATE_TITLE ".zip"
/* The only folder that is updated in place. */
#define TUBE_UPDATE_INSTALL_DIR "/data/homebrew/" TUBE_UPDATE_TITLE
#define TUBE_UPDATE_USER_AGENT "ProsperoTube-update"
#define TUBE_UPDATE_NOTES_SIZE 16384
#define TUBE_UPDATE_MAX_ARCHIVE (1024ull * 1024 * 1024)
#define TUBE_UPDATE_MAX_UNPACKED (2048ull * 1024 * 1024)

/* ---- Versions ---- */
/* "01.000.010", "v1.0.10" or "0.1.0": three numbers, an optional leading v. 1 = parsed. */
int tube_update_version_parse(const char *text, unsigned parts[3]);
/* <0, 0, >0 as a is older than, equal to, newer than b. Unparsable sorts lowest. */
int tube_update_version_compare(const char *a, const char *b);

/* ---- Release metadata ---- */
typedef struct tube_update_offer {
    char version[20];                 /* the release's tag without its v: the content version */
    char name[80];                    /* the release's title, for people */
    char url[600];                    /* the ZIP */
    char sha256[65];
    uint64_t size;
    char page[300];                   /* the release's page, to read on another device */
    char notes[TUBE_UPDATE_NOTES_SIZE];   /* plain text; "" when the release has none */
    int notes_truncated;
} tube_update_offer;

enum {
    TUBE_UPDATE_MANIFEST_OK = 0,          /* the newest release, complete */
    TUBE_UPDATE_MANIFEST_NONE,            /* a valid answer without any version release */
    TUBE_UPDATE_MANIFEST_NOT_INSTALLABLE, /* newest release: no ZIP, or no usable size, address or digest; version and page are set */
    TUBE_UPDATE_MANIFEST_INVALID          /* not the expected JSON */
};
/* Reads GitHub's "list releases" answer. Drafts and tags that are not versions
 * are passed over; pre-releases count. `download_prefix` is what the ZIP's
 * address must start with. */
int tube_update_manifest_parse(const char *json, size_t length, const char *download_prefix, tube_update_offer *offer);
/* Markdown as GitHub shows release notes to plain lines ("- " bullets, blank
 * line before headings). Returns 1 when the text did not fit. */
int tube_update_plain_notes(const char *markdown, char *out, size_t size);

/* ---- Transport ---- */
typedef struct tube_update_reply {
    int error;                    /* 0, or a transport failure (text in `message`) */
    long status;
    unsigned char *body;          /* malloc'd; the caller frees */
    size_t length;
    char location[2048];          /* of a 3xx answer */
    char content_range[96];
    char message[128];
} tube_update_reply;
typedef struct tube_update_transport {
    void *context;
    /* One blocking GET that follows no redirect. `ranged`: ask for bytes
     * first..last. `limit`: refuse a larger body. `cancel` may become non-zero
     * at any time: return soon with error set. Returns 0 when `reply` is filled. */
    int (*get)(void *context, const char *url, int ranged, uint64_t first, uint64_t last, size_t limit,
               const volatile int *cancel, tube_update_reply *reply);
} tube_update_transport;

/* ---- Engine ---- */
enum {  /* tube_update_status.check */
    TUBE_UPDATE_UNKNOWN = 0,      /* never asked, or no usable answer */
    TUBE_UPDATE_UP_TO_DATE,
    TUBE_UPDATE_AVAILABLE,
    TUBE_UPDATE_NOT_INSTALLABLE   /* newer, but it cannot be installed from here: read the release page */
};
enum {  /* tube_update_status.phase */
    TUBE_UPDATE_IDLE = 0,
    TUBE_UPDATE_CHECKING,
    TUBE_UPDATE_DOWNLOADING,      /* done/total are bytes of the ZIP */
    TUBE_UPDATE_VERIFYING,
    TUBE_UPDATE_UNPACKING,        /* done/total are bytes unpacked */
    TUBE_UPDATE_STAGED,           /* verified and waiting for tube_update_apply() */
    TUBE_UPDATE_APPLIED,          /* the files are in place: close the app */
    TUBE_UPDATE_FAILED            /* `error` says why; the installed app is untouched */
};
enum {  /* tube_update_status.blocked: why this copy cannot update itself */
    TUBE_UPDATE_ALLOWED = 0,
    TUBE_UPDATE_BLOCKED_DEVELOPMENT,   /* a development build */
    TUBE_UPDATE_BLOCKED_LOCATION,      /* not running from the install folder */
    TUBE_UPDATE_BLOCKED_IMAGE,         /* installed as an image */
    TUBE_UPDATE_BLOCKED_VERSION,       /* the install folder holds another version, or none */
    TUBE_UPDATE_BLOCKED_STORAGE        /* the update folder cannot be written */
};
enum {  /* tube_update_boot() */
    TUBE_UPDATE_BOOT_NONE = 0,
    TUBE_UPDATE_BOOT_PENDING,     /* first start of a new version: call tube_update_confirm_start() once it runs */
    TUBE_UPDATE_BOOT_STAGED,      /* an update staged earlier is still waiting */
    TUBE_UPDATE_BOOT_RECOVERED,   /* an interrupted replacement was undone */
    TUBE_UPDATE_BOOT_ROLLED_BACK  /* the new version did not start properly: the previous one is back; restart */
};
enum { TUBE_UPDATE_FAULT_NONE = 0, TUBE_UPDATE_FAULT_ERROR, TUBE_UPDATE_FAULT_STOP };

typedef struct tube_update_config {
    const char *title_id;         /* NULL = TUBE_UPDATE_TITLE */
    const char *data_dir;         /* the app's data folder; the engine uses <data_dir>/update */
    const char *install_dir;      /* NULL = TUBE_UPDATE_INSTALL_DIR */
    const char *running_dir;      /* the folder the running app was started from (holds sce_sys/param.json) */
    /* Where the console registers apps ("/user"): ShadowMountPlus's record
     * <registered_dir>/app/<title>/mount.lnk or mount_img.lnk, and the copies of
     * sce_sys the home screen reads. NULL = none. */
    const char *registered_dir;
    const char *manifest_url;     /* NULL = TUBE_UPDATE_MANIFEST_URL */
    const char *download_prefix;  /* NULL = TUBE_UPDATE_DOWNLOAD_PREFIX */
    /* 1 = a development build: it never updates itself from the releases. It
     * alone reads <data_dir>/update-offer.txt, a test offer (docs/UPDATES.md). */
    int development;
    int any_host;                 /* tests: any redirect target and plain http are accepted */
    size_t chunk_bytes;           /* one download request; 0 = 4 MiB */
    long check_interval_s;        /* 0 = one day */
    int start_limit;              /* unconfirmed starts of a new version before it is rolled back; 0 = 2 */
    tube_update_transport transport;
    long long (*now)(void);                         /* NULL = time() */
    long long (*free_bytes)(const char *path);      /* NULL = statvfs; <0 = unknown */
    /* Tests: named step about to run. ERROR makes it fail as an I/O error would;
     * STOP abandons the operation there and then, as a power loss would. */
    int (*fault)(const char *step);
} tube_update_config;

typedef struct tube_update_status {
    int phase, check, blocked;
    int manual;                   /* the last check was asked for by the user */
    int busy;                     /* the worker is running */
    int notify;                   /* "Notify about updates" */
    int from_cache;               /* the answer is the stored one */
    char installed[20];
    char latest[20], latest_name[80], page[300];
    char skipped[20];             /* "Skip this version" */
    char staged[20];              /* the version waiting in the staging folder */
    uint64_t done, total, size;
    long long checked_at;         /* 0 = never */
    char error[280];
    char blocked_text[280];
    unsigned revision;            /* changes whenever anything above does */
} tube_update_status;

typedef struct tube_update tube_update;

tube_update *tube_update_new(const tube_update_config *config);
/* Cancels and joins the worker. */
void tube_update_free(tube_update *update);
/* Once, at start, before the interface: finishes or undoes what an earlier run
 * left, and counts starts of a version that has not been confirmed. */
int tube_update_boot(tube_update *update);
/* The app is up: the new version stays and the previous one is deleted.
 * Returns 1 when this confirmed an update (status.installed is the new version). */
int tube_update_confirm_start(tube_update *update);

/* Worker requests: 1 = started, 0 = busy or not possible now. */
int tube_update_check(tube_update *update, int manual);
int tube_update_download(tube_update *update);
void tube_update_cancel(tube_update *update);
/* The same work on the calling thread (what the worker runs). */
void tube_update_run_check(tube_update *update, int manual);
void tube_update_run_download(tube_update *update);
/* Staged -> in place. Quick (renames). 1 = done, close the app now; 0 = see status.error. */
int tube_update_apply(tube_update *update);
/* Drops a staged or partly downloaded update. */
void tube_update_discard(tube_update *update);

void tube_update_get(tube_update *update, tube_update_status *status);
/* The newest release's notes; valid until the next check. */
const char *tube_update_notes(tube_update *update);
int tube_update_notes_truncated(tube_update *update);
void tube_update_set_notify(tube_update *update, int enabled);
/* "" forgets the skipped version. */
void tube_update_skip(tube_update *update, const char *version);
/* 1 once per run when a version worth telling about is known: newer, notify on, not skipped. */
int tube_update_take_offer(tube_update *update);

/* ---- The transport over the app's HTTPS client (tube_update_http.c) ---- */
typedef struct tube_update_http tube_update_http;
/* `http_config` is a tube_js_http_config; what it points at must stay valid.
 * Certificate verification is that client's, which is always on. Requests
 * carry the generic user agent and nothing that identifies the console or
 * the user. */
tube_update_http *tube_update_http_new(const void *http_config);
void tube_update_http_free(tube_update_http *http);
tube_update_transport tube_update_http_transport(tube_update_http *http);

/* ---- The app's one updater (tube_update_app.c). Main thread. ---- */
typedef struct tube_update_row { char id[64], title[160], detail[200]; } tube_update_row;
/* Creates the updater and runs tube_update_boot(). Safe to call again. */
void tube_update_app_init(void);
void tube_update_app_shutdown(void);
/* Every frame: starts the daily check once `service_ready`, confirms a new version's start. */
void tube_update_app_tick(int service_ready);
/* About's rows: "settings:update:<action>" ids, and "description:update" for
 * the release notes page. Returns how many were written. */
int tube_update_app_rows(tube_update_row *rows, int capacity);
/* A selected row's action ("check", "notify", "install", "cancel", "apply", "skip", "discard"). 1 = known. */
int tube_update_app_action(const char *action);
/* Release notes for the reader, and the title of that page. "" = none. */
const char *tube_update_app_notes(void);
const char *tube_update_app_notes_title(void);
/* One-shot messages for the host: a notice to show (NULL = none), the list to
 * redraw (only while `focused_id` is one of the update rows), the app to close. */
const char *tube_update_app_take_notice(void);
int tube_update_app_take_refresh(const char *focused_id);
int tube_update_app_take_restart(void);
/* Tests: the updater the app functions use, replacing the console's (the
 * caller frees it), and the clock in seconds. */
void tube_update_app_use(tube_update *update);
void tube_update_app_set_clock(long long (*clock)(void));

#ifdef __cplusplus
}
#endif
#endif
