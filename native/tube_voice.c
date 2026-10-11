/* Scoped PCM upload/job lifecycle. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_voice.h"
#include "tube_microphone.h"
#include "tube_remote.h"
#include "evo_net.h"
#include "cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { CREATE = 1, POLL, DELETE };
typedef struct {
    int used, kind;
    unsigned generation;
    char source[512], token[128], job[65];
} VoiceRequest;
static VoiceRequest requests[4];
static char source[512], token[128], profile[33], job[65], query[801];
static unsigned generation = 1, owner;
static int state, inflight;
static int64_t clock_ms, deadline, next_poll;

static void wipe(void *memory, size_t bytes) {
    volatile unsigned char *p = memory;
    while (bytes--) *p++ = 0;
}
static int identifier(const char *value, size_t maximum) {
    size_t n = strlen(value);
    if (!n || n > maximum) return 0;
    for (size_t i = 0; i < n; ++i)
        if (!((value[i] >= 'A' && value[i] <= 'Z') || (value[i] >= 'a' && value[i] <= 'z') ||
              (value[i] >= '0' && value[i] <= '9')) && value[i] != '_' && value[i] != '-') return 0;
    return 1;
}
static int printable(const char *value) {
    for (; *value; ++value) if ((unsigned char)*value < 32 || (unsigned char)*value == 127) return 0;
    return 1;
}
static const char *text(const cJSON *root, const char *key) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsString(v) && v->valuestring ? v->valuestring : "";
}
static int valid_query(const char *value) {
    const unsigned char *p = (const unsigned char *)value;
    unsigned characters = 0; int visible = 0;
    while (*p) {
        unsigned c = *p++, tails = 0, minimum = 0;
        if (c >= 0xc2 && c <= 0xdf) { tails = 1; c &= 31; minimum = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { tails = 2; c &= 15; minimum = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { tails = 3; c &= 7; minimum = 0x10000; }
        else if (c >= 0x80) return 0;
        for (unsigned i = 0; i < tails; ++i) {
            if ((*p & 0xc0) != 0x80) return 0;
            c = (c << 6) | (*p++ & 63);
        }
        if (c < minimum || c < 32 || c == 127 || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) return 0;
        if (++characters > 200) return 0;
        if (c != 32) visible = 1;
    }
    return visible;
}
static cJSON *parse(const char *body, size_t bytes) {
    if (!body || bytes < 2 || bytes > 8192 || memchr(body, 0, bytes)) return NULL;
    /* cJSON strings carry no decoded length. Reject escaped NUL instead of
     * accidentally accepting a truncated job identifier or transcript. */
    for (size_t i = 0; i < bytes; ++i) if (body[i] == '\\') {
        if (bytes - i >= 6 && !memcmp(body + i, "\\u0000", 6)) return NULL;
        if (i + 1 < bytes) ++i;
    }
    const char *end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(body, bytes, &end, 0);
    while (end && end < body + bytes && isspace((unsigned char)*end)) ++end;
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "api_version");
    if (end != body + bytes || !cJSON_IsObject(root) || !cJSON_IsNumber(version) || version->valuedouble != 1) {
        cJSON_Delete(root); return NULL;
    }
    return root;
}
static void response(int ok, int status, const char *body, size_t bytes, void *context);
static VoiceRequest *allocate(void) {
    for (size_t i = 0; i < 4; ++i) if (!requests[i].used) {
        VoiceRequest *r = &requests[i]; memset(r, 0, sizeof *r);
        r->used = 1; r->generation = generation;
        snprintf(r->source, sizeof r->source, "%s", source);
        snprintf(r->token, sizeof r->token, "%s", token); return r;
    }
    return NULL;
}
static int voice_send(VoiceRequest *r, int kind, const char *id, const char *payload) {
    char url[608], auth[160];
    const char *headers[] = {"Content-Type: application/json", auth};
    snprintf(url, sizeof url, "%s/v1/%s%s", r->source, kind == CREATE ? "voice" : "jobs/", kind == CREATE ? "" : id);
    snprintf(auth, sizeof auth, "Authorization: Bearer %s", r->token);
    r->kind = kind;
    if (kind == POLL) snprintf(r->job, sizeof r->job, "%s", id);
    int rc = evo_net_request_async(kind == CREATE ? "POST" : kind == POLL ? "GET" : "DELETE",
                                   url, payload, headers, 2, response, r);
    if (rc) { wipe(r, sizeof *r); return 0; }
    return 1;
}
static void delete_current(void) {
    if (*job) {
        VoiceRequest *r = allocate();
        if (r) voice_send(r, DELETE, job, NULL);
        job[0] = 0;
    }
}
void tube_voice_cancel(void) {
    delete_current();
    if (!++generation) ++generation;
    tube_microphone_cancel(); tube_microphone_release();
    wipe(query, sizeof query); owner = 0; inflight = 0; state = TUBE_VOICE_IDLE;
}
static void fail(void) {
    tube_voice_cancel(); state = TUBE_VOICE_FAILED;
}
void tube_voice_configure(const char *src, const char *bearer, const char *selected) {
    int valid = src && bearer && selected && strlen(src) < sizeof source &&
        strlen(bearer) < sizeof token && identifier(selected, 32) && printable(src) && printable(bearer) &&
        (!strncmp(src, "http://", 7) || !strncmp(src, "https://", 8));
    if (!valid) { tube_voice_cancel(); source[0] = profile[0] = 0; wipe(token, sizeof token); return; }
    if (!strcmp(source, src) && !strcmp(token, bearer) && !strcmp(profile, selected)) return;
    tube_voice_cancel();
    snprintf(source, sizeof source, "%s", src); snprintf(token, sizeof token, "%s", bearer);
    snprintf(profile, sizeof profile, "%s", selected);
}
int tube_voice_begin(unsigned remote_owner, int64_t now) {
    if (state == TUBE_VOICE_RECORDING || state == TUBE_VOICE_TRANSCRIBING || !*source || !*token ||
        now < 0 || now > INT64_MAX - 75000 || (remote_owner && !tube_remote_owner_current(remote_owner))) return 0;
    int available = 0; for (size_t i = 0; i < 4; ++i) if (!requests[i].used) available = 1;
    if (!available) return 0;
    tube_voice_cancel();
    if (tube_microphone_poll(NULL, NULL) != 0) tube_microphone_release();
    if (!tube_microphone_begin(6000)) { state = TUBE_VOICE_FAILED; return 0; }
    owner = remote_owner; clock_ms = now; deadline = now + 75000;
    state = TUBE_VOICE_RECORDING; return 1;
}
void tube_voice_finish(void) { if (state == TUBE_VOICE_RECORDING) tube_microphone_finish(); }
static char *payload(const int16_t *pcm, size_t samples) {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (!pcm || samples < 8000 || samples > 160000) return NULL;
    size_t bytes = samples * 2, encoded = (bytes + 2) / 3 * 4;
    char *body = malloc(encoded + 100); if (!body) return NULL;
    int prefix = snprintf(body, 100, "{\"profile\":\"%s\",\"pcm_s16le\":\"", profile);
    if (prefix < 0 || prefix >= 100) { free(body); return NULL; }
    char *out = body + prefix;
    /* Explicit little endian rather than assuming the host's native byte order. */
    for (size_t i = 0; i < bytes; i += 3) {
        unsigned b[3] = {0};
        for (size_t j = 0; j < 3 && i + j < bytes; ++j)
            b[j] = ((uint16_t)pcm[(i + j) / 2] >> ((i + j) % 2 * 8)) & 255;
        *out++ = alphabet[b[0] >> 2]; *out++ = alphabet[((b[0] & 3) << 4) | (b[1] >> 4)];
        *out++ = i + 1 < bytes ? alphabet[((b[1] & 15) << 2) | (b[2] >> 6)] : '=';
        *out++ = i + 2 < bytes ? alphabet[b[2] & 63] : '=';
    }
    memcpy(out, "\"}", 3); return body;
}
void tube_voice_tick(int64_t now) {
    clock_ms = now;
    if (state != TUBE_VOICE_RECORDING && tube_microphone_poll(NULL, NULL) != 0) tube_microphone_release();
    if (state != TUBE_VOICE_RECORDING && state != TUBE_VOICE_TRANSCRIBING && state != TUBE_VOICE_READY) return;
    if ((owner && !tube_remote_owner_current(owner)) || now < 0 || now >= deadline) { fail(); return; }
    if (state == TUBE_VOICE_RECORDING) {
        const int16_t *pcm = NULL; size_t samples = 0;
        int captured = tube_microphone_poll(&pcm, &samples);
        if (!captured) return;
        if (captured < 0) { fail(); return; }
        char *body = payload(pcm, samples);
        tube_microphone_release();
        VoiceRequest *r = body ? allocate() : NULL;
        int sent = r && voice_send(r, CREATE, NULL, body);
        if (body) { wipe(body, strlen(body)); free(body); }
        if (!sent) { fail(); return; }
        state = TUBE_VOICE_TRANSCRIBING; inflight = 1;
    } else if (state == TUBE_VOICE_TRANSCRIBING && *job && !inflight && now >= next_poll) {
        VoiceRequest *r = allocate();
        if (!r || !voice_send(r, POLL, job, NULL)) { fail(); return; }
        inflight = 1;
    }
}
static void response(int ok, int status, const char *body, size_t bytes, void *context) {
    VoiceRequest *r = context;
    if (r->kind == DELETE) { wipe(r, sizeof *r); return; }
    cJSON *root = ok && status == 200 ? parse(body, bytes) : NULL;
    int kind = r->kind, current = r->generation == generation && state == TUBE_VOICE_TRANSCRIBING;
    char returned[65] = "";
    if (root && identifier(text(root, "job"), 64)) snprintf(returned, sizeof returned, "%s", text(root, "job"));
    if (!current) {
        if (kind == CREATE && *returned) voice_send(r, DELETE, returned, NULL);
        else if (kind == POLL && *r->job) voice_send(r, DELETE, r->job, NULL);
        else wipe(r, sizeof *r);
        cJSON_Delete(root); return;
    }
    inflight = 0;
    const char *wire_state = text(root, "state");
    if (kind == CREATE && *returned) {
        /* Keep even an invalid response's valid job ID for owned cleanup. */
        snprintf(job, sizeof job, "%s", returned);
    }
    if (!root || (kind == CREATE && (!*job || strcmp(wire_state, "pending")))) goto failed;
    if (kind == POLL && !strcmp(wire_state, "done")) {
        const char *value = text(cJSON_GetObjectItemCaseSensitive(root, "result"), "query");
        if (strlen(value) >= sizeof query || !valid_query(value)) goto failed;
        snprintf(query, sizeof query, "%s", value); state = TUBE_VOICE_READY;
        voice_send(r, DELETE, job, NULL); job[0] = 0;
    } else if (!strcmp(wire_state, "pending")) {
        next_poll = clock_ms + 250; wipe(r, sizeof *r);
    } else goto failed;
    cJSON_Delete(root); return;
failed:
    wipe(r, sizeof *r); cJSON_Delete(root); fail();
}
int tube_voice_state(void) { return state; }
int tube_voice_take(char *destination, size_t capacity) {
    if (state != TUBE_VOICE_READY || !destination || strlen(query) >= capacity ||
        (owner && !tube_remote_owner_current(owner))) return 0;
    memcpy(destination, query, strlen(query) + 1); tube_voice_cancel(); return 1;
}
void tube_voice_shutdown(void) {
    tube_voice_cancel(); tube_microphone_shutdown();
    source[0] = profile[0] = 0; wipe(token, sizeof token);
}
