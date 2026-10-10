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
#include "tube_sponsor.h"

#define RECORDS 384
#define REQUESTS 8
#define CHOICES 8
#define SEGMENTS 128
#define LOCAL_PAGE 24
#define CHAT_HISTORY 256
#define LIBRARY_PAGES 1001

typedef struct {
    evo_provider_item_t item;
    char profile[33];
    char dearrow_title[256], dearrow_art[EVO_PROVIDER_MAX_URL];
    int favorite, queued, subscribed, queued_music;
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
    int page, music_origin;
    unsigned intent;
    char parent[160];
    char profile[33];
    int64_t watch_position;
    int has_chat_position;
    int64_t chat_position_ms;
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
/* Navigation origin, not a content classifier. Resolve snapshots the origin
 * before asynchronous work; queue entries retain the origin of their insertion. */
static int g_browse_music, g_browse_queue, g_resolved_music;
static struct { char id[160], profile[33]; int music; } g_launch_origin;
static int g_height = 1080, g_codec, g_auto_captions, g_captions = 1;
static int g_speed = 100;
/* Modes: all videos, per channel, per video, this session only. */
static int g_speed_mode = 1, g_last_speed = 100;
#define SPEED_MEMORIES 1024
static struct { char profile[33], key[25]; int mode, percent; } g_speeds[SPEED_MEMORIES];
static int g_speed_count;
static char g_speed_session_id[160], g_speed_session_profile[33];
static int g_speed_session = 100;
static const int tube_speeds[] = {25,50,75,80,85,90,95,100,105,110,115,120,125,130,135,140,145,150,160,175,180,200,225,250,275,300,325,350,375,400};
static int valid_speed(double speed) {
    for (unsigned i = 0; i < sizeof tube_speeds / sizeof *tube_speeds; ++i)
        if (speed == tube_speeds[i]) return 1;
    return 0;
}
int tube_playback_speed(void) { return g_speed; }
int tube_speed_count(void) { return (int)(sizeof tube_speeds / sizeof *tube_speeds); }
int tube_speed_at(int index) { return index >= 0 && index < tube_speed_count() ? tube_speeds[index] : 0; }
static void save(void);
int tube_set_playback_speed(int percent) {
    if (!valid_speed(percent)) return 0;
    if (percent != 100) g_last_speed = percent;
    else if (g_speed != 100) g_last_speed = g_speed;
    g_speed = percent; save(); return 1;
}
int tube_last_playback_speed(void) { return g_last_speed; }
static int g_sponsor, g_autoplay, g_finished, g_ask_stream;
static int g_player_chat_mode;
static tube_player_chat_row_t g_player_chat_rows[6];
static int g_player_chat_count;
static double g_player_chat_position = -1;
static int g_history_sync = 1;
static int g_paid_notifications;
static int g_dearrow_titles, g_dearrow_thumbnails;
static char g_paid_text[256];
static int g_paid_shown;
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
    if (clear_resolution) {
        g_resolved_id[0] = 0; g_resolved_profile[0] = 0; g_resolved_music = 0;
        g_browse_music = g_browse_queue = 0;
        memset(&g_launch_origin, 0, sizeof g_launch_origin);
    }
}
static char g_language[16] = "en", g_audio_language[16];
static double g_segments[SEGMENTS][2];
static unsigned char g_segment_categories[SEGMENTS], g_segment_seen[SEGMENTS];
static tube_sb_policy g_sb;
static unsigned g_sb_serial, g_sb_active_serial;
static char g_sb_source[512];
static char g_sb_channel[25], g_sb_profile[33], g_sb_prompt_text[128];
static int g_sb_prompt = -1, g_sb_message;
static double g_sb_prompt_start, g_sb_prompt_end, g_sb_last_position = -1, g_sb_finish_target = -1;
static int g_highlight_guard = -1;
static double g_highlight_origin;
static int g_segment_count, g_skip_guard = -1;
static double g_skip_until;
static double g_chapters[128];
static int g_chapter_count;
static void sponsor_invalidate(void) {
    g_paid_text[0] = 0; g_paid_shown = 0;
    if (!++g_sb_serial) ++g_sb_serial;
    g_sb_active_serial = 0; g_sb_source[0] = 0; g_segment_count = 0;
    g_sb_prompt = -1; g_skip_guard = -1; g_sb_finish_target = -1;
    g_highlight_guard = -1;
}
static long long g_last_save, g_queue_order;
static char g_library_profile[33] = "guest", g_library_parent[160];
static struct { int phase, attempts, subtitles; double position;
    char source[512], token[128], profile[33], id[160];
    evo_stream_choice_t choice; } g_recovery;
void tube_recovery_finish(void) { g_recovery.phase = 0; }
static int recovery_scope(void) {
    return *g_recovery.id && !strcmp(g_recovery.id, g_active_id) &&
        !strcmp(g_recovery.profile, g_active_profile) && !strcmp(g_recovery.profile, g_library_profile) &&
        !strcmp(g_recovery.source, g_source) && !strcmp(g_recovery.token, g_token);
}
int tube_recovery_active(void) { return g_recovery.phase == 2 && recovery_scope(); }
int tube_recovery_pending(void) { return g_recovery.phase == 1 && recovery_scope(); }
static char g_library_cursors[LIBRARY_PAGES][65];
static struct { char id[160], content[16385], rating[65], profile[33]; unsigned rating_options; } g_comments[LOCAL_PAGE + 1];
static struct { char id[160], content[16385]; } g_chat_messages[CHAT_HISTORY];
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
static void chat_cleanup_request(Request *r, const cJSON *root);
static void cancel_requests(int preserve_chat);
static void chat_close_remote(const char *source, const char *token, const char *profile, const char *cursor);
static struct { int state, player, replay, seek; unsigned serial; long long next_poll;
    int64_t position_ms, seek_position_ms;
    char parent[160], video[12], mode[8], profile[33], cursor[65], source[512], token[128]; } g_chat;
static struct { int phase, pending; unsigned ticket, chat_serial;
    char text[801], session[65], cursor[65], video[12], profile[33]; } g_message;
static int message_scope(void) {
    return g_message.ticket && g_message.chat_serial == g_chat.serial && g_chat.state == 2 &&
        !strcmp(g_message.profile, g_library_profile) && !strcmp(g_message.profile, g_chat.profile) &&
        !strcmp(g_message.video, g_chat.video) && !strcmp(g_message.cursor, g_chat.cursor) &&
        !strcmp(g_chat.source, g_source) && !strcmp(g_chat.token, g_token);
}
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
    if (id) for (int i = 0; i < CHAT_HISTORY; ++i)
        if (*g_chat_messages[i].id && !strcmp(id, g_chat_messages[i].id)) return g_chat_messages[i].content;
    if (id) for (int i = 0; i <= LOCAL_PAGE; ++i)
        if (*g_comments[i].id && !strcmp(id, g_comments[i].id)) return g_comments[i].content;
    return "";
}

const char *tube_comment_rating(const char *id) {
    if (id && strcmp(g_library_profile, "guest")) for (int i = 0; i < LOCAL_PAGE; ++i)
        if (!strcmp(g_comments[i].id, id) && !strcmp(g_comments[i].profile, g_library_profile) && g_comments[i].rating_options)
            return g_comments[i].rating;
    return "";
}

static long long now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

int tube_chat_due(const char *parent) {
    if (!parent || g_chat.state != 2 || strcmp(parent, g_chat.parent) ||
        strcmp(g_chat.profile, g_library_profile) || !valid_cursor(g_chat.cursor) || now_ms() < g_chat.next_poll) return 0;
    for (int i = 0; i < REQUESTS; ++i) if (g_requests[i].used && g_requests[i].kind == 10) return 0;
    return 1;
}

