/* ProsperoTube native YouTube provider; original work, no Android dependency.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_provider.h"
#include "evo_net.h"
#include "evo_data_path.h"
#include "evo_error.h"
#include "cJSON.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define RECORDS 384
#define REQUESTS 8
#define CHOICES 8
#define SEGMENTS 128
#define LOCAL_PAGE 24
#define LIBRARY_PAGES 1001

typedef struct {
    evo_provider_item_t item;
    char profile[33];
    int favorite, queued, subscribed;
    long long updated, queue_order;
} Record;

typedef struct Request {
    int used, kind, stage, inflight;
    unsigned generation;
    long long deadline, next_poll;
    char job[64], payload[2048];
    evo_provider_items_cb items_cb;
    evo_provider_resolve_cb resolve_cb;
    void *ud;
    evo_stream_choice_t choices[CHOICES];
    int choice_count;
    int page;
    unsigned intent;
    char parent[160];
    char profile[33];
    int64_t watch_position;
    char auth_source[512], auth_token[128];
} Request;

static Record g_records[RECORDS];
static int g_count;
static int g_store_writable = 1;
static Request g_requests[REQUESTS];
static unsigned g_generation = 1;
static char g_source[512], g_token[128], g_pin[7];
static char g_caption_path[512], g_active_id[160], g_next_id[160];
static char g_active_profile[33], g_next_profile[33];
static int g_height = 1080, g_codec, g_auto_captions, g_captions = 1;
static int g_sponsor, g_autoplay, g_finished, g_ask_stream;
static int g_history_sync = 1;
static char g_resolved_id[160], g_resolved_profile[33];
static unsigned g_watch_serial;
static struct { unsigned serial; char id[160], profile[33], session[64];
    int dirty, stopped, failed, status, sent;
    int64_t position; long long last_sent; } g_watch;
static void watch_flush(int terminal_flush);
static void watch_reset(int clear_resolution) {
    memset(&g_watch, 0, sizeof g_watch);
    if (!++g_watch_serial) ++g_watch_serial;
    g_watch.serial = g_watch_serial;
    if (clear_resolution) { g_resolved_id[0] = 0; g_resolved_profile[0] = 0; }
}
static char g_language[16] = "en", g_audio_language[16];
static double g_segments[SEGMENTS][2];
static int g_segment_count, g_skip_guard = -1;
static double g_skip_until;
static double g_chapters[128];
static int g_chapter_count;
static long long g_last_save, g_queue_order;
static char g_library_profile[33] = "guest", g_library_parent[160];
static char g_library_cursors[LIBRARY_PAGES][65];
static struct { char id[160], content[16385]; } g_comments[LOCAL_PAGE];
static unsigned g_account_serial;
static unsigned g_accounts_revision;
unsigned tube_accounts_revision(void) { return g_accounts_revision; }
static void account_removed(void);
enum { AUTH_IDLE, AUTH_STARTING, AUTH_PENDING, AUTH_AUTHORIZED, AUTH_CANCELED,
       AUTH_EXPIRED, AUTH_DENIED, AUTH_FAILED };
static struct { int state; unsigned serial, revision;
    char session[33], code[33], url[64], profile[33], name[601];
    long long deadline, next_poll; } g_auth;
static void ignored(int ok, int status, const char *body, size_t size, void *ud);
static void auth_cleanup_request(Request *r, const cJSON *root);
static struct {
    unsigned serial;
    int state; /* 0 confirmation, 1 submitted, 2 confirmed, -1 unconfirmed */
    char action[32], identity[101], profile[33], playlist[101], title[601];
} g_account_intent;
static struct { tube_playlist_prompt_t prompt; int pending, active, create;
    char identity[101], profile[33]; } g_playlist_prompt;
static const struct { const char *action, *title; int channel; } g_account_actions[] = {
    {"like", "Like video", 0}, {"dislike", "Dislike video", 0}, {"unrate", "Remove video rating", 0},
    {"watch_later_add", "Add to YouTube Watch later", 0},
    {"watch_later_remove", "Remove from YouTube Watch later", 0},
    {"subscribe", "Subscribe on YouTube", 1}, {"unsubscribe", "Unsubscribe on YouTube", 1}
};

static int valid_cursor(const char *value) {
    return value && *value && strlen(value) <= 64 &&
        strspn(value, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == strlen(value);
}

const char *tube_comment_text(const char *id) {
    if (id) for (int i = 0; i < LOCAL_PAGE; ++i)
        if (*g_comments[i].id && !strcmp(id, g_comments[i].id)) return g_comments[i].content;
    return "";
}

static long long now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

static void copy(char *out, size_t cap, const char *src) {
    if (!cap) return;
    if (!src) src = "";
    size_t n = strlen(src);
    if (n >= cap) {
        n = cap - 1;
        while (n && ((unsigned char)src[n] & 0xc0) == 0x80) --n;
    }
    memcpy(out, src, n); out[n] = 0;
}

static const char *str(const cJSON *o, const char *key) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsString(v) && v->valuestring ? v->valuestring : "";
}

static double num(const cJSON *o, const char *key) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsNumber(v) && isfinite(v->valuedouble) && v->valuedouble >= 0 && v->valuedouble <= 1e12 ? v->valuedouble : 0;
}

static int boolean(const cJSON *o, const char *key) {
    return cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o, key));
}

static int valid_id(const char *id) {
    if (!id || strlen(id) != 11) return 0;
    for (int i = 0; i < 11; ++i)
        if (!isalnum((unsigned char)id[i]) && id[i] != '_' && id[i] != '-') return 0;
    return 1;
}

