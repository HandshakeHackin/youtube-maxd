/* Update check and in-place self-update; see tube_update.h.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_update.h"
#include "tube_update_fs.h"
#include "tube_update_sha256.h"
#include "tube_update_zip.h"

#include "cJSON.h"

#include <ctype.h>
#include <pthread.h>
#include <stdarg.h>
#include <strings.h>
#ifndef EVO_APP_MODULE
#include <sys/statvfs.h>
#endif
#include <time.h>

#define MANIFEST_LIMIT (2u * 1024 * 1024)
#define SMALL_FILE (256u * 1024)
#define DEFAULT_CHUNK (4u * 1024 * 1024)
#define SPACE_MARGIN (16ll * 1024 * 1024)
#define REDIRECTS 5
#define CHUNK_ATTEMPTS 3

enum { JOB_NONE = 0, JOB_CHECK, JOB_DOWNLOAD };

struct tube_update {
    tube_update_config config;
    char title[16], work[TUBE_UPDATE_PATH], install[TUBE_UPDATE_PATH], running[TUBE_UPDATE_PATH], registered[TUBE_UPDATE_PATH];
    char data[TUBE_UPDATE_PATH], manifest[600], prefix[300];
    pthread_mutex_t lock;
    pthread_t thread;
    int thread_live, job, job_manual;
    volatile int cancel;
    tube_update_status status;
    tube_update_offer offer;          /* the newest release known; version "" = none */
    char notes[TUBE_UPDATE_NOTES_SIZE];   /* the main thread's copy */
    int offered, pending, dev_offer;
    char pending_to[20], failed[20];  /* failed: a version that was rolled back */
};

/* snprintf under a name of its own: cutting a long message short is intended here. */
static int format(char *out, size_t size, const char *pattern, ...) __attribute__((format(printf, 3, 4)));
static int format(char *out, size_t size, const char *pattern, ...) {
    va_list arguments;
    va_start(arguments, pattern);
    const int written = vsnprintf(out, size, pattern, arguments);
    va_end(arguments);
    return written;
}

static void text(char *out, size_t size, const char *value) { format(out, size, "%s", value ? value : ""); }

/* ---- Versions ---- */
int tube_update_version_parse(const char *value, unsigned parts[3]) {
    if (!value) return 0;
    if (*value == 'v' || *value == 'V') ++value;
    for (int i = 0; i < 3; ++i) {
        unsigned number = 0;
        int digits = 0;
        for (; isdigit((unsigned char)*value); ++value, ++digits) {
            if (digits >= 6) return 0;
            number = number * 10 + (unsigned)(*value - '0');
        }
        if (!digits || *value != (i < 2 ? '.' : 0)) return 0;
        if (i < 2) ++value;
        parts[i] = number;
    }
    return 1;
}

int tube_update_version_compare(const char *a, const char *b) {
    unsigned left[3], right[3];
    const int has_left = tube_update_version_parse(a, left), has_right = tube_update_version_parse(b, right);
    if (!has_left || !has_right) return has_left - has_right;
    for (int i = 0; i < 3; ++i)
        if (left[i] != right[i]) return left[i] < right[i] ? -1 : 1;
    return 0;
}

/* ---- Release metadata ---- */
static const char *json_text(const cJSON *object, const char *name) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(value) && value->valuestring ? value->valuestring : "";
}

static int printable(const char *value) {
    for (; *value; ++value)
        if ((unsigned char)*value <= 0x20 || (unsigned char)*value >= 0x7f) return 0;
    return 1;
}

static int sha256_text(const char *value) {
    return strlen(value) == 64 && strspn(value, "0123456789abcdefABCDEF") == 64;
}

int tube_update_plain_notes(const char *markdown, char *out, size_t size) {
    size_t used = 0;
    int truncated = 0, blank = 1;   /* no blank line at the top */
    if (!size) return 1;
    for (const char *line = markdown ? markdown : ""; *line && !truncated;) {
        const char *end = strchr(line, '\n'), *next = end ? end + 1 : line + strlen(line);
        char row[1024];
        size_t length = (size_t)((end ? end : next) - line), made = 0;
        int heading = 0, bullet = 0;
        while (length && (line[length - 1] == '\r' || line[length - 1] == ' ' || line[length - 1] == '\t')) --length;
        const char *p = line, *stop = line + length;
        while (p < stop && (*p == ' ' || *p == '\t')) ++p;
        while (p < stop && *p == '>') { ++p; if (p < stop && *p == ' ') ++p; }
        if (p < stop && *p == '#') { while (p < stop && *p == '#') ++p; heading = 1; }
        else if (p + 1 < stop && (*p == '-' || *p == '*' || *p == '+') && p[1] == ' ') { bullet = 1; ++p; }
        while (p < stop && *p == ' ') ++p;
        if (bullet) { row[made++] = '-'; row[made++] = ' '; }
        for (; p < stop && made + 1 < sizeof row; ++p) {
            if (*p == '`' || (*p == '*' && p + 1 < stop && p[1] == '*')) { if (*p == '*') ++p; continue; }
            if (*p == '[') {   /* [words](address) -> words */
                const char *close = memchr(p, ']', (size_t)(stop - p));
                const char *paren = close && close + 1 < stop && close[1] == '(' ? memchr(close, ')', (size_t)(stop - close)) : NULL;
                if (paren) {
                    for (const char *q = p + 1; q < close && made + 1 < sizeof row; ++q) row[made++] = *q;
                    p = paren; continue;
                }
            }
            if ((unsigned char)*p < 0x20) continue;
            row[made++] = *p;
        }
        row[made] = 0;
        line = next;
        if (!made) { if (!blank && used) { if (used + 1 < size) out[used++] = '\n'; else truncated = 1; blank = 1; } continue; }
        if (heading && !blank && used) { if (used + 1 < size) out[used++] = '\n'; else truncated = 1; }
        if (used + made + 1 >= size) {
            made = size - used - 1;
            while (made && ((unsigned char)row[made] & 0xc0) == 0x80) --made;   /* whole characters only */
            truncated = 1;
        }
        memcpy(out + used, row, made); used += made;
        if (used + 1 < size) out[used++] = '\n';
        blank = 0;
    }
    while (used && out[used - 1] == '\n') --used;
    out[used] = 0;
    return truncated;
}

