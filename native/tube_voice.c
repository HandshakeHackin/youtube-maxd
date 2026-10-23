/* Scoped PCM upload/job lifecycle. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_voice.h"
#include "tube_microphone.h"
#include "tube_remote.h"
#include "tube_search_query.h"
#include "evo_net.h"
#include "cJSON.h"
#ifdef TUBE_LOCAL_VOICE
#include "local_asr.h"
#include "maxd_voice_model.h"
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { CREATE = 1, POLL, DELETE, HEALTH };
typedef struct {
    int used, kind;
    unsigned generation, scope;
    char source[512], token[128], job[65];
} VoiceRequest;
static VoiceRequest requests[4];
static char source[512], token[128], profile[33], job[65], query[801];
static unsigned generation = 1, scope = 1, owner;
static int state, inflight, available, health_inflight;
static int64_t clock_ms, deadline, next_poll, next_health;
static int local_mode;
static char failure[160];

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
        r->scope = scope;
        snprintf(r->source, sizeof r->source, "%s", source);
        snprintf(r->token, sizeof r->token, "%s", token); return r;
    }
    return NULL;
}
static int voice_send(VoiceRequest *r, int kind, const char *id, const char *payload) {
    char url[608], auth[160];
    const char *headers[] = {"Content-Type: application/json", auth};
    snprintf(url, sizeof url, "%s/v1/%s%s", r->source,
             kind == CREATE ? "voice" : kind == HEALTH ? "health" : "jobs/",
             kind == CREATE || kind == HEALTH ? "" : id);
    snprintf(auth, sizeof auth, "Authorization: Bearer %s", r->token);
    r->kind = kind;
    if (kind == POLL) snprintf(r->job, sizeof r->job, "%s", id);
    int rc = evo_net_request_async(kind == CREATE ? "POST" : kind == POLL || kind == HEALTH ? "GET" : "DELETE",
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
#ifdef TUBE_LOCAL_VOICE
    maxd_asr_cancel(); maxd_asr_release();
#endif
    failure[0] = 0;
    wipe(query, sizeof query); owner = 0; inflight = 0; state = TUBE_VOICE_IDLE;
}
static void fail_with(const char *reason) {
    char saved[sizeof failure]; snprintf(saved, sizeof saved, "%s", reason);
    tube_voice_cancel(); state = TUBE_VOICE_FAILED;
    snprintf(failure, sizeof failure, "%s", saved);
}
static void fail(void) { fail_with("Voice search could not finish. Check the microphone and try again."); }
void tube_voice_configure_local(const char *selected) {
    if (!selected || !identifier(selected, 32)) selected = "guest";
    if (local_mode && !strcmp(profile, selected)) return;
    tube_voice_cancel();
    if (!++scope) ++scope;
    local_mode = 1; health_inflight = 0; next_health = 0;
    source[0] = 0; wipe(token, sizeof token);
    snprintf(profile, sizeof profile, "%s", selected);
#ifdef TUBE_LOCAL_VOICE
    available = maxd_asr_available(MAXD_VOICE_MODEL_PATH) ? 1 : -1;
#else
    available = -1;
#endif
}
void tube_voice_configure(const char *src, const char *bearer, const char *selected) {
    if (local_mode) {
        tube_voice_cancel(); local_mode = 0;
        source[0] = profile[0] = 0; wipe(token, sizeof token);
        if (!++scope) ++scope;
        health_inflight = 0; available = 0; next_health = 0;
    }
    int valid = src && bearer && selected && strlen(src) < sizeof source &&
        strlen(bearer) < sizeof token && identifier(selected, 32) && printable(src) && printable(bearer) &&
        (!strncmp(src, "http://", 7) || !strncmp(src, "https://", 8));
    if (!valid) {
        tube_voice_cancel(); if (!++scope) ++scope;
        available = health_inflight = 0; next_health = 0;
        source[0] = profile[0] = 0; wipe(token, sizeof token); return;
    }
    if (!strcmp(source, src) && !strcmp(token, bearer) && !strcmp(profile, selected)) return;
    tube_voice_cancel();
    if (!++scope) ++scope;
    available = health_inflight = 0; next_health = 0;
    snprintf(source, sizeof source, "%s", src); snprintf(token, sizeof token, "%s", bearer);
    snprintf(profile, sizeof profile, "%s", selected);
}
int tube_voice_begin(unsigned remote_owner, int64_t now) {
    if (state == TUBE_VOICE_RECORDING || state == TUBE_VOICE_TRANSCRIBING) return 0;
#ifdef TUBE_LOCAL_VOICE
    /* Configuration caches availability once per profile. A launch-time failure must
     * not latch local voice off: retry the cheap file probe on each L3 request.
     * The worker still checks the full model digest before loading it. */
    if (local_mode) available = maxd_asr_available(MAXD_VOICE_MODEL_PATH) ? 1 : -1;