static int valid_profile(const char *id) {
    return id && *id && strlen(id) <= 32 &&
        strspn(id, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == strlen(id);
}

static int valid_catalog_id(const char *id) {
    if (valid_id(id)) return 1;
    const char *value = NULL;
    if (id && !strncmp(id, "channel:UC", 10) && strlen(id) == 32) value = id + 8;
    if (id && !strncmp(id, "playlist:", 9) && strlen(id + 9) >= 10 && strlen(id + 9) <= 100) value = id + 9;
    if (id && !strncmp(id, "library:playlist:", 17) && strlen(id + 17) <= 100 &&
        (strlen(id + 17) >= 10 || !strcmp(id + 17, "WL") || !strcmp(id + 17, "LL"))) value = id + 17;
    return value && strspn(value, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == strlen(value);
}

static int valid_playlist(const char *id) {
    char parent[128]; snprintf(parent, sizeof parent, "library:playlist:%s", id ? id : "");
    return valid_catalog_id(parent);
}

static unsigned account_intent(const char *action, const char *identity, const char *playlist, const char *title) {
    memset(&g_account_intent, 0, sizeof g_account_intent);
    if (!++g_account_serial) ++g_account_serial;
    g_account_intent.serial = g_account_serial;
    copy(g_account_intent.action, sizeof g_account_intent.action, action);
    copy(g_account_intent.identity, sizeof g_account_intent.identity, identity);
    copy(g_account_intent.playlist, sizeof g_account_intent.playlist, playlist);
    copy(g_account_intent.title, sizeof g_account_intent.title, title);
    copy(g_account_intent.profile, sizeof g_account_intent.profile, g_library_profile);
    return g_account_intent.serial;
}

int tube_take_playlist_prompt(tube_playlist_prompt_t *prompt) {
    if (!prompt || !g_playlist_prompt.pending) return 0;
    *prompt = g_playlist_prompt.prompt; g_playlist_prompt.pending = 0; return 1;
}

int tube_submit_playlist_name(unsigned ticket, const char *text, char *parent, size_t capacity) {
    if (!g_playlist_prompt.active || ticket != g_playlist_prompt.prompt.ticket || !text ||
        !parent || capacity < 48 || strcmp(g_playlist_prompt.profile, g_library_profile)) return -1;
    while (*text && isspace((unsigned char)*text)) ++text;
    size_t length = strlen(text);
    while (length && isspace((unsigned char)text[length - 1])) --length;
    if (!length || length > 600) return -1;
    size_t points = 0;
    for (size_t i = 0; i < length;) {
        unsigned char c = (unsigned char)text[i];
        size_t n = c < 0x80 ? 1 : c >= 0xc2 && c <= 0xdf ? 2 : c >= 0xe0 && c <= 0xef ? 3 : c >= 0xf0 && c <= 0xf4 ? 4 : 0;
        if (!n || c < 0x20 || c == 0x7f || i + n > length || ++points > 150) return -1;
        for (size_t k = 1; k < n; ++k) if (((unsigned char)text[i+k] & 0xc0) != 0x80) return -1;
        unsigned char next = n > 1 ? (unsigned char)text[i+1] : 0;
        if ((c == 0xe0 && next < 0xa0) || (c == 0xed && next >= 0xa0) ||
            (c == 0xf0 && next < 0x90) || (c == 0xf4 && next >= 0x90)) return -1;
        i += n;
    }
    char title[601]; memcpy(title, text, length); title[length] = 0;
    unsigned serial = account_intent(g_playlist_prompt.create ? "playlist_create" : "playlist_rename",
        g_playlist_prompt.create ? g_playlist_prompt.identity : "",
        g_playlist_prompt.create ? "" : g_playlist_prompt.identity, title);
    g_playlist_prompt.active = 0;
    snprintf(parent, capacity, "playlist:review:%u", serial); return 0;
}

static Record *record_for(const char *id, const char *profile, int create) {
    if (!valid_id(id) || !valid_profile(profile)) return NULL;
    for (int i = 0; i < g_count; ++i)
        if (!strcmp(g_records[i].item.id, id) && !strcmp(g_records[i].profile, profile)) return &g_records[i];
    if (!create) return NULL;
    evo_provider_item_t metadata = {0};
    for (int i = 0; i < g_count; ++i) if (!strcmp(g_records[i].item.id, id)) {
        metadata = g_records[i].item; metadata.resume_pos_sec = 0; metadata.played = 0; break;
    }
    int slot = g_count;
    if (slot >= RECORDS) {
        slot = -1;
        for (int i = 0; i < g_count; ++i)
            if (!g_records[i].favorite && !g_records[i].queued && !g_records[i].subscribed &&
                (slot < 0 || g_records[i].updated < g_records[slot].updated)) slot = i;
        if (slot < 0) return NULL;
    } else ++g_count;
    memset(&g_records[slot], 0, sizeof(Record));
    g_records[slot].item = metadata;
    copy(g_records[slot].profile, sizeof g_records[slot].profile, profile);
    copy(g_records[slot].item.id, sizeof g_records[slot].item.id, id);
    g_records[slot].item.kind = EVO_MEDIA_VIDEO;
    return &g_records[slot];
}

static Record *record(const char *id, int create) { return record_for(id, g_library_profile, create); }
static int current_record(const Record *r) { return !strcmp(r->profile, g_library_profile); }

void tube_set_error(const char *message) {
    /* Never echo remote URLs, headers, or arbitrary resolver diagnostics. */
    char clean[256]; copy(clean, sizeof clean, message);
    for (size_t i = 0; clean[i]; ++i) if ((unsigned char)clean[i] < 32) clean[i] = ' ';
    if (strstr(clean, "://") || strstr(clean, "Bearer")) copy(clean, sizeof clean, "The companion request failed. Check its configuration and retry.");
    evo_error_set("%s", clean);
}

static void save(void) {
    if (!g_store_writable) {
        tube_set_error("Local data could not be loaded. The existing file has not been overwritten."); return;
    }
    cJSON *root = cJSON_CreateObject(), *rows = cJSON_CreateArray();
    if (!root || !rows) { cJSON_Delete(root); cJSON_Delete(rows); return; }
    cJSON_AddNumberToObject(root, "version", 2);
    cJSON_AddStringToObject(root, "source", g_source);
    cJSON_AddStringToObject(root, "token", g_token);
    cJSON_AddNumberToObject(root, "max_height", g_height);
    cJSON_AddNumberToObject(root, "codec", g_codec);
    cJSON_AddBoolToObject(root, "captions", g_captions);
    cJSON_AddBoolToObject(root, "automatic_captions", g_auto_captions);
    cJSON_AddStringToObject(root, "language", g_language);
    cJSON_AddStringToObject(root, "audio_language", g_audio_language);
    cJSON_AddStringToObject(root, "library_profile", g_library_profile);
    cJSON_AddBoolToObject(root, "sponsorblock", g_sponsor);
    cJSON_AddBoolToObject(root, "autoplay", g_autoplay);
    cJSON_AddBoolToObject(root, "ask_stream", g_ask_stream);
    cJSON_AddBoolToObject(root, "history_sync", g_history_sync);
    for (int i = 0; i < g_count; ++i) {
        Record *r = &g_records[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "id", r->item.id);
        cJSON_AddStringToObject(o, "profile", r->profile);
        cJSON_AddStringToObject(o, "title", r->item.title);
        cJSON_AddStringToObject(o, "subtitle", r->item.subtitle);
        cJSON_AddStringToObject(o, "overview", r->item.overview);
        cJSON_AddStringToObject(o, "art_url", r->item.art_url);
        cJSON_AddStringToObject(o, "channel_id", r->item.parent_id);
        cJSON_AddNumberToObject(o, "position", (double)r->item.resume_pos_sec);
        cJSON_AddNumberToObject(o, "duration", (double)r->item.duration_sec);
        cJSON_AddNumberToObject(o, "updated", (double)r->updated);
        cJSON_AddNumberToObject(o, "queue_order", (double)r->queue_order);
        cJSON_AddBoolToObject(o, "favorite", r->favorite);
        cJSON_AddBoolToObject(o, "queued", r->queued);
        cJSON_AddBoolToObject(o, "subscribed", r->subscribed);
        cJSON_AddBoolToObject(o, "played", r->item.played);
        cJSON_AddBoolToObject(o, "is_live", r->item.is_live);
        cJSON_AddItemToArray(rows, o);
    }
    cJSON_AddItemToObject(root, "records", rows);
    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) return;
    evo_mkdir(evo_data_dir());
    char tmp[512], dst[512];
    copy(tmp, sizeof tmp, evo_data_path("youtube.json.tmp"));
    copy(dst, sizeof dst, evo_data_path("youtube.json"));
    FILE *f = fopen(tmp, "wb");
    if (f) {
        size_t size = strlen(body);
        int ok = fwrite(body, 1, size, f) == size && fflush(f) == 0;
        if (fclose(f)) ok = 0;
        if (ok && rename(tmp, dst) == 0) g_last_save = now_ms();
        else tube_set_error("Local library could not be saved. Check the ProsperoTube data directory.");
    }
    free(body);
}

static void account_removed(void) {
    if (!strcmp(g_auth.profile, g_library_profile)) {
        unsigned serial = g_auth.serial + 1, revision = g_auth.revision + 1;
        memset(&g_auth, 0, sizeof g_auth);
        g_auth.serial = serial; g_auth.revision = revision; g_auth.state = AUTH_CANCELED;
    }
    copy(g_library_profile, sizeof g_library_profile, "guest");
    watch_reset(1);
    g_next_id[0] = 0; g_next_profile[0] = 0;
    memset(&g_playlist_prompt, 0, sizeof g_playlist_prompt);
    memset(g_comments, 0, sizeof g_comments);
    g_library_parent[0] = 0; memset(g_library_cursors, 0, sizeof g_library_cursors);
    save();
    ++g_accounts_revision;
}

static void parse_item(evo_provider_item_t *item, const cJSON *o) {
    memset(item, 0, sizeof *item);
    copy(item->id, sizeof item->id, str(o, "id"));
    copy(item->title, sizeof item->title, str(o, "title"));
    copy(item->subtitle, sizeof item->subtitle, str(o, "subtitle"));
    copy(item->overview, sizeof item->overview, str(o, "overview"));
    copy(item->art_url, sizeof item->art_url, str(o, "art_url"));
    copy(item->parent_id, sizeof item->parent_id, str(o, "channel_id"));
    item->duration_sec = (int64_t)num(o, "duration_sec");
    item->is_live = boolean(o, "is_live");
    item->is_folder = boolean(o, "is_folder");
    item->kind = EVO_MEDIA_VIDEO;
}

static int init(void) {
    tube_auth_cancel();
    unsigned auth_serial = g_auth.serial + 1, auth_revision = g_auth.revision + 1;
    memset(&g_auth, 0, sizeof g_auth);
    g_auth.serial = auth_serial ? auth_serial : 1; g_auth.revision = auth_revision;
    g_store_writable = 0;
    watch_reset(1);
    g_active_id[0] = 0; g_active_profile[0] = 0; g_next_id[0] = 0; g_next_profile[0] = 0;
    memset(g_comments, 0, sizeof g_comments);
    memset(&g_account_intent, 0, sizeof g_account_intent);
    memset(&g_playlist_prompt, 0, sizeof g_playlist_prompt);
    FILE *f = fopen(evo_data_path("youtube.json"), "rb");
    if (!f) { if (errno == ENOENT) { g_store_writable = 1; return 0; } return -1; }
    if (fseek(f, 0, SEEK_END) || ftell(f) < 0 || ftell(f) > 2 * 1024 * 1024) { fclose(f); return -1; }
    long n = ftell(f); rewind(f);
    char *body = calloc(1, (size_t)n + 1);
    if (!body) { fclose(f); return -1; }
    size_t read = fread(body, 1, (size_t)n, f); fclose(f);
    cJSON *root = read == (size_t)n ? cJSON_ParseWithLength(body, (size_t)n + 1) : NULL;
    free(body);
    double version = num(root, "version");
    if (!root || (version != 1 && version != 2) || !cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(root, "records"))) {
        cJSON_Delete(root); return -1;
    }
    g_store_writable = 1;
    copy(g_source, sizeof g_source, str(root, "source"));
    copy(g_token, sizeof g_token, str(root, "token"));
    const char *profile = str(root, "library_profile");
    copy(g_library_profile, sizeof g_library_profile, valid_profile(profile) ? profile : "guest");
    int height = (int)num(root, "max_height");
    if (height >= 144 && height <= 2160) g_height = height;
    int codec = (int)num(root, "codec"); if (codec <= 2) g_codec = codec;
    g_captions = boolean(root, "captions");
    g_auto_captions = boolean(root, "automatic_captions");
    g_sponsor = boolean(root, "sponsorblock");
    g_autoplay = boolean(root, "autoplay");
    g_ask_stream = boolean(root, "ask_stream");
    const cJSON *history_sync = cJSON_GetObjectItemCaseSensitive(root, "history_sync");
    g_history_sync = !cJSON_IsBool(history_sync) || cJSON_IsTrue(history_sync);
    const char *language = str(root, "language"), *audio = str(root, "audio_language");
    const char *valid = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-";
    if (*language && strlen(language) < sizeof g_language && strspn(language, valid) == strlen(language))
        copy(g_language, sizeof g_language, language);
    if (strlen(audio) < sizeof g_audio_language && strspn(audio, valid) == strlen(audio))
        copy(g_audio_language, sizeof g_audio_language, audio);
    g_count = 0; g_queue_order = 0;
    cJSON *o;
    cJSON_ArrayForEach(o, cJSON_GetObjectItemCaseSensitive(root, "records")) {
        const char *owner = version == 1 ? "guest" : str(o, "profile");
        if (record_for(str(o, "id"), owner, 0)) continue;
        Record *r = record_for(str(o, "id"), owner, 1); if (!r) continue;
        parse_item(&r->item, o);
        r->item.resume_pos_sec = (int64_t)num(o, "position");
        r->item.duration_sec = (int64_t)num(o, "duration");
        r->item.played = boolean(o, "played");
        r->updated = (long long)num(o, "updated");
        r->queue_order = (long long)num(o, "queue_order");
        if (r->queue_order > g_queue_order) g_queue_order = r->queue_order;
        r->favorite = boolean(o, "favorite"); r->queued = boolean(o, "queued");
        r->subscribed = boolean(o, "subscribed");
    }
    cJSON_Delete(root); if (version == 1) save(); return 0;
}

/* Local library and preferences remain usable before pairing or while offline. */
static int configured(void) { return 1; }
static const char *get_source(void) { return g_source; }

static int set_source(const char *value) {
    if (!value || strlen(value) >= sizeof g_source) return -1;
    char base[512]; copy(base, sizeof base, value);
    char *hash = strchr(base, '#');
    char pin[7] = "";
    if (hash) {
        *hash++ = 0;
        if (strlen(hash) != 6) return -1;
        for (int i = 0; i < 6; ++i) if (!isdigit((unsigned char)hash[i])) return -1;
        copy(pin, sizeof pin, hash);
    }
    char host[256]; int port, tls;
    if (evo_provider_parse_web_source(base, host, sizeof host, &port, &tls, 8765)) return -1;
    if (strchr(base, '@') || strchr(base, '?') || strchr(base, '\r') || strchr(base, '\n')) return -1;
    char normalized[512];
    snprintf(normalized, sizeof normalized, "%s://%s:%d", tls ? "https" : "http", host, port);
    tube_cancel();
    watch_reset(1);
    unsigned auth_serial = g_auth.serial + 1, auth_revision = g_auth.revision + 1;
    memset(&g_auth, 0, sizeof g_auth);
    g_auth.serial = auth_serial ? auth_serial : 1; g_auth.revision = auth_revision;
    memset(&g_account_intent, 0, sizeof g_account_intent);
    memset(&g_playlist_prompt, 0, sizeof g_playlist_prompt);
    memset(g_comments, 0, sizeof g_comments);
    g_library_parent[0] = 0; memset(g_library_cursors, 0, sizeof g_library_cursors);
    if (strcmp(normalized, g_source)) copy(g_library_profile, sizeof g_library_profile, "guest");
    if (strcmp(normalized, g_source) || *pin) g_token[0] = 0;
    copy(g_source, sizeof g_source, normalized); copy(g_pin, sizeof g_pin, pin);
    ++g_generation; save(); return 0;
}

static void complete(Request *r, int ok) {
    if (r->kind == 9 && r->intent == g_auth.serial && !ok &&
        (g_auth.state == AUTH_STARTING || g_auth.state == AUTH_PENDING)) {
        auth_cleanup_request(r, NULL);
        tube_auth_cancel(); g_auth.state = AUTH_FAILED; ++g_auth.revision;
    }
    if (r->kind == 8 && r->intent == g_watch.serial && !ok) {
        g_watch.failed = 1; g_watch.dirty = 0; g_watch.status = -1;
    }
    if (r->kind == 6 && r->intent == g_account_intent.serial && g_account_intent.state == 1)
        g_account_intent.state = -1;
    evo_provider_items_cb items_cb = r->items_cb;
    evo_provider_resolve_cb resolve_cb = r->resolve_cb;
    void *ud = r->ud;
    if (resolve_cb) resolve_cb(ok, ok ? r->choices : NULL, ok ? r->choice_count : 0, ud);
    if (items_cb) items_cb(0, NULL, 0, 0, ud);
    memset(r, 0, sizeof *r);
}