int tube_update_manifest_parse(const char *json, size_t length, const char *download_prefix, tube_update_offer *offer) {
    cJSON *root = cJSON_ParseWithLength(json, length);
    const cJSON *release, *best = NULL, *asset, *found = NULL;
    memset(offer, 0, sizeof *offer);
    if (!cJSON_IsArray(root)) { cJSON_Delete(root); return TUBE_UPDATE_MANIFEST_INVALID; }
    cJSON_ArrayForEach(release, root) {
        unsigned parts[3];
        if (!cJSON_IsObject(release)) { cJSON_Delete(root); return TUBE_UPDATE_MANIFEST_INVALID; }
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(release, "draft")) ||
            !tube_update_version_parse(json_text(release, "tag_name"), parts)) continue;
        if (!best || tube_update_version_compare(json_text(release, "tag_name"), json_text(best, "tag_name")) > 0) best = release;
    }
    if (!best) { cJSON_Delete(root); return TUBE_UPDATE_MANIFEST_NONE; }
    const char *tag = json_text(best, "tag_name"), *page = json_text(best, "html_url");
    text(offer->version, sizeof offer->version, tag + (*tag == 'v' || *tag == 'V'));
    text(offer->name, sizeof offer->name, *json_text(best, "name") ? json_text(best, "name") : offer->version);
    for (char *p = offer->name; *p; ++p) if ((unsigned char)*p < 0x20) *p = ' ';
    if (!strncmp(page, "https://github.com/", 19) && strlen(page) < sizeof offer->page && printable(page)) text(offer->page, sizeof offer->page, page);
    offer->notes_truncated = tube_update_plain_notes(json_text(best, "body"), offer->notes, sizeof offer->notes);
    cJSON_ArrayForEach(asset, cJSON_GetObjectItemCaseSensitive(best, "assets"))
        if (cJSON_IsObject(asset) && !strcmp(json_text(asset, "name"), TUBE_UPDATE_ASSET)) { found = asset; break; }
    int result = TUBE_UPDATE_MANIFEST_NOT_INSTALLABLE;
    if (found) {
        const cJSON *size = cJSON_GetObjectItemCaseSensitive(found, "size");
        const char *url = json_text(found, "browser_download_url"), *digest = json_text(found, "digest"), *state = json_text(found, "state");
        if ((!*state || !strcmp(state, "uploaded")) && cJSON_IsNumber(size) && size->valuedouble >= 1 &&
            size->valuedouble <= (double)TUBE_UPDATE_MAX_ARCHIVE && size->valuedouble == (double)(uint64_t)size->valuedouble &&
            download_prefix && !strncmp(url, download_prefix, strlen(download_prefix)) && strlen(url) < sizeof offer->url && printable(url) &&
            !strncmp(digest, "sha256:", 7) && sha256_text(digest + 7)) {
            offer->size = (uint64_t)size->valuedouble;
            text(offer->url, sizeof offer->url, url);
            for (int i = 0; i < 64; ++i) offer->sha256[i] = (char)tolower((unsigned char)digest[7 + i]);
            result = TUBE_UPDATE_MANIFEST_OK;
        }
    }
    cJSON_Delete(root);
    return result;
}

/* ---- Small helpers ---- */
static long long now(const tube_update *u) { return u->config.now ? u->config.now() : (long long)time(NULL); }

static int fault(const tube_update *u, const char *step) { return u->config.fault ? u->config.fault(step) : TUBE_UPDATE_FAULT_NONE; }

static const char *work_path(const tube_update *u, const char *leaf) {
    static _Thread_local char paths[4][TUBE_UPDATE_PATH];
    static _Thread_local int slot;
    slot = (slot + 1) % 4;
    tube_update_join(paths[slot], sizeof paths[slot], u->work, leaf);
    return paths[slot];
}

static long long free_bytes(const tube_update *u) {
    if (u->config.free_bytes) return u->config.free_bytes(u->work);
#ifdef EVO_APP_MODULE
    /* The app's runtime has no statvfs(), so the space is not known beforehand
     * there. A full disk then shows as a write that fails while downloading or
     * unpacking, which is before anything installed is touched. */
    return -1;
#else
    struct statvfs info;
    if (statvfs(u->work, &info) != 0) return -1;
    return (long long)info.f_bavail * (long long)info.f_frsize;
#endif
}

static void touch(tube_update *u) { ++u->status.revision; }

static void set_phase(tube_update *u, int phase, const char *error) {
    pthread_mutex_lock(&u->lock);
    u->status.phase = phase;
    text(u->status.error, sizeof u->status.error, error);
    touch(u);
    pthread_mutex_unlock(&u->lock);
}

static void set_progress(tube_update *u, uint64_t done, uint64_t total) {
    pthread_mutex_lock(&u->lock);
    u->status.done = done; u->status.total = total;
    touch(u);
    pthread_mutex_unlock(&u->lock);
}

/* titleId and contentVersion of <folder>/sce_sys/param.json. 1 = both read. */
static int read_param(const char *folder, char *title, size_t title_size, char *version, size_t version_size) {
    char path[TUBE_UPDATE_PATH];
    if (tube_update_join(path, sizeof path, folder, "sce_sys/param.json") != 0) return 0;
    char *body = tube_update_read(path, SMALL_FILE, NULL);
    cJSON *root = body ? cJSON_Parse(body) : NULL;
    unsigned parts[3];
    const int ok = cJSON_IsObject(root) && *json_text(root, "titleId") && tube_update_version_parse(json_text(root, "contentVersion"), parts);
    if (ok) { text(title, title_size, json_text(root, "titleId")); text(version, version_size, json_text(root, "contentVersion")); }
    cJSON_Delete(root); free(body);
    return ok;
}

static long long file_size(const char *folder, const char *leaf) {
    char path[TUBE_UPDATE_PATH];
    struct stat info;
    if (tube_update_join(path, sizeof path, folder, leaf) != 0 || stat(path, &info) != 0 || !S_ISREG(info.st_mode)) return -1;
    return (long long)info.st_size;
}

/* ---- What is stored between runs ---- */
static void save_check(tube_update *u) {
    cJSON *root = cJSON_CreateObject(), *offer = cJSON_CreateObject();
    static const char *const names[] = {"unknown", "up_to_date", "available", "not_installable"};
    cJSON_AddBoolToObject(root, "notify", u->status.notify);
    cJSON_AddStringToObject(root, "skipped", u->status.skipped);
    cJSON_AddStringToObject(root, "failed", u->failed);
    cJSON_AddNumberToObject(root, "checked_at", (double)u->status.checked_at);
    cJSON_AddStringToObject(root, "check", names[u->status.check]);
    cJSON_AddStringToObject(root, "installed", u->status.installed);
    cJSON_AddStringToObject(offer, "version", u->offer.version);
    cJSON_AddStringToObject(offer, "name", u->offer.name);
    cJSON_AddStringToObject(offer, "url", u->offer.url);
    cJSON_AddStringToObject(offer, "sha256", u->offer.sha256);
    cJSON_AddNumberToObject(offer, "size", (double)u->offer.size);
    cJSON_AddStringToObject(offer, "page", u->offer.page);
    cJSON_AddStringToObject(offer, "notes", u->offer.notes);
    cJSON_AddBoolToObject(offer, "notes_truncated", u->offer.notes_truncated);
    cJSON_AddItemToObject(root, "offer", offer);
    char *body = cJSON_PrintUnformatted(root);
    if (body && tube_update_mkdirs(u->data, u->work) == 0) (void)tube_update_write(work_path(u, "check.json"), body, strlen(body));
    cJSON_free(body); cJSON_Delete(root);
}

/* Decides `check` from the offer and the installed version. */
static void decide(tube_update *u, int installable) {
    if (!*u->offer.version || !*u->status.installed) u->status.check = TUBE_UPDATE_UNKNOWN;
    else if (tube_update_version_compare(u->offer.version, u->status.installed) <= 0) u->status.check = TUBE_UPDATE_UP_TO_DATE;
    else u->status.check = installable ? TUBE_UPDATE_AVAILABLE : TUBE_UPDATE_NOT_INSTALLABLE;
    text(u->status.latest, sizeof u->status.latest, u->offer.version);
    text(u->status.latest_name, sizeof u->status.latest_name, u->offer.name);
    text(u->status.page, sizeof u->status.page, u->offer.page);
    u->status.size = u->offer.size;
}

