/* Original bounded native Lounge driver. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_remote.h"
#include "evo_net.h"
#include "cJSON.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { START = 1, PAIR, POLL, UPDATE, STOP };
typedef struct { int used, kind, update, ack; unsigned generation;
    char source[512], token[128], session[33]; } RemoteRequest;
static RemoteRequest requests[4];
static unsigned generation = 1, revision;
static int enabled, state, want_code, ack, count, update_id, dirty;
static int voice_enabled;
static int64_t clock_ms, next_poll, next_report;
static char source[512], token[128], profile[33], session[33], device[37], screen[129], code[13];
static tube_remote_command queue[32];
static tube_remote_player player;

static int text(const cJSON *root, const char *key, char *dst, size_t cap) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsString(item) || !item->valuestring || strlen(item->valuestring) >= cap) return 0;
    for (const unsigned char *p = (const unsigned char *)item->valuestring; *p; ++p) if (*p < 32) return 0;
    memcpy(dst, item->valuestring, strlen(item->valuestring) + 1); return 1;
}
static int safe_id(const char *value, size_t length, int hexadecimal) {
    if (strlen(value) != length) return 0;
    for (const unsigned char *p = (const unsigned char *)value; *p; ++p)
        if (hexadecimal ? !(*p >= '0' && *p <= '9') && !(*p >= 'a' && *p <= 'f') :
            !((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')) && *p != '_' && *p != '-') return 0;
    return 1;
}
static int uuid(const char *value) {
    if (strlen(value) != 36) return 0;
    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) { if (value[i] != '-') return 0; }
        else if (!((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'a' && value[i] <= 'f'))) return 0;
    }
    return 1;
}
static int ascii_id(const char *value) {
    for (const unsigned char *p = (const unsigned char *)value; *p; ++p)
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_')) return 0;
    return 1;
}
static int printable(const char *value) {
    for (const unsigned char *p = (const unsigned char *)value; *p; ++p) if (*p < 32) return 0;
    return 1;
}
static int number(const cJSON *root, const char *key, double low, double high, double *out) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble) || item->valuedouble < low || item->valuedouble > high) return 0;
    *out = item->valuedouble; return 1;
}
static int integer(const cJSON *root, const char *key, int low, int high, int *out) {
    double value; if (!number(root, key, low, high, &value) || floor(value) != value) return 0;
    *out = (int)value; return 1;
}
static int command(const cJSON *root, tube_remote_command *out, int expected) {
    char action[32]; memset(out, 0, sizeof *out);
    if (!cJSON_IsObject(root) || !integer(root, "sequence", expected, expected, &out->sequence) ||
        !text(root, "action", action, sizeof action)) return 0;
    static const char *names[] = {"", "play", "pause", "next", "previous", "stopVideo", "getNowPlaying",
        "setPlaylist", "updatePlaylist", "seekTo", "setVolume", "remoteConnected", "remoteDisconnected",
        "loungeStatus", "dpadCommand", "voiceCommand", "setSubtitlesTrack"};
    for (int i = 1; i <= TUBE_REMOTE_CAPTIONS; ++i) if (!strcmp(action, names[i])) out->action = i;
    if (!out->action) return 0;
    if (out->action == TUBE_REMOTE_PLAYLIST || out->action == TUBE_REMOTE_QUEUE) {
        if (!text(root, "video_id", out->video, sizeof out->video) || (*out->video && !safe_id(out->video, 11, 0)) ||
            (out->action == TUBE_REMOTE_PLAYLIST && !*out->video) ||
            !text(root, "playlist", out->playlist, sizeof out->playlist) || !ascii_id(out->playlist) ||
            !text(root, "ctt", out->ctt, sizeof out->ctt) ||
            !integer(root, "index", 0, 100000, &out->index) || !number(root, "position_sec", 0, 1000000, &out->position)) return 0;
        const cJSON *rows = cJSON_GetObjectItemCaseSensitive(root, "video_ids"), *row;
        if (!cJSON_IsArray(rows) || cJSON_GetArraySize(rows) > 128) return 0;
        cJSON_ArrayForEach(row, rows) {
            if (!cJSON_IsString(row) || !row->valuestring || !safe_id(row->valuestring, 11, 0)) return 0;
            memcpy(out->videos[out->count++], row->valuestring, 12);
        }
    } else if (out->action == TUBE_REMOTE_SEEK) {
        if (!number(root, "position_sec", 0, 1000000, &out->position)) return 0;
    } else if (out->action == TUBE_REMOTE_VOLUME) {
        out->has_volume = cJSON_HasObjectItem(root, "volume"); out->has_delta = cJSON_HasObjectItem(root, "delta");
        if ((!out->has_volume && !out->has_delta) || (out->has_volume && !number(root, "volume", 0, 100, &out->volume)) ||
            (out->has_delta && !number(root, "delta", -100, 100, &out->delta))) return 0;
    } else if (out->action == TUBE_REMOTE_CONNECTED || out->action == TUBE_REMOTE_DISCONNECTED) {
        if (!text(root, "device_id", out->device, sizeof out->device) || !text(root, "name", out->name, sizeof out->name)) return 0;
    } else if (out->action == TUBE_REMOTE_DEVICES) {
        const cJSON *rows = cJSON_GetObjectItemCaseSensitive(root, "devices"), *row;
        if (!cJSON_IsArray(rows) || cJSON_GetArraySize(rows) > 32) return 0;
        cJSON_ArrayForEach(row, rows) {
            char id[513], name[401];
            if (!text(row, "device_id", id, sizeof id) || !text(row, "name", name, sizeof name)) return 0;
            ++out->devices;
        }
    } else if (out->action == TUBE_REMOTE_DPAD) {
        if (!text(root, "key", out->key, sizeof out->key) ||
            (strcmp(out->key,"UP") && strcmp(out->key,"DOWN") && strcmp(out->key,"LEFT") && strcmp(out->key,"RIGHT") && strcmp(out->key,"ENTER") && strcmp(out->key,"BACK"))) return 0;
    } else if (out->action == TUBE_REMOTE_VOICE) {
        if (!text(root, "status", out->voice, sizeof out->voice) || (strcmp(out->voice,"INITIATED") && strcmp(out->voice,"CANCELED"))) return 0;
    } else if (out->action == TUBE_REMOTE_CAPTIONS) {
        if (!text(root, "video_id", out->video, sizeof out->video) || (*out->video && !safe_id(out->video,11,0)) ||
            !text(root, "language", out->language, sizeof out->language) || !ascii_id(out->language)) return 0;
    }
    return 1;
}
static cJSON *parse(const char *body, size_t size) {
    if (!body || !size || size > 262144) return NULL;
    if (body[size - 1] == 0) --size;
    if (!size || memchr(body, 0, size)) return NULL;
    const char *end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(body, size, &end, 0);
    if (!root) return NULL;
    while (end && end < body + size && isspace((unsigned char)*end)) ++end;
    int version;
    if (end != body + size || !cJSON_IsObject(root) || !integer(root,"api_version",1,1,&version)) { cJSON_Delete(root); return NULL; }
    return root;
}
static void response(int ok, int status, const char *body, size_t size, void *ud);
static int remote_send(RemoteRequest *request, int kind, const cJSON *root) {
    char url[544], auth[160]; const char *headers[] = {"Content-Type: application/json", auth};
    char *body = cJSON_PrintUnformatted(root); if (!body) { request->used = 0; return 0; }
    snprintf(url, sizeof url, "%s/v1/remote", request->source);
    snprintf(auth, sizeof auth, "Authorization: Bearer %s", request->token);
    request->kind = kind;
    int result = evo_net_request_async("POST", url, body, headers, 2, response, request);
    free(body); if (result) request->used = 0;
    return result == 0;
}
static RemoteRequest *allocate(void) {
    for (int i = 0; i < 4; ++i) if (!requests[i].used) {
        RemoteRequest *r = &requests[i]; memset(r,0,sizeof *r); r->used = 1; r->generation = generation;
        memcpy(r->source, source, sizeof source); memcpy(r->token, token, sizeof token);
        memcpy(r->session, session, sizeof session); return r;
    }
    return NULL;
}
static int stop_request(RemoteRequest *r, const char *identity) {
    cJSON *root = cJSON_CreateObject(); if (!root) { r->used = 0; return 0; }
    cJSON_AddStringToObject(root,"action","stop"); cJSON_AddStringToObject(root,"session",identity);
    int result = remote_send(r,STOP,root); cJSON_Delete(root); return result;
}
static void invalidate(int failed) {
    ++generation; if (!generation) ++generation;
    code[0] = 0; count = ack = update_id = dirty = 0; want_code = 0;
    if (*session) { RemoteRequest *r = allocate(); if (r) stop_request(r,session); }
    session[0] = 0; state = failed ? TUBE_REMOTE_FAILED : TUBE_REMOTE_OFF; ++revision;
}
static void response(int ok, int status, const char *body, size_t size, void *ud) {
    RemoteRequest *r = ud; cJSON *root = ok && status == 200 ? parse(body,size) : NULL;
    const int kind = r->kind, current = r->generation == generation && enabled;
    if (kind == STOP) { r->used = 0; cJSON_Delete(root); return; }
    char returned[33] = "";
    if (root) text(root,"session",returned,sizeof returned);
    if (!current) {
        if (kind == START && safe_id(returned,32,1)) stop_request(r,returned);
        else r->used = 0;
        cJSON_Delete(root); return;
    }
    if (!root || !safe_id(returned,32,1) || (kind != START && strcmp(returned,r->session))) goto failed;
    char wire_state[16];
    if (!text(root,"state",wire_state,sizeof wire_state) || strcmp(wire_state,"listening")) goto failed;
    if (kind == START) {
        char new_device[37], new_screen[129];
        if (!text(root,"device_id",new_device,sizeof new_device) || !uuid(new_device) ||
            !text(root,"screen_id",new_screen,sizeof new_screen) || !*new_screen || !ascii_id(new_screen) ||
            (*device && strcmp(device,new_device)) || (*screen && strcmp(screen,new_screen))) {
            /* A successful but invalid bind still belongs to this request. */
            invalidate(1); stop_request(r,returned); cJSON_Delete(root); return;
        }
        snprintf(device,sizeof device,"%s",new_device); snprintf(screen,sizeof screen,"%s",new_screen);
        memcpy(session,returned,sizeof session); state = TUBE_REMOTE_LISTENING; ++revision; next_poll = 0;
    } else if (kind == PAIR) {
        char new_code[13]; if (!text(root,"pairing_code",new_code,sizeof new_code) ||
            (strlen(new_code)!=9 && strlen(new_code)!=12) || strspn(new_code,"0123456789")!=strlen(new_code)) goto failed;
        snprintf(code,sizeof code,"%s",new_code); want_code = 0; ++revision;
    } else if (kind == POLL) {
        int confirmed;
        const cJSON *rows = cJSON_GetObjectItemCaseSensitive(root,"commands"), *row;
        if (!integer(root,"ack",r->ack,r->ack,&confirmed) || !cJSON_IsArray(rows) || cJSON_GetArraySize(rows)>32) goto failed;
        tube_remote_command *staged = calloc(32,sizeof *staged); if (!staged) goto failed;
        int n = 0, wire_index = 0, valid = 1;
        cJSON_ArrayForEach(row,rows) {
            if (r->ack > INT32_MAX - wire_index - 1 || !command(row,&staged[n],r->ack+wire_index+1)) { valid = 0; break; }
            ++wire_index;
            if (staged[n].sequence > ack) ++n;
        }
        /* Local application may advance ack while a poll is in flight. Ignore
         * its already-applied prefix, retaining the exact outstanding commands. */
        if (n < count || memcmp(queue,staged,(size_t)count*sizeof *queue)) valid = 0;
        if (valid) { memcpy(queue,staged,(size_t)n*sizeof *queue); count = n; }
        free(staged); if (!valid) goto failed;
        next_poll = clock_ms + 500;
    } else if (kind == UPDATE) { next_report = clock_ms + 2000; }
    r->used = 0; cJSON_Delete(root); return;