static Request *request_new(int kind, void *ud) {
    for (int i = 0; i < REQUESTS; ++i) if (!g_requests[i].used) {
        Request *r = &g_requests[i]; memset(r, 0, sizeof *r);
        r->used = 1; r->kind = kind; r->ud = ud; r->generation = g_generation;
        r->deadline = now_ms() + 65000; return r;
    }
    tube_set_error("Too many requests are pending. Cancel the current request and retry.");
    return NULL;
}

static int companion_url(const char *url) {
    size_t n = strlen(g_source);
    return n && !strncmp(url, g_source, n) && url[n] == '/' && strlen(url) < EVO_PROVIDER_MAX_URL;
}

static void response(int ok, int status, const char *body, size_t size, void *ud);
static void folder(evo_provider_item_t *out, const char *id, const char *title, const char *hint);

static int send_request(Request *r, const char *method, const char *path, const char *payload) {
    char url[2048], auth[160]; const char *headers[2] = {"Content-Type: application/json", auth};
    snprintf(url, sizeof url, "%s%s", g_source, path);
    snprintf(auth, sizeof auth, "Authorization: Bearer %s", g_token);
    int rc = evo_net_request_async(method, url, payload, headers, *g_token ? 2 : 1, response, r);
    r->inflight = rc == 0;
    if (rc) { tube_set_error("Could not queue the companion request. Retry."); complete(r, 0); }
    return rc;
}

static int begin(Request *r) {
    if (!*g_source) { tube_set_error("Set the companion LAN address in Options first."); complete(r, 0); return 0; }
    if (!*g_token) {
        if (!*g_pin) { tube_set_error("Pair first: enter the companion address followed by # and its six-digit code in Options."); complete(r, 0); return 0; }
        char payload[64]; snprintf(payload, sizeof payload, "{\"code\":\"%s\"}", g_pin);
        r->stage = 0; return send_request(r, "POST", "/v1/pair", payload);
    }
    if (r->kind == 9) {
        copy(r->auth_source, sizeof r->auth_source, g_source);
        copy(r->auth_token, sizeof r->auth_token, g_token);
    }
    r->stage = 1; return send_request(r, "POST", "/v1/jobs", r->payload);
}

static int auth_session_valid(const char *value) {
    return value && strlen(value) == 32 && strspn(value, "0123456789abcdef") == 32;
}

unsigned tube_auth_revision(void) { return g_auth.revision; }

static void auth_cancel_remote(const char *source, const char *token, const char *session) {
    if (!*source || !*token || !auth_session_valid(session)) return;
    char url[640], auth[160], body[80];
    snprintf(url, sizeof url, "%s/v1/auth/cancel", source);
    snprintf(auth, sizeof auth, "Authorization: Bearer %s", token);
    snprintf(body, sizeof body, "{\"session\":\"%s\"}", session);
    const char *headers[] = {"Content-Type: application/json", auth};
    evo_net_request_async("POST", url, body, headers, 2, ignored, NULL);
}

void tube_auth_cancel(void) {
    if (g_auth.state != AUTH_PENDING && g_auth.state != AUTH_STARTING) return;
    auth_cancel_remote(g_source, g_token, g_auth.session);
    g_auth.state = AUTH_CANCELED;
    g_auth.code[0] = g_auth.url[0] = 0;
    if (!++g_auth.serial) ++g_auth.serial;
    ++g_auth.revision;
}

static void auth_cleanup_request(Request *r, const cJSON *root) {
    char url[640], auth[160];
    const char *job = r->stage == 1 ? str(root, "job") : r->job;
    if (valid_cursor(job) && *r->auth_source && *r->auth_token) {
        snprintf(url, sizeof url, "%s/v1/jobs/%s", r->auth_source, job);
        snprintf(auth, sizeof auth, "Authorization: Bearer %s", r->auth_token);
        const char *headers[] = {auth};
        evo_net_request_async("DELETE", url, NULL, headers, 1, ignored, NULL);
    }
    const cJSON *result = cJSON_GetObjectItemCaseSensitive(root, "result");
    if (!strcmp(str(result, "state"), "pending"))
        auth_cancel_remote(r->auth_source, r->auth_token, str(result, "session"));
}

static int auth_rows(evo_provider_items_cb cb, void *ud) {
    evo_provider_item_t rows[4]; int n = 0;
    if (g_auth.state == AUTH_PENDING) {
        folder(&rows[n++], "auth:info:code", g_auth.code, "Enter this code on your phone or computer");
        folder(&rows[n++], "auth:info:url", g_auth.url, "Open this address to authorize YouTube");
        folder(&rows[n++], "auth:cancel", "Cancel sign-in", "Circle also cancels when leaving sign-in");
        snprintf(rows[0].overview, sizeof rows[0].overview,
            "Open %s on your phone or computer and enter %s. Waiting for authorization; status checks are automatic. The code expires automatically if sign-in is not completed.",
            g_auth.url, g_auth.code);
    } else if (g_auth.state == AUTH_AUTHORIZED) {
        char id[64]; snprintf(id, sizeof id, "library:profile:%s", g_auth.profile);
        folder(&rows[n++], id, "Use authorized YouTube account", g_auth.name);
        folder(&rows[n++], "library:profiles", "All profiles", "Account saved privately on the companion");
    } else {
        const char *title = g_auth.state == AUTH_CANCELED ? "Sign-in canceled" :
            g_auth.state == AUTH_EXPIRED ? "Sign-in code expired" :
            g_auth.state == AUTH_DENIED ? "YouTube authorization denied" :
            g_auth.state == AUTH_STARTING ? "Requesting YouTube sign-in code" : "YouTube sign-in could not be completed";
        folder(&rows[n++], "auth:info", title, "Account credentials stay on the companion");
        if (g_auth.state != AUTH_STARTING)
            folder(&rows[n++], "auth:start", "Start a new sign-in", "Authorize using your phone or computer");
    }
    if (cb) cb(1, rows, n, 0, ud);
    return 0;
}

static void auth_result(Request *r, const cJSON *result) {
    int changed = 1;
    const char *state = str(result, "state"), *session = str(result, "session");
    if (!auth_session_valid(session) || (*g_auth.session && strcmp(g_auth.session, session))) {
        tube_set_error("The sign-in response was invalid."); complete(r, 0); return;
    }
    if (!strcmp(state, "pending")) {
        const char *code = str(result, "user_code"), *url = str(result, "verification_url");
        double expires = num(result, "expires_in"), delay = num(result, "poll_after");
        if (!*code || strlen(code) > 32 || strspn(code, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-") != strlen(code) ||
            (strcmp(url, "https://www.google.com/device") && strcmp(url, "https://google.com/device") &&
             strcmp(url, "https://www.youtube.com/activate") && strcmp(url, "https://youtube.com/activate") &&
             strcmp(url, "https://www.google.com/activate") && strcmp(url, "https://google.com/activate") &&
             strcmp(url, "https://www.youtube.com/device") && strcmp(url, "https://youtube.com/device")) ||
            !cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(result, "expires_in")) ||
            !cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(result, "poll_after")) ||
            cJSON_GetObjectItemCaseSensitive(result, "poll_after")->valuedouble < 0 ||
            cJSON_GetObjectItemCaseSensitive(result, "poll_after")->valuedouble > 60 ||
            expires < 1 || expires > 1800 || floor(expires) != expires || floor(delay) != delay || delay > 60) {
            auth_cancel_remote(r->auth_source, r->auth_token, session);
            tube_set_error("The sign-in response was invalid."); complete(r, 0); return;
        }
        changed = g_auth.state != AUTH_PENDING || strcmp(g_auth.code, code) || strcmp(g_auth.url, url);
        copy(g_auth.session, sizeof g_auth.session, session);
        copy(g_auth.code, sizeof g_auth.code, code); copy(g_auth.url, sizeof g_auth.url, url);
        long long deadline = now_ms() + (long long)(expires * 1000);
        if (g_auth.state != AUTH_PENDING || deadline < g_auth.deadline) g_auth.deadline = deadline;
        g_auth.state = AUTH_PENDING;
        g_auth.next_poll = now_ms() + (long long)((delay < 1 ? 1 : delay) * 1000);
    } else if (!strcmp(state, "authorized")) {
        const cJSON *profile = cJSON_GetObjectItemCaseSensitive(result, "profile");
        const char *id = str(profile, "id"), *name = str(profile, "name");
        if (g_auth.state != AUTH_PENDING || !valid_profile(id) || strlen(id) != 22 || strncmp(id, "oauth_", 6) ||
            strspn(id + 6, "0123456789abcdef") != 16 || !*name || strlen(name) > 600) {
            tube_set_error("The authorized profile response was invalid."); complete(r, 0); return;
        }
        g_auth.state = AUTH_AUTHORIZED;
        copy(g_auth.profile, sizeof g_auth.profile, id); copy(g_auth.name, sizeof g_auth.name, name);
    } else if (!strcmp(state, "canceled")) g_auth.state = AUTH_CANCELED;
    else if (!strcmp(state, "expired")) g_auth.state = AUTH_EXPIRED;
    else if (!strcmp(state, "denied")) g_auth.state = AUTH_DENIED;
    else if (!strcmp(state, "failed")) g_auth.state = AUTH_FAILED;
    else { tube_set_error("The sign-in response was invalid."); complete(r, 0); return; }
    if (g_auth.state != AUTH_PENDING) g_auth.code[0] = g_auth.url[0] = 0;
    if (changed) ++g_auth.revision;
    evo_provider_items_cb cb = r->items_cb; void *data = r->ud;
    memset(r, 0, sizeof *r); auth_rows(cb, data);
}

static void watch_flush(int terminal_flush) {
    if (!g_history_sync || !*g_token || !g_watch.dirty || g_watch.failed ||
        !valid_id(g_watch.id) || !valid_profile(g_watch.profile) || !strcmp(g_watch.profile, "guest")) return;
    if (g_watch.position <= 0 && !g_watch.sent) { g_watch.dirty = 0; return; }
    int free_slot = 0;
    for (int i = 0; i < REQUESTS; ++i) {
        if (g_requests[i].used && g_requests[i].kind == 8 && g_requests[i].intent == g_watch.serial && !terminal_flush) return;
        if (!g_requests[i].used) free_slot = 1;
    }
    if (!free_slot || (g_watch.sent && !g_watch.stopped && now_ms() - g_watch.last_sent < 30000)) return;
    Request *r = request_new(8, NULL); if (!r) return;
    r->intent = g_watch.serial; r->watch_position = g_watch.position; r->stage = 3;
    snprintf(r->payload, sizeof r->payload,
        "{\"action\":\"watch_progress\",\"profile\":\"%s\",\"id\":\"%s\",\"session\":\"%s\",\"position_sec\":%lld,\"state\":\"%s\"}",
        g_watch.profile, g_watch.id, g_watch.session, (long long)g_watch.position, g_watch.stopped ? "stopped" : "playing");
    g_watch.dirty = 0; g_watch.sent = 1; g_watch.last_sent = now_ms(); g_watch.status = 1;
    send_request(r, "POST", "/v1/library", r->payload);
}