static void load_check(tube_update *u) {
    char *body = tube_update_read(work_path(u, "check.json"), SMALL_FILE, NULL);
    cJSON *root = body ? cJSON_Parse(body) : NULL;
    const cJSON *offer = cJSON_GetObjectItemCaseSensitive(root, "offer"), *value;
    u->status.notify = 1;
    if (cJSON_IsObject(root)) {
        unsigned parts[3];
        if (cJSON_IsBool(value = cJSON_GetObjectItemCaseSensitive(root, "notify"))) u->status.notify = cJSON_IsTrue(value);
        if (tube_update_version_parse(json_text(root, "skipped"), parts)) text(u->status.skipped, sizeof u->status.skipped, json_text(root, "skipped"));
        if (tube_update_version_parse(json_text(root, "failed"), parts)) text(u->failed, sizeof u->failed, json_text(root, "failed"));
        value = cJSON_GetObjectItemCaseSensitive(root, "checked_at");
        if (cJSON_IsNumber(value) && value->valuedouble > 0 && value->valuedouble < 1e12) u->status.checked_at = (long long)value->valuedouble;
        value = cJSON_GetObjectItemCaseSensitive(offer, "size");
        if (cJSON_IsObject(offer) && tube_update_version_parse(json_text(offer, "version"), parts) && cJSON_IsNumber(value) &&
            value->valuedouble >= 0 && value->valuedouble <= (double)TUBE_UPDATE_MAX_ARCHIVE) {
            const char *url = json_text(offer, "url"), *hash = json_text(offer, "sha256");
            text(u->offer.version, sizeof u->offer.version, json_text(offer, "version"));
            text(u->offer.name, sizeof u->offer.name, json_text(offer, "name"));
            text(u->offer.page, sizeof u->offer.page, json_text(offer, "page"));
            text(u->offer.notes, sizeof u->offer.notes, json_text(offer, "notes"));
            u->offer.notes_truncated = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(offer, "notes_truncated"));
            /* The stored address is believed only as far as a fresh answer would be. */
            if (!strncmp(url, u->prefix, strlen(u->prefix)) && strlen(url) < sizeof u->offer.url && printable(url) && sha256_text(hash) &&
                value->valuedouble >= 1) {
                text(u->offer.url, sizeof u->offer.url, url); text(u->offer.sha256, sizeof u->offer.sha256, hash);
                u->offer.size = (uint64_t)value->valuedouble;
            }
            decide(u, *u->offer.url != 0);
            u->status.from_cache = 1;
        }
    }
    cJSON_Delete(root); free(body);
}

/* ---- Where this copy runs from ---- */
static void block(tube_update *u, int reason, const char *why) {
    u->status.blocked = reason;
    text(u->status.blocked_text, sizeof u->status.blocked_text, why);
}

static void locate(tube_update *u) {
    char title[32] = "", version[20] = "", record[TUBE_UPDATE_PATH], image[TUBE_UPDATE_PATH], message[280];
    block(u, TUBE_UPDATE_ALLOWED, "");
    u->status.installed[0] = 0;
    if (read_param(u->running, title, sizeof title, version, sizeof version) && !strcmp(title, u->title))
        text(u->status.installed, sizeof u->status.installed, version);
    if (u->config.development && !u->dev_offer) {
        block(u, TUBE_UPDATE_BLOCKED_DEVELOPMENT, "This is a development build. It does not update itself: install a release to get updates.");
        return;
    }
    if (*u->registered) {
        format(record, sizeof record, "%s/app/%s/mount.lnk", u->registered, u->title);
        format(image, sizeof image, "%s/app/%s/mount_img.lnk", u->registered, u->title);
        char *source = tube_update_read(record, 1024, NULL);
        if (source) {
            size_t length = strlen(source);
            while (length && (source[length - 1] == '\n' || source[length - 1] == '\r' || source[length - 1] == ' ' || source[length - 1] == '/')) source[--length] = 0;
            if (strcmp(source, u->install)) {
                format(message, sizeof message, "ProsperoTube runs from %.60s. Only a copy in %.60s updates itself; get the new version from the release page.", source, u->install);
                block(u, TUBE_UPDATE_BLOCKED_LOCATION, message);
            }
            free(source);
            if (u->status.blocked) return;
        } else if (tube_update_kind(image) == 1) {
            block(u, TUBE_UPDATE_BLOCKED_IMAGE, "ProsperoTube is installed as an image, which cannot be updated in place. Replace the image with the new version's.");
            return;
        }
    }
    title[0] = version[0] = 0;
    if (file_size(u->install, "eboot.bin") < 0 || !read_param(u->install, title, sizeof title, version, sizeof version) || strcmp(title, u->title)) {
        format(message, sizeof message, "ProsperoTube is not installed in %.80s, the only folder it updates in place. Get the new version from the release page.", u->install);
        block(u, TUBE_UPDATE_BLOCKED_LOCATION, message);
    } else if (!*u->status.installed || tube_update_version_compare(version, u->status.installed) != 0 ||
               (strcmp(u->running, u->install) && file_size(u->running, "eboot.bin") >= 0 &&
                file_size(u->running, "eboot.bin") != file_size(u->install, "eboot.bin"))) {
        format(message, sizeof message, "The copy in %.80s is not the one that is running, so it is left alone. Get the new version from the release page.", u->install);
        block(u, TUBE_UPDATE_BLOCKED_VERSION, message);
    } else if (tube_update_mkdirs(u->data, u->work) != 0) {
        block(u, TUBE_UPDATE_BLOCKED_STORAGE, "The update folder in the app's data could not be created.");
    }
}

/* ---- The journal ---- */
typedef struct { char name[256]; int had; } journal_entry;
typedef struct {
    char phase[16], from[20], to[20];
    int starts, count;
    journal_entry entries[64];
} journal;

static int journal_save(tube_update *u, const journal *state) {
    cJSON *root = cJSON_CreateObject(), *entries = cJSON_AddArrayToObject(root, "entries");
    cJSON_AddStringToObject(root, "phase", state->phase);
    cJSON_AddStringToObject(root, "from", state->from);
    cJSON_AddStringToObject(root, "to", state->to);
    cJSON_AddNumberToObject(root, "starts", state->starts);
    for (int i = 0; i < state->count; ++i) {
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "name", state->entries[i].name);
        cJSON_AddBoolToObject(entry, "had", state->entries[i].had);
        cJSON_AddItemToArray(entries, entry);
    }
    char *body = cJSON_PrintUnformatted(root);
    const int result = body ? tube_update_write(work_path(u, "state.json"), body, strlen(body)) : -1;
    cJSON_free(body); cJSON_Delete(root);
    return result;
}

