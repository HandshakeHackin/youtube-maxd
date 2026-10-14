/* Adapter for the shared signed-catalog/helper kit; installed files are never
 * modified by the app. Copyright (C) 2026 BlackBearReloaded. GPL-3.0-or-later */
#include "tube_update.h"
#include "update_check.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct tube_update {
    const self_update_platform *platform;
    self_update_offer offer;
    self_update_job job;
    tube_update_status status, last;
    pthread_mutex_t lock;
    pthread_t checker;
    int checking, joined, announced, development;
    char settings[512], notes[sizeof(((self_update_offer *)0)->notes)];
};

static void save_settings(tube_update *u) {
    char temp[520];
    snprintf(temp, sizeof temp, "%s.tmp", u->settings);
    FILE *f = fopen(temp, "wb");
    int ok = f && fprintf(f, "%d\n%s\n", u->status.notify, u->status.skipped) > 0;
    if (f && fclose(f)) ok = 0;
    if (!ok || rename(temp, u->settings)) {
        remove(temp);
        snprintf(u->status.error, sizeof u->status.error, "Update preferences could not be saved.");
    }
}

tube_update *tube_update_new(const tube_update_config *config) {
    unsigned parts[3];
    if (!config || !config->platform || !config->data_dir ||
        !update_check_version_parse(config->installed, parts)) return NULL;
    tube_update *u = calloc(1, sizeof *u);
    if (!u) return NULL;
    if (pthread_mutex_init(&u->lock, NULL)) { free(u); return NULL; }
    u->platform = config->platform;
    u->development = config->development;
    u->status.notify = 1;
    snprintf(u->status.installed, sizeof u->status.installed, "%s", config->installed);
    if (snprintf(u->settings, sizeof u->settings, "%s/update-preferences", config->data_dir) >= (int)sizeof u->settings) {
        pthread_mutex_destroy(&u->lock); free(u); return NULL;
    }
    FILE *f = fopen(u->settings, "rb");
    if (f) {
        char notify[8], skipped[20];
        if (fgets(notify, sizeof notify, f) && (!strcmp(notify,"0\n") || !strcmp(notify,"1\n"))) {
            u->status.notify = notify[0] == '1';
            if (fgets(skipped, sizeof skipped, f)) {
                skipped[strcspn(skipped, "\r\n")] = 0;
                if (update_check_version_parse(skipped, parts)) snprintf(u->status.skipped, sizeof u->status.skipped, "%s", skipped);
            }
        }
        fclose(f);
    }
    if (u->development) {
        u->status.blocked = TUBE_UPDATE_BLOCKED_DEVELOPMENT;
        snprintf(u->status.blocked_text, sizeof u->status.blocked_text, "This is a development build. Install a release to update from the catalog.");
    }
    return u;
}

void tube_update_free(tube_update *u) {
    if (!u) return;
    /* shortcut: catalog shutdown joins up to 120 s of GET budgets plus synchronous DNS; add cancellable transport if console acceptance requires it. */
    if (u->joined) pthread_join(u->checker, NULL);
    self_update_cancel(&u->job);
    self_update_finish(&u->job);
    pthread_mutex_destroy(&u->lock);
    free(u);
}

static void *check_worker(void *context) {
    tube_update *u = context;
    self_update_offer offer = {0};
    const self_update_check_result result = self_update_check(u->platform, TUBE_UPDATE_TITLE, u->status.installed, &offer);
    pthread_mutex_lock(&u->lock);
    u->offer = offer;
    u->status.check = result == SELF_UPDATE_AVAILABLE ? TUBE_UPDATE_AVAILABLE :
        result == SELF_UPDATE_UP_TO_DATE ? TUBE_UPDATE_UP_TO_DATE :
        result == SELF_UPDATE_NOT_INSTALLABLE ? TUBE_UPDATE_NOT_INSTALLABLE : TUBE_UPDATE_UNKNOWN;
    snprintf(u->status.latest, sizeof u->status.latest, "%s", offer.available);
    snprintf(u->status.latest_name, sizeof u->status.latest_name, "%s", offer.version);
    snprintf(u->status.page, sizeof u->status.page, "%s", offer.page);
    u->status.size = offer.size;
    if (result == SELF_UPDATE_UNTRUSTED)
        snprintf(u->status.error, sizeof u->status.error, "The catalog signature, hash or sequence could not be verified. No update was accepted.");
    else if (result == SELF_UPDATE_UNKNOWN && u->status.manual)
        snprintf(u->status.error, sizeof u->status.error, "The catalog could not be checked, or ProsperoTube is not listed yet.");
    u->status.phase = TUBE_UPDATE_IDLE;
    u->checking = 0;
    pthread_mutex_unlock(&u->lock);
    return NULL;
}