static void caption_response(int ok, int status, const char *body, size_t size, void *ud) {
    Request *r = ud; r->inflight = 0;
    if (!r->used) return;
    if (r->generation != g_generation) { complete(r, 0); return; }
    if (ok && status == 200 && size < 2 * 1024 * 1024 && !memchr(body, 0, size)) {
        char path[512]; copy(path, sizeof path, evo_data_path("active.srt"));
        FILE *f = fopen(path, "wb");
        if (f) {
            int success = fwrite(body, 1, size, f) == size;
            if (fclose(f)) success = 0;
            if (success) copy(g_caption_path, sizeof g_caption_path, evo_data_path("active.mp4"));
        }
    }
    complete(r, 1); /* A missing caption never prevents ordinary playback. */
}

static void sponsor_response(int ok, int status, const char *body, size_t size, void *ud) {
    unsigned gen = (unsigned)(size_t)ud;
    if (!ok || status != 200 || gen != g_generation || size > 65536) return;
    cJSON *root = cJSON_ParseWithLength(body, size);
    if (!root) return;
    g_segment_count = 0;
    cJSON *s;
    cJSON_ArrayForEach(s, cJSON_GetObjectItemCaseSensitive(root, "segments")) {
        double start = num(s, "start"), end = num(s, "end");
        if (end > start && end < 604800 && g_segment_count < SEGMENTS) {
            g_segments[g_segment_count][0] = start; g_segments[g_segment_count++][1] = end;
        }
    }
    cJSON_Delete(root);
}

static void resolved(Request *r, const cJSON *root) {
    if (strcmp(str(root, "id"), r->parent)) {
        tube_set_error("The playback response belongs to another video."); complete(r, 0); return;
    }
    r->choice_count = 0;
    cJSON *c;
    cJSON_ArrayForEach(c, cJSON_GetObjectItemCaseSensitive(root, "choices")) {
        if (r->choice_count >= CHOICES) break;
        if (!companion_url(str(c, "url"))) continue;
        evo_stream_choice_t *out = &r->choices[r->choice_count++];
        copy(out->url, sizeof out->url, str(c, "url"));
        copy(out->label, sizeof out->label, str(c, "label"));
        copy(out->container, sizeof out->container, str(c, "container"));
        copy(out->video_codec, sizeof out->video_codec, str(c, "video_codec"));
        copy(out->audio_codec, sizeof out->audio_codec, str(c, "audio_codec"));
        copy(out->video_range, sizeof out->video_range, "SDR");
        out->width = (int)num(c, "width"); out->height = (int)num(c, "height");
        out->bitrate_bps = (int64_t)num(c, "bitrate_bps"); out->is_live = boolean(c, "is_live");
        out->audio_channels = (int)num(c, "audio_channels");
    }
    if (!r->choice_count) { tube_set_error("The companion returned no supported playback choices."); complete(r, 0); return; }
    copy(g_resolved_id, sizeof g_resolved_id, r->parent);
    copy(g_resolved_profile, sizeof g_resolved_profile, r->profile);
    Record *cached = record(str(root, "id"), 1);
    if (cached) {
        copy(cached->item.title, sizeof cached->item.title, str(root, "title"));
        cached->item.duration_sec = (int64_t)num(root, "duration_sec");
    }
    g_caption_path[0] = 0; g_segment_count = 0; g_skip_guard = -1;
    g_chapter_count = 0;
    cJSON_ArrayForEach(c, cJSON_GetObjectItemCaseSensitive(root, "chapters")) {
        if (g_chapter_count < 128) g_chapters[g_chapter_count++] = num(c, "start");
    }
    if (g_sponsor && cached) {
        char url[1024], auth[160];
        snprintf(url, sizeof url, "%s/v1/sponsor/%s", g_source, cached->item.id);
        snprintf(auth, sizeof auth, "Authorization: Bearer %s", g_token);
        const char *headers[] = {auth};
        evo_net_request_async("GET", url, NULL, headers, 1, sponsor_response, (void *)(size_t)g_generation);
    }
    if (g_captions) {
        cJSON_ArrayForEach(c, cJSON_GetObjectItemCaseSensitive(root, "captions")) {
            if (strcmp(str(c, "language"), g_language) || !companion_url(str(c, "url"))) continue;
            r->stage = 3;
            int rc = evo_net_request_async("GET", str(c, "url"), NULL, NULL, 0, caption_response, r);
            r->inflight = rc == 0;
            if (!rc) return;
            break;
        }
    }
    complete(r, 1);
}

static void response(int ok, int status, const char *body, size_t size, void *ud) {
    Request *r = ud; r->inflight = 0;
    if (!r->used) return;
    if (r->kind == 9 && (r->generation != g_generation || r->intent != g_auth.serial)) {
        cJSON *late = size <= 2 * 1024 * 1024 && body ? cJSON_ParseWithLength(body, size) : NULL;
        auth_cleanup_request(r, late); cJSON_Delete(late); complete(r, 0); return;
    }
    if (r->generation != g_generation) { complete(r, 0); return; }
    if (r->kind == 8 && r->intent != g_watch.serial) { complete(r, 0); return; }
    cJSON *root = size <= 2 * 1024 * 1024 && body ? cJSON_ParseWithLength(body, size) : NULL;
    if (!ok || status < 200 || status >= 300 || !root) {
        const cJSON *error = root ? cJSON_GetObjectItemCaseSensitive(root, "error") : NULL;
        tube_set_error(*str(error, "message") ? str(error, "message") : "Could not reach the companion. Check that it is running and the LAN address is correct.");
        if (status == 401) { g_token[0] = 0; save(); }
        cJSON_Delete(root); complete(r, 0); return;
    }
    if (r->kind == 8) {
        int confirmed = boolean(root, "success") && cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(root, "position_sec")) &&
            num(root, "position_sec") == r->watch_position;
        if (confirmed && r->intent == g_watch.serial) g_watch.status = 2;
        cJSON_Delete(root); complete(r, confirmed); return;
    }
    if (r->stage == 0) {
        const char *token = str(root, "token");
        if (strlen(token) < 24 || strlen(token) >= sizeof g_token || strchr(token, '\r') || strchr(token, '\n')) {
            cJSON_Delete(root); tube_set_error("The pairing response was invalid."); complete(r, 0); return;
        }
        copy(g_token, sizeof g_token, token); g_pin[0] = 0; save();
        cJSON_Delete(root); begin(r); return;
    }
    if (r->stage == 1) {
        const char *job = str(root, "job");
        if (!*job || strlen(job) >= sizeof r->job || strspn(job, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") != strlen(job)) {
            cJSON_Delete(root); tube_set_error("The companion job response was invalid."); complete(r, 0); return;
        }
        copy(r->job, sizeof r->job, job); r->stage = 2; r->next_poll = now_ms() + 250;
        cJSON_Delete(root); return;
    }
    const char *state = str(root, "state");
    if (!strcmp(state, "pending")) {
        r->next_poll = now_ms() + 500; cJSON_Delete(root); return;
    }
    if (strcmp(state, "done")) {
        const cJSON *error = cJSON_GetObjectItemCaseSensitive(root, "error");
        tube_set_error(*str(error, "message") ? str(error, "message") : "The request was cancelled or could not be completed.");
        cJSON_Delete(root); complete(r, 0); return;
    }
    const cJSON *result = cJSON_GetObjectItemCaseSensitive(root, "result");
    if (r->kind == 9) { auth_result(r, result); cJSON_Delete(root); return; }
    if (r->kind == 2) { resolved(r, result); cJSON_Delete(root); return; }
    if (r->kind == 6) {
        if (!boolean(result, "success")) {
            cJSON_Delete(root); tube_set_error("YouTube did not confirm the account change. Check your account before repeating it."); complete(r, 0); return;
        }
        int removing = !strcmp(g_account_intent.action, "profile_remove");
        if (removing && strcmp(str(result, "profile"), g_account_intent.profile)) {
            cJSON_Delete(root); tube_set_error("The companion did not confirm removal of this profile."); complete(r, 0); return;
        }
        if (r->intent == g_account_intent.serial) g_account_intent.state = 2;
        evo_provider_item_t item;
        folder(&item, "account:notice", removing ? "Account removed" : "YouTube account updated",
            removing ? "Select another profile or continue as Guest" : "The companion confirmed this change");
        if (!strcmp(g_account_intent.action, "playlist_create") && valid_playlist(str(result, "playlist_id"))) {
            snprintf(item.id, sizeof item.id, "library:playlist:%s", str(result, "playlist_id"));
            copy(item.title, sizeof item.title, "Open created playlist");
        }
        snprintf(item.overview, sizeof item.overview, "Operation: %s | Profile: %s. Circle returns to the video browser.",
            g_account_intent.action, g_account_intent.profile);
        evo_provider_items_cb cb = r->items_cb; void *data = r->ud;
        if (removing) account_removed();
        memset(r, 0, sizeof *r); if (cb) cb(1, &item, 1, 0, data);
        cJSON_Delete(root); return;
    }
    evo_provider_item_t *items = calloc(LOCAL_PAGE, sizeof *items);
    if (!items) { cJSON_Delete(root); complete(r, 0); return; }
    int n = 0; cJSON *o;
    if (r->kind == 4) {
        const cJSON *profiles = cJSON_GetObjectItemCaseSensitive(result, "profiles");
        if (!cJSON_IsArray(profiles)) {
            free(items); cJSON_Delete(root); tube_set_error("The companion profile list was invalid."); complete(r, 0); return;
        }
        int selected = !strcmp(g_library_profile, "guest"), guest = 0;
        cJSON_ArrayForEach(o, profiles) {
            if (n >= LOCAL_PAGE || !valid_profile(str(o, "id"))) continue;
            if (!strcmp(str(o, "id"), "guest")) guest = 1;
            if (!strcmp(str(o, "id"), g_library_profile)) selected = 1;
            char id[64]; snprintf(id, sizeof id, "library:profile:%s", str(o, "id"));
            folder(&items[n++], id, str(o, "name"), boolean(o, "account_configured") ? "Account configured on the companion" : "Browse as guest");
        }
        if (!guest) {
            free(items); cJSON_Delete(root); tube_set_error("The companion profile list was invalid."); complete(r, 0); return;
        }
        if (!selected) account_removed();
        evo_provider_items_cb cb = r->items_cb; void *data = r->ud;
        memset(r, 0, sizeof *r);
        if (cb) cb(1, items, n, 0, data);
        free(items); cJSON_Delete(root); return;
    }
    if (r->kind == 5 && (strcmp(str(result, "kind"), "comments") ||
        !cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(result, "items")))) {
        free(items); cJSON_Delete(root); tube_set_error("The comment response was invalid."); complete(r, 0); return;
    }
    if (r->kind == 3 || r->kind == 5 || r->kind == 7) {
        const char *cursor = str(result, "cursor");
        int more = boolean(result, "has_more");
        if (more && !valid_cursor(cursor)) {
            free(items); cJSON_Delete(root); tube_set_error("The library pagination response was invalid."); complete(r, 0); return;
        }
        if (!strcmp(r->parent, g_library_parent) && r->page + 1 < LIBRARY_PAGES)
            copy(g_library_cursors[r->page + 1], sizeof g_library_cursors[0], more ? cursor : "");
    }
    if (r->kind == 5) {
        memset(g_comments, 0, sizeof g_comments);
        cJSON_ArrayForEach(o, cJSON_GetObjectItemCaseSensitive(result, "items")) {
            if (n >= LOCAL_PAGE || !*str(o, "id")) continue;
            char id[160], hint[256];
            const char *reply = str(o, "reply_cursor");
            if (*reply && !valid_cursor(reply)) continue;
            if (*reply) snprintf(id, sizeof id, "discussion:replies:%s", reply);
            else snprintf(id, sizeof id, "discussion:leaf:%u:%d", r->generation, n);
            snprintf(hint, sizeof hint, "%.80s | %.32s likes | %.32s replies%s%s",
                str(o, "published"), str(o, "likes"), str(o, "replies"),
                boolean(o, "pinned") ? " | Pinned" : "", boolean(o, "owner") ? " | Creator" : "");
            folder(&items[n], id, *str(o, "author") ? str(o, "author") : "Comment", hint);
            copy(items[n].overview, sizeof items[n].overview, str(o, "content"));
            copy(g_comments[n].id, sizeof g_comments[n].id, id);
            copy(g_comments[n].content, sizeof g_comments[n].content, str(o, "content"));
            ++n;
        }
        evo_provider_items_cb cb = r->items_cb; void *data = r->ud;
        int more = boolean(result, "has_more");
        memset(r, 0, sizeof *r);
        if (cb) cb(1, items, n, more, data);
        free(items); cJSON_Delete(root); return;
    }
    cJSON_ArrayForEach(o, cJSON_GetObjectItemCaseSensitive(result, "items")) {
        if (n >= LOCAL_PAGE || !valid_catalog_id(str(o, "id"))) continue;
        int duplicate = 0;
        for (int i = 0; i < n; ++i) if (!strcmp(items[i].id, str(o, "id"))) duplicate = 1;
        if (duplicate) continue;
        parse_item(&items[n], o);
        if (r->kind == 7) {
            if (strncmp(items[n].id, "library:playlist:", 17)) continue;
            const char *video = strrchr(r->parent, ':');
            char target[160];
            snprintf(target, sizeof target, "playlist:confirm:%s:%.11s:%.100s",
                !strncmp(r->parent, "playlist:pick:add:", 18) ? "add" : "remove",
                video ? video + 1 : "", items[n].id + 17);
            for (int i = 0; i < n; ++i) if (!strcmp(items[i].id, target)) duplicate = 1;
            if (duplicate) continue;
            copy(items[n].id, sizeof items[n].id, target); items[n].is_folder = 1;
            copy(items[n].subtitle, sizeof items[n].subtitle, "Select this playlist, then confirm the change");
            ++n; continue;
        }
        Record *cached = record(items[n].id, 1);
        if (cached) {
            items[n].resume_pos_sec = cached->item.resume_pos_sec;
            items[n].played = cached->item.played;
            cached->item = items[n];
        }
        ++n;
    }
    evo_provider_items_cb cb = r->items_cb; void *data = r->ud;
    int more = boolean(result, "has_more");
    memset(r, 0, sizeof *r);
    if (cb) cb(1, items, n, more, data);
    free(items); cJSON_Delete(root);
}