/* 1 = a journal was read; 0 = none; -1 = unreadable (treated as none, and removed). */
static int journal_load(tube_update *u, journal *state) {
    char *body = tube_update_read(work_path(u, "state.json"), SMALL_FILE, NULL);
    cJSON *root = body ? cJSON_Parse(body) : NULL;
    const cJSON *entry, *value;
    int result = body ? -1 : 0;
    memset(state, 0, sizeof *state);
    if (cJSON_IsObject(root)) {
        unsigned parts[3];
        text(state->phase, sizeof state->phase, json_text(root, "phase"));
        text(state->from, sizeof state->from, json_text(root, "from"));
        text(state->to, sizeof state->to, json_text(root, "to"));
        value = cJSON_GetObjectItemCaseSensitive(root, "starts");
        state->starts = cJSON_IsNumber(value) && value->valuedouble >= 0 && value->valuedouble < 1000 ? (int)value->valuedouble : 0;
        result = tube_update_version_parse(state->to, parts) &&
                 (!strcmp(state->phase, "staged") || !strcmp(state->phase, "swapping") || !strcmp(state->phase, "applied")) ? 1 : -1;
        cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(root, "entries")) {
            const char *name = json_text(entry, "name");
            /* A name is one plain entry of the app folder, never a path. */
            if (state->count >= 64 || !*name || strlen(name) >= sizeof state->entries[0].name || strchr(name, '/') ||
                !strcmp(name, ".") || !strcmp(name, "..")) { result = -1; break; }
            text(state->entries[state->count].name, sizeof state->entries[0].name, name);
            state->entries[state->count++].had = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(entry, "had"));
        }
    }
    cJSON_Delete(root); free(body);
    if (result < 0) unlink(work_path(u, "state.json"));
    return result > 0;
}

static void remove_work(tube_update *u, int download_too) {
    (void)tube_update_remove(work_path(u, "stage"));
    (void)tube_update_remove(work_path(u, "previous"));
    unlink(work_path(u, "state.json"));
    if (download_too) { unlink(work_path(u, "download.zip")); unlink(work_path(u, "download.json")); }
}

/* The home screen's copies of sce_sys, made when the app was first registered. Best effort. */
static void refresh_registered(tube_update *u) {
    char source[TUBE_UPDATE_PATH], names[64][64];
    int count = 0;
    if (!*u->registered || tube_update_join(source, sizeof source, u->install, "sce_sys") != 0) return;
    tube_update_dir *dir = tube_update_opendir(source);
    if (!dir) return;
    for (struct dirent *entry; count < 64 && (entry = tube_update_readdir(dir)) != NULL;)
        if (entry->d_name[0] != '.' && strlen(entry->d_name) < sizeof names[0]) text(names[count++], sizeof names[0], entry->d_name);
    tube_update_closedir(dir);
    for (int target = 0; target < 2; ++target) {
        char folder[TUBE_UPDATE_PATH];
        format(folder, sizeof folder, target ? "%s/app/%s/sce_sys" : "%s/appmeta/%s", u->registered, u->title);
        if (tube_update_kind(folder) != 2) continue;
        for (int i = 0; i < count; ++i) {
            char from[TUBE_UPDATE_PATH], to[TUBE_UPDATE_PATH];
            size_t length = 0;
            if (tube_update_join(from, sizeof from, source, names[i]) != 0 || tube_update_join(to, sizeof to, folder, names[i]) != 0 ||
                tube_update_kind(from) != 1) continue;
            char *body = tube_update_read(from, 8u * 1024 * 1024, &length);
            if (body) (void)tube_update_write(to, body, length);
            free(body);
        }
    }
}

/* Puts every entry the journal names back as it was before the replacement. */
static int roll_back(tube_update *u, const journal *state) {
    int failed = 0;
    for (int i = state->count - 1; i >= 0; --i) {
        char installed[TUBE_UPDATE_PATH], previous[TUBE_UPDATE_PATH], staged[TUBE_UPDATE_PATH], leaf[300];
        const journal_entry *entry = &state->entries[i];
        format(leaf, sizeof leaf, "previous/%s", entry->name); text(previous, sizeof previous, work_path(u, leaf));
        format(leaf, sizeof leaf, "stage/%s", entry->name); text(staged, sizeof staged, work_path(u, leaf));
        if (tube_update_join(installed, sizeof installed, u->install, entry->name) != 0) { failed = 1; continue; }
        if (tube_update_kind(previous)) {   /* the old one was moved out: whatever is in its place is new */
            if (tube_update_remove(installed) != 0 || rename(previous, installed) != 0) failed = 1;
        } else if (!entry->had && !tube_update_kind(staged) && tube_update_kind(installed)) {
            if (tube_update_remove(installed) != 0) failed = 1;   /* a new entry that had been moved in */
        }
    }
    if (!failed) remove_work(u, 1);
    refresh_registered(u);
    return failed ? -1 : 0;
}

/* ---- Start-up ---- */
int tube_update_boot(tube_update *u) {
    journal state;
    int result = TUBE_UPDATE_BOOT_NONE;
    const int limit = u->config.start_limit > 0 ? u->config.start_limit : 2;
    pthread_mutex_lock(&u->lock);
    if (!journal_load(u, &state)) {
        /* No update in progress: what is left of one that never got that far goes. A part download stays, to be resumed. */
        (void)tube_update_remove(work_path(u, "stage"));
        (void)tube_update_remove(work_path(u, "previous"));
    } else if (!strcmp(state.phase, "staged")) {
        char title[32], version[20];
        if (!u->status.blocked && tube_update_version_compare(state.to, u->status.installed) > 0 &&
            read_param(work_path(u, "stage"), title, sizeof title, version, sizeof version) && !strcmp(title, u->title) &&
            !tube_update_version_compare(version, state.to)) {
            (void)tube_update_remove(work_path(u, "previous"));
            u->status.phase = TUBE_UPDATE_STAGED;
            text(u->status.staged, sizeof u->status.staged, state.to);
            result = TUBE_UPDATE_BOOT_STAGED;
        } else remove_work(u, 1);
    } else if (!strcmp(state.phase, "swapping")) {
        (void)roll_back(u, &state);
        result = TUBE_UPDATE_BOOT_RECOVERED;
    } else if (tube_update_version_compare(state.to, u->status.installed) != 0) {
        (void)roll_back(u, &state);   /* applied, yet another version is what runs */
        result = TUBE_UPDATE_BOOT_RECOVERED;
    } else if (state.starts >= limit) {
        (void)roll_back(u, &state);
        text(u->failed, sizeof u->failed, state.to);
        result = TUBE_UPDATE_BOOT_ROLLED_BACK;
    } else {
        ++state.starts;
        (void)journal_save(u, &state);
        u->pending = 1; text(u->pending_to, sizeof u->pending_to, state.to);
        result = TUBE_UPDATE_BOOT_PENDING;
    }
    if (result == TUBE_UPDATE_BOOT_RECOVERED || result == TUBE_UPDATE_BOOT_ROLLED_BACK) {
        locate(u);
        if (result == TUBE_UPDATE_BOOT_ROLLED_BACK) save_check(u);
        decide(u, *u->offer.url != 0);
    }
    touch(u);
    pthread_mutex_unlock(&u->lock);
    return result;
}

