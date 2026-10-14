/* Signed homebrew.page catalog and loader-started helper adapter.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_UPDATE_H
#define TUBE_UPDATE_H
#include <stddef.h>
#include <stdint.h>
#include "self_update.h"
#ifdef __cplusplus
extern "C" {
#endif
#define TUBE_UPDATE_TITLE "PPSA99142"
#define TUBE_UPDATE_INSTALL_DIR "/data/homebrew/" TUBE_UPDATE_TITLE
enum { TUBE_UPDATE_UNKNOWN, TUBE_UPDATE_UP_TO_DATE, TUBE_UPDATE_AVAILABLE, TUBE_UPDATE_NOT_INSTALLABLE };
enum { TUBE_UPDATE_IDLE, TUBE_UPDATE_CHECKING, TUBE_UPDATE_DOWNLOADING, TUBE_UPDATE_VERIFYING,
       TUBE_UPDATE_UNPACKING, TUBE_UPDATE_STAGED, TUBE_UPDATE_APPLIED, TUBE_UPDATE_FAILED };
enum { TUBE_UPDATE_ALLOWED, TUBE_UPDATE_BLOCKED_DEVELOPMENT };
typedef struct tube_update_config {
    const char *data_dir;
    const char *installed;
    int development;
    const self_update_platform *platform;
} tube_update_config;
typedef struct tube_update_status {
    int phase, check, blocked, manual, busy, notify;
    char installed[20], latest[20], latest_name[80], page[300], skipped[20], staged[20];
    uint64_t done, total, size;
    char error[280], blocked_text[280];
    unsigned revision;
} tube_update_status;
typedef struct tube_update tube_update;
tube_update *tube_update_new(const tube_update_config *config);
void tube_update_free(tube_update *update);
int tube_update_check(tube_update *update, int manual);
int tube_update_download(tube_update *update);
void tube_update_cancel(tube_update *update);
int tube_update_apply(tube_update *update);
void tube_update_discard(tube_update *update);
void tube_update_get(tube_update *update, tube_update_status *status);
const char *tube_update_notes(tube_update *update);
int tube_update_notes_truncated(tube_update *update);
void tube_update_set_notify(tube_update *update, int enabled);
void tube_update_skip(tube_update *update, const char *version);
int tube_update_take_offer(tube_update *update);
/* ---- The app's one updater (tube_update_app.c). Main thread. ---- */
typedef struct tube_update_row { char id[64], title[160], detail[200]; } tube_update_row;
/* Creates the signed catalog/helper adapter. Safe to call again. */
void tube_update_app_init(void);
void tube_update_app_shutdown(void);
/* Every frame: starts the catalog check once `service_ready`. */
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