void tube_poll(void) {
    for (int i = 0; i < REQUESTS; ++i) {
        Request *r = &g_requests[i];
        if (!r->used || r->inflight) continue;
        if (r->generation != g_generation || now_ms() > r->deadline) {
            tube_set_error(r->kind == 6 ? "The account change was not confirmed. Check your account before repeating it." :
                "The companion request was cancelled or timed out. Retry."); complete(r, 0); continue;
        }
        if (r->stage == 2 && now_ms() >= r->next_poll) {
            char path[128]; snprintf(path, sizeof path, "/v1/jobs/%s", r->job);
            send_request(r, "GET", path, NULL);
        }
    }
    if (g_auth.state == AUTH_PENDING) {
        if (now_ms() >= g_auth.deadline) {
            tube_auth_cancel(); g_auth.state = AUTH_EXPIRED; ++g_auth.revision;
        } else if (now_ms() >= g_auth.next_poll) {
            int busy = 0;
            for (int i = 0; i < REQUESTS; ++i)
                if (g_requests[i].used && g_requests[i].kind == 9 && g_requests[i].intent == g_auth.serial) busy = 1;
            if (!busy) {
                Request *r = request_new(9, NULL);
                if (r) {
                    r->intent = g_auth.serial;
                    snprintf(r->payload, sizeof r->payload,
                        "{\"action\":\"auth\",\"auth\":{\"action\":\"status\",\"session\":\"%s\"}}", g_auth.session);
                    begin(r);
                }
            }
        }
    }
    watch_flush(0);
}

static void ignored(int ok, int status, const char *body, size_t size, void *ud) {
    (void)ok; (void)status; (void)body; (void)size; (void)ud;
}

void tube_cancel(void) {
    tube_auth_cancel();
    ++g_generation;
    for (int i = 0; i < REQUESTS; ++i) {
        Request *r = &g_requests[i];
        if (!r->used) continue;
        /* Browsing cancellation must not cancel a playback history report. */
        if (r->kind == 8) { r->generation = g_generation; continue; }
        if (*r->job && *g_source && *g_token) {
            char url[1024], auth[160];
            snprintf(url, sizeof url, "%s/v1/jobs/%s", g_source, r->job);
            snprintf(auth, sizeof auth, "Authorization: Bearer %s", g_token);
            const char *headers[] = {auth};
            evo_net_request_async("DELETE", url, NULL, headers, 1, ignored, NULL);
        }
        if (!r->inflight) complete(r, 0);
    }
}

static void folder(evo_provider_item_t *out, const char *id, const char *title, const char *hint) {
    memset(out, 0, sizeof *out);
    copy(out->id, sizeof out->id, id); copy(out->title, sizeof out->title, title);
    copy(out->subtitle, sizeof out->subtitle, hint); out->is_folder = 1; out->kind = EVO_MEDIA_VIDEO;
}