int tube_update_confirm_start(tube_update *u) {
    pthread_mutex_lock(&u->lock);
    const int pending = u->pending;
    if (pending) {
        remove_work(u, 1);
        u->pending = 0;
        if (!strcmp(u->failed, u->pending_to)) { u->failed[0] = 0; save_check(u); }
        touch(u);
    }
    pthread_mutex_unlock(&u->lock);
    return pending;
}

/* ---- Network ---- */
static int host_allowed(const tube_update *u, const char *url) {
    if (u->config.any_host) return !strncmp(url, "http://", 7) || !strncmp(url, "https://", 8);
    if (strncmp(url, "https://", 8) || !printable(url)) return 0;
    const char *host = url + 8;
    const size_t length = strcspn(host, "/:?#@");
    if (host[length] == '@' || (host[length] == ':' && strncmp(host + length, ":443/", 5))) return 0;
    static const char suffix[] = ".githubusercontent.com";
    return (length == 10 && !strncmp(host, "github.com", 10)) || (length == 14 && !strncmp(host, "api.github.com", 14)) ||
           (length > sizeof suffix - 1 && !strncmp(host + length - (sizeof suffix - 1), suffix, sizeof suffix - 1));
}

/* One GET with its redirects followed here, each target checked first. */
static void fetch(tube_update *u, const char *url, int ranged, uint64_t first, uint64_t last, size_t limit, tube_update_reply *reply) {
    char current[2048];
    text(current, sizeof current, url);
    for (int hop = 0;; ++hop) {
        memset(reply, 0, sizeof *reply);
        if (!u->config.transport.get) { reply->error = 1; text(reply->message, sizeof reply->message, "no network transport"); return; }
        if (strlen(url) >= sizeof current || !host_allowed(u, current)) { reply->error = 1; text(reply->message, sizeof reply->message, "the address is not one of GitHub's"); return; }
        if (u->config.transport.get(u->config.transport.context, current, ranged, first, last, limit, &u->cancel, reply) != 0 && !reply->error) reply->error = 1;
        if (reply->error) { free(reply->body); reply->body = NULL; if (!*reply->message) text(reply->message, sizeof reply->message, "the request failed"); return; }
        if (reply->status != 301 && reply->status != 302 && reply->status != 303 && reply->status != 307 && reply->status != 308) return;
        free(reply->body); reply->body = NULL;
        if (hop >= REDIRECTS || !*reply->location) { reply->error = 1; text(reply->message, sizeof reply->message, "too many redirects"); return; }
        if (reply->location[0] == '/') {   /* the same origin */
            const char *path = strchr(current + 8, '/');
            char origin[512];
            format(origin, sizeof origin, "%.*s", (int)(path ? path - current : (long)strlen(current)), current);
            if (format(current, sizeof current, "%s%s", origin, reply->location) >= (int)sizeof current) current[0] = 0;
        } else text(current, sizeof current, strlen(reply->location) < sizeof current ? reply->location : "");
    }
}

/* ---- The check ---- */
/* Development builds only: <data>/update-offer.txt, five lines (content
 * version, name, ZIP address on GitHub, SHA-256, size) and then the notes. */
static int read_dev_offer(tube_update *u, tube_update_offer *offer) {
    char path[TUBE_UPDATE_PATH], *lines[5];
    unsigned parts[3];
    memset(offer, 0, sizeof *offer);
    if (!u->config.development || tube_update_join(path, sizeof path, u->data, "update-offer.txt") != 0) return 0;
    char *body = tube_update_read(path, SMALL_FILE, NULL), *cursor = body;
    if (!body) return 0;
    for (int i = 0; i < 5; ++i) {
        lines[i] = cursor;
        cursor += strcspn(cursor, "\n");
        if (*cursor) *cursor++ = 0;
        lines[i][strcspn(lines[i], "\r")] = 0;
    }
    char *end = NULL;
    const unsigned long long size = strtoull(lines[4], &end, 10);
    const int ok = tube_update_version_parse(lines[0], parts) && strlen(lines[0]) < sizeof offer->version && *lines[1] &&
        strlen(lines[2]) < sizeof offer->url && host_allowed(u, lines[2]) && (u->config.any_host || !strncmp(lines[2], "https://github.com/", 19)) &&
        sha256_text(lines[3]) && *lines[4] && !*end && size >= 1 && size <= TUBE_UPDATE_MAX_ARCHIVE;
    if (ok) {
        text(offer->version, sizeof offer->version, lines[0] + (*lines[0] == 'v'));
        text(offer->name, sizeof offer->name, lines[1]);
        text(offer->url, sizeof offer->url, lines[2]);
        for (int i = 0; i < 64; ++i) offer->sha256[i] = (char)tolower((unsigned char)lines[3][i]);
        offer->size = size;
        offer->notes_truncated = tube_update_plain_notes(cursor, offer->notes, sizeof offer->notes);
    }
    free(body);
    return ok;
}

void tube_update_run_check(tube_update *u, int manual) {
    tube_update_offer *offer = (tube_update_offer *)calloc(1, sizeof *offer);
    tube_update_reply reply;
    char error[280] = "";
    const long interval = u->config.check_interval_s > 0 ? u->config.check_interval_s : 86400;
    const long long started = now(u);
    int parsed = -1, previous, from_file;
    if (!offer) return;
    pthread_mutex_lock(&u->lock);
    previous = u->status.phase;
    u->status.manual = manual;
    const int fresh = !manual && u->status.checked_at && started >= u->status.checked_at && started - u->status.checked_at < interval &&
                      !u->config.development;
    if (!fresh && (previous == TUBE_UPDATE_IDLE || previous == TUBE_UPDATE_FAILED)) { u->status.phase = TUBE_UPDATE_CHECKING; u->status.error[0] = 0; }
    touch(u);
    pthread_mutex_unlock(&u->lock);
    if (fresh) { free(offer); return; }   /* asked within the day: the stored answer stands */

    from_file = read_dev_offer(u, offer);
    if (from_file) parsed = TUBE_UPDATE_MANIFEST_OK;
    else {
        fetch(u, u->manifest, 0, 0, 0, MANIFEST_LIMIT, &reply);
        if (reply.error) format(error, sizeof error, "The update server could not be reached (%.100s).", reply.message);
        else if (reply.status == 403 || reply.status == 429) text(error, sizeof error, "The update server is answering too many requests. Try again in an hour.");
        else if (reply.status == 404) text(error, sizeof error, "No releases were found at the update address.");
        else if (reply.status != 200) format(error, sizeof error, "The update server answered with status %ld.", reply.status);
        else {
            parsed = tube_update_manifest_parse((const char *)reply.body, reply.length, u->prefix, offer);
            if (parsed == TUBE_UPDATE_MANIFEST_INVALID) { parsed = -1; text(error, sizeof error, "The update server's answer could not be read."); }
        }
        free(reply.body);
    }
    pthread_mutex_lock(&u->lock);
    if (u->status.phase == TUBE_UPDATE_CHECKING) u->status.phase = previous == TUBE_UPDATE_FAILED ? TUBE_UPDATE_IDLE : previous;
    if (parsed >= 0) {
        if (parsed == TUBE_UPDATE_MANIFEST_NONE) memset(&u->offer, 0, sizeof u->offer);
        else u->offer = *offer;
        decide(u, parsed == TUBE_UPDATE_MANIFEST_OK);
        if (parsed == TUBE_UPDATE_MANIFEST_NONE) u->status.check = *u->status.installed ? TUBE_UPDATE_UP_TO_DATE : TUBE_UPDATE_UNKNOWN;
        u->status.checked_at = started; u->status.from_cache = 0; u->status.error[0] = 0;
        if (u->dev_offer != from_file) { u->dev_offer = from_file; locate(u); }   /* the test offer appeared or went */
        if (!from_file) save_check(u);   /* a test offer is never the stored answer */
    } else if (manual) text(u->status.error, sizeof u->status.error, error);   /* an automatic check fails silently */
    touch(u);
    pthread_mutex_unlock(&u->lock);
    free(offer);
}