failed:
    if (kind == START && safe_id(returned,32,1)) {
        cJSON_Delete(root); invalidate(1); stop_request(r,returned); return;
    }
    r->used = 0; cJSON_Delete(root); invalidate(1);
}

void tube_remote_configure(const char *src, const char *bearer, const char *selected,
                          int active, const char *saved_device, const char *saved_screen) {
    if (!src || !bearer || !selected || !saved_device || !saved_screen || strlen(src)>=sizeof source ||
        strlen(bearer)>=sizeof token || strlen(selected)>=sizeof profile || strlen(saved_device)>=sizeof device ||
        strlen(saved_screen)>=sizeof screen || (*saved_device && !uuid(saved_device)) || !ascii_id(saved_screen) ||
        !printable(src) || !printable(bearer) || !ascii_id(selected)) {
        invalidate(1); enabled = 0; return;
    }
    if (enabled == !!active && !strcmp(src,source) && !strcmp(bearer,token) && !strcmp(selected,profile) &&
        !strcmp(saved_device,device) && !strcmp(saved_screen,screen)) return;
    invalidate(0);
    snprintf(source,sizeof source,"%s",src); snprintf(token,sizeof token,"%s",bearer);
    snprintf(profile,sizeof profile,"%s",selected); snprintf(device,sizeof device,"%s",saved_device); snprintf(screen,sizeof screen,"%s",saved_screen);
    enabled = !!active;
    if (enabled) { state = *source && *token && *profile ? TUBE_REMOTE_STARTING : TUBE_REMOTE_FAILED; ++revision; }
}
void tube_remote_retry(void) { if (enabled) { invalidate(0); state = *source && *token && *profile ? TUBE_REMOTE_STARTING : TUBE_REMOTE_FAILED; ++revision; } }
void tube_remote_pair(void) { if (enabled) { if (state == TUBE_REMOTE_FAILED) tube_remote_retry(); want_code = 1; code[0] = 0; ++revision; } }
void tube_remote_report(const tube_remote_player *snapshot) {
    if (!snapshot || !enabled || state == TUBE_REMOTE_FAILED || !isfinite(snapshot->position) || !isfinite(snapshot->duration) ||
        snapshot->position < 0 || snapshot->duration < 0 || snapshot->position > 1000000 || snapshot->duration > 1000000 ||
        !memchr(snapshot->video,0,sizeof snapshot->video) || (*snapshot->video && !safe_id(snapshot->video,11,0)) ||
        !memchr(snapshot->playlist,0,sizeof snapshot->playlist) || !ascii_id(snapshot->playlist) ||
        !memchr(snapshot->ctt,0,sizeof snapshot->ctt) || !printable(snapshot->ctt) || !memchr(snapshot->language,0,sizeof snapshot->language) || !ascii_id(snapshot->language) ||
        !memchr(snapshot->vss_id,0,sizeof snapshot->vss_id) || snapshot->state < -1 || snapshot->state > 4 ||
        snapshot->volume < 0 || snapshot->volume > 100 || snapshot->index < 0 || snapshot->index > 100000) return;
    player = *snapshot; dirty = 1;
}
void tube_remote_tick(int64_t now_ms) {
    clock_ms = now_ms;
    if (!enabled || state == TUBE_REMOTE_FAILED || !*source || !*token || !*profile) return;
    for (int i = 0; i < 4; ++i) if (requests[i].used) return; /* Includes old-scope cleanup. */
    int kind = !*session ? START : want_code ? PAIR : dirty && now_ms >= next_report ? UPDATE : now_ms >= next_poll ? POLL : 0;
    if (!kind) return;
    RemoteRequest *r = allocate(); cJSON *root = cJSON_CreateObject();
    if (!r || !root) { if (r) r->used = 0; cJSON_Delete(root); invalidate(1); return; }
    static const char *actions[] = {"","start","pair","poll","update","stop"};
    cJSON_AddStringToObject(root,"action",actions[kind]);
    if (kind == START) {
        cJSON_AddStringToObject(root,"device_id",device); cJSON_AddStringToObject(root,"screen_id",screen);
        cJSON_AddStringToObject(root,"name","ProsperoTube PS5");
        if (voice_enabled) cJSON_AddBoolToObject(root,"voice_enabled",1);
    } else cJSON_AddStringToObject(root,"session",session);
    if (kind == POLL) { r->ack = ack; cJSON_AddNumberToObject(root,"ack",ack); }
    if (kind == UPDATE) {
        if (update_id == INT32_MAX) { r->used = 0; cJSON_Delete(root); invalidate(1); return; }
        cJSON_AddNumberToObject(root,"update_id",++update_id); r->update = update_id;
        cJSON *p = cJSON_AddObjectToObject(root,"player");
        cJSON_AddStringToObject(p,"video_id",player.video); cJSON_AddStringToObject(p,"playlist",player.playlist); cJSON_AddStringToObject(p,"ctt",player.ctt);
        cJSON_AddStringToObject(p,"language",player.language); cJSON_AddStringToObject(p,"vss_id",player.vss_id);
        cJSON_AddNumberToObject(p,"state",player.state); cJSON_AddNumberToObject(p,"position_sec",player.position); cJSON_AddNumberToObject(p,"duration_sec",player.duration);
        cJSON_AddNumberToObject(p,"index",player.index); cJSON_AddNumberToObject(p,"volume",player.volume);
        cJSON_AddBoolToObject(p,"muted",player.muted); cJSON_AddBoolToObject(p,"has_previous",player.previous); cJSON_AddBoolToObject(p,"has_next",player.next);
        dirty = 0;
    }
    int sent = remote_send(r,kind,root); cJSON_Delete(root); if (!sent) invalidate(1);
}
int tube_remote_take(tube_remote_command *out) { if (!out || !enabled || state != TUBE_REMOTE_LISTENING || !count) return 0; *out = queue[0]; out->owner = generation; return 1; }
int tube_remote_owner_current(unsigned owner) { return owner == generation && enabled && state == TUBE_REMOTE_LISTENING; }
unsigned tube_remote_current_owner(void) { return enabled && state == TUBE_REMOTE_LISTENING ? generation : 0; }
int tube_remote_command_current(unsigned owner, int sequence) { return tube_remote_owner_current(owner) && count && queue[0].sequence == sequence; }
int tube_remote_finish(unsigned owner, int sequence, int applied) {
    if (owner != generation || !enabled || state != TUBE_REMOTE_LISTENING || !count || sequence != queue[0].sequence) return 0;
    if (!applied) { invalidate(1); return 0; }
    ack = sequence; --count; memmove(queue,queue+1,(size_t)count*sizeof *queue); return 1;
}
int tube_remote_state(void) { return state; }
void tube_remote_voice_available(int available) {
    available = !!available;
    if (voice_enabled == available) return;
    int failed = state == TUBE_REMOTE_FAILED;
    voice_enabled = available; invalidate(failed);
    if (enabled && !failed) { state = TUBE_REMOTE_STARTING; ++revision; }
}
const char *tube_remote_code(void) { return code; }
const char *tube_remote_device(void) { return device; }
const char *tube_remote_screen(void) { return screen; }
unsigned tube_remote_revision(void) { return revision; }
int tube_remote_identity_valid(const char *value) { return value && uuid(value); }
void tube_remote_shutdown(void) { invalidate(0); enabled = 0; voice_enabled = 0; }