static int local_catalog(const char *parent, int page, evo_provider_items_cb cb, void *ud) {
    evo_provider_item_t out[LOCAL_PAGE]; int n = 0;
    if (parent && !strncmp(parent, "playlist:", 9)) {
        if (!strchr(parent + 9, ':')) return 0; /* ordinary public playlist browsing */
        if (!strncmp(parent, "playlist:pick:", 14)) return 0;
        if (!strcmp(g_library_profile, "guest")) {
            folder(&out[n++], "account:notice", "Choose an account profile first", "Discover > Accounts & library > Profile");
        } else if (!strncmp(parent, "playlist:menu:", 14) && valid_playlist(parent + 14) &&
            strcmp(parent + 14, "WL") && strcmp(parent + 14, "LL")) {
            char id[160]; snprintf(id, sizeof id, "playlist:prompt:rename:%.100s", parent + 14);
            folder(&out[n++], id, "Rename YouTube playlist", "Enter a new name, then confirm");
            snprintf(id, sizeof id, "playlist:confirm:delete:%.100s", parent + 14);
            folder(&out[n++], id, "Delete YouTube playlist", "Requires confirmation for the selected profile");
        } else if (!strncmp(parent, "playlist:prompt:", 16)) {
            int create = !strncmp(parent, "playlist:prompt:create:", 23);
            int rename = !strncmp(parent, "playlist:prompt:rename:", 23);
            const char *identity = (create || rename) ? parent + 23 : "";
            if ((!create && !rename) || (create && *identity && !valid_id(identity)) ||
                (rename && (!valid_playlist(identity) || !strcmp(identity, "WL") || !strcmp(identity, "LL")))) {
                tube_set_error("Select a valid playlist or video."); cb(0, NULL, 0, 0, ud); return 1;
            }
            memset(&g_account_intent, 0, sizeof g_account_intent);
            memset(&g_playlist_prompt, 0, sizeof g_playlist_prompt);
            if (!++g_account_serial) ++g_account_serial;
            g_playlist_prompt.prompt.ticket = g_account_serial;
            copy(g_playlist_prompt.prompt.label, sizeof g_playlist_prompt.prompt.label,
                create ? "New private YouTube playlist name" : "New YouTube playlist name");
            copy(g_playlist_prompt.identity, sizeof g_playlist_prompt.identity, identity);
            copy(g_playlist_prompt.profile, sizeof g_playlist_prompt.profile, g_library_profile);
            g_playlist_prompt.create = create; g_playlist_prompt.pending = g_playlist_prompt.active = 1;
            folder(&out[n++], "account:notice", "Enter a playlist name", "Circle returns without changing your account");
        } else if (!strncmp(parent, "playlist:confirm:", 17)) {
            const char *value = parent + 17, *identity = NULL, *playlist = NULL, *action = NULL;
            char video[12] = "";
            if (!strncmp(value, "delete:", 7)) { playlist = value + 7; action = "playlist_delete"; }
            else {
                int add = !strncmp(value, "add:", 4), remove = !strncmp(value, "remove:", 7);
                identity = add ? value + 4 : remove ? value + 7 : "";
                if ((add || remove) && strlen(identity) > 12 && identity[11] == ':') {
                    memcpy(video, identity, 11); playlist = identity + 12;
                    action = add ? "playlist_add" : "playlist_remove";
                }
            }
            if (!action || !valid_playlist(playlist) ||
                (strcmp(action, "playlist_delete") ? !valid_id(video) : (!strcmp(playlist, "WL") || !strcmp(playlist, "LL")))) {
                tube_set_error("The playlist change was invalid."); cb(0, NULL, 0, 0, ud); return 1;
            }
            unsigned serial = account_intent(action, video, playlist, "");
            char id[80], hint[160]; snprintf(id, sizeof id, "account:apply:%u", serial);
            snprintf(hint, sizeof hint, "Confirm for YouTube profile: %s", g_library_profile);
            folder(&out[n++], id, !strcmp(action, "playlist_delete") ? "Delete this YouTube playlist" :
                !strcmp(action, "playlist_add") ? "Add video to this playlist" : "Remove video from this playlist", hint);
            snprintf(out[0].overview, sizeof out[0].overview, "Playlist: %s | Video: %s. Cross applies this change; Circle returns without submitting it.", playlist, *video ? video : "none");
            copy(g_comments[0].id, sizeof g_comments[0].id, out[0].id);
            copy(g_comments[0].content, sizeof g_comments[0].content, out[0].overview);
        } else if (!strncmp(parent, "playlist:review:", 16)) {
            char expected[80]; snprintf(expected, sizeof expected, "playlist:review:%u", g_account_intent.serial);
            if (!g_account_intent.serial || strcmp(parent, expected) || strcmp(g_account_intent.profile, g_library_profile)) {
                tube_set_error("Reopen this playlist action for the current profile."); cb(0, NULL, 0, 0, ud); return 1;
            }
            char id[80], hint[160]; snprintf(id, sizeof id, "account:apply:%u", g_account_intent.serial);
            snprintf(hint, sizeof hint, "Confirm for YouTube profile: %s", g_library_profile);
            folder(&out[n++], id, !strcmp(g_account_intent.action, "playlist_create") ? "Create private YouTube playlist" : "Rename YouTube playlist", hint);
            copy(g_comments[0].id, sizeof g_comments[0].id, out[0].id);
            snprintf(g_comments[0].content, sizeof g_comments[0].content, "Name: %s\nPlaylist: %s\nVideo: %s\nCross confirms; Circle returns without submitting it.",
                g_account_intent.title, g_account_intent.playlist, g_account_intent.identity);
            copy(out[0].overview, sizeof out[0].overview, g_comments[0].content);
        } else { tube_set_error("The playlist screen was invalid."); cb(0, NULL, 0, 0, ud); return 1; }
        cb(1, out, n, 0, ud); return 1;
    }
    if (parent && !strncmp(parent, "account:", 8)) {
        if (!strcmp(parent, "account:remove")) {
            if (!strcmp(g_library_profile, "guest")) {
                tube_set_error("Select an account profile first."); cb(0, NULL, 0, 0, ud); return 1;
            }
            memset(&g_account_intent, 0, sizeof g_account_intent);
            memset(&g_playlist_prompt, 0, sizeof g_playlist_prompt);
            g_account_intent.serial = ++g_account_serial;
            copy(g_account_intent.action, sizeof g_account_intent.action, "profile_remove");
            copy(g_account_intent.profile, sizeof g_account_intent.profile, g_library_profile);
            char id[80]; snprintf(id, sizeof id, "account:apply:%u", g_account_intent.serial);
            folder(&out[n++], id, "Remove selected account", "Cross confirms; Circle returns without removing it");
            snprintf(out[0].overview, sizeof out[0].overview,
                "Profile: %s. Remove this account and its saved sign-in from ProsperoTube. Continue as Guest; other accounts remain available.", g_library_profile);
            copy(g_comments[0].id, sizeof g_comments[0].id, id);
            copy(g_comments[0].content, sizeof g_comments[0].content, out[0].overview);
        } else if (!strncmp(parent, "account:menu:", 13) && valid_id(parent + 13)) {
            if (!strcmp(g_library_profile, "guest")) {
                folder(&out[n++], "account:notice", "Choose an account profile first", "Discover > Accounts & library > Profile");
            } else {
                Record *r = record(parent + 13, 0);
                for (size_t i = 0; i < sizeof g_account_actions / sizeof g_account_actions[0]; ++i) {
                    const char *identity = g_account_actions[i].channel ? r ? r->item.parent_id : "" : parent + 13;
                    if (!*identity) continue;
                    char id[160], hint[160];
                    snprintf(id, sizeof id, "account:confirm:%.32s:%.100s", g_account_actions[i].action, identity);
                    snprintf(hint, sizeof hint, "YouTube profile: %s", g_library_profile);
                    folder(&out[n++], id, g_account_actions[i].title, hint);
                }
                char id[80]; snprintf(id, sizeof id, "playlist:pick:add:%s", parent + 13);
                folder(&out[n++], id, "Add to a YouTube playlist", "Choose a playlist, then confirm");
                snprintf(id, sizeof id, "playlist:pick:remove:%s", parent + 13);
                folder(&out[n++], id, "Remove from a YouTube playlist", "Choose a playlist containing this video");
                snprintf(id, sizeof id, "playlist:prompt:create:%s", parent + 13);
                folder(&out[n++], id, "Create playlist with this video", "A new private playlist on the selected account");
            }
        } else if (!strncmp(parent, "account:confirm:", 16)) {
            const char *value = parent + 16, *colon = strchr(value, ':');
            int found = -1;
            for (size_t i = 0; colon && i < sizeof g_account_actions / sizeof g_account_actions[0]; ++i)
                if ((size_t)(colon - value) == strlen(g_account_actions[i].action) &&
                    !strncmp(value, g_account_actions[i].action, (size_t)(colon - value))) found = (int)i;
            char catalog_id[128]; snprintf(catalog_id, sizeof catalog_id, "channel:%s", colon ? colon + 1 : "");
            if (found < 0 || !strcmp(g_library_profile, "guest") ||
                (g_account_actions[found].channel ? !valid_catalog_id(catalog_id) : !valid_id(colon + 1))) {
                tube_set_error("Select a valid video or channel and account profile."); cb(0, NULL, 0, 0, ud); return 1;
            }
            memset(&g_account_intent, 0, sizeof g_account_intent);
            memset(&g_playlist_prompt, 0, sizeof g_playlist_prompt);
            g_account_intent.serial = ++g_account_serial;
            copy(g_account_intent.action, sizeof g_account_intent.action, g_account_actions[found].action);
            copy(g_account_intent.identity, sizeof g_account_intent.identity, colon + 1);
            copy(g_account_intent.profile, sizeof g_account_intent.profile, g_library_profile);
            char id[80], hint[160]; snprintf(id, sizeof id, "account:apply:%u", g_account_intent.serial);
            snprintf(hint, sizeof hint, "Confirm for YouTube profile: %s", g_library_profile);
            folder(&out[n++], id, g_account_actions[found].title, hint);
            const char *label = colon + 1;
            Record *target = record(colon + 1, 0);
            if (target && *target->item.title) label = target->item.title;
            if (g_account_actions[found].channel) for (int i = 0; i < g_count; ++i)
                if (current_record(&g_records[i]) && !strcmp(g_records[i].item.parent_id, colon + 1) && *g_records[i].item.subtitle) {
                    label = g_records[i].item.subtitle; break;
                }
            snprintf(out[0].overview, sizeof out[0].overview,
                "Selected %s: %.128s (%s). Cross applies this change to YouTube; Circle returns without submitting it.",
                g_account_actions[found].channel ? "channel" : "video", label, colon + 1);
        } else if (!strncmp(parent, "account:apply:", 14)) {
            char expected[80]; snprintf(expected, sizeof expected, "account:apply:%u", g_account_intent.serial);
            if (!g_account_intent.serial || strcmp(parent, expected) || strcmp(g_account_intent.profile, g_library_profile)) {
                tube_set_error("Reopen the account action to confirm it for the current profile."); cb(0, NULL, 0, 0, ud); return 1;
            }
            if (!g_account_intent.state) return 0; /* first explicit confirmation submits one job */
            folder(&out[n++], "account:notice", g_account_intent.state == 2 ? "YouTube account updated" :
                g_account_intent.state == 1 ? "Account change pending" : "Account change not confirmed",
                g_account_intent.state == 1 ? "Wait for the companion response" : "Check your account before repeating an unconfirmed change");
        } else if (!strcmp(parent, "account:notice")) {
            folder(&out[n++], "account:notice", "Circle to return", "No account operation submitted");
        } else { tube_set_error("The selected account action was invalid."); cb(0, NULL, 0, 0, ud); return 1; }
        cb(1, out, n, 0, ud); return 1;
    }
    if (!parent || !*parent) {
        folder(&out[n++], "discover", "Discover", "Search public videos by topic");
        folder(&out[n++], "history", "Continue watching & history", "Stored on this PS5");
        folder(&out[n++], "favorites", "Favorites", "Your local saved videos");
        folder(&out[n++], "queue", "Play queue", "R2 on a video to add or remove");
        folder(&out[n++], "subscriptions", "Followed channels", "Local follows, independent of Google accounts");
        folder(&out[n++], "settings", "YouTube preferences", "Quality, codec, captions and SponsorBlock");
        folder(&out[n++], "app:settings", "Player settings", "Display, audio and controller preferences");
        folder(&out[n++], "app:exit", "Close ProsperoTube", "Release playback, then close from the PS5 switcher");
    } else if (!strcmp(parent, "discover")) {
        folder(&out[n++], "library:home", "Home recommendations", "From the companion library service");
        folder(&out[n++], "library", "Accounts & library", "Profiles, subscriptions, history and playlists");
        folder(&out[n++], "search:science", "Science", "Explore science videos");
        folder(&out[n++], "search:technology", "Technology", "Explore technology videos");
        folder(&out[n++], "search:gaming", "Gaming", "Explore gaming videos");
        folder(&out[n++], "search:music", "Music", "Explore music videos");
        folder(&out[n++], "search:documentary", "Documentaries", "Explore documentaries");
        folder(&out[n++], "search:education", "Learning", "Explore educational videos");
    } else if (!strcmp(parent, "library") || !strncmp(parent, "library:profile:", 16)) {
        if (!strncmp(parent, "library:profile:", 16)) {
            if (!valid_profile(parent + 16)) { tube_set_error("The selected profile was invalid."); cb(0, NULL, 0, 0, ud); return 1; }
            memset(&g_account_intent, 0, sizeof g_account_intent);
            memset(&g_playlist_prompt, 0, sizeof g_playlist_prompt);
            copy(g_library_profile, sizeof g_library_profile, parent + 16);
            watch_reset(1);
            g_next_id[0] = 0; g_next_profile[0] = 0;
            memset(g_comments, 0, sizeof g_comments);
            g_library_parent[0] = 0; memset(g_library_cursors, 0, sizeof g_library_cursors); save();
        }
        char label[96]; snprintf(label, sizeof label, "Profile: %s", g_library_profile);
        folder(&out[n++], "library:profiles", label, "Choose an account configured on the companion");
        folder(&out[n++], "library:home", "Home", "Recommendations for the selected profile");
        folder(&out[n++], "library:subscriptions", "Subscriptions", "Videos from your YouTube subscriptions");
        folder(&out[n++], "library:history", "YouTube history", "History from the selected account");
        folder(&out[n++], "library:watch_later", "Watch later", "Videos saved to your account");
        folder(&out[n++], "library:playlists", "Your playlists", "Playlists from the selected account");
        folder(&out[n++], "playlist:prompt:create:", "Create YouTube playlist", "A private playlist on the selected account");
        folder(&out[n++], "auth:start", "Sign in to YouTube", "Authorize on your phone or computer");
        if (strcmp(g_library_profile, "guest"))
            folder(&out[n++], "account:remove", "Remove selected account", "Review before removing its saved sign-in");
    } else if (!strncmp(parent, "settings", 8)) {
        if (!strcmp(parent, "settings:quality")) {
            g_height = g_height == 720 ? 1080 : g_height == 1080 ? 1440 : g_height == 1440 ? 2160 : 720; save();
        } else if (!strcmp(parent, "settings:codec")) { g_codec = (g_codec + 1) % 3; save(); }
        else if (!strcmp(parent, "settings:captions")) { g_captions = !g_captions; save(); }
        else if (!strcmp(parent, "settings:automatic")) { g_auto_captions = !g_auto_captions; save(); }
        else if (!strcmp(parent, "settings:sponsor")) { g_sponsor = !g_sponsor; save(); }
        else if (!strcmp(parent, "settings:autoplay")) { g_autoplay = !g_autoplay; save(); }
        else if (!strcmp(parent, "settings:ask-stream")) { g_ask_stream = !g_ask_stream; save(); }
        else if (!strcmp(parent, "settings:history-sync")) {
            g_history_sync = !g_history_sync;
            if (!g_history_sync) g_watch.dirty = 0;
            save();
        }
        else if (!strncmp(parent, "settings:lang:", 14)) { copy(g_language, sizeof g_language, parent + 14); save(); }
        else if (!strncmp(parent, "settings:audio:", 15)) { copy(g_audio_language, sizeof g_audio_language, parent + 15); save(); }
        else if (!strcmp(parent, "settings:clear-history")) {
            for (int i = 0; i < g_count; ++i) if (current_record(&g_records[i])) {
                g_records[i].updated = 0; g_records[i].item.resume_pos_sec = 0; g_records[i].item.played = 0;
            } save();
        } else if (!strcmp(parent, "settings:clear-queue")) {
            for (int i = 0; i < g_count; ++i) if (current_record(&g_records[i])) g_records[i].queued = 0;
            save();
        }
        if (!strcmp(parent, "settings:languages") || !strcmp(parent, "settings:audio-languages")) {
            const char *langs[] = {"en", "fr", "es", "de", "it", "pt", "ja", "ko", "zh", "ru"};
            for (int i = 0; i < 10; ++i) { char id[64]; snprintf(id, sizeof id, "%s%s", !strcmp(parent, "settings:languages") ? "settings:lang:" : "settings:audio:", langs[i]); folder(&out[n++], id, langs[i], "Select this language for the next video"); }
        } else {
            char label[96]; snprintf(label, sizeof label, "Maximum quality: %dp", g_height);
            folder(&out[n++], "settings:quality", label, "Cycle 720 / 1080 / 1440 / 2160; console validation pending");
            snprintf(label, sizeof label, "Video codec: %s", g_codec == 1 ? "AVC" : g_codec == 2 ? "VP9" : "Automatic");
            folder(&out[n++], "settings:codec", label, "Use supported SDR tracks; AV1 and HDR deferred");
            folder(&out[n++], "settings:captions", g_captions ? "Captions: on" : "Captions: off", "Apply when opening the next video");
            snprintf(label, sizeof label, "Caption language: %s", g_language);
            folder(&out[n++], "settings:languages", label, "Choose a caption language");
            snprintf(label, sizeof label, "Audio language: %s", *g_audio_language ? g_audio_language : "default");
            folder(&out[n++], "settings:audio-languages", label, "Prefer an alternate audio language when available");
            folder(&out[n++], "settings:automatic", g_auto_captions ? "Automatic captions: allowed" : "Automatic captions: off", "Human captions are preferred");
            folder(&out[n++], "settings:sponsor", g_sponsor ? "SponsorBlock: on" : "SponsorBlock: off", "Also enable --sponsorblock on the companion");
            folder(&out[n++], "settings:autoplay", g_autoplay ? "Queue autoplay: on" : "Queue autoplay: off", "Play the next queued video at the end");
            folder(&out[n++], "settings:ask-stream", g_ask_stream ? "Choose stream before playback: on" : "Choose stream before playback: off", "Off plays the preferred quality immediately; on shows available choices");
            folder(&out[n++], "settings:history-sync", g_history_sync ? "Sync YouTube history: on" : "Sync YouTube history: off",
                g_watch.status < 0 ? "Last report unconfirmed; this playback session will not retry" :
                g_watch.status == 1 ? "History report pending" : g_watch.status == 2 ? "Last history report confirmed" :
                "Report watched videos and positions for the selected account");
            folder(&out[n++], "settings:clear-history", "Clear local history", "Remove resume positions and watched markers for this profile");
            folder(&out[n++], "settings:clear-queue", "Clear play queue", "Remove this profile's queued videos; keep favorites");
        }
    } else if (!strcmp(parent, "history") || !strcmp(parent, "favorites") || !strcmp(parent, "queue") || !strcmp(parent, "subscriptions")) {
        int indexes[RECORDS], total = 0;
        for (int i = 0; i < g_count; ++i) {
            Record *r = &g_records[i];
            int include = !strcmp(parent, "history") ? r->updated > 0 : !strcmp(parent, "favorites") ? r->favorite : !strcmp(parent, "queue") ? r->queued : r->subscribed;
            if (include && current_record(r)) indexes[total++] = i;
        }
        for (int i = 0; i < total; ++i) for (int j = i + 1; j < total; ++j) {
            Record *a = &g_records[indexes[i]], *b = &g_records[indexes[j]];
            if ((!strcmp(parent, "queue") && b->queue_order < a->queue_order) ||
                (strcmp(parent, "queue") && b->updated > a->updated)) {
                int tmp = indexes[i]; indexes[i] = indexes[j]; indexes[j] = tmp;
            }
        }
        for (int i = page * LOCAL_PAGE; i < total && n < LOCAL_PAGE; ++i) {
            Record *r = &g_records[indexes[i]];
            if (!strcmp(parent, "subscriptions")) {
                if (!*r->item.parent_id) continue;
                int duplicate = 0;
                for (int k = 0; k < n; ++k) if (!strcmp(out[k].id + 8, r->item.parent_id)) duplicate = 1;
                if (duplicate) continue;
                char id[160]; snprintf(id, sizeof id, "channel:%.24s", r->item.parent_id);
                folder(&out[n++], id, r->item.subtitle, "Followed locally on this PS5");
            } else out[n++] = r->item;
        }
        cb(1, out, n, (page + 1) * LOCAL_PAGE < total, ud); return 1;
    } else return 0;
    cb(1, out, n, 0, ud); return 1;
}