/* ---- The download ---- */
typedef struct { tube_update *update; } unpack_context;

static int unpack_progress(void *context, uint64_t done, uint64_t total) {
    tube_update *u = ((unpack_context *)context)->update;
    set_progress(u, done, total);
    return u->cancel;
}

static void megabytes(char *out, size_t size, const char *pattern, long long bytes) {
    format(out, size, pattern, (bytes + 1024 * 1024 - 1) / (1024 * 1024));
}

/* 1 = the part file on disk belongs to this offer; `sha` then holds its bytes. */
static uint64_t resume_offset(tube_update *u, const tube_update_offer *offer, tube_update_sha256 *sha) {
    char *body = tube_update_read(work_path(u, "download.json"), 4096, NULL);
    cJSON *root = body ? cJSON_Parse(body) : NULL;
    const cJSON *size = cJSON_GetObjectItemCaseSensitive(root, "size");
    uint64_t offset = 0;
    const int same = cJSON_IsObject(root) && !strcmp(json_text(root, "sha256"), offer->sha256) && !strcmp(json_text(root, "url"), offer->url) &&
                     cJSON_IsNumber(size) && (uint64_t)size->valuedouble == offer->size;
    cJSON_Delete(root); free(body);
    tube_update_sha256_init(sha);
    FILE *file = same ? fopen(work_path(u, "download.zip"), "rb") : NULL;
    if (file) {
        unsigned char *buffer = (unsigned char *)malloc(65536);
        size_t got;
        while (buffer && (got = fread(buffer, 1, 65536, file)) > 0) { tube_update_sha256_add(sha, buffer, got); offset += got; }
        const int bad = !buffer || ferror(file) || offset > offer->size;
        free(buffer); fclose(file);
        if (!bad) return offset;
    }
    tube_update_sha256_init(sha);
    unlink(work_path(u, "download.zip"));
    cJSON *record = cJSON_CreateObject();
    cJSON_AddStringToObject(record, "sha256", offer->sha256);
    cJSON_AddStringToObject(record, "url", offer->url);
    cJSON_AddNumberToObject(record, "size", (double)offer->size);
    cJSON_AddStringToObject(record, "version", offer->version);
    char *made = cJSON_PrintUnformatted(record);
    if (made) (void)tube_update_write(work_path(u, "download.json"), made, strlen(made));
    cJSON_free(made); cJSON_Delete(record);
    return 0;
}

/* Fetches the ZIP into download.zip. 0 = complete and matching; 1 = cancelled (the part stays); -1 = `error`. */
static int download(tube_update *u, const tube_update_offer *offer, char *error, size_t error_size) {
    const size_t chunk = u->config.chunk_bytes ? u->config.chunk_bytes : DEFAULT_CHUNK;
    tube_update_sha256 sha;
    char digest[65];
    uint64_t offset = resume_offset(u, offer, &sha);
    FILE *file = fopen(work_path(u, "download.zip"), "ab");
    int result = 0;
    if (!file) { text(error, error_size, "The download could not be saved in the app's data."); return -1; }
    set_progress(u, offset, offer->size);
    while (offset < offer->size && !result) {
        const uint64_t last = offset + chunk < offer->size ? offset + chunk - 1 : offer->size - 1;
        const size_t wanted = (size_t)(last - offset + 1);
        tube_update_reply reply;
        memset(&reply, 0, sizeof reply);
        for (int attempt = 0; attempt < CHUNK_ATTEMPTS; ++attempt) {
            if (u->cancel) break;
            free(reply.body);
            /* A server may ignore the range and send a small file whole. */
            fetch(u, offer->url, 1, offset, last, !offset && offer->size <= 64u * 1024 * 1024 ? (size_t)offer->size : wanted, &reply);
            if (!reply.error && reply.status < 500) break;
        }
        const int stop = fault(u, "download:chunk");
        if (u->cancel || stop == TUBE_UPDATE_FAULT_STOP) result = 1;
        else if (reply.error || stop) { format(error, error_size, "The download stopped (%.100s). Select the update again to continue it.", stop ? "simulated failure" : reply.message); result = -1; }
        else if (reply.status == 200 && !offset && reply.length == offer->size) { /* the whole file at once */ }
        else if (reply.status != 206 || reply.length != wanted) {
            if (reply.status == 206 || reply.status == 200) text(error, error_size, "The download does not match the size the release lists.");
            else format(error, error_size, "The download server answered with status %ld.", reply.status);
            result = -1;
        } else if (*reply.content_range) {
            char expected[96];
            format(expected, sizeof expected, "bytes %llu-%llu/%llu", (unsigned long long)offset, (unsigned long long)last, (unsigned long long)offer->size);
            if (strcasecmp(reply.content_range, expected)) { text(error, error_size, "The download does not match the size the release lists."); result = -1; }
        }
        if (!result) {
            if (fwrite(reply.body, 1, reply.length, file) != reply.length || fflush(file) != 0) {
                text(error, error_size, "The download could not be written. Is the storage full?"); result = -1;
            } else {
                tube_update_sha256_add(&sha, reply.body, reply.length);
                offset += reply.length;
                set_progress(u, offset, offer->size);
            }
        }
        free(reply.body);
    }
    if (!result && fsync(fileno(file)) != 0) { text(error, error_size, "The download could not be written. Is the storage full?"); result = -1; }
    fclose(file);
    if (result) return result;
    tube_update_sha256_finish(&sha, digest);
    if (offset != offer->size || strcmp(digest, offer->sha256)) {
        unlink(work_path(u, "download.zip")); unlink(work_path(u, "download.json"));   /* nothing of it can be trusted: start over */
        text(error, error_size, "The download does not match the release's SHA-256. Nothing was changed.");
        return -1;
    }
    return 0;
}