void tube_chat_stop(void) {
    g_player_chat_position = -1;
    memset(g_player_chat_rows, 0, sizeof g_player_chat_rows); g_player_chat_count = 0;
    memset(g_chat_messages, 0, sizeof g_chat_messages);
    chat_close_remote(g_chat.source, g_chat.token, g_chat.profile, g_chat.cursor);
    unsigned serial = g_chat.serial + 1;
    memset(&g_chat, 0, sizeof g_chat); g_chat.serial = serial ? serial : 1;
    memset(&g_message, 0, sizeof g_message);
    memset(&g_comments[LOCAL_PAGE], 0, sizeof g_comments[LOCAL_PAGE]);
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

static const char *topic_section(const char *id) {
    static const char *const names[] = {"music", "gaming", "news", "sports", "live", "movies"};
    if (!id || strncmp(id, "section:", 8)) return NULL;
    for (unsigned i = 0; i < sizeof names / sizeof *names; ++i) {
        size_t n = strlen(names[i]);
        if (!strncmp(id + 8, names[i], n) &&
            (!id[8 + n] || (id[8 + n] == ':' && valid_cursor(id + 9 + n)))) return names[i];
    }
    return NULL;
}

static int valid_catalog_id(const char *id) {
    if (topic_section(id)) return 1;
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

int tube_take_chat_prompt(tube_chat_prompt_t *prompt) {
    if (!prompt || !g_message.pending || g_message.phase != 1 || !message_scope()) return 0;
    memset(prompt, 0, sizeof *prompt); prompt->ticket = g_message.ticket;
    copy(prompt->label, sizeof prompt->label, "YouTube live-chat message");
    g_message.pending = 0; return 1;
}

void tube_cancel_chat_prompt(unsigned ticket) {
    if (ticket == g_message.ticket && g_message.phase == 1) memset(&g_message, 0, sizeof g_message);
}

int tube_submit_chat_message(unsigned ticket, const char *text, char *parent, size_t capacity) {
    if (ticket != g_message.ticket || g_message.phase != 1 || !message_scope() || !text || !parent || capacity < 48) return -1;
    size_t length = strnlen(text, 801), points = 0; int visible = 0;
    if (!length || length > 800) return -1;
    for (size_t i = 0; i < length;) {
        unsigned char c = (unsigned char)text[i];
        size_t n = c < 0x80 ? 1 : c >= 0xc2 && c <= 0xdf ? 2 : c >= 0xe0 && c <= 0xef ? 3 : c >= 0xf0 && c <= 0xf4 ? 4 : 0;
        if (!n || c < 0x20 || c == 0x7f || i + n > length || ++points > 200) return -1;
        for (size_t k = 1; k < n; ++k) if (((unsigned char)text[i+k] & 0xc0) != 0x80) return -1;
        unsigned char next = n > 1 ? (unsigned char)text[i+1] : 0;
        if ((c == 0xe0 && next < 0xa0) || (c == 0xed && next >= 0xa0) ||
            (c == 0xf0 && next < 0x90) || (c == 0xf4 && next >= 0x90)) return -1;
        unsigned cp = c & (n == 1 ? 0x7f : n == 2 ? 0x1f : n == 3 ? 0x0f : 0x07);
        for (size_t k = 1; k < n; ++k) cp = (cp << 6) | ((unsigned char)text[i+k] & 0x3f);
        if (cp != 0x20 && cp != 0x85 && cp != 0xa0 && cp != 0x1680 &&
            !(cp >= 0x2000 && cp <= 0x200a) && cp != 0x2028 && cp != 0x2029 &&
            cp != 0x202f && cp != 0x205f && cp != 0x3000) visible = 1;
        i += n;
    }
    if (!visible) return -1;
    copy(g_message.text, sizeof g_message.text, text); g_message.phase = 2;
    snprintf(parent, capacity, "message:review:%u", ticket); return 0;
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

static const char *speed_profile(const char *id) {
    return id && !strcmp(id, g_active_id) && valid_profile(g_active_profile) ? g_active_profile : g_library_profile;
}
static const char *speed_key(const char *id, const char *profile) {
    if (!valid_id(id)) return "";
    if (g_speed_mode == 2) return id;
    const Record *r = record_for(id, profile, 0);
    /* Channel identity is public video metadata; memory remains profile-keyed. */
    if (!r) for (int i = 0; i < g_count; ++i)
        if (!strcmp(g_records[i].item.id, id)) { r = &g_records[i]; break; }
    return r && tube_sb_channel_valid(r->item.parent_id) ? r->item.parent_id : "";
}
static int speed_lookup(const char *id, const char *profile) {
    if (g_speed_mode == 0) return g_speed;
    if (g_speed_mode == 3) return 100;
    const char *key = speed_key(id, profile);
    for (int i = 0; *key && i < g_speed_count; ++i)
        if (g_speeds[i].mode == g_speed_mode && !strcmp(g_speeds[i].profile, profile) && !strcmp(g_speeds[i].key, key))
            return g_speeds[i].percent;
    return 100;
}
int tube_playback_speed_for(const char *id) {
    if (!id || !valid_id(id)) return 100;
    const char *profile = speed_profile(id);
    if (!strcmp(id, g_speed_session_id) && !strcmp(profile, g_speed_session_profile)) return g_speed_session;
    return speed_lookup(id, profile);
}
int tube_remember_playback_speed(const char *id, int percent) {
    if (!valid_id(id) || !valid_speed(percent)) return 0;
    const int previous = tube_playback_speed_for(id);
    const char *profile = speed_profile(id);
    const char *key = speed_key(id, profile);
    if ((g_speed_mode == 1 || g_speed_mode == 2) && *key) {
        int slot = -1;
        for (int i = 0; i < g_speed_count; ++i)
            if (g_speeds[i].mode == g_speed_mode && !strcmp(g_speeds[i].profile, profile) && !strcmp(g_speeds[i].key, key)) { slot = i; break; }
        /* Normal speed removes an override. The bounded list retains the most
         * recently changed entries and never evicts library/favorite records. */
        if (slot >= 0) {
            memmove(&g_speeds[slot], &g_speeds[slot + 1], (size_t)(g_speed_count - slot - 1) * sizeof g_speeds[0]);
            --g_speed_count;
        }
        if (percent != 100) {
            if (g_speed_count == SPEED_MEMORIES) {
                memmove(g_speeds, g_speeds + 1, (SPEED_MEMORIES - 1) * sizeof g_speeds[0]); --g_speed_count;
            }
            slot = g_speed_count++;
            copy(g_speeds[slot].profile, sizeof g_speeds[slot].profile, profile);
            copy(g_speeds[slot].key, sizeof g_speeds[slot].key, key);
            g_speeds[slot].mode = g_speed_mode; g_speeds[slot].percent = percent;
        }
    }
    copy(g_speed_session_id, sizeof g_speed_session_id, id);
    copy(g_speed_session_profile, sizeof g_speed_session_profile, profile);
    g_speed_session = percent;
    g_speed = previous;
    return tube_set_playback_speed(percent);
}

static void speed_load(const cJSON *root) {
    g_speed_count = 0; g_speed_session_id[0] = g_speed_session_profile[0] = 0;
    double last = num(root, "last_speed_percent"), mode = num(root, "speed_memory_mode");
    g_last_speed = valid_speed(last) ? (int)last : (g_speed != 100 ? g_speed : 100);
    const cJSON *setting = cJSON_GetObjectItemCaseSensitive(root, "speed_memory_mode");
    g_speed_mode = setting && cJSON_IsNumber(setting) && mode >= 0 && mode <= 3 && mode == (int)mode ? (int)mode :
        cJSON_HasObjectItem(root, "speed_percent") ? 0 : 1;
    const cJSON *rows = cJSON_GetObjectItemCaseSensitive(root, "speed_memories"), *row;
    if (!cJSON_IsArray(rows)) return;
    cJSON_ArrayForEach(row, rows) {
        const char *profile = str(row, "profile"), *key = str(row, "key");
        double kind = num(row, "mode"), percent = num(row, "percent");
        if (!valid_profile(profile) || !valid_speed(percent) || percent == 100 ||
            !((kind == 1 && tube_sb_channel_valid(key)) || (kind == 2 && valid_id(key)))) continue;
        int duplicate = 0;
        for (int i = 0; i < g_speed_count; ++i)
            if (g_speeds[i].mode == kind && !strcmp(g_speeds[i].key, key) && !strcmp(g_speeds[i].profile, profile)) duplicate = 1;
        if (duplicate) continue;
        if (g_speed_count == SPEED_MEMORIES) break;
        int i = g_speed_count++;
        copy(g_speeds[i].profile, sizeof g_speeds[i].profile, profile);
        copy(g_speeds[i].key, sizeof g_speeds[i].key, key);
        g_speeds[i].mode = (int)kind; g_speeds[i].percent = (int)percent;
    }
}

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
    cJSON_AddNumberToObject(root, "speed_percent", g_speed);
    cJSON_AddNumberToObject(root, "last_speed_percent", g_last_speed);
    cJSON_AddNumberToObject(root, "speed_memory_mode", g_speed_mode);
    cJSON *speeds = cJSON_AddArrayToObject(root, "speed_memories");
    for (int i = 0; speeds && i < g_speed_count; ++i) {
        cJSON *row = cJSON_CreateObject(); if (!row) break;
        cJSON_AddStringToObject(row, "profile", g_speeds[i].profile);
        cJSON_AddStringToObject(row, "key", g_speeds[i].key);
        cJSON_AddNumberToObject(row, "mode", g_speeds[i].mode);
        cJSON_AddNumberToObject(row, "percent", g_speeds[i].percent);
        cJSON_AddItemToArray(speeds, row);
    }
    cJSON_AddNumberToObject(root, "codec", g_codec);
    cJSON_AddBoolToObject(root, "captions", g_captions);
    cJSON_AddBoolToObject(root, "automatic_captions", g_auto_captions);
    cJSON_AddStringToObject(root, "language", g_language);
    cJSON_AddStringToObject(root, "audio_language", g_audio_language);
    cJSON_AddStringToObject(root, "library_profile", g_library_profile);
    cJSON_AddBoolToObject(root, "sponsorblock", g_sponsor);
    cJSON_AddNumberToObject(root, "player_chat_mode", g_player_chat_mode);
    cJSON_AddBoolToObject(root, "paid_content_notifications", g_paid_notifications);
    cJSON_AddBoolToObject(root, "dearrow_titles", g_dearrow_titles);
    cJSON_AddBoolToObject(root, "dearrow_thumbnails", g_dearrow_thumbnails);
    tube_sb_save(&g_sb, root);
    cJSON_AddBoolToObject(root, "autoplay", g_autoplay);
    cJSON_AddBoolToObject(root, "ask_stream", g_ask_stream);
    cJSON_AddBoolToObject(root, "history_sync", g_history_sync);
    for (int i = 0; i < g_count; ++i) {
        Record *r = &g_records[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "id", r->item.id);
        cJSON_AddStringToObject(o, "profile", r->profile);
        cJSON_AddStringToObject(o, "title", r->item.title);
        cJSON_AddStringToObject(o, "dearrow_title", r->dearrow_title);
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
        cJSON_AddBoolToObject(o, "queued_music", r->queued && r->queued_music);
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
    tube_recovery_finish();
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
    tube_sb_defaults(&g_sb);
    sponsor_invalidate(); g_sb_last_position = -1;
    tube_chat_stop();
    tube_auth_cancel();
    unsigned auth_serial = g_auth.serial + 1, auth_revision = g_auth.revision + 1;
    memset(&g_auth, 0, sizeof g_auth);
    g_auth.serial = auth_serial ? auth_serial : 1; g_auth.revision = auth_revision;
    g_paid_notifications = 0; g_dearrow_titles = 0; g_dearrow_thumbnails = 0;
    g_speed = g_last_speed = 100; g_speed_mode = 1; g_speed_count = 0;
    g_speed_session_id[0] = g_speed_session_profile[0] = 0;
    g_browse_music = g_browse_queue = 0;
    g_store_writable = 0;
    watch_reset(1);
    g_active_id[0] = 0; g_active_profile[0] = 0; g_next_id[0] = 0; g_next_profile[0] = 0;
    memset(&g_recovery, 0, sizeof g_recovery);
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
    double speed = num(root, "speed_percent");
    g_speed = valid_speed(speed) ? (int)speed : 100;
    speed_load(root);
    int codec = (int)num(root, "codec"); if (codec <= 2) g_codec = codec;
    g_captions = boolean(root, "captions");
    g_auto_captions = boolean(root, "automatic_captions");
    g_sponsor = boolean(root, "sponsorblock");
    double chat_mode = num(root, "player_chat_mode");
    g_player_chat_mode = chat_mode >= 0 && chat_mode <= 2 && floor(chat_mode) == chat_mode ? (int)chat_mode : 0;
    g_paid_notifications = boolean(root, "paid_content_notifications");
    g_dearrow_titles = boolean(root, "dearrow_titles");
    g_dearrow_thumbnails = boolean(root, "dearrow_thumbnails");
    tube_sb_load(&g_sb, root);
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
        copy(r->dearrow_title, sizeof r->dearrow_title, str(o, "dearrow_title"));
        r->dearrow_art[0] = 0; /* Expiring thumbnail capabilities are never persisted. */
        r->item.resume_pos_sec = (int64_t)num(o, "position");
        r->item.duration_sec = (int64_t)num(o, "duration");
        r->item.played = boolean(o, "played");
        r->updated = (long long)num(o, "updated");
        r->queue_order = (long long)num(o, "queue_order");
        if (r->queue_order > g_queue_order) g_queue_order = r->queue_order;
        r->favorite = boolean(o, "favorite"); r->queued = boolean(o, "queued");
        r->queued_music = r->queued && boolean(o, "queued_music");
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
    sponsor_invalidate();
    for (int i = 0; i < g_count; ++i) { g_records[i].dearrow_title[0] = 0; g_records[i].dearrow_art[0] = 0; }
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
    if (r->kind == 11 && r->intent == g_message.ticket && g_message.phase == 3) g_message.phase = -1;
    if (r->kind == 10 && !ok) {
        chat_cleanup_request(r, NULL);
        if (r->intent == g_chat.serial) {
            chat_close_remote(g_chat.source, g_chat.token, g_chat.profile, g_chat.cursor);
            g_chat.cursor[0] = 0; g_chat.state = -1;
        }
    }
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

static void dearrow_item(evo_provider_item_t *item, const Record *cached) {
    if (!cached || item->is_folder) return;
    if (g_dearrow_titles && *cached->dearrow_title) copy(item->title, sizeof item->title, cached->dearrow_title);
    if (g_dearrow_thumbnails && companion_url(cached->dearrow_art)) copy(item->art_url, sizeof item->art_url, cached->dearrow_art);
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
    if (r->kind == 9 || r->kind == 10) {
        copy(r->auth_source, sizeof r->auth_source, g_source);
        copy(r->auth_token, sizeof r->auth_token, g_token);
    }
    if (r->kind == 10) { r->stage = 4; return send_request(r, "POST", "/v1/library", r->payload); }
    if ((r->kind == 1 || r->kind == 3) && (g_dearrow_titles || g_dearrow_thumbnails)) {
        cJSON *job = cJSON_Parse(r->payload);
        if (!job) { complete(r, 0); return -1; }
        cJSON_AddBoolToObject(job, "dearrow_titles", g_dearrow_titles);
        cJSON_AddBoolToObject(job, "dearrow_thumbnails", g_dearrow_thumbnails);
        char *body = cJSON_PrintUnformatted(job); cJSON_Delete(job);
        if (!body || strlen(body) >= sizeof r->payload) { free(body); complete(r, 0); return -1; }
        copy(r->payload, sizeof r->payload, body); free(body);
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

static void chat_close_remote(const char *source, const char *token, const char *profile, const char *cursor) {
    if (!*source || !*token || !valid_profile(profile) || !valid_cursor(cursor)) return;
    char url[640], auth[160], payload[256];
    snprintf(url, sizeof url, "%s/v1/library", source);
    snprintf(auth, sizeof auth, "Authorization: Bearer %s", token);
    snprintf(payload, sizeof payload, "{\"action\":\"chat_close\",\"profile\":\"%s\",\"cursor\":\"%s\"}", profile, cursor);
    const char *headers[] = {"Content-Type: application/json", auth};
    evo_net_request_async("POST", url, payload, headers, 2, ignored, NULL);
}

static void chat_cleanup_request(Request *r, const cJSON *root) {
    const char *job = r->stage == 1 ? str(root, "job") : r->job;
    if (valid_cursor(job) && *r->auth_source && *r->auth_token) {
        char url[640], auth[160];
        snprintf(url, sizeof url, "%s/v1/jobs/%s", r->auth_source, job);
        snprintf(auth, sizeof auth, "Authorization: Bearer %s", r->auth_token);
        const char *headers[] = {auth};
        evo_net_request_async("DELETE", url, NULL, headers, 1, ignored, NULL);
    }
    const cJSON *result = !strcmp(str(root, "kind"), "live_chat") ? root : cJSON_GetObjectItemCaseSensitive(root, "result");
    if (!strcmp(str(result, "kind"), "live_chat"))
        chat_close_remote(r->auth_source, r->auth_token, r->profile, str(result, "cursor"));
}

static void chat_row_id(char *out, size_t capacity, const char *id) {
    unsigned long long hash = 14695981039346656037ULL;
    for (const unsigned char *p = (const unsigned char *)id; *p; ++p) { hash ^= *p; hash *= 1099511628211ULL; }
    snprintf(out, capacity, "chat:leaf:%u:%016llx", g_chat.serial, hash);
}

static void chat_result(Request *r, const cJSON *result) {
    const cJSON *rows = cJSON_GetObjectItemCaseSensitive(result, "items");
    const cJSON *delay = cJSON_GetObjectItemCaseSensitive(result, "poll_after_ms");
    const cJSON *ended = cJSON_GetObjectItemCaseSensitive(result, "ended");
    int count = cJSON_GetArraySize(rows);
    const cJSON *replay = cJSON_GetObjectItemCaseSensitive(result, "is_replay");
    const cJSON *position = cJSON_GetObjectItemCaseSensitive(result, "position_ms");
    int replay_valid = cJSON_IsFalse(replay) || (cJSON_IsTrue(replay) && g_chat.player &&
        r->has_chat_position && cJSON_IsNumber(position) && isfinite(position->valuedouble) &&
        position->valuedouble == (double)r->chat_position_ms);
    int valid = num(result, "api_version") == 1 && !strcmp(str(result, "kind"), "live_chat") &&
        !strcmp(str(result, "id"), g_chat.video) && !strcmp(str(result, "chat_mode"), g_chat.mode) &&
        replay_valid &&
        cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(result, "has_more")) &&
        valid_cursor(str(result, "cursor")) && (cJSON_IsTrue(ended) || cJSON_IsFalse(ended)) &&
        cJSON_IsNumber(delay) && isfinite(delay->valuedouble) && delay->valuedouble >= 0 && delay->valuedouble <= 30000 &&
        floor(delay->valuedouble) == delay->valuedouble && cJSON_IsArray(rows) && count <= CHAT_HISTORY;
    char ids[CHAT_HISTORY][160];
    for (int i = 0; valid && i < count; ++i) {
        const cJSON *row = cJSON_GetArrayItem(rows, i);
        valid = *str(row, "id") && strlen(str(row, "id")) <= 640 &&
            cJSON_IsString(cJSON_GetObjectItemCaseSensitive(row, "content")) &&
            cJSON_IsString(cJSON_GetObjectItemCaseSensitive(row, "author"));
        chat_row_id(ids[i], sizeof ids[i], str(row, "id"));
        for (int j = 0; j < i; ++j) if (!strcmp(ids[i], ids[j])) valid = 0;
    }
    if (!valid) {
        chat_cleanup_request(r, result);
        tube_set_error("The chat response was invalid. Reopen the chat to retry."); complete(r, 0); return;
    }
    evo_provider_item_t *items = calloc(CHAT_HISTORY, sizeof *items);
    if (!items) { chat_cleanup_request(r, result); complete(r, 0); return; }
    memset(g_comments, 0, sizeof g_comments);
    memset(g_chat_messages, 0, sizeof g_chat_messages);
    for (int i = 0; i < count; ++i) {
        const cJSON *row = cJSON_GetArrayItem(rows, i);
        const char *type = str(row, "type"), *label = "";
        if (!strcmp(type, "LiveChatPaidMessage")) label = "Super Chat";
        else if (!strcmp(type, "LiveChatPaidSticker")) label = "Super Sticker";
        else if (!strcmp(type, "LiveChatMembershipItem")) label = "Member";
        else if (strstr(type, "Gift")) label = "Membership gift";
        char hint[256];
        snprintf(hint, sizeof hint, "%s%s%s%s%s%s%s", str(row, "published"), *label ? " | " : "", label,
            *str(row, "amount") ? " | " : "", str(row, "amount"),
            boolean(row, "moderator") ? " | Moderator" : "", boolean(row, "verified") ? " | Verified" : "");
        folder(&items[i], ids[i], *str(row, "author") ? str(row, "author") : "Viewer", hint);
        copy(items[i].overview, sizeof items[i].overview, str(row, "content"));
        copy(items[i].art_url, sizeof items[i].art_url, str(row, "art_url"));
        copy(g_chat_messages[i].id, sizeof g_chat_messages[i].id, ids[i]);
        copy(g_chat_messages[i].content, sizeof g_chat_messages[i].content, str(row, "content"));
    }
    copy(g_chat.cursor, sizeof g_chat.cursor, str(result, "cursor"));
    copy(g_chat.source, sizeof g_chat.source, r->auth_source); copy(g_chat.token, sizeof g_chat.token, r->auth_token);
    g_chat.state = cJSON_IsTrue(ended) ? 3 : 2;
    g_chat.replay = cJSON_IsTrue(replay);
    g_chat.position_ms = g_chat.replay ? r->chat_position_ms : 0;
    g_chat.next_poll = now_ms() + (delay->valuedouble < 1000 ? 1000 : (long long)delay->valuedouble);
    evo_provider_items_cb callback = r->items_cb; void *data = r->ud;
    memset(r, 0, sizeof *r);
    if (callback) callback(1, items, count, 0, data);
    free(items);
}

static void player_chat_text(char *out, size_t capacity, const char *text) {
    size_t length = strlen(text);
    if (length >= capacity) {
        length = capacity - 1;
        while (length && ((unsigned char)text[length] & 0xc0) == 0x80) --length;
    }
    /* The upstream fixed-size item may itself have clipped the final codepoint. */
    size_t tail = length;
    while (tail && ((unsigned char)text[tail - 1] & 0xc0) == 0x80) --tail;
    if (tail) {
        --tail;
        unsigned char lead = (unsigned char)text[tail];
        size_t needed = lead >= 0xf0 ? 4 : lead >= 0xe0 ? 3 : lead >= 0xc0 ? 2 : 1;
        if (length - tail < needed) length = tail;
    }
    memcpy(out, text, length); out[length] = 0;
}
static void player_chat_result(int ok, const evo_provider_item_t *items, int count, int more, void *ud) {
    (void)more;
    if (!g_chat.player || (unsigned)(size_t)ud != g_chat.serial) return;
    if (ok && g_chat.replay && (g_chat.position_ms / 1000.0 > g_player_chat_position + .001 ||
        (g_chat.seek && g_chat.position_ms < g_chat.seek_position_ms))) {
        g_player_chat_count = 0; g_chat.seek = 1; g_chat.next_poll = 0;
        return; /* Keep the returned cursor, but never render a pre-seek view. */
    }
    g_chat.seek = 0;
    memset(g_player_chat_rows, 0, sizeof g_player_chat_rows); g_player_chat_count = 0;
    if (!ok || !items || count < 0) return;
    int first = count > 6 ? count - 6 : 0;
    for (int i = first; i < count; ++i) {
        tube_player_chat_row_t *row = &g_player_chat_rows[g_player_chat_count++];
        player_chat_text(row->author, sizeof row->author, items[i].title);
        player_chat_text(row->message, sizeof row->message, items[i].overview);
    }
}

int tube_player_chat_mode(void) { return g_chat.player ? g_player_chat_mode : 0; }
const char *tube_player_chat_title(void) { return g_chat.replay ? "CHAT REPLAY" : "TOP CHAT"; }
int tube_player_chat_preference(void) { return g_player_chat_mode; }
int tube_player_chat_set_mode(int mode) {
    if (mode < 0 || mode > 2) return 0;
    g_player_chat_mode = mode;
    if (g_chat.player && (!mode || g_chat.state < 0)) tube_chat_stop();
    save();
    return 1;
}
int tube_player_chat_rows(const tube_player_chat_row_t **rows) {
    if (rows) *rows = g_player_chat_rows;
    return g_chat.player ? g_player_chat_count : 0;
}
const char *tube_player_chat_status(void) {
    return g_chat.state < 0 ? "Chat unavailable. Toggle the sidebar to retry." :
           g_chat.state == 3 ? "Chat ended" : g_player_chat_count ? "" :
           g_chat.state == 1 ? "Loading chat" : "Waiting for messages";
}
void tube_player_chat_tick(int visible) {
    tube_player_chat_tick_at(visible, g_player_chat_position >= 0 ? g_player_chat_position : 0);
}
void tube_player_chat_tick_at(int visible, double position_sec) {
    if (!visible || !g_player_chat_mode || !valid_id(g_active_id) ||
        strcmp(g_active_profile, g_library_profile) || !*g_source || !*g_token ||
        !isfinite(position_sec) || position_sec < 0 || position_sec > 1000000) {
        if (g_chat.player) tube_chat_stop();
        return;
    }
    if (g_chat.player && (strcmp(g_chat.video, g_active_id) || strcmp(g_chat.profile, g_active_profile))) tube_chat_stop();
    if (g_chat.player && g_player_chat_position >= 0 &&
        (position_sec < g_player_chat_position - .25 || position_sec > g_player_chat_position + 10)) {
        g_player_chat_count = 0; g_chat.seek = 1; g_chat.next_poll = 0;
        g_chat.seek_position_ms = (int64_t)floor(position_sec * 1000 + .000001);
    }
    g_player_chat_position = position_sec;
    if (!g_chat.player && g_chat.state) return; /* The browser reader owns this chat. */
    if (g_chat.player && (g_chat.state < 0 || (!g_chat.seek &&
        (g_chat.state == 3 || now_ms() < g_chat.next_poll)))) return;
    for (int i = 0; i < REQUESTS; ++i)
        if (g_requests[i].used && g_requests[i].kind != 8) return; /* Foreground work has priority. */
    int reuse = g_chat.player && (g_chat.state == 2 || g_chat.state == 3) && valid_cursor(g_chat.cursor);
    if (!reuse) {
        g_chat.player = 1; g_chat.state = 1;
        copy(g_chat.video, sizeof g_chat.video, g_active_id); copy(g_chat.profile, sizeof g_chat.profile, g_active_profile);
        copy(g_chat.mode, sizeof g_chat.mode, "top");
        snprintf(g_chat.parent, sizeof g_chat.parent, "chat:top:%.11s", g_active_id);
    }
    Request *r = request_new(10, (void *)(size_t)g_chat.serial);
    if (!r) return;
    r->items_cb = player_chat_result; r->intent = g_chat.serial; copy(r->profile, sizeof r->profile, g_active_profile);
    r->has_chat_position = 1; r->chat_position_ms = (int64_t)floor(position_sec * 1000 + .000001);
    snprintf(r->payload, sizeof r->payload,
        "{\"action\":\"%s\",\"id\":\"%s\",\"chat_mode\":\"top\",\"profile\":\"%s\",\"cursor\":\"%s\",\"chat_history\":false,\"chat_position_sec\":%.3f}",
        reuse ? "chat_poll" : "live_chat", g_active_id, g_active_profile, reuse ? g_chat.cursor : "", r->chat_position_ms / 1000.0);
    begin(r);
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
    if (!ok || status != 200 || gen != g_sb_serial || strcmp(g_sb_source, g_source) ||
        strcmp(g_sb_profile, g_library_profile) || size > 65536) return;
    cJSON *root = cJSON_ParseWithLength(body, size);
    if (!root) return;
    g_segment_count = 0;
    cJSON *s;
    cJSON_ArrayForEach(s, cJSON_GetObjectItemCaseSensitive(root, "segments")) {
        double start = num(s, "start"), end = num(s, "end");
        const cJSON *a = cJSON_GetObjectItemCaseSensitive(s, "start"), *b = cJSON_GetObjectItemCaseSensitive(s, "end");
        const char *category = str(s, "category");
        int kind = tube_sb_category(*category ? category : "sponsor");
        if (cJSON_IsNumber(a) && cJSON_IsNumber(b) && isfinite(start) && isfinite(end) &&
            kind >= 0 && start >= 0 && (kind == 8 ? end == start : end > start) &&
            end < 604800 && g_segment_count < SEGMENTS) {
            g_segment_categories[g_segment_count] = (unsigned char)kind;
            g_segment_seen[g_segment_count] = 0;
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
    if (!tube_recovery_active()) {
        copy(g_paid_text, sizeof g_paid_text, str(root, "paid_content_text")); g_paid_shown = 0;
    }
    copy(g_resolved_id, sizeof g_resolved_id, r->parent);
    copy(g_resolved_profile, sizeof g_resolved_profile, r->profile);
    g_resolved_music = r->music_origin;
    Record *cached = record(str(root, "id"), 1);
    if (cached) {
        copy(cached->item.title, sizeof cached->item.title, str(root, "title"));
        cached->item.duration_sec = (int64_t)num(root, "duration_sec");
        cached->item.is_live = r->choices[0].is_live;
    }
    g_caption_path[0] = 0; g_segment_count = 0; g_skip_guard = -1;
    g_sb_prompt = -1; g_sb_last_position = -1; g_sb_message = 0; g_sb_finish_target = -1;
    g_highlight_guard = -1;
    if (!++g_sb_serial) ++g_sb_serial;
    copy(g_sb_source, sizeof g_sb_source, g_source);
    copy(g_sb_channel, sizeof g_sb_channel, cached ? cached->item.parent_id : "");
    copy(g_sb_profile, sizeof g_sb_profile, g_library_profile);
    g_chapter_count = 0;
    cJSON_ArrayForEach(c, cJSON_GetObjectItemCaseSensitive(root, "chapters")) {
        if (g_chapter_count < 128) g_chapters[g_chapter_count++] = num(c, "start");
    }
    if (g_sponsor && cached && !cached->item.is_live && r->choice_count > 0 &&
        !r->choices[0].is_live && !tube_sb_excluded(&g_sb, g_sb_channel)) {
        char url[1024], auth[160];
        char categories[160] = {0};
        for (int i = 0; i < TUBE_SB_CATEGORIES; ++i) if (g_sb.actions[i] != 3 || g_sb.colors[i]) {
            size_t used = strlen(categories);
            snprintf(categories + used, sizeof categories - used, "%s%s", used ? "," : "", tube_sb_names[i]);
        }
        snprintf(url, sizeof url, "%s/v1/sponsor/%s?categories=%s", g_source, cached->item.id, categories);
        snprintf(auth, sizeof auth, "Authorization: Bearer %s", g_token);
        const char *headers[] = {auth};
        if (*categories) evo_net_request_async("GET", url, NULL, headers, 1, sponsor_response, (void *)(size_t)g_sb_serial);
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
    if (r->kind == 10 && (r->generation != g_generation || r->intent != g_chat.serial)) {
        cJSON *late = size <= 2 * 1024 * 1024 && body ? cJSON_ParseWithLength(body, size) : NULL;
        chat_cleanup_request(r, late); cJSON_Delete(late); complete(r, 0); return;
    }
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
    if (r->kind == 10 && r->stage == 4) { chat_result(r, root); cJSON_Delete(root); return; }
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
    if (r->kind == 11) {
        if (r->intent != g_message.ticket || !message_scope() || !boolean(result, "success") ||
            num(result, "api_version") != 1 || strcmp(str(result, "id"), g_message.video) ||
            strcmp(str(result, "session"), g_message.session) || !*str(result, "message_id") ||
            strlen(str(result, "message_id")) > 160) {
            cJSON_Delete(root); tube_set_error("The message was not confirmed. Check the chat before sending again."); complete(r, 0); return;
        }
        g_message.phase = 4; g_message.text[0] = 0;
        memset(&g_comments[LOCAL_PAGE], 0, sizeof g_comments[LOCAL_PAGE]);
        evo_provider_item_t item; folder(&item, "account:notice", "Chat message sent", "Circle returns to the chat");
        if (r->items_cb) { r->items_cb(1, &item, 1, 0, r->ud); r->items_cb = NULL; }
        cJSON_Delete(root); complete(r, 1); return;
    }
    if (r->kind == 6) {
        if (!boolean(result, "success")) {
            cJSON_Delete(root); tube_set_error("YouTube did not confirm the account change. Check your account before repeating it."); complete(r, 0); return;
        }
        int removing = !strcmp(g_account_intent.action, "profile_remove");
        if (!strcmp(g_account_intent.action, "comment_rate") && strcmp(str(result, "rating"), g_account_intent.identity)) {
            cJSON_Delete(root); tube_set_error("The comment rating was not confirmed."); complete(r, 0); return;
        }
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
            const char *rating = str(o, "rating_cursor");
            if (valid_cursor(rating)) {
                copy(g_comments[n].rating, sizeof g_comments[n].rating, rating);
                copy(g_comments[n].profile, sizeof g_comments[n].profile, g_library_profile);
                g_comments[n].rating_options = (boolean(o, "rating_like") ? 1u : 0u) |
                    (boolean(o, "rating_dislike") ? 2u : 0u) | (boolean(o, "rating_remove") ? 4u : 0u);
            }
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
            copy(cached->dearrow_title, sizeof cached->dearrow_title, str(o, "dearrow_title"));
            copy(cached->dearrow_art, sizeof cached->dearrow_art,
                 companion_url(str(o, "dearrow_art_url")) ? str(o, "dearrow_art_url") : "");
            dearrow_item(&items[n], cached);
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

static void cancel_requests(int preserve_chat) {
    if (!preserve_chat) tube_chat_stop();
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

void tube_cancel(void) { tube_recovery_finish(); cancel_requests(0); }

static void folder(evo_provider_item_t *out, const char *id, const char *title, const char *hint) {
    memset(out, 0, sizeof *out);
    copy(out->id, sizeof out->id, id); copy(out->title, sizeof out->title, title);
    copy(out->subtitle, sizeof out->subtitle, hint); out->is_folder = 1; out->kind = EVO_MEDIA_VIDEO;
}

static int local_catalog(const char *parent, int page, evo_provider_items_cb cb, void *ud) {
    evo_provider_item_t out[LOCAL_PAGE]; int n = 0;
    if (parent && !strncmp(parent, "message:", 8)) {
        if (page || !strcmp(g_library_profile, "guest") || g_chat.state != 2 || !valid_cursor(g_chat.cursor) ||
            strcmp(g_chat.profile, g_library_profile) || strcmp(g_chat.source, g_source) || strcmp(g_chat.token, g_token)) {
            tube_set_error("Open an active chat with an account profile before composing a message."); cb(0, NULL, 0, 0, ud); return 1;
        }
        if (!strcmp(parent, "message:compose")) {
            memset(&g_message, 0, sizeof g_message);
            if (!++g_account_serial) ++g_account_serial;
            g_message.ticket = g_account_serial; g_message.chat_serial = g_chat.serial;
            g_message.phase = g_message.pending = 1;
            copy(g_message.video, sizeof g_message.video, g_chat.video);
            copy(g_message.profile, sizeof g_message.profile, g_library_profile);
            copy(g_message.cursor, sizeof g_message.cursor, g_chat.cursor);
            snprintf(g_message.session, sizeof g_message.session, "chat-%lld-%u", now_ms(), g_message.ticket);
            folder(&out[n++], "account:notice", "Compose a chat message", "Review before sending; Circle cancels");
        } else {
            char review[80], send[80];
            snprintf(review, sizeof review, "message:review:%u", g_message.ticket);
            snprintf(send, sizeof send, "message:send:%u", g_message.ticket);
            if (!message_scope() || (strcmp(parent, review) && strcmp(parent, send)) || g_message.phase < 2) {
                tube_set_error("Reopen chat to compose a new message for the current profile."); cb(0, NULL, 0, 0, ud); return 1;
            }
            if (!strcmp(parent, send) && g_message.phase == 2) return 0;
            if (g_message.phase == 2) {
                folder(&out[n++], send, "Send this chat message", "Cross sends once; Circle cancels");
                copy(g_comments[LOCAL_PAGE].id, sizeof g_comments[LOCAL_PAGE].id, send);
                snprintf(g_comments[LOCAL_PAGE].content, sizeof g_comments[LOCAL_PAGE].content,
                    "Profile: %s\nVideo: %s\nMessage:\n%s\nCross sends once; Circle returns without sending.",
                    g_message.profile, g_message.video, g_message.text);
                copy(out[0].overview, sizeof out[0].overview, g_comments[LOCAL_PAGE].content);
            } else folder(&out[n++], "account:notice", g_message.phase == 4 ? "Chat message sent" : "Chat message pending",
                          "Check the chat before composing another message");
        }
        cb(1, out, n, 0, ud); return 1;
    }
    if (parent && !strncmp(parent, "comment:", 8)) {
        const char *cursor = !strncmp(parent, "comment:menu:", 13) ? parent + 13 :
            !strncmp(parent, "comment:confirm:", 16) ? strchr(parent + 16, ':') : NULL;
        const int reviewing = !strncmp(parent, "comment:confirm:", 16);
        if (reviewing && cursor) ++cursor;
        int found = -1;
        if (valid_cursor(cursor) && strcmp(g_library_profile, "guest"))
            for (int i = 0; i < LOCAL_PAGE; ++i)
                if (!strcmp(g_comments[i].rating, cursor) && !strcmp(g_comments[i].profile, g_library_profile)) found = i;
        if (page || found < 0) {
            tube_set_error("Reload the comments for the current profile before rating a comment.");
            cb(0, NULL, 0, 0, ud); return 1;
        }
        const char *ratings[] = {"like", "dislike", "none"};
        const char *titles[] = {"Like comment", "Dislike comment", "Remove comment rating"};
        if (!reviewing) {
            for (int i = 0; i < 3; ++i) if (g_comments[found].rating_options & (1u << i)) {
                char id[160]; snprintf(id, sizeof id, "comment:confirm:%s:%s", ratings[i], cursor);
                folder(&out[n++], id, titles[i], "Review the selected comment before confirming");
            }
        } else {
            int choice = -1;
            for (int i = 0; i < 3; ++i) {
                char expected[160]; snprintf(expected, sizeof expected, "comment:confirm:%s:%s", ratings[i], cursor);
                if (!strcmp(expected, parent) && (g_comments[found].rating_options & (1u << i))) choice = i;
            }
            if (choice < 0) { tube_set_error("Select an available comment rating."); cb(0, NULL, 0, 0, ud); return 1; }
            char content[16001]; copy(content, sizeof content, g_comments[found].content);
            unsigned serial = account_intent("comment_rate", ratings[choice], cursor, "");
            char id[80]; snprintf(id, sizeof id, "account:apply:%u", serial);
            folder(&out[n++], id, titles[choice], "Cross confirms for the selected YouTube profile; Circle cancels");
            copy(g_comments[LOCAL_PAGE].id, sizeof g_comments[LOCAL_PAGE].id, id);
            snprintf(g_comments[LOCAL_PAGE].content, sizeof g_comments[LOCAL_PAGE].content, "Profile: %s\nComment:\n%s\nCross confirms; Circle returns without submitting it.",
                     g_library_profile, content);
            copy(out[0].overview, sizeof out[0].overview, g_comments[LOCAL_PAGE].content);
        }
        cb(1, out, n, 0, ud); return 1;
    }
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
        folder(&out[n++], "section:gaming", "Gaming", "YouTube gaming shelves");
        folder(&out[n++], "section:news", "News", "YouTube news shelves");
        folder(&out[n++], "section:music", "Music", "YouTube music shelves");
        folder(&out[n++], "search:documentary", "Documentaries", "Explore documentaries");
        folder(&out[n++], "search:education", "Learning", "Explore educational videos");
        folder(&out[n++], "section:sports", "Sports", "YouTube sports shelves");
        folder(&out[n++], "section:live", "Live", "YouTube live shelves");
        folder(&out[n++], "section:movies", "Movies", "YouTube movies and shows");
    } else if (!strcmp(parent, "library") || !strncmp(parent, "library:profile:", 16)) {
        if (!strncmp(parent, "library:profile:", 16)) {
            tube_recovery_finish();
            if (!valid_profile(parent + 16)) { tube_set_error("The selected profile was invalid."); cb(0, NULL, 0, 0, ud); return 1; }
            memset(&g_account_intent, 0, sizeof g_account_intent);
            memset(&g_playlist_prompt, 0, sizeof g_playlist_prompt);
            copy(g_library_profile, sizeof g_library_profile, parent + 16);
            watch_reset(1);
            sponsor_invalidate();
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
        if (!strncmp(parent, "settings:speed-mode:", 20)) {
            const char *modes[] = {"all", "channel", "video", "none"}; int mode = -1;
            for (int i = 0; i < 4; ++i) if (!strcmp(parent + 20, modes[i])) mode = i;
            if (mode < 0) { cb(0, NULL, 0, 0, ud); return 1; }
            g_speed_mode = mode; g_speed_session_id[0] = 0; save();
        } else if (!strncmp(parent, "settings:speed:", 15)) {
            char *end; long speed = strtol(parent + 15, &end, 10);
            if (!*(parent + 15) || *end || !valid_speed((double)speed)) { cb(0, NULL, 0, 0, ud); return 1; }
            g_speed_mode = 0; g_speed_session_id[0] = 0; tube_set_playback_speed((int)speed);
        } else if (!strcmp(parent, "settings:quality")) {
            g_height = g_height == 720 ? 1080 : g_height == 1080 ? 1440 : g_height == 1440 ? 2160 : 720; save();
        } else if (!strcmp(parent, "settings:codec")) { g_codec = (g_codec + 1) % 3; save(); }
        else if (!strcmp(parent, "settings:captions")) { g_captions = !g_captions; save(); }
        else if (!strcmp(parent, "settings:automatic")) { g_auto_captions = !g_auto_captions; save(); }
        else if (!strcmp(parent, "settings:sponsor")) { g_sponsor = !g_sponsor; g_sb_prompt = -1; g_sb_finish_target = -1; g_highlight_guard = -1; save(); }
        else if (!strcmp(parent, "settings:dearrow:titles")) { g_dearrow_titles = !g_dearrow_titles; save(); }
        else if (!strcmp(parent, "settings:dearrow:thumbnails")) { g_dearrow_thumbnails = !g_dearrow_thumbnails; save(); }
        else if (!strcmp(parent, "settings:sb:paid")) { g_paid_notifications = !g_paid_notifications; save(); }
        else if (!strncmp(parent, "settings:sb:action:", 19)) {
            int category = tube_sb_category(parent + 19);
            if (category < 0) { cb(0, NULL, 0, 0, ud); return 1; }
            g_sb.actions[category] = (g_sb.actions[category] + 1) % 4; g_sb_prompt = -1; g_sb_finish_target = -1; g_highlight_guard = -1; save();
        } else if (!strncmp(parent, "settings:sb:color:", 18)) {
            int category = tube_sb_category(parent + 18);
            if (category < 0) { cb(0, NULL, 0, 0, ud); return 1; }
            g_sb.colors[category] = !g_sb.colors[category]; save();
        } else if (!strcmp(parent, "settings:sb:minimum")) {
            g_sb.minimum = g_sb.minimum < 1 ? 1 : g_sb.minimum < 3 ? 3 : g_sb.minimum < 5 ? 5 :
                g_sb.minimum < 10 ? 10 : g_sb.minimum < 15 ? 15 : g_sb.minimum < 30 ? 30 :
                g_sb.minimum < 60 ? 60 : 0; g_sb_finish_target = -1; save();
        } else if (!strcmp(parent, "settings:sb:repeat")) { g_sb.no_repeat = !g_sb.no_repeat; save(); }
        else if (!strncmp(parent, "settings:sb:exclude:", 20)) {
            const char *channel = parent + 20;
            if (!tube_sb_channel_valid(channel)) { cb(0, NULL, 0, 0, ud); return 1; }
            int found = -1;
            for (int i = 0; i < g_sb.excluded_count; ++i) if (!strcmp(channel, g_sb.excluded[i])) found = i;
            if (found >= 0) {
                if (found + 1 < g_sb.excluded_count)
                    memmove(g_sb.excluded[found], g_sb.excluded[found + 1],
                        (size_t)(g_sb.excluded_count - found - 1) * sizeof g_sb.excluded[0]);
                --g_sb.excluded_count;
            } else if (g_sb.excluded_count < TUBE_SB_EXCLUSIONS) {
                memcpy(g_sb.excluded[g_sb.excluded_count++], channel, 25);
            } else { tube_set_error("The excluded channel list is full."); cb(0, NULL, 0, 0, ud); return 1; }
            g_sb_prompt = -1; g_sb_finish_target = -1; g_highlight_guard = -1; save();
        }
        else if (!strcmp(parent, "settings:autoplay")) { g_autoplay = !g_autoplay; save(); }
        else if (!strcmp(parent, "settings:player-chat")) {
            tube_player_chat_set_mode((g_player_chat_mode + 1) % 3);
        }
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
        if (!strcmp(parent, "settings:speed-mode") || !strncmp(parent, "settings:speed-mode:", 20)) {
            const char *modes[] = {"all", "channel", "video", "none"};
            const char *labels[] = {"All videos", "Each channel", "Each video", "This playback only"};
            for (int i = 0; i < 4; ++i) {
                char id[64], label[96]; snprintf(id, sizeof id, "settings:speed-mode:%s", modes[i]);
                snprintf(label, sizeof label, "%s%s", i == g_speed_mode ? "Selected: " : "", labels[i]);
                folder(&out[n++], id, label, "Choose where a speed selected during playback is remembered");
            }
        } else if (!strcmp(parent, "settings:speed") || !strncmp(parent, "settings:speed:", 15)) {
            const int total = (int)(sizeof tube_speeds / sizeof *tube_speeds);
            int start = page >= 0 && page < (total + LOCAL_PAGE - 1) / LOCAL_PAGE ? page * LOCAL_PAGE : total;
            for (int i = start; i < total && n < LOCAL_PAGE; ++i) {
                char id[64], label[96];
                snprintf(id, sizeof id, "settings:speed:%d", tube_speeds[i]);
                snprintf(label, sizeof label, "%s%.3gx", g_speed == tube_speeds[i] ? "Selected: " : "", tube_speeds[i] / 100.0);
                folder(&out[n++], id, label, "Use this speed for all on-demand videos; live streams play at normal speed");
            }
            cb(1, out, n, start + n < total, ud); return 1;
        } else if (!strncmp(parent, "settings:dearrow", 16)) {
            folder(&out[n++], "settings:dearrow:titles", g_dearrow_titles ? "DeArrow titles: on" : "DeArrow titles: off", "Community titles; turn off to restore original titles");
            folder(&out[n++], "settings:dearrow:thumbnails", g_dearrow_thumbnails ? "DeArrow thumbnails: on" : "DeArrow thumbnails: off", "Community thumbnails; changes apply to newly loaded videos");
            folder(&out[n++], "settings:dearrow", "DeArrow community attribution", "Ajay Ramachandran and contributors; CC BY-NC-SA 4.0; dearrow.ajay.app");
        } else if (!strcmp(parent, "settings:sb:exclusions") || !strncmp(parent, "settings:sb:exclude:", 20)) {
            char channels[RECORDS + TUBE_SB_EXCLUSIONS + 1][25]; int count = 0;
            if (tube_sb_channel_valid(g_sb_channel)) memcpy(channels[count++], g_sb_channel, 25);
            for (int i = 0; i < g_sb.excluded_count; ++i) {
                int duplicate = 0;
                for (int j = 0; j < count; ++j) if (!strcmp(channels[j], g_sb.excluded[i])) duplicate = 1;
                if (!duplicate) memcpy(channels[count++], g_sb.excluded[i], 25);
            }
            for (int i = 0; i < g_count; ++i) if (current_record(&g_records[i]) && g_records[i].subscribed &&
                tube_sb_channel_valid(g_records[i].item.parent_id)) {
                int duplicate = 0;
                for (int j = 0; j < count; ++j) if (!strcmp(channels[j], g_records[i].item.parent_id)) duplicate = 1;
                if (!duplicate) memcpy(channels[count++], g_records[i].item.parent_id, 25);
            }
            for (int i = page * LOCAL_PAGE; i < count && n < LOCAL_PAGE; ++i) {
                char id[80], label[96]; snprintf(id, sizeof id, "settings:sb:exclude:%s", channels[i]);
                const char *name = !strcmp(channels[i], g_sb_channel) ? "Current channel" : channels[i];
                for (int j = 0; j < g_count; ++j)
                    if (!strcmp(g_records[j].item.parent_id, channels[i]) && *g_records[j].item.subtitle) {
                        name = g_records[j].item.subtitle; break;
                    }
                snprintf(label, sizeof label, "%s: %.70s", tube_sb_excluded(&g_sb, channels[i]) ? "Excluded" : "Enabled", name);
                folder(&out[n++], id, label, "Select to toggle; current and followed channels are listed");
            }
            cb(1, out, n, (page + 1) * LOCAL_PAGE < count, ud); return 1;
        } else if (!strcmp(parent, "settings:sb:colors") || !strncmp(parent, "settings:sb:color:", 18)) {
            for (int i = 0; i < TUBE_SB_CATEGORIES; ++i) {
                char id[80], label[128]; snprintf(id, sizeof id, "settings:sb:color:%s", tube_sb_names[i]);
                snprintf(label, sizeof label, "%s marker: %s", tube_sb_labels[i], g_sb.colors[i] ? "on" : "off");
                folder(&out[n++], id, label, "Select to toggle; independent of automatic skip actions");
            }
        } else if (!strncmp(parent, "settings:sb", 11)) {
            for (int i = 0; i < TUBE_SB_CATEGORIES; ++i) {
                static const char *const point_actions[] = {"Jump silently", "Jump with message", "Ask before jumping", "Do nothing"};
                char id[80], label[128]; snprintf(id, sizeof id, "settings:sb:action:%s", tube_sb_names[i]);
                snprintf(label, sizeof label, "%s: %s", tube_sb_labels[i],
                    i == 8 ? point_actions[g_sb.actions[i]] : tube_sb_actions[g_sb.actions[i]]);
                folder(&out[n++], id, label, i == 8 ?
                    "R3 jumps to the next highlight; select to cycle; new categories load on next video" :
                    "Select to cycle the action; fetch new categories on the next video");
            }
            char label[96]; snprintf(label, sizeof label, "Minimum remaining skip: %.0f seconds", g_sb.minimum);
            folder(&out[n++], "settings:sb:minimum", label, "Cycle 0 / 1 / 3 / 5 / 10 / 15 / 30 / 60 seconds");
            folder(&out[n++], "settings:sb:repeat", g_sb.no_repeat ? "Don't skip again: on" : "Don't skip again: off", "Keep skipped segments when rewinding this video");
            folder(&out[n++], "settings:sb:exclusions", "Excluded channels", "Toggle the current channel or a followed channel");
            folder(&out[n++], "settings:sb:colors", "Color markers on timeline", "Choose categories independently of skip actions");
            folder(&out[n++], "settings:sb:paid", g_paid_notifications ? "Paid-content notices: on" : "Paid-content notices: off", "Show YouTube paid-promotion disclosures when playback starts");
        } else if (!strcmp(parent, "settings:languages") || !strcmp(parent, "settings:audio-languages")) {
            const char *langs[] = {"en", "fr", "es", "de", "it", "pt", "ja", "ko", "zh", "ru"};
            for (int i = 0; i < 10; ++i) { char id[64]; snprintf(id, sizeof id, "%s%s", !strcmp(parent, "settings:languages") ? "settings:lang:" : "settings:audio:", langs[i]); folder(&out[n++], id, langs[i], "Select this language for the next video"); }
        } else {
            char label[96]; snprintf(label, sizeof label, "Maximum quality: %dp", g_height);
            folder(&out[n++], "settings:quality", label, "Cycle 720 / 1080 / 1440 / 2160; console validation pending");
            snprintf(label, sizeof label, "Speed for all videos: %.3gx", g_speed / 100.0);
            folder(&out[n++], "settings:speed", g_speed_mode == 0 ? label : "Choose a speed for all videos", "Select a speed and enable all-videos memory");
            const char *speed_modes[] = {"all videos", "each channel", "each video", "this playback only"};
            snprintf(label, sizeof label, "Remember speed: %s", speed_modes[g_speed_mode]);
            folder(&out[n++], "settings:speed-mode", label, "Video and channel memories are separate for each profile");
            snprintf(label, sizeof label, "Video codec: %s", g_codec == 1 ? "AVC" : g_codec == 2 ? "VP9" : "Automatic");
            folder(&out[n++], "settings:codec", label, "Use supported SDR tracks; AV1 and HDR deferred");
            folder(&out[n++], "settings:captions", g_captions ? "Captions: on" : "Captions: off", "Apply when opening the next video");
            snprintf(label, sizeof label, "Caption language: %s", g_language);
            folder(&out[n++], "settings:languages", label, "Choose a caption language");
            snprintf(label, sizeof label, "Audio language: %s", *g_audio_language ? g_audio_language : "default");
            folder(&out[n++], "settings:audio-languages", label, "Prefer an alternate audio language when available");
            folder(&out[n++], "settings:automatic", g_auto_captions ? "Automatic captions: allowed" : "Automatic captions: off", "Human captions are preferred");
            folder(&out[n++], "settings:sponsor", g_sponsor ? "SponsorBlock: on" : "SponsorBlock: off", "Also enable --sponsorblock on the companion");
            folder(&out[n++], "settings:dearrow", "DeArrow titles and thumbnails", "Optional community replacements; original metadata is retained");
            folder(&out[n++], "settings:player-chat", g_player_chat_mode == 1 ? "Live chat sidebar: left" :
                g_player_chat_mode == 2 ? "Live chat sidebar: right" : "Live chat sidebar: off",
                "Select to cycle Off / Left / Right; shows chat beside playing video");
            folder(&out[n++], "settings:sb", "SponsorBlock actions", "Category actions, minimum skip, excluded channels and rewind behavior");
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
            } else { out[n] = r->item; dearrow_item(&out[n], r); ++n; }
        }
        cb(1, out, n, (page + 1) * LOCAL_PAGE < total, ud); return 1;
    } else return 0;
    cb(1, out, n, 0, ud); return 1;
}

static int catalog(const char *parent, int page, evo_provider_items_cb cb, void *ud) {
    if (!cb || page < 0 || page > 1000) return -1;
    const char *section = topic_section(parent);
    g_browse_music = section && !strcmp(section, "music");
    g_browse_queue = parent && !strcmp(parent, "queue");
    memset(&g_launch_origin, 0, sizeof g_launch_origin);
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
    if (parent && !strncmp(parent, "chat:", 5)) {
        if (g_chat.player) tube_chat_stop();
        memset(&g_message, 0, sizeof g_message);
        memset(&g_comments[LOCAL_PAGE], 0, sizeof g_comments[LOCAL_PAGE]);
        const char *id = NULL, *mode = "top";
        if (!strncmp(parent, "chat:top:", 9)) id = parent + 9;
        else if (!strncmp(parent, "chat:live:", 10)) { id = parent + 10; mode = "live"; }
        if (page || !valid_id(id)) {
            tube_set_error("Select a valid live video to open chat."); cb(0, NULL, 0, 0, ud); return 0;
        }
        int reuse = (g_chat.state == 2 || g_chat.state == 3) && !strcmp(parent, g_chat.parent) &&
            !strcmp(g_chat.profile, g_library_profile) && valid_cursor(g_chat.cursor);
        cancel_requests(reuse);
        if (!reuse) {
            g_chat.state = 1;
            copy(g_chat.parent, sizeof g_chat.parent, parent); copy(g_chat.video, sizeof g_chat.video, id);
            copy(g_chat.mode, sizeof g_chat.mode, mode); copy(g_chat.profile, sizeof g_chat.profile, g_library_profile);
        }
        Request *r = request_new(10, ud); if (!r) { g_chat.state = -1; return -1; }
        r->items_cb = cb; r->intent = g_chat.serial; copy(r->profile, sizeof r->profile, g_library_profile);
        snprintf(r->payload, sizeof r->payload,
            "{\"action\":\"%s\",\"id\":\"%s\",\"chat_mode\":\"%s\",\"profile\":\"%s\",\"cursor\":\"%s\",\"chat_history\":true}",
            reuse ? "chat_poll" : "live_chat", id, mode, g_library_profile, reuse ? g_chat.cursor : "");
        begin(r); return 0;
    }
    if (parent && !strncmp(parent, "message:", 8)) {
        for (int i = 0; i < REQUESTS; ++i) if (g_requests[i].used && g_requests[i].kind == 10) {
            tube_set_error("Wait for the chat update before composing a message."); cb(0, NULL, 0, 0, ud); return 0;
        }
        if (!strncmp(parent, "message:send:", 13) && g_message.phase != 2)
            return local_catalog(parent, page, cb, ud) ? 0 : -1;
        cancel_requests(1);
        if (local_catalog(parent, page, cb, ud)) return 0;
        Request *r = request_new(11, ud); if (!r) { g_message.phase = -1; return -1; }
        r->items_cb = cb; r->intent = g_message.ticket; g_message.phase = 3;
        cJSON *job = cJSON_CreateObject(), *operation = cJSON_CreateObject();
        if (!job || !operation) { cJSON_Delete(job); cJSON_Delete(operation); complete(r, 0); return -1; }
        cJSON_AddStringToObject(job, "action", "library"); cJSON_AddItemToObject(job, "library", operation);
        cJSON_AddStringToObject(operation, "action", "chat_send");
        cJSON_AddStringToObject(operation, "profile", g_message.profile);
        cJSON_AddStringToObject(operation, "id", g_message.video);
        cJSON_AddStringToObject(operation, "cursor", g_message.cursor);
        cJSON_AddStringToObject(operation, "session", g_message.session);
        cJSON_AddStringToObject(operation, "text", g_message.text);
        char *body = cJSON_PrintUnformatted(job); cJSON_Delete(job);
        if (!body || strlen(body) >= sizeof r->payload) { free(body); complete(r, 0); return -1; }
        copy(r->payload, sizeof r->payload, body); free(body); begin(r); return 0;
    }
    tube_cancel();
    if (local_catalog(parent, page, cb, ud)) return 0;
    if (parent && !strncmp(parent, "section:", 8)) {
        if (!section || page >= LIBRARY_PAGES ||
            (page && (strcmp(parent, g_library_parent) || !*g_library_cursors[page]))) {
            tube_set_error("Reload this section to continue browsing."); cb(0, NULL, 0, 0, ud); return 0;
        }
        if (!page) { copy(g_library_parent, sizeof g_library_parent, parent); memset(g_library_cursors, 0, sizeof g_library_cursors); }
        const char *key = parent + 8 + strlen(section);
        Request *r = request_new(3, ud); if (!r) return -1;
        r->items_cb = cb; r->page = page; copy(r->parent, sizeof r->parent, parent);
        snprintf(r->payload, sizeof r->payload,
            "{\"action\":\"library\",\"library\":{\"action\":\"%s\",\"profile\":\"%s\",\"section\":\"%s\",\"id\":\"%s\",\"cursor\":\"%s\"}}",
            page ? "continue" : *key ? "topic_open" : "topics", g_library_profile, section,
            *key ? key + 1 : "", page ? g_library_cursors[page] : "");
        begin(r); return 0;
    }
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
        if (!strcmp(g_account_intent.action, "comment_rate")) {
            cJSON_AddStringToObject(operation, "cursor", g_account_intent.playlist);
            cJSON_AddStringToObject(operation, "rating", g_account_intent.identity);
        }
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
    int recovering = tube_recovery_active() && !strcmp(id, g_recovery.id);
    if (recovering) cancel_requests(0); else { g_paid_text[0] = 0; g_paid_shown = 0; tube_cancel(); }
    Request *r = request_new(2, ud); if (!r) return -1;
    r->resolve_cb = cb;
    copy(r->parent, sizeof r->parent, id); copy(r->profile, sizeof r->profile, g_library_profile);
    const Record *origin_record = record(id, 0);
    r->music_origin = g_browse_music || (g_browse_queue && origin_record && origin_record->queued_music);
    if (!strcmp(g_launch_origin.id, id) && !strcmp(g_launch_origin.profile, g_library_profile))
        r->music_origin = g_launch_origin.music;
    memset(&g_launch_origin, 0, sizeof g_launch_origin);
    snprintf(r->payload, sizeof r->payload,
        "{\"action\":\"resolve\",\"playback\":{\"id\":\"%s\",\"profile\":\"%s\",\"max_height\":%d,\"codec\":\"%s\",\"caption_language\":\"%s\",\"audio_language\":\"%s\",\"automatic_captions\":%s,\"fresh\":%s}}",
        id, g_library_profile, g_height, g_codec == 1 ? "avc" : g_codec == 2 ? "vp9" : "auto", g_language, g_audio_language, g_auto_captions ? "true" : "false", recovering ? "true" : "false");
    begin(r); return 0;
}

static void progress(const char *id, int64_t pos, int64_t dur, evo_provider_play_state_t state) {
    if (*g_active_id && strcmp(id, g_active_id)) return;
    const char *profile = *g_active_profile ? g_active_profile : g_library_profile;
    Record *r = record_for(id, profile, 1); if (!r) return;
    if (dur > 0) r->item.duration_sec = dur;
    if (g_finished && state != EVO_PROVIDER_PLAY_START && !strcmp(id, g_active_id)) {
        /* Explicit SponsorBlock completion stops before the old clock reaches
         * the end. Its final stop report must not restore an outro resume. */
        r->item.played = 1; r->item.resume_pos_sec = 0;
    } else if (state != EVO_PROVIDER_PLAY_START || pos > 0) r->item.resume_pos_sec = pos > 0 ? pos : 0;
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
    int recovering = tube_recovery_active() && !strcmp(id, g_recovery.id);
    (void)choice; copy(g_active_id, sizeof g_active_id, id); g_finished = 0; g_skip_guard = -1;
    g_sb_active_serial = !strcmp(id, g_resolved_id) ? g_sb_serial : 0;
    g_sb_finish_target = -1;
    char profile[33]; copy(profile, sizeof profile, !strcmp(id, g_resolved_id) ? g_resolved_profile : "");
    copy(g_active_profile, sizeof g_active_profile, valid_profile(profile) ? profile : g_library_profile);
    if (!recovering || strcmp(g_speed_session_id, id) || strcmp(g_speed_session_profile, g_active_profile)) {
        g_speed_session = !strcmp(id, g_resolved_id) && !strcmp(g_active_profile, g_resolved_profile) && g_resolved_music
            ? 100 : speed_lookup(id, g_active_profile);
        copy(g_speed_session_id, sizeof g_speed_session_id, id);
        copy(g_speed_session_profile, sizeof g_speed_session_profile, g_active_profile);
    }
    if (!recovering) memset(&g_recovery, 0, sizeof g_recovery);
    if (choice) {
        g_recovery.choice = *choice;
        copy(g_recovery.id, sizeof g_recovery.id, id);
        copy(g_recovery.profile, sizeof g_recovery.profile, g_active_profile);
        copy(g_recovery.source, sizeof g_recovery.source, g_source);
        copy(g_recovery.token, sizeof g_recovery.token, g_token);
    }
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
    r->queued = !r->queued;
    if (r->queued) { r->queue_order = ++g_queue_order; r->queued_music = g_browse_music; }
    else r->queued_music = 0;
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
const char *tube_take_paid_content_notice(void) {
    if (!g_paid_notifications || g_paid_shown || !*g_paid_text || g_finished ||
        !*g_active_id || !*g_sb_source || strcmp(g_sb_source, g_source) ||
        strcmp(g_active_id, g_resolved_id) || strcmp(g_active_profile, g_resolved_profile) ||
        strcmp(g_active_profile, g_library_profile) || g_sb_active_serial != g_sb_serial) return NULL;
    g_paid_shown = 1;
    return g_paid_text;
}
int tube_autoplay_enabled(void) { return g_autoplay; }

static int sponsor_scope(void) {
    return g_sponsor && *g_sb_source && !strcmp(g_sb_source, g_source) && !strcmp(g_sb_profile, g_library_profile) &&
        !tube_sb_excluded(&g_sb, g_sb_channel);
}
static int sponsor_playing_scope(void) {
    return sponsor_scope() && !g_finished && g_sb_active_serial == g_sb_serial &&
        !strcmp(g_active_id, g_resolved_id) && !strcmp(g_active_profile, g_sb_profile) &&
        !g_recovery.choice.is_live;
}
static void highlight_observe(double position) {
    if (g_highlight_guard >= 0 && (g_highlight_guard >= g_segment_count ||
        position >= g_segments[g_highlight_guard][0] - .25 || position < g_highlight_origin - .5 ||
        position > g_highlight_origin + .5))
        g_highlight_guard = -1;
}
void tube_highlight_seek_failed(double target) {
    if (g_highlight_guard >= 0 && g_highlight_guard < g_segment_count &&
        target == g_segments[g_highlight_guard][0]) {
        g_highlight_guard = -1; g_sb_message = 0;
    }
}
void tube_highlight_retry_idle(void) {
    if (sponsor_playing_scope()) { g_highlight_guard = -1; g_sb_message = 0; }
}
double tube_highlight_target(double position, double duration) {
    g_sb_message = 0;
    if (!sponsor_playing_scope() || !isfinite(position) || position < 0 ||
        !isfinite(duration) || duration <= 0 || duration > 1000000 || position >= duration)
        return -1;
    highlight_observe(position);
    if (tube_sponsor_prompt(position)) return -1; // Keep an existing review intact.
    int action = g_sb.actions[8], index = -1;
    if (action == 3) return -1;
    for (int i = 0; i < g_segment_count; ++i)
        if (g_segment_categories[i] == 8 && g_segments[i][0] > position + .25 &&
            g_segments[i][0] < duration && (index < 0 || g_segments[i][0] < g_segments[index][0])) index = i;
    if (index < 0 || index == g_highlight_guard) return -1;
    double target = g_segments[index][0];
    g_sb_finish_target = -1; // A point cannot authorize end-of-video completion.
    if (action == 2) {
        g_sb_prompt = index; g_sb_prompt_start = position; g_sb_prompt_end = target;
        snprintf(g_sb_prompt_text, sizeof g_sb_prompt_text, "Jump to highlight? Cross: jump   Circle: keep playing");
        return -1;
    }
    // Explicit intent ignores automatic minimum/repeat policy. Guard only the
    // same request while its seek clock has not moved; observing movement or
    // arrival releases it. A refused paused request is released explicitly.
    g_highlight_guard = index; g_highlight_origin = position;
    g_segment_seen[index] = 1; g_sb_message = action == 1;
    return target;
}
int tube_sponsor_markers(double duration, tube_sponsor_marker_t *markers, int capacity) {
    static const unsigned colors[TUBE_SB_CATEGORIES] = {
        0x00ff00, 0x00ffff, 0x0000ff, 0xff00ff, 0xffff00, 0xff9900, 0x2793e6, 0x7300ff, 0xff1684
    };
    if (!markers || capacity <= 0 || !isfinite(duration) || duration <= 0 || !sponsor_playing_scope()) return 0;
    int count = 0;
    for (int i = 0; i < g_segment_count && count < capacity; ++i) {
        int category = g_segment_categories[i];
        if (!g_sb.colors[category] || g_segments[i][0] > duration ||
            (category != 8 && g_segments[i][0] == duration)) continue;
        markers[count++] = (tube_sponsor_marker_t){g_segments[i][0] / duration,
            fmin(g_segments[i][1], duration) / duration, colors[category], category == 8};
    }
    return count;
}
const char *tube_sponsor_prompt(double position) {
    if (isfinite(position)) highlight_observe(position);
    if (!sponsor_scope() || !isfinite(position) || g_sb_prompt < 0 ||
        (g_segment_categories[g_sb_prompt] == 8 && !sponsor_playing_scope()) ||
        position < g_sb_prompt_start - .5 || position >= g_sb_prompt_end - .25) {
        g_sb_prompt = -1; return NULL;
    }
    return g_sb_prompt_text;
}
double tube_sponsor_confirm(double position, int accept) {
    if (!tube_sponsor_prompt(position)) return -1;
    int index = g_sb_prompt; double end = g_sb_prompt_end; g_sb_prompt = -1;
    if (g_segment_categories[index] == 8) {
        g_segment_seen[index] = accept ? 1 : 2;
        if (accept) { g_highlight_guard = index; g_highlight_origin = position; }
        g_sb_finish_target = -1;
        return accept ? end : -1;
    }
    g_segment_seen[index] = accept ? 1 : 2;
    g_skip_guard = index; g_skip_until = end;
    g_sb_finish_target = accept ? end : -1;
    return accept ? end : -1;
}
int tube_sponsor_show_message(void) { return g_sb_message; }
static int sponsor_match(double position, int index, double rate) {
    return g_segment_categories[index] != 8 && position >= g_segments[index][0] &&
        position <= fmin(g_segments[index][0] + 2.0 * rate, g_segments[index][1]);
}
double tube_skip_target_at_rate(double position, double rate) {
    if (!sponsor_scope() || !isfinite(position) || position < 0 || !isfinite(rate) || rate <= 0) {
        g_sb_prompt = -1; g_sb_finish_target = -1; return -1;
    }
    if (g_sb_last_position >= 0 && position < g_sb_last_position - .5) {
        g_skip_guard = -1; g_sb_prompt = -1; g_sb_finish_target = -1;
        if (!g_sb.no_repeat) memset(g_segment_seen, 0, sizeof g_segment_seen);
    }
    g_sb_last_position = position;
    if (tube_sponsor_prompt(position)) return -1;
    if (g_highlight_guard >= 0) return -1;
    if (g_skip_guard >= 0 && position < g_skip_until + .25) return -1;
    int selected[SEGMENTS], count = 0, last = -1;
    for (int i = 0; i < g_segment_count; ++i) {
        int action = g_sb.actions[g_segment_categories[i]];
        if (action == 3 || g_segment_seen[i] == 2 || (g_sb.no_repeat && g_segment_seen[i])) continue;
        if (!count) {
            if (i == g_skip_guard || !sponsor_match(position, i, rate)) continue;
            selected[count++] = i; last = i;
            if (action == 2) break; /* Reference dialogs apply to one segment. */
        } else if (action < 2 && sponsor_match(g_segments[last][1] + 3.0, i, rate)) {
            selected[count++] = i; last = i;
        }
    }
    if (!count) return -1;
    double end = g_segments[last][1];
    int action = g_sb.actions[g_segment_categories[last]];
    if (g_sb.no_repeat) for (int i = 0; i < count; ++i) g_segment_seen[selected[i]] = 1;
    const Record *media = record_for(g_resolved_id, g_sb_profile, 0);
    double bounded_end = media && media->item.duration_sec > 0 ? fmin(end, (double)media->item.duration_sec) : end;
    if (bounded_end - position < g_sb.minimum) return -1;
    if (action == 2) {
        g_sb_prompt = last; g_sb_prompt_start = g_segments[last][0]; g_sb_prompt_end = end;
        snprintf(g_sb_prompt_text, sizeof g_sb_prompt_text, "Skip %s? Cross: skip   Circle: keep", tube_sb_labels[g_segment_categories[last]]);
        return -1;
    }
    for (int i = 0; i < count; ++i) g_segment_seen[selected[i]] = 1;
    g_sb_message = action == 1;
    g_skip_guard = selected[0]; g_skip_until = end; g_sb_finish_target = end;
    return end;
}
/* The current PS5 engine plays at 1x; future rate controls must pass the actual rate. */
double tube_skip_target(double position) { return tube_skip_target_at_rate(position, 1.0); }

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

int tube_sponsor_finish(double target, double duration) {
    /* An acknowledged optional skip may reach (or overrun) the known end.
     * Finish deliberately instead of asking the demuxer to seek past its last
     * decodable frame. This is separate from natural presentation drain. */
    if (!sponsor_scope() || g_finished || g_sb_active_serial != g_sb_serial ||
        strcmp(g_active_id, g_resolved_id) || strcmp(g_active_profile, g_sb_profile) ||
        g_recovery.choice.is_live || !isfinite(target) || !isfinite(duration) ||
        duration <= 0 || duration > 1000000 || target < duration ||
        target != g_sb_finish_target || target != g_skip_until || g_skip_guard < 0) return 0;
    g_sb_finish_target = -1;
    (void)finish_and_next();
    return 1; /* The caller stops and returns even with autoplay disabled. */
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
    copy(g_launch_origin.id, sizeof g_launch_origin.id, r->item.id);
    copy(g_launch_origin.profile, sizeof g_launch_origin.profile, r->profile);
    g_launch_origin.music = r->queued_music;
    *item = r->item; dearrow_item(item, r); return 1;
}

int tube_choose_stream_enabled(void) { return g_ask_stream; }

int tube_queue_recovery(double position, int live, int subtitles) {
    if (g_recovery.phase || g_recovery.attempts >= 3 || !recovery_scope() ||
        !isfinite(position) || position < 0 || position > 1000000 ||
        !!live != !!g_recovery.choice.is_live || !companion_url(g_recovery.choice.url)) return 0;
    g_recovery.position = live ? 0 : position;
    g_recovery.subtitles = !!subtitles;
    ++g_recovery.attempts; g_recovery.phase = 1;
    return 1;
}

int tube_take_recovery(evo_provider_item_t *item, double *position, int *subtitles) {
    if (g_recovery.phase != 1) return 0;
    if (!item || !position || !subtitles || !recovery_scope()) { tube_recovery_finish(); return 0; }
    Record *r = record_for(g_recovery.id, g_recovery.profile, 0);
    if (!r) { tube_recovery_finish(); return 0; }
    *item = r->item; dearrow_item(item, r); item->is_live = g_recovery.choice.is_live;
    *position = g_recovery.position; *subtitles = g_recovery.subtitles;
    g_recovery.phase = 2; return 1;
}

int tube_recovery_choice(const evo_stream_choice_t *choices, int count) {
    if (!tube_recovery_active() || !choices || count < 1 || count > CHOICES) return -1;
    const evo_stream_choice_t *old = &g_recovery.choice;
    int found = -1;
    for (int i = 0; i < count; ++i) {
        const evo_stream_choice_t *c = &choices[i];
        if (!companion_url(c->url) || c->width != old->width || c->height != old->height ||
            !!c->is_live != !!old->is_live || c->audio_channels != old->audio_channels ||
            strcmp(c->container, old->container) || strcmp(c->video_codec, old->video_codec) ||
            strcmp(c->audio_codec, old->audio_codec) || strcmp(c->video_range, old->video_range)) continue;
        if (found >= 0) return -1; /* ambiguous replacement requires a new user selection */
        found = i;
    }
    return found;
}

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