static int catalog(const char *parent, int page, evo_provider_items_cb cb, void *ud) {
    if (!cb || page < 0 || page > 1000) return -1;
    if (parent && !strncmp(parent, "auth:", 5)) {
        if (page || (strcmp(parent, "auth:start") && strcmp(parent, "auth:session") &&
            strcmp(parent, "auth:cancel") && strncmp(parent, "auth:info", 9))) {
            tube_set_error("The sign-in screen was invalid."); cb(0, NULL, 0, 0, ud); return 0;
        }
        if (!strcmp(parent, "auth:cancel")) { tube_cancel(); return auth_rows(cb, ud); }
        if (strcmp(parent, "auth:start") || g_auth.state == AUTH_STARTING || g_auth.state == AUTH_PENDING)
            return auth_rows(cb, ud);
        tube_cancel();
        unsigned serial = g_auth.serial + 1, revision = g_auth.revision + 1;
        memset(&g_auth, 0, sizeof g_auth); g_auth.serial = serial ? serial : 1;
        g_auth.revision = revision; g_auth.state = AUTH_STARTING;
        Request *r = request_new(9, ud);
        if (!r) { g_auth.state = AUTH_FAILED; ++g_auth.revision; cb(0, NULL, 0, 0, ud); return 0; }
        r->items_cb = cb; r->intent = g_auth.serial;
        copy(r->payload, sizeof r->payload, "{\"action\":\"auth\",\"auth\":{\"action\":\"start\"}}");
        return begin(r);
    }
    if (parent && (!strncmp(parent, "account:", 8) ||
        (!strncmp(parent, "playlist:", 9) && strchr(parent + 9, ':') && strncmp(parent, "playlist:pick:", 14))) && page != 0) {
        tube_set_error("Account action screens do not have additional pages."); cb(0, NULL, 0, 0, ud); return 0;
    }
    if (parent && !strncmp(parent, "account:apply:", 14) && g_account_intent.state)
        return local_catalog(parent, page, cb, ud) ? 0 : -1;
    tube_cancel();
    if (local_catalog(parent, page, cb, ud)) return 0;
    if (parent && !strncmp(parent, "account:apply:", 14)) {
        Request *r = request_new(6, ud); if (!r) return -1;
        r->items_cb = cb; r->intent = g_account_intent.serial;
        g_account_intent.state = 1;
        cJSON *job = cJSON_CreateObject(), *operation = cJSON_CreateObject();
        if (!job || !operation) { cJSON_Delete(job); cJSON_Delete(operation); complete(r, 0); return -1; }
        cJSON_AddStringToObject(job, "action", "library"); cJSON_AddItemToObject(job, "library", operation);
        cJSON_AddStringToObject(operation, "action", g_account_intent.action);
        cJSON_AddStringToObject(operation, "profile", g_account_intent.profile);
        cJSON_AddStringToObject(operation, "id", g_account_intent.identity);
        cJSON_AddStringToObject(operation, "playlist", g_account_intent.playlist);
        cJSON_AddStringToObject(operation, "title", g_account_intent.title);
        char *body = cJSON_PrintUnformatted(job); cJSON_Delete(job);
        if (!body || strlen(body) >= sizeof r->payload) { free(body); complete(r, 0); return -1; }
        copy(r->payload, sizeof r->payload, body); free(body);
        begin(r); return 0;
    }
    if (parent && !strncmp(parent, "playlist:pick:", 14)) {
        int add = !strncmp(parent, "playlist:pick:add:", 18), remove = !strncmp(parent, "playlist:pick:remove:", 21);
        const char *video = add ? parent + 18 : remove ? parent + 21 : "";
        if ((!add && !remove) || !valid_id(video) || !strcmp(g_library_profile, "guest")) {
            tube_set_error("Select a video and an account profile first."); cb(0, NULL, 0, 0, ud); return 0;
        }
        if (page >= LIBRARY_PAGES || (page > 0 && (strcmp(parent, g_library_parent) || !*g_library_cursors[page]))) {
            tube_set_error("Reload the playlist picker to continue."); cb(0, NULL, 0, 0, ud); return 0;
        }
        if (!page) { copy(g_library_parent, sizeof g_library_parent, parent); memset(g_library_cursors, 0, sizeof g_library_cursors); }
        Request *r = request_new(7, ud); if (!r) return -1;
        r->items_cb = cb; r->page = page; copy(r->parent, sizeof r->parent, parent);
        snprintf(r->payload, sizeof r->payload, "{\"action\":\"library\",\"library\":{\"action\":\"%s\",\"profile\":\"%s\",\"cursor\":\"%s\"}}",
            page ? "continue" : "playlists", g_library_profile, page ? g_library_cursors[page] : "");
        begin(r); return 0;
    }
    if (parent && !strncmp(parent, "discussion:", 11)) {
        const char *action = parent + 11, *identity = NULL, *sort = "top";
        int replies = !strncmp(action, "replies:", 8);
        if (replies) identity = action + 8;
        else if (!strncmp(action, "top:", 4)) identity = action + 4;
        else if (!strncmp(action, "newest:", 7)) { identity = action + 7; sort = "newest"; }
        if (!identity || (replies ? !valid_cursor(identity) : !valid_id(identity))) {
            tube_set_error("The selected discussion was invalid."); cb(0, NULL, 0, 0, ud); return 0;
        }
        if (page >= LIBRARY_PAGES || (page > 0 && (strcmp(parent, g_library_parent) || !*g_library_cursors[page]))) {
            tube_set_error("Reload the discussion to continue reading."); cb(0, NULL, 0, 0, ud); return 0;
        }
        if (page == 0) { copy(g_library_parent, sizeof g_library_parent, parent); memset(g_library_cursors, 0, sizeof g_library_cursors); }
        Request *r = request_new(5, ud); if (!r) return -1;
        r->items_cb = cb; r->page = page; copy(r->parent, sizeof r->parent, parent);
        snprintf(r->payload, sizeof r->payload,
            "{\"action\":\"library\",\"library\":{\"action\":\"%s\",\"profile\":\"%s\",\"sort\":\"%s\",\"cursor\":\"%s\",\"id\":\"%s\"}}",
            page ? "continue" : replies ? "replies" : "comments", g_library_profile, sort,
            page ? g_library_cursors[page] : replies ? identity : "", replies ? "" : identity);
        begin(r); return 0;
    }
    if (parent && !strncmp(parent, "library:", 8)) {
        const char *action = parent + 8;
        int playlist = !strncmp(action, "playlist:", 9) && valid_catalog_id(parent);
        if (strcmp(action, "home") && strcmp(action, "subscriptions") && strcmp(action, "history") &&
            strcmp(action, "watch_later") && strcmp(action, "playlists") && strcmp(action, "profiles") && !playlist) {
            tube_set_error("The selected library screen is unavailable."); cb(0, NULL, 0, 0, ud); return 0;
        }
        if (page >= LIBRARY_PAGES || (page > 0 && (strcmp(parent, g_library_parent) || !*g_library_cursors[page]))) {
            tube_set_error("Reload this library screen to continue browsing."); cb(0, NULL, 0, 0, ud); return 0;
        }
        if (page == 0) { copy(g_library_parent, sizeof g_library_parent, parent); memset(g_library_cursors, 0, sizeof g_library_cursors); }
        Request *r = request_new(!strcmp(action, "profiles") ? 4 : 3, ud); if (!r) return -1;
        r->items_cb = cb; r->page = page; copy(r->parent, sizeof r->parent, parent);
        snprintf(r->payload, sizeof r->payload,
            "{\"action\":\"library\",\"library\":{\"action\":\"%s\",\"profile\":\"%s\",\"cursor\":\"%s\",\"id\":\"%s\"}}",
            page ? "continue" : playlist ? "playlist" : action, g_library_profile,
            page ? g_library_cursors[page] : "", playlist ? action + 9 : "");
        begin(r); return 0;
    }
    Request *r = request_new(1, ud); if (!r) return -1;
    r->items_cb = cb;
    cJSON *o = cJSON_CreateObject();
    int search = parent && !strncmp(parent, "search:", 7);
    cJSON_AddStringToObject(o, "action", search ? "search" : "catalog");
    cJSON_AddStringToObject(o, "profile", g_library_profile);
    cJSON_AddStringToObject(o, search ? "query" : "parent", search ? parent + 7 : parent);
    cJSON_AddNumberToObject(o, "page", page);
    char *body = cJSON_PrintUnformatted(o); cJSON_Delete(o);
    if (!body) { memset(r, 0, sizeof *r); return -1; }
    copy(r->payload, sizeof r->payload, body); free(body);
    begin(r); return 0;
}