void tube_update_run_download(tube_update *u) {
    tube_update_offer *offer = (tube_update_offer *)malloc(sizeof *offer);
    tube_update_zip *zip = NULL;
    char error[280] = "", title[32] = "", version[20] = "", stage[TUBE_UPDATE_PATH];
    journal state;
    int ready, result, stop;
    if (!offer) return;
    pthread_mutex_lock(&u->lock);
    *offer = u->offer;
    ready = u->status.check == TUBE_UPDATE_AVAILABLE && *offer->url && !u->status.blocked && !u->pending &&
            (u->status.phase == TUBE_UPDATE_IDLE || u->status.phase == TUBE_UPDATE_FAILED);
    if (ready) { u->status.phase = TUBE_UPDATE_DOWNLOADING; u->status.error[0] = 0; u->status.done = 0; u->status.total = offer->size; u->status.staged[0] = 0; }
    else if (u->status.blocked) text(u->status.error, sizeof u->status.error, u->status.blocked_text);
    touch(u);
    pthread_mutex_unlock(&u->lock);
    if (!ready) { free(offer); return; }
    text(stage, sizeof stage, work_path(u, "stage"));

    long long available = free_bytes(u), needed = (long long)offer->size * 4 + SPACE_MARGIN;
    if (tube_update_mkdirs(u->data, u->work) != 0) text(error, sizeof error, "The update folder in the app's data could not be created.");
    else if (available >= 0 && available < needed) megabytes(error, sizeof error, "Not enough free space: the update needs about %lld MB in the app's storage.", needed);
    if (*error) goto failed;
    (void)tube_update_remove(stage); (void)tube_update_remove(work_path(u, "previous")); unlink(work_path(u, "state.json"));

    result = download(u, offer, error, sizeof error);
    if (result > 0) goto cancelled;
    if (result < 0) goto failed;

    set_phase(u, TUBE_UPDATE_VERIFYING, "");
    stop = fault(u, "verify");
    if (stop == TUBE_UPDATE_FAULT_STOP) goto cancelled;
    zip = stop ? NULL : tube_update_zip_open(work_path(u, "download.zip"), u->title, TUBE_UPDATE_MAX_UNPACKED, error, sizeof error);
    if (!zip) {
        if (stop) text(error, sizeof error, "simulated failure");
        unlink(work_path(u, "download.zip")); unlink(work_path(u, "download.json"));
        goto failed;
    }
    available = free_bytes(u); needed = (long long)tube_update_zip_bytes(zip) + SPACE_MARGIN;
    if (available >= 0 && available < needed) {
        megabytes(error, sizeof error, "Not enough free space to unpack the update: it needs about %lld MB.", needed);
        goto failed;
    }
    pthread_mutex_lock(&u->lock);
    u->status.phase = TUBE_UPDATE_UNPACKING; u->status.done = 0; u->status.total = tube_update_zip_bytes(zip); touch(u);
    pthread_mutex_unlock(&u->lock);
    unpack_context context = {u};
    stop = fault(u, "unpack");
    if (stop == TUBE_UPDATE_FAULT_STOP) goto cancelled;
    result = stop ? -1 : tube_update_zip_unpack(zip, stage, unpack_progress, &context, error, sizeof error);
    if (stop) text(error, sizeof error, "simulated failure");
    if (result == 0 && !u->cancel) {
        stop = fault(u, "verify:files");
        if (stop == TUBE_UPDATE_FAULT_STOP) goto cancelled;
        result = stop ? -1 : tube_update_zip_verify(zip, stage, error, sizeof error);
        if (stop) text(error, sizeof error, "simulated failure");
    }
    if (u->cancel) { (void)tube_update_remove(stage); goto cancelled; }
    if (result != 0) { (void)tube_update_remove(stage); goto failed; }
    if (file_size(stage, "eboot.bin") < 0 || !read_param(stage, title, sizeof title, version, sizeof version) || strcmp(title, u->title) ||
        tube_update_version_compare(version, offer->version) != 0) {
        text(error, sizeof error, "The release does not hold this app at the listed version. Nothing was changed.");
        (void)tube_update_remove(stage); unlink(work_path(u, "download.zip")); unlink(work_path(u, "download.json"));
        goto failed;
    }
    memset(&state, 0, sizeof state);
    text(state.phase, sizeof state.phase, "staged"); text(state.to, sizeof state.to, offer->version);
    pthread_mutex_lock(&u->lock);
    text(state.from, sizeof state.from, u->status.installed);
    pthread_mutex_unlock(&u->lock);
    stop = fault(u, "stage:commit");
    if (stop == TUBE_UPDATE_FAULT_STOP) goto cancelled;
    if (stop || journal_save(u, &state) != 0) {
        text(error, sizeof error, "The staged update could not be recorded. Nothing was changed.");
        (void)tube_update_remove(stage);
        goto failed;
    }
    unlink(work_path(u, "download.zip")); unlink(work_path(u, "download.json"));   /* the staged files are the update now */
    tube_update_zip_close(zip); zip = NULL;
    pthread_mutex_lock(&u->lock);
    u->status.phase = TUBE_UPDATE_STAGED; text(u->status.staged, sizeof u->status.staged, offer->version); touch(u);
    pthread_mutex_unlock(&u->lock);
    free(offer);
    return;
cancelled:
    tube_update_zip_close(zip); free(offer);
    set_phase(u, TUBE_UPDATE_IDLE, "");
    return;
failed:
    tube_update_zip_close(zip); free(offer);
    set_phase(u, TUBE_UPDATE_FAILED, error);
}

/* ---- The replacement ---- */
int tube_update_apply(tube_update *u) {
    journal state;
    char stage[TUBE_UPDATE_PATH], previous[TUBE_UPDATE_PATH], error[280] = "";
    int stop = 0, failed = 0;
    pthread_mutex_lock(&u->lock);
    const int ready = u->status.phase == TUBE_UPDATE_STAGED && !u->status.blocked && !u->status.busy && journal_load(u, &state) && !strcmp(state.phase, "staged");
    pthread_mutex_unlock(&u->lock);
    if (!ready) { set_phase(u, u->status.phase, u->status.blocked ? u->status.blocked_text : "No verified update is waiting."); return 0; }
    text(stage, sizeof stage, work_path(u, "stage")); text(previous, sizeof previous, work_path(u, "previous"));
    state.count = 0;
    tube_update_dir *dir = tube_update_opendir(stage);
    if (dir) {
        for (struct dirent *entry; (entry = tube_update_readdir(dir)) != NULL;) {
            char installed[TUBE_UPDATE_PATH];
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
            if (state.count >= 64 || strlen(entry->d_name) >= sizeof state.entries[0].name ||
                tube_update_join(installed, sizeof installed, u->install, entry->d_name) != 0) { failed = 1; break; }
            text(state.entries[state.count].name, sizeof state.entries[0].name, entry->d_name);
            state.entries[state.count++].had = tube_update_kind(installed) != 0;
        }
        tube_update_closedir(dir);
    }
    if (!dir || failed || !state.count || tube_update_remove(previous) != 0 || tube_update_mkdir(previous) != 0) {
        set_phase(u, TUBE_UPDATE_FAILED, "The staged update could not be read. Nothing was changed.");
        remove_work(u, 1);
        return 0;
    }
    /* From here every step is in the journal: a start that finds "swapping" puts the old entries back. */
    text(state.phase, sizeof state.phase, "swapping");
    stop = fault(u, "swap:journal");
    if (stop == TUBE_UPDATE_FAULT_STOP) return 0;
    if (stop || journal_save(u, &state) != 0) { failed = 1; text(error, sizeof error, "The update could not be recorded. The installed version is unchanged."); }
    for (int i = 0; i < state.count && !failed; ++i) {
        char installed[TUBE_UPDATE_PATH], kept[TUBE_UPDATE_PATH], fresh[TUBE_UPDATE_PATH], step[32];
        (void)tube_update_join(installed, sizeof installed, u->install, state.entries[i].name);
        (void)tube_update_join(kept, sizeof kept, previous, state.entries[i].name);
        (void)tube_update_join(fresh, sizeof fresh, stage, state.entries[i].name);
        format(step, sizeof step, "swap:out:%d", i);
        stop = fault(u, step);
        if (stop == TUBE_UPDATE_FAULT_STOP) return 0;
        if (stop || (state.entries[i].had && rename(installed, kept) != 0)) { failed = 1; break; }
        format(step, sizeof step, "swap:in:%d", i);
        stop = fault(u, step);
        if (stop == TUBE_UPDATE_FAULT_STOP) return 0;
        if (stop || rename(fresh, installed) != 0) failed = 1;
    }
    if (!failed) {
        text(state.phase, sizeof state.phase, "applied"); state.starts = 0;
        stop = fault(u, "swap:commit");
        if (stop == TUBE_UPDATE_FAULT_STOP) return 0;
        if (stop || journal_save(u, &state) != 0) failed = 1;
    }
    if (failed) {
        const int undone = roll_back(u, &state);
        set_phase(u, TUBE_UPDATE_FAILED, *error ? error : undone == 0 ? "A file could not be replaced. The update was undone; the installed version is unchanged." :
                  "A file could not be replaced and the update could not be undone completely. It is undone at the next start.");
        pthread_mutex_lock(&u->lock); u->status.staged[0] = 0; touch(u); pthread_mutex_unlock(&u->lock);
        return 0;
    }
    refresh_registered(u);
    set_phase(u, TUBE_UPDATE_APPLIED, "");
    return 1;
}