int tube_update_check(tube_update *u, int manual) {
    tube_update_status s;
    tube_update_get(u, &s);
    if (s.busy || s.phase == TUBE_UPDATE_STAGED || s.phase == TUBE_UPDATE_APPLIED) return 0;
    if (u->joined) { pthread_join(u->checker, NULL); u->joined = 0; }
    self_update_finish(&u->job);
    pthread_mutex_lock(&u->lock);
    u->checking = 1; u->status.manual = manual; u->status.error[0] = 0;
    u->status.phase = TUBE_UPDATE_CHECKING;
    if (pthread_create(&u->checker, NULL, check_worker, u)) {
        u->checking = 0; u->status.phase = TUBE_UPDATE_FAILED;
        snprintf(u->status.error, sizeof u->status.error, "The update checker could not start.");
        pthread_mutex_unlock(&u->lock); return 0;
    }
    u->joined = 1;
    pthread_mutex_unlock(&u->lock);
    return 1;
}

int tube_update_download(tube_update *u) {
    tube_update_status s;
    tube_update_get(u, &s);
    if (s.busy || s.blocked || s.check != TUBE_UPDATE_AVAILABLE ||
        s.phase == TUBE_UPDATE_STAGED || s.phase == TUBE_UPDATE_APPLIED) return 0;
    self_update_finish(&u->job);
    return self_update_start(&u->job, u->platform, &u->offer);
}
void tube_update_cancel(tube_update *u) { self_update_cancel(&u->job); }
int tube_update_apply(tube_update *u) { return self_update_apply(&u->job); }
void tube_update_discard(tube_update *u) { self_update_cancel(&u->job); }

void tube_update_get(tube_update *u, tube_update_status *status) {
    self_update_status job;
    self_update_poll(&u->job, &job);
    pthread_mutex_lock(&u->lock);
    u->status.busy = u->checking;
    if (u->job.started) {
        static const int phases[] = {TUBE_UPDATE_IDLE, TUBE_UPDATE_VERIFYING, TUBE_UPDATE_DOWNLOADING,
            TUBE_UPDATE_UNPACKING, TUBE_UPDATE_STAGED, TUBE_UPDATE_APPLIED, TUBE_UPDATE_IDLE, TUBE_UPDATE_FAILED};
        u->status.phase = phases[job.phase];
        u->status.busy |= job.phase >= SELF_UPDATE_STARTING && job.phase <= SELF_UPDATE_UNPACKING;
        u->status.done = job.done; u->status.total = job.total;
        if (job.phase == SELF_UPDATE_FAILED) snprintf(u->status.error, sizeof u->status.error, "%s", job.error);
        if (job.phase == SELF_UPDATE_READY) snprintf(u->status.staged, sizeof u->status.staged, "%s", u->offer.available);
    }
    if (memcmp(&u->last, &u->status, sizeof u->status)) ++u->status.revision;
    u->last = u->status;
    *status = u->status;
    pthread_mutex_unlock(&u->lock);
}
const char *tube_update_notes(tube_update *u) {
    pthread_mutex_lock(&u->lock);
    /* Only the main thread changes this snapshot; a completing check cannot invalidate it. */
    if (u->checking) u->notes[0] = 0;
    else memcpy(u->notes, u->offer.notes, sizeof u->notes);
    pthread_mutex_unlock(&u->lock);
    return u->notes;
}
int tube_update_notes_truncated(tube_update *u) {
    pthread_mutex_lock(&u->lock);
    const int truncated = !u->checking && u->offer.notes_truncated;
    pthread_mutex_unlock(&u->lock);
    return truncated;
}
void tube_update_set_notify(tube_update *u, int enabled) {
    pthread_mutex_lock(&u->lock);
    u->status.notify = !!enabled; save_settings(u);
    pthread_mutex_unlock(&u->lock);
}
void tube_update_skip(tube_update *u, const char *version) {
    unsigned parts[3];
    if (!version || (*version && !update_check_version_parse(version, parts))) return;
    pthread_mutex_lock(&u->lock);
    snprintf(u->status.skipped, sizeof u->status.skipped, "%s", version); save_settings(u);
    pthread_mutex_unlock(&u->lock);
}
int tube_update_take_offer(tube_update *u) {
    tube_update_status s;
    tube_update_get(u, &s);
    if (u->announced || !s.notify || !strcmp(s.skipped, s.latest) ||
        (s.check != TUBE_UPDATE_AVAILABLE && s.check != TUBE_UPDATE_NOT_INSTALLABLE)) return 0;
    u->announced = 1; return 1;
}
