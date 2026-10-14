/* The app's one updater: what the provider lists in About and what the host
 * shows; see tube_update.h.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_update.h"
#include "update_check.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(TUBE_EMBEDDED_SERVICE) && defined(EVO_APP_MODULE)
#include <sys/stat.h>
#include "evo_data_path.h"
#include "tube_update_paths.h"
#endif

#define CHECK_AFTER_S 20        /* the startup check does not wait for the service longer than this */

static tube_update *g_update;
static int g_owned;
static int g_tried, g_checked, g_manual, g_restart, g_have_notice, g_watching;
static long long g_started, g_refreshed;
static unsigned g_listed;
static int g_phase;
static char g_notice[320], g_notes_title[120];

static long long (*g_clock)(void);
static long long clock_now(void) { return g_clock ? g_clock() : (long long)time(NULL); }

static void notice(const char *message) { snprintf(g_notice, sizeof g_notice, "%s", message); g_have_notice = 1; }

static void adopt(tube_update *update) {
    tube_update_status status;
    g_update = update;
    g_checked = g_manual = g_restart = g_have_notice = g_watching = 0;
    g_started = clock_now(); g_refreshed = 0;
    if (!update) return;
    tube_update_get(update, &status);
    g_phase = status.phase; g_listed = status.revision;
}

void tube_update_app_use(tube_update *update) { adopt(update); g_tried = 1; g_owned = 0; }
void tube_update_app_set_clock(long long (*clock)(void)) { g_clock = clock; }

void tube_update_app_init(void) {
    if (g_tried) return;
    g_tried = 1;
#if defined(TUBE_EMBEDDED_SERVICE) && defined(EVO_APP_MODULE)
    static char data[512];
    char title[10], installed[12];
    const char *param = tube_update_param_path();
    snprintf(data, sizeof data, "%s", evo_data_dir());
    if (!update_check_read_param(param, title, installed) || strcmp(title, TUBE_UPDATE_TITLE)) return;
    tube_update_config config = {.data_dir = data, .installed = installed, .platform = self_update_console()};
#ifdef EVO_USB_REMOTE
    config.development = 1;
#endif
    adopt(tube_update_new(&config));
    g_owned = g_update != NULL;
#endif
}

void tube_update_app_shutdown(void) {
    if (g_owned) tube_update_free(g_update);
    g_update = NULL; g_owned = 0; g_tried = 0;
}

void tube_update_app_tick(int service_ready) {
    tube_update_status status;
    if (!g_update) return;
    const long long elapsed = clock_now() - g_started;
    if (!g_checked && (service_ready || elapsed >= CHECK_AFTER_S)) g_checked = tube_update_check(g_update, 0);
    tube_update_get(g_update, &status);
    if (g_manual && !status.busy) {   /* only a check the user asked for reports its outcome */
        g_manual = 0;
        if (*status.error) notice(status.error);
        else if (status.check == TUBE_UPDATE_UP_TO_DATE) notice("ProsperoTube is up to date.");
        else if (status.check == TUBE_UPDATE_UNKNOWN) notice("No release was found.");
    }
    if (status.phase != g_phase) {
        if (status.phase == TUBE_UPDATE_STAGED) notice("The update is ready. Select Restart to finish in About.");
        else if (status.phase == TUBE_UPDATE_FAILED && *status.error) notice(status.error);
        g_phase = status.phase;
    }
    /* A version that is already prepared has its own notice. */
    if (status.phase != TUBE_UPDATE_STAGED && status.phase != TUBE_UPDATE_APPLIED && tube_update_take_offer(g_update)) {
        tube_update_get(g_update, &status);
        snprintf(g_notice, sizeof g_notice, status.check == TUBE_UPDATE_AVAILABLE && !status.blocked ?
                 "Version %s is available. Open Settings, About to read what is new and update." :
                 "Version %s is available. Settings, About has the release page.", status.latest);
        g_have_notice = 1;
    }
}

static tube_update_row *row(tube_update_row *rows, int *count, int capacity, const char *action, const char *title, const char *detail) {
    if (*count >= capacity) return NULL;
    tube_update_row *out = &rows[(*count)++];
    snprintf(out->id, sizeof out->id, "settings:update:%s", action);
    snprintf(out->title, sizeof out->title, "%s", title);
    snprintf(out->detail, sizeof out->detail, "%s", detail);
    return out;
}