void tube_update_discard(tube_update *u) {
    tube_update_cancel(u);
    pthread_mutex_lock(&u->lock);
    if (!u->status.busy) u->cancel = 0;   /* nothing was running: the request is spent */
    if (!u->status.busy && !u->pending && u->status.phase != TUBE_UPDATE_APPLIED) {
        remove_work(u, 1);
        u->status.phase = TUBE_UPDATE_IDLE; u->status.staged[0] = u->status.error[0] = 0; u->status.done = u->status.total = 0;
        touch(u);
    }
    pthread_mutex_unlock(&u->lock);
}

/* ---- Worker ---- */
static void *worker(void *argument) {
    tube_update *u = (tube_update *)argument;
    if (u->job == JOB_CHECK) tube_update_run_check(u, u->job_manual);
    else tube_update_run_download(u);
    pthread_mutex_lock(&u->lock);
    u->status.busy = 0; touch(u);
    pthread_mutex_unlock(&u->lock);
    return NULL;
}

static int start(tube_update *u, int job, int manual) {
    pthread_attr_t attributes;
    pthread_mutex_lock(&u->lock);
    const int busy = u->status.busy;
    if (!busy) u->status.busy = 1;
    pthread_mutex_unlock(&u->lock);
    if (busy) return 0;
    if (u->thread_live) { pthread_join(u->thread, NULL); u->thread_live = 0; }
    u->job = job; u->job_manual = manual; u->cancel = 0;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, 1024 * 1024);
    const int refused = pthread_create(&u->thread, &attributes, worker, u);
    pthread_attr_destroy(&attributes);
    if (refused) {
        pthread_mutex_lock(&u->lock); u->status.busy = 0; pthread_mutex_unlock(&u->lock);
        return 0;
    }
    u->thread_live = 1;
    return 1;
}

int tube_update_check(tube_update *u, int manual) { return start(u, JOB_CHECK, manual); }
int tube_update_download(tube_update *u) { return start(u, JOB_DOWNLOAD, 0); }
void tube_update_cancel(tube_update *u) { u->cancel = 1; }

/* ---- State for the interface ---- */
void tube_update_get(tube_update *u, tube_update_status *status) {
    pthread_mutex_lock(&u->lock);
    *status = u->status;
    pthread_mutex_unlock(&u->lock);
}

const char *tube_update_notes(tube_update *u) {
    pthread_mutex_lock(&u->lock);
    memcpy(u->notes, u->offer.notes, sizeof u->notes);
    pthread_mutex_unlock(&u->lock);
    return u->notes;
}

int tube_update_notes_truncated(tube_update *u) {
    pthread_mutex_lock(&u->lock);
    const int truncated = u->offer.notes_truncated;
    pthread_mutex_unlock(&u->lock);
    return truncated;
}

void tube_update_set_notify(tube_update *u, int enabled) {
    pthread_mutex_lock(&u->lock);
    u->status.notify = enabled != 0;
    save_check(u); touch(u);
    pthread_mutex_unlock(&u->lock);
}

void tube_update_skip(tube_update *u, const char *version) {
    unsigned parts[3];
    pthread_mutex_lock(&u->lock);
    text(u->status.skipped, sizeof u->status.skipped, tube_update_version_parse(version, parts) ? version : "");
    save_check(u); touch(u);
    pthread_mutex_unlock(&u->lock);
}

int tube_update_take_offer(tube_update *u) {
    pthread_mutex_lock(&u->lock);
    const int tell = !u->offered && u->status.notify && !u->status.busy &&
        (u->status.check == TUBE_UPDATE_AVAILABLE || u->status.check == TUBE_UPDATE_NOT_INSTALLABLE) &&
        strcmp(u->status.latest, u->status.skipped) && strcmp(u->status.latest, u->failed);
    if (tell) u->offered = 1;
    pthread_mutex_unlock(&u->lock);
    return tell;
}

/* ---- Life cycle ---- */
tube_update *tube_update_new(const tube_update_config *config) {
    if (!config || !config->data_dir || strlen(config->data_dir) + 64 >= TUBE_UPDATE_PATH) return NULL;
    tube_update *u = (tube_update *)calloc(1, sizeof *u);
    if (!u) return NULL;
    u->config = *config;
    text(u->title, sizeof u->title, config->title_id ? config->title_id : TUBE_UPDATE_TITLE);
    text(u->data, sizeof u->data, config->data_dir);
    format(u->work, sizeof u->work, "%s/update", config->data_dir);
    text(u->install, sizeof u->install, config->install_dir ? config->install_dir : TUBE_UPDATE_INSTALL_DIR);
    text(u->running, sizeof u->running, config->running_dir ? config->running_dir : u->install);
    text(u->registered, sizeof u->registered, config->registered_dir);
    text(u->manifest, sizeof u->manifest, config->manifest_url ? config->manifest_url : TUBE_UPDATE_MANIFEST_URL);
    text(u->prefix, sizeof u->prefix, config->download_prefix ? config->download_prefix : TUBE_UPDATE_DOWNLOAD_PREFIX);
    pthread_mutex_init(&u->lock, NULL);
    tube_update_offer probe;
    u->dev_offer = read_dev_offer(u, &probe);
    locate(u);
    load_check(u);
    if (u->dev_offer) u->offer = probe;
    decide(u, *u->offer.url != 0);
    return u;
}

void tube_update_free(tube_update *u) {
    if (!u) return;
    u->cancel = 1;
    if (u->thread_live) pthread_join(u->thread, NULL);
    pthread_mutex_destroy(&u->lock);
    free(u);
}