#endif
    if (available != 1 ||
        (!local_mode && (!*source || !*token)) || (local_mode && remote_owner) ||
        now < 0 || now > INT64_MAX - 75000 || (remote_owner && !tube_remote_owner_current(remote_owner))) return 0;
    int available = 0; for (size_t i = 0; i < 4; ++i) if (!requests[i].used) available = 1;
    if (!available) return 0;
    tube_voice_cancel();
    if (tube_microphone_poll(NULL, NULL) != 0) tube_microphone_release();
    if (!tube_microphone_begin(6000)) { fail_with("The microphone could not start. Check its connection and mute button."); return 0; }
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
    if (!local_mode && *source && *token && now >= 0 && now >= next_health && !health_inflight &&
        (state == TUBE_VOICE_IDLE || state == TUBE_VOICE_FAILED)) {
        VoiceRequest *r = allocate();
        if (r && voice_send(r, HEALTH, NULL, NULL)) health_inflight = 1;
        else {
            available = -1;
            next_health = now <= INT64_MAX - 10000 ? now + 10000 : INT64_MAX;
        }
    }
    if (state != TUBE_VOICE_RECORDING && tube_microphone_poll(NULL, NULL) != 0) tube_microphone_release();
    if (state != TUBE_VOICE_RECORDING && state != TUBE_VOICE_TRANSCRIBING && state != TUBE_VOICE_READY) return;
    if (available != 1 || (owner && !tube_remote_owner_current(owner)) || now < 0 || now >= deadline) { fail(); return; }
    if (state == TUBE_VOICE_RECORDING) {
        const int16_t *pcm = NULL; size_t samples = 0;
        int captured = tube_microphone_poll(&pcm, &samples);
        if (!captured) return;
        if (captured < 0) { fail_with("No speech captured. Unmute the microphone and speak clearly, then retry."); return; }
#ifdef TUBE_LOCAL_VOICE
        if (local_mode) {
            int started = maxd_asr_start(MAXD_VOICE_MODEL_PATH, pcm, samples);
            tube_microphone_release();
            if (!started) { fail_with(maxd_asr_error()); return; }
            state = TUBE_VOICE_TRANSCRIBING;
            return;
        }
#endif
        char *body = payload(pcm, samples);
        tube_microphone_release();
        VoiceRequest *r = body ? allocate() : NULL;
        int sent = r && voice_send(r, CREATE, NULL, body);
        if (body) { wipe(body, strlen(body)); free(body); }
        if (!sent) { fail(); return; }
        state = TUBE_VOICE_TRANSCRIBING; inflight = 1;
    }
#ifdef TUBE_LOCAL_VOICE
    else if (local_mode && state == TUBE_VOICE_TRANSCRIBING) {
        int recognized = maxd_asr_poll(query, sizeof query);
        if (!recognized) return;
        if (recognized < 0) { fail_with(maxd_asr_error()); return; }
        if (!tube_search_query_valid(query)) { fail_with("No clear search recognized. Please try again."); return; }
        maxd_asr_release(); state = TUBE_VOICE_READY;
    }
#endif
    else if (state == TUBE_VOICE_TRANSCRIBING && *job && !inflight && now >= next_poll) {
        VoiceRequest *r = allocate();
        if (!r || !voice_send(r, POLL, job, NULL)) { fail(); return; }
        inflight = 1;
    }
}
static void response(int ok, int status, const char *body, size_t bytes, void *context) {
    VoiceRequest *r = context;
    if (r->kind == DELETE) { wipe(r, sizeof *r); return; }
    cJSON *root = ok && status == 200 ? parse(body, bytes) : NULL;
    if (r->kind == HEALTH) {
        if (r->scope == scope) {
            const cJSON *features = cJSON_GetObjectItemCaseSensitive(root, "features"), *feature;
            health_inflight = 0; available = -1;
            if (cJSON_IsArray(features) && cJSON_GetArraySize(features) <= 64)
                cJSON_ArrayForEach(feature, features)
                    if (cJSON_IsString(feature) && feature->valuestring && !strcmp(feature->valuestring, "voice")) available = 1;
            int delay = available == 1 ? 60000 : 10000;
            next_health = clock_ms >= 0 && clock_ms <= INT64_MAX - delay ? clock_ms + delay : INT64_MAX;
        }
        wipe(r, sizeof *r); cJSON_Delete(root); return;
    }
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
        if (strlen(value) >= sizeof query || !tube_search_query_valid(value)) goto failed;
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
int tube_voice_available(void) { return available; }
unsigned tube_voice_owner(void) { return owner; }
const char *tube_voice_result(void) { return state == TUBE_VOICE_READY && (!owner || tube_remote_owner_current(owner)) ? query : ""; }
const char *tube_voice_error(void) {
#ifdef TUBE_LOCAL_VOICE
    if (local_mode && available != 1) return maxd_asr_error();
#endif
    if (*failure) return failure;
    return local_mode ? "Voice model is unavailable. Reinstall the complete app and restart it." :
        "Enable local speech recognition on the paired companion first.";
}
int tube_voice_take(char *destination, size_t capacity) {
    if (state != TUBE_VOICE_READY || !destination || strlen(query) >= capacity ||
        (owner && !tube_remote_owner_current(owner))) return 0;
    memcpy(destination, query, strlen(query) + 1); tube_voice_cancel(); return 1;
}
void tube_voice_shutdown(void) {
    tube_voice_cancel(); tube_microphone_shutdown();
#ifdef TUBE_LOCAL_VOICE
    maxd_asr_shutdown();
#endif
    local_mode = 0;
    if (!++scope) ++scope;
    available = health_inflight = 0; next_health = 0;
    source[0] = profile[0] = 0; wipe(token, sizeof token);
}