static unsigned megabytes(uint64_t bytes) { return (unsigned)((bytes + 1024 * 1024 - 1) / (1024 * 1024)); }
static unsigned percent(uint64_t done, uint64_t total) { return total ? (unsigned)(done * 100 / total) : 0; }

int tube_update_app_rows(tube_update_row *rows, int capacity) {
    tube_update_status s;
    char title[160], detail[200];
    int n = 0;
    if (!g_update) return 0;
    tube_update_get(g_update, &s);
    g_listed = s.revision; g_watching = 1;
    const int newer = s.check == TUBE_UPDATE_AVAILABLE || s.check == TUBE_UPDATE_NOT_INSTALLABLE;
    if (s.phase == TUBE_UPDATE_CHECKING) snprintf(detail, sizeof detail, "Checking...");
    else if (*s.error && s.manual && s.phase != TUBE_UPDATE_FAILED) snprintf(detail, sizeof detail, "%.199s", s.error);
    else if (newer) snprintf(detail, sizeof detail, "Installed %s; latest %s (%.60s)", s.installed, s.latest, s.latest_name);
    else if (s.check == TUBE_UPDATE_UP_TO_DATE) snprintf(detail, sizeof detail, "Installed %s is the latest version", s.installed);
    else snprintf(detail, sizeof detail, "Installed %s; the latest version is not known yet", *s.installed ? s.installed : "version unknown");
    row(rows, &n, capacity, "check", "Check for updates", detail);
    row(rows, &n, capacity, "notify", s.notify ? "Notify about updates: on" : "Notify about updates: off",
        "A notice once per start when a newer version exists; the check itself runs once per launch");
    if (!newer && s.phase != TUBE_UPDATE_STAGED && s.phase != TUBE_UPDATE_APPLIED) return n;
    if (*tube_update_notes(g_update)) {
        snprintf(title, sizeof title, "What is new in %.80s", *s.latest_name ? s.latest_name : s.latest);
        /* A page of its own, in the long-text reader. */
        tube_update_row *notes = row(rows, &n, capacity, "notes", title, "The release notes, as the developer wrote them");
        if (notes) snprintf(notes->id, sizeof notes->id, "description:update");
    }
    switch (s.phase) {
    case TUBE_UPDATE_DOWNLOADING:
        snprintf(title, sizeof title, "Downloading: %u%% - select to cancel", percent(s.done, s.total));
        snprintf(detail, sizeof detail, "%u of %u MB; cancelling leaves the installed app untouched", megabytes(s.done), megabytes(s.total));
        row(rows, &n, capacity, "cancel", title, detail); break;
    case TUBE_UPDATE_VERIFYING:
    case TUBE_UPDATE_UNPACKING:
        snprintf(title, sizeof title, "Checking and unpacking: %u%% - select to cancel", s.phase == TUBE_UPDATE_UNPACKING ? percent(s.done, s.total) : 0);
        row(rows, &n, capacity, "cancel", title, "The installed version is not touched yet"); break;
    case TUBE_UPDATE_STAGED:
        snprintf(title, sizeof title, "Restart to finish updating to %s", s.staged);
        row(rows, &n, capacity, "apply", title, "Closes ProsperoTube so the helper can replace its files; open it again after the notification");
        row(rows, &n, capacity, "discard", "Discard the prepared update", "Removes the downloaded version; the installed one stays"); break;
    case TUBE_UPDATE_APPLIED:
        row(rows, &n, capacity, "apply", "Close ProsperoTube to finish the update", "The new version starts when you open the app again"); break;
    default:
        if (s.check == TUBE_UPDATE_NOT_INSTALLABLE)
            row(rows, &n, capacity, "blocked", "This release cannot be installed from here", "Its files are listed on the release page: Releases and source code has the address");
        else if (s.blocked) row(rows, &n, capacity, "blocked", "This copy does not update itself", s.blocked_text);
        else {
            snprintf(title, sizeof title, "Update to %s now", s.latest);
            if (s.phase == TUBE_UPDATE_FAILED && *s.error) snprintf(detail, sizeof detail, "%.199s", s.error);
            else snprintf(detail, sizeof detail, "Downloads %u MB, checks it against the signed catalog and prepares the new version", megabytes(s.size));
            row(rows, &n, capacity, "install", title, detail);
        }
    }
    if (newer && s.phase != TUBE_UPDATE_STAGED && s.phase != TUBE_UPDATE_APPLIED) {
        const int skipped = !strcmp(s.skipped, s.latest);
        snprintf(title, sizeof title, skipped ? "Version %s is skipped - select to be told again" : "Skip version %s", s.latest);
        row(rows, &n, capacity, "skip", title, "No notice at start for this version; it stays listed here");
    }
    return n;
}