static int search(const char *query, int page, evo_provider_items_cb cb, void *ud) {
    if (!query || strlen(query) > 200 || !cb) return -1;
    char parent[208]; snprintf(parent, sizeof parent, "search:%s", query);
    return catalog(parent, page, cb, ud);
}

static int resolve(const char *id, evo_provider_resolve_cb cb, void *ud) {
    if (!valid_id(id) || !cb) return -1;
    tube_cancel();
    Request *r = request_new(2, ud); if (!r) return -1;
    r->resolve_cb = cb;
    copy(r->parent, sizeof r->parent, id); copy(r->profile, sizeof r->profile, g_library_profile);
    snprintf(r->payload, sizeof r->payload,
        "{\"action\":\"resolve\",\"playback\":{\"id\":\"%s\",\"profile\":\"%s\",\"max_height\":%d,\"codec\":\"%s\",\"caption_language\":\"%s\",\"audio_language\":\"%s\",\"automatic_captions\":%s}}",
        id, g_library_profile, g_height, g_codec == 1 ? "avc" : g_codec == 2 ? "vp9" : "auto", g_language, g_audio_language, g_auto_captions ? "true" : "false");
    begin(r); return 0;
}

static void progress(const char *id, int64_t pos, int64_t dur, evo_provider_play_state_t state) {
    if (*g_active_id && strcmp(id, g_active_id)) return;
    const char *profile = *g_active_profile ? g_active_profile : g_library_profile;
    Record *r = record_for(id, profile, 1); if (!r) return;
    if (dur > 0) r->item.duration_sec = dur;
    if (state != EVO_PROVIDER_PLAY_START || pos > 0) r->item.resume_pos_sec = pos > 0 ? pos : 0;
    r->updated = (long long)time(NULL);
    if (dur > 0 && pos >= dur * 0.95 && pos >= dur - 10) { r->item.played = 1; r->item.resume_pos_sec = 0; }
    if (state == EVO_PROVIDER_PLAY_START) { g_finished = 0; g_skip_guard = -1; }
    if (!strcmp(id, g_watch.id) && !g_watch.failed && !g_watch.stopped && pos >= 0 && pos <= 1000000 &&
        (state == EVO_PROVIDER_PLAY_UPDATE || state == EVO_PROVIDER_PLAY_STOP)) {
        g_watch.position = pos; g_watch.stopped = state == EVO_PROVIDER_PLAY_STOP;
        g_watch.dirty = 1; watch_flush(0);
    }
    if (state == EVO_PROVIDER_PLAY_STOP || now_ms() - g_last_save >= 10000) save();
}

static void chosen(const char *id, const evo_stream_choice_t *choice) {
    (void)choice; copy(g_active_id, sizeof g_active_id, id); g_finished = 0; g_skip_guard = -1;
    char profile[33]; copy(profile, sizeof profile, !strcmp(id, g_resolved_id) ? g_resolved_profile : "");
    copy(g_active_profile, sizeof g_active_profile, valid_profile(profile) ? profile : g_library_profile);
    if (g_watch.stopped && g_watch.dirty) watch_flush(1);
    watch_reset(0);
    if (valid_id(id) && valid_profile(profile) && strcmp(profile, "guest")) {
        copy(g_watch.id, sizeof g_watch.id, id); copy(g_watch.profile, sizeof g_watch.profile, profile);
        snprintf(g_watch.session, sizeof g_watch.session, "%llx-%lx-%u", (unsigned long long)time(NULL), (unsigned long)getpid(), g_watch.serial);
    }
}

int tube_toggle_favorite(const char *id) {
    Record *r = record(id, 1); if (!r) return -1;
    r->favorite = !r->favorite; save(); return r->favorite;
}

int tube_toggle_queue(const char *id) {
    Record *r = record(id, 1); if (!r) return -1;
    r->queued = !r->queued; if (r->queued) r->queue_order = ++g_queue_order;
    save(); return r->queued;
}

int tube_toggle_subscription(const char *id) {
    Record *r = record(id, 1); if (!r || strncmp(r->item.parent_id, "UC", 2) || strlen(r->item.parent_id) != 24) return -1;
    int subscribed = 0;
    for (int i = 0; i < g_count; ++i)
        if (current_record(&g_records[i]) && !strcmp(g_records[i].item.parent_id, r->item.parent_id) && g_records[i].subscribed) subscribed = 1;
    int follow = !subscribed;
    for (int i = 0; i < g_count; ++i)
        if (current_record(&g_records[i]) && !strcmp(g_records[i].item.parent_id, r->item.parent_id)) g_records[i].subscribed = follow;
    save(); return follow;
}

const char *tube_caption_media_path(void) { return g_caption_path; }
int tube_autoplay_enabled(void) { return g_autoplay; }

double tube_skip_target(double position) {
    if (!g_sponsor || !isfinite(position) || position < 0) return -1;
    if (g_skip_guard >= 0 && position < g_segments[g_skip_guard][0] - 2) g_skip_guard = -1;
    if (g_skip_guard >= 0 && position < g_skip_until + .25) return -1;
    for (int i = 0; i < g_segment_count; ++i) {
        if (i != g_skip_guard && position >= g_segments[i][0] && position < g_segments[i][1] - .25) {
            double end = g_segments[i][1];
            for (int pass = 0; pass < g_segment_count; ++pass) {
                double previous = end;
                for (int j = 0; j < g_segment_count; ++j)
                    if (g_segments[j][0] <= end && g_segments[j][1] > end) end = g_segments[j][1];
                if (end == previous) break;
            }
            g_skip_guard = i; g_skip_until = end; return end;
        }
    }
    return -1;
}

double tube_chapter_target(double position, int direction) {
    if (!isfinite(position) || !g_chapter_count) return -1;
    double target = -1;
    for (int i = 0; i < g_chapter_count; ++i) {
        double start = g_chapters[i];
        if (direction > 0 && start > position + 1 && (target < 0 || start < target)) target = start;
        if (direction < 0 && start < position - 3 && start > target) target = start;
    }
    return target;
}

int tube_playback_at_end(double position, double duration) {
    if (!isfinite(position) || position < 0 || !isfinite(duration)) return 0;
    const Record *r = record_for(g_active_id, g_active_profile, 0);
    if (duration <= 0 && r) duration = (double)r->item.duration_sec;
    /* EVO's EOF flag means input was exhausted, including while several
     * seconds remain buffered. Do not mark watched or advance the queue then.
     * Unknown-duration streams require a separate presentation-drain signal. */
    return duration > 0 && position >= duration - fmin(0.1, duration * 0.01);
}

static int finish_and_next(void) {
    if (g_finished || !*g_active_id) return 0;
    g_finished = 1;
    Record *r = record_for(g_active_id, g_active_profile, 0);
    if (r) { r->queued = 0; r->item.played = 1; r->item.resume_pos_sec = 0; save(); }
    if (!g_autoplay || strcmp(g_active_profile, g_library_profile)) return 0;
    Record *next = NULL;
    for (int i = 0; i < g_count; ++i)
        if (current_record(&g_records[i]) && g_records[i].queued && (!next || g_records[i].queue_order < next->queue_order)) next = &g_records[i];
    if (!next) return 0;
    copy(g_next_id, sizeof g_next_id, next->item.id);
    copy(g_next_profile, sizeof g_next_profile, next->profile); return 1;
}

int tube_finish_and_next(double position, double duration) {
    return tube_playback_at_end(position, duration) ? finish_and_next() : 0;
}

int tube_finish_drained_and_next(double position) {
    /* Called only with the engine's natural EOF + audio/video retirement proof.
     * Unknown/inaccurate metadata duration must not prevent a clean completion. */
    return isfinite(position) && position >= 0 ? finish_and_next() : 0;
}

int tube_take_next(evo_provider_item_t *item) {
    if (!*g_next_id || strcmp(g_next_profile, g_library_profile)) { g_next_id[0] = 0; return 0; }
    Record *r = record_for(g_next_id, g_next_profile, 0); g_next_id[0] = 0; g_next_profile[0] = 0;
    if (!r || !item) return 0;
    *item = r->item; return 1;
}

int tube_choose_stream_enabled(void) { return g_ask_stream; }

static void tube_shutdown(void) { tube_cancel(); save(); }
const evo_provider_t evo_provider_youtube = {
    .id = "youtube", .name = "ProsperoTube", .icon = "icon_emby.png",
    .caps = EVO_PROVIDER_CAP_CATALOG | EVO_PROVIDER_CAP_SEARCH | EVO_PROVIDER_CAP_RESOLVE |
            EVO_PROVIDER_CAP_PROGRESS | EVO_PROVIDER_CAP_CONFIG | EVO_PROVIDER_CAP_PICK,
    .api_version = EVO_PROVIDER_API_VERSION,
    .init = init, .shutdown = tube_shutdown, .is_configured = configured,
    .list_catalog = catalog, .search = search, .resolve = resolve,
    .report_progress = progress, .stream_chosen = chosen,
    .get_source = get_source, .set_source = set_source, .ui_embedded = "youtube"
};
