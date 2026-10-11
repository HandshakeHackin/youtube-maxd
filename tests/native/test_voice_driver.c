#include "tube_voice.h"
#include "tube_microphone.h"
#include "evo_net.h"
#include "cJSON.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char method[8], url[608], auth[160]; char *body; evo_net_cb callback; void *context; } Request;
static Request calls[128]; static int count, net_fail, capture_state = -1, capture_busy, begins, releases, finishes;
static unsigned current_owner = 7;
static int16_t audio[160000]; static size_t audio_samples = 8000;
int tube_remote_owner_current(unsigned owner) { return owner == current_owner; }
int tube_microphone_begin(unsigned milliseconds) {
    assert(milliseconds == 6000); ++begins;
    if (capture_busy) return 0;
    capture_busy = 1; capture_state = 0; return 1;
}
void tube_microphone_cancel(void) { if (capture_state != 0) capture_state = -1; }
void tube_microphone_finish(void) { ++finishes; }
int tube_microphone_poll(const int16_t **pcm, size_t *samples) {
    if (pcm) *pcm = capture_state == 1 ? audio : NULL;
    if (samples) *samples = capture_state == 1 ? audio_samples : 0;
    return capture_state;
}
void tube_microphone_release(void) { ++releases; if (capture_state != 0) capture_busy = 0; }
void tube_microphone_shutdown(void) { capture_state = -1; capture_busy = 0; }
int evo_net_request_async(const char *method, const char *url, const char *body,
                          const char **headers, int n, evo_net_cb cb, void *context) {
    if (net_fail) return -1;
    assert(count < 128 && n == 2 && !strcmp(headers[0], "Content-Type: application/json"));
    Request *r = &calls[count++];
    snprintf(r->method, sizeof r->method, "%s", method); snprintf(r->url, sizeof r->url, "%s", url);
    snprintf(r->auth, sizeof r->auth, "%s", headers[1]); r->body = body ? strdup(body) : NULL;
    r->callback = cb; r->context = context; return 0;
}
static void reply(int index, int ok, int status, const char *body) {
    assert(index >= 0 && index < count && calls[index].callback);
    evo_net_cb cb = calls[index].callback; calls[index].callback = NULL;
    cb(ok, status, body, body ? strlen(body) : 0, calls[index].context);
}
static void reset(void) {
    tube_voice_shutdown();
    /* Network handles remain alive through retirement until callbacks drain. */
    for (int i = 0; i < count; ++i) if (calls[i].callback) reply(i, 0, 500, NULL);
    for (int i = 0; i < count; ++i) free(calls[i].body);
    memset(calls, 0, sizeof calls); count = net_fail = 0; audio_samples = 8000;
    capture_state = -1; capture_busy = 0; current_owner = 7;
    tube_voice_configure("http://127.0.0.1:8765", "synthetic-token", "guest");
}
static void upload(void) {
    assert(tube_voice_begin(0, 100));
    assert(!tube_voice_begin(0, 101));
    tube_voice_finish(); capture_state = 1; tube_voice_tick(200);
    assert(tube_voice_state() == TUBE_VOICE_TRANSCRIBING && !capture_busy);
    assert(count == 1 && !strcmp(calls[0].method, "POST") && strstr(calls[0].url, "/v1/voice"));
}
static void created(void) {
    reply(0, 1, 200, "{\"api_version\":1,\"job\":\"owned_job\",\"state\":\"pending\"}");
    tube_voice_tick(450);
    assert(count == 2 && !strcmp(calls[1].method, "GET") && strstr(calls[1].url, "/v1/jobs/owned_job"));
}
int main(void) {
    reset(); assert(!tube_voice_begin(9, 100) && !tube_voice_begin(0, -1));
    audio[0] = 0x1234; audio[1] = -1; upload();
    cJSON *body = cJSON_Parse(calls[0].body); assert(body);
    assert(!strcmp(cJSON_GetObjectItem(body, "profile")->valuestring, "guest"));
    const char *encoded = cJSON_GetObjectItem(body, "pcm_s16le")->valuestring;
    assert(strlen(encoded) == 21336 && !strncmp(encoded, "NBL//w", 6) && !strcmp(encoded + strlen(encoded) - 2, "=="));
    cJSON_Delete(body); created();
    reply(1, 1, 200, "{\"api_version\":1,\"state\":\"pending\"}");
    tube_voice_tick(699); assert(count == 2); tube_voice_tick(700); assert(count == 3);
    reply(2, 1, 200, "{\"api_version\":1,\"state\":\"done\",\"result\":{\"query\":\"music\"}}");
    assert(tube_voice_state() == TUBE_VOICE_READY && count == 4 && !strcmp(calls[3].method, "DELETE"));
    char query[801]; assert(!tube_voice_take(query, 4) && tube_voice_take(query, sizeof query));
    assert(!strcmp(query, "music") && tube_voice_state() == TUBE_VOICE_IDLE && !tube_voice_take(query, sizeof query));
    reset(); upload(); tube_voice_configure("http://127.0.0.1:9999", "replacement-token", "other");
    reply(0, 1, 200, "{\"api_version\":1,\"job\":\"late_job\",\"state\":\"pending\"}");
    assert(count == 2 && !strcmp(calls[1].method, "DELETE") && strstr(calls[1].url, "8765/v1/jobs/late_job"));
    assert(!strcmp(calls[1].auth, "Authorization: Bearer synthetic-token") && tube_voice_state() == TUBE_VOICE_IDLE);
    reset(); upload(); created(); tube_voice_cancel();
    assert(count == 3 && !strcmp(calls[2].method, "DELETE"));
    reply(1, 1, 200, "{\"api_version\":1,\"state\":\"done\",\"result\":{\"query\":\"retired\"}}");
    assert(!tube_voice_take(query, sizeof query));
    reset(); assert(tube_voice_begin(7, 100)); current_owner = 8; tube_voice_tick(150);
    assert(tube_voice_state() == TUBE_VOICE_FAILED && !count);
    capture_state = -1; tube_voice_tick(160); assert(tube_voice_begin(0, 200));
    tube_voice_tick(75200); assert(tube_voice_state() == TUBE_VOICE_FAILED);
    reset(); upload(); created(); tube_voice_tick(75100);
    assert(tube_voice_state() == TUBE_VOICE_FAILED && count == 3 && !strcmp(calls[2].method, "DELETE"));
    reset(); net_fail = 1; assert(tube_voice_begin(0, 0)); capture_state = 1; tube_voice_tick(1);
    assert(tube_voice_state() == TUBE_VOICE_FAILED && !capture_busy);
    reset(); assert(tube_voice_begin(0, 0)); capture_state = -1; tube_voice_tick(1);
    assert(tube_voice_state() == TUBE_VOICE_FAILED && !count);
    const char *bad[] = {
        "{}", "{\"api_version\":2,\"state\":\"pending\"}",
        "{\"api_version\":1,\"job\":\"../escape\",\"state\":\"pending\"}",
        "{\"api_version\":1,\"job\":\"owned_job\",\"state\":\"bad\"}",
        "{\"api_version\":1,\"job\":\"owned_job\",\"state\":\"pending\"} junk"};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; ++i) {
        reset(); upload(); reply(0, 1, 200, bad[i]); assert(tube_voice_state() == TUBE_VOICE_FAILED);
    }
    const char *invalid_query[] = {"", "   ", "bad\\nquery", "\\ud800", "\\udc00", "prefix\\u0000suffix"};
    for (size_t i = 0; i < sizeof invalid_query / sizeof *invalid_query; ++i) {
        reset(); upload(); created(); char response[1024];
        snprintf(response, sizeof response, "{\"api_version\":1,\"state\":\"done\",\"result\":{\"query\":\"%s\"}}", invalid_query[i]);
        reply(1, 1, 200, response); assert(tube_voice_state() == TUBE_VOICE_FAILED && !tube_voice_take(query, sizeof query));
    }
    reset(); upload(); created();
    reply(1, 1, 200, "{\"api_version\":1,\"state\":\"done\",\"result\":{\"query\":\"\\u65e5\\u672c\\u8a9e\"}}");
    assert(tube_voice_take(query, sizeof query) && strlen(query) == 9);
    reset(); upload(); created();
    char long_query[1200]; memset(long_query, 'a', 201); long_query[201] = 0;
    char long_response[1400];
    snprintf(long_response, sizeof long_response, "{\"api_version\":1,\"state\":\"done\",\"result\":{\"query\":\"%s\"}}", long_query);
    reply(1, 1, 200, long_response); assert(tube_voice_state() == TUBE_VOICE_FAILED);
    reset(); audio_samples = 160001; assert(tube_voice_begin(0, 100)); capture_state = 1; tube_voice_tick(200);
    assert(tube_voice_state() == TUBE_VOICE_FAILED && !count);
    reset(); assert(tube_voice_begin(7, 100)); capture_state = 1; tube_voice_tick(200); created();
    reply(1, 1, 200, "{\"api_version\":1,\"state\":\"done\",\"result\":{\"query\":\"scoped\"}}");
    current_owner = 8; assert(!tube_voice_take(query, sizeof query)); tube_voice_tick(700);
    assert(tube_voice_state() == TUBE_VOICE_FAILED);
    reset();
    for (int i = 0; i < 4; ++i) {
        assert(tube_voice_begin(0, 100)); capture_state = 1; tube_voice_tick(200); tube_voice_cancel();
    }
    assert(count == 4 && !tube_voice_begin(0, 300));
    for (int i = 0; i < 4; ++i) {
        reply(i, 1, 200, "{\"api_version\":1,\"job\":\"late_job\",\"state\":\"pending\"}");
        assert(!strcmp(calls[4 + i].method, "DELETE"));
        reply(4 + i, 1, 200, "{\"state\":\"cancelled\"}");
    }
    assert(tube_voice_begin(0, 400));
    reset();
    for (int i = 0; i < 3; ++i) {
        assert(tube_voice_begin(0, 100)); capture_state = 1; tube_voice_tick(200); tube_voice_cancel();
    }
    assert(tube_voice_begin(0, 100)); capture_state = 1; tube_voice_tick(200);
    reply(3, 1, 200, "{\"api_version\":1,\"job\":\"busy_cleanup\",\"state\":\"pending\"}");
    tube_voice_tick(450); assert(count == 5 && !strcmp(calls[4].method, "GET"));
    tube_voice_cancel(); assert(count == 5); /* all four slots still in flight */
    reply(4, 1, 200, "{\"api_version\":1,\"state\":\"pending\"}");
    assert(count == 6 && !strcmp(calls[5].method, "DELETE") && strstr(calls[5].url, "busy_cleanup"));
    reset(); tube_voice_configure("http://127.0.0.1", "bad\r\ntoken", "guest");
    assert(!tube_voice_begin(0, 0));
    reset(); tube_voice_shutdown(); for (int i = 0; i < count; ++i) free(calls[i].body);
    puts("voice driver: PCM upload, jobs, UTF-8, scope retirement, deadlines and cleanup passed");
    return 0;
}