int tube_update_app_action(const char *action) {
    tube_update_status s;
    if (!g_update || !action) return 0;
    tube_update_get(g_update, &s);
    if (!strcmp(action, "check")) { if (tube_update_check(g_update, 1)) g_manual = 1; }
    else if (!strcmp(action, "notify")) tube_update_set_notify(g_update, !s.notify);
    else if (!strcmp(action, "install")) { if (!tube_update_download(g_update)) notice("The updater is busy. Try again in a moment."); }
    else if (!strcmp(action, "cancel")) tube_update_cancel(g_update);
    else if (!strcmp(action, "discard")) tube_update_discard(g_update);
    else if (!strcmp(action, "skip")) tube_update_skip(g_update, strcmp(s.skipped, s.latest) ? s.latest : "");
    else if (!strcmp(action, "apply")) {
        if (s.phase == TUBE_UPDATE_APPLIED || tube_update_apply(g_update)) {
            notice("ProsperoTube closes now. Wait for the update notification before opening it again.");
            g_restart = 1;
        } else {
            tube_update_get(g_update, &s);
            notice(*s.error ? s.error : "The update could not be applied.");
        }
    } else return !strcmp(action, "blocked");
    return 1;
}

const char *tube_update_app_notes(void) { return g_update ? tube_update_notes(g_update) : ""; }

const char *tube_update_app_notes_title(void) {
    tube_update_status s;
    g_notes_title[0] = 0;
    if (!g_update) return g_notes_title;
    tube_update_get(g_update, &s);
    snprintf(g_notes_title, sizeof g_notes_title, "What is new in %.80s", *s.latest_name ? s.latest_name : s.latest);
    return g_notes_title;
}

const char *tube_update_app_take_notice(void) {
    if (!g_have_notice) return NULL;
    g_have_notice = 0;
    return g_notice;
}

int tube_update_app_take_refresh(const char *focused_id) {
    tube_update_status s;
    if (!g_update || !g_watching) return 0;
    if (!focused_id || strncmp(focused_id, "settings:update:", 16)) { g_watching = 0; return 0; }
    tube_update_get(g_update, &s);
    const long long current = clock_now();
    if (s.revision == g_listed || current == g_refreshed) return 0;   /* progress redraws once a second */
    g_refreshed = current;
    return 1;
}

int tube_update_app_take_restart(void) {
    const int restart = g_restart;
    g_restart = 0;
    return restart;
}

#if defined(TUBE_EMBEDDED_SERVICE) && defined(EVO_APP_MODULE)
const char *tube_update_param_path(void) {
    static char path[512];
    const char *folders[] = {"/app0", "/system_ex/app/" TUBE_UPDATE_TITLE, TUBE_UPDATE_INSTALL_DIR};
    struct stat info;
    for (unsigned i = 0; i < sizeof folders / sizeof *folders; ++i) {
        snprintf(path, sizeof path, "%s/sce_sys/param.json", folders[i]);
        if (!stat(path, &info)) return path;
    }
    return path;
}
const char *tube_update_helper_path(void) {
    static char path[512];
    snprintf(path, sizeof path, "%s", tube_update_param_path());
    char *suffix = strstr(path, "/sce_sys/param.json");
    if (suffix) snprintf(suffix, sizeof path - (size_t)(suffix - path), "/self-updater.elf");
    return path;
}
#endif
