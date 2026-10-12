/* The provider with the built-in service as its source, against a stand-in
 * service and queued HTTP mocks: start without pairing, the token on the first
 * request, a kept external pairing, failure and retry, stop, the development
 * switch back to an external companion, direct artwork hosts, stream headers.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_provider.h"
#include "tube_service.h"
#include "evo_net.h"
#include "evo_data_path.h"
#include "cJSON.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---- The stand-in service ---- */
#define TOKEN_A "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
#define TOKEN_B "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210"
static int service_fails, service_up, service_starts, service_stops;
static unsigned short service_port = 23456;
static const char *service_token = TOKEN_A;
static char service_dir[512];

int tube_service_start(const char *state_dir, unsigned short *port, char token[65]) {
    ++service_starts;
    snprintf(service_dir, sizeof service_dir, "%s", state_dir ? state_dir : "");
    if (service_fails) { service_up = 0; return -1; }
    service_up = 1; *port = service_port; snprintf(token, 65, "%s", service_token); return 0;
}
void tube_service_stop(void) { ++service_stops; service_up = 0; }
int tube_service_running(void) { return service_up; }
const char *tube_service_error(void) { return service_fails ? "no free port between 20000 and 40000" : ""; }
static int service_sponsorblock;
void tube_service_configure(const tube_service_options *options) { service_sponsorblock = options && options->sponsorblock; }

/* ---- Queued HTTP ---- */
typedef struct { char method[16], url[2048], body[2048], headers[512]; evo_net_cb cb; void *ud; } Pending;
static Pending pending[32];
static int pending_count, item_calls, last_ok, last_count, resolve_calls;
static evo_provider_item_t last_items[26];
static evo_stream_choice_t last_choice;
static char error_text[256];

int evo_net_request_async(const char *method, const char *url, const char *post,
                         const char **headers, int header_count, evo_net_cb cb, void *ud) {
    assert(pending_count < 32);
    Pending *p = &pending[pending_count++];
    memset(p, 0, sizeof *p);
    snprintf(p->method, sizeof p->method, "%s", method);
    snprintf(p->url, sizeof p->url, "%s", url);
    snprintf(p->body, sizeof p->body, "%s", post ? post : "");
    for (int i = 0; i < header_count; ++i)
        snprintf(p->headers + strlen(p->headers), sizeof p->headers - strlen(p->headers), "%s\n", headers[i]);
    p->cb = cb; p->ud = ud; return 0;
}
void evo_error_set(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vsnprintf(error_text, sizeof error_text, fmt, ap); va_end(ap);
}
static void reply(int index, int ok, int status, const char *body) {
    assert(index < pending_count);
    Pending p = pending[index];
    memmove(&pending[index], &pending[index + 1], (size_t)(--pending_count - index) * sizeof(Pending));
    p.cb(ok, status, body, strlen(body) + 1, p.ud);
}
static void drop_pending(void) { while (pending_count) reply(0, 0, 0, ""); }
static void items_cb(int ok, const evo_provider_item_t *items, int count, int more, void *ud) {
    (void)ud; (void)more; ++item_calls; last_ok = ok; last_count = count;
    assert(count <= 26);
    if (items) memcpy(last_items, items, (size_t)count * sizeof *items);
}
static void resolve_cb(int ok, const evo_stream_choice_t *choices, int count, void *ud) {
    (void)ud; ++resolve_calls; last_ok = ok;
    if (ok) { assert(count == 1); last_choice = choices[0]; }
}
static int find(const char *id) { for (int i = 0; i < last_count; ++i) if (!strcmp(last_items[i].id, id)) return i; return -1; }
static void poll_job(const char *response) {
    reply(0, 1, 200, "{\"job\":\"fixture_job\",\"state\":\"pending\"}");
    usleep(300000); tube_poll();
    assert(pending_count >= 1 && strstr(pending[0].url, "/v1/jobs/fixture_job"));
    reply(0, 1, 200, response);
}
static const char *stored(const char *key) {
    static char value[512];
    FILE *f = fopen(evo_data_path("youtube.json"), "rb"); assert(f);
    static char text[1 << 16]; size_t n = fread(text, 1, sizeof text - 1, f); text[n] = 0; fclose(f);
    cJSON *root = cJSON_Parse(text); assert(root);
    cJSON *item = cJSON_GetObjectItem(root, key);
    snprintf(value, sizeof value, "%s", cJSON_IsString(item) ? item->valuestring : "<absent>");
    cJSON_Delete(root); return value;
}
static const char *none = "{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":false}}";
/* The one pending request is a job for the service at `port`, carrying `token`. */
static void sent_to_service(unsigned port, const char *token) {
    char prefix[64], header[128];
    snprintf(prefix, sizeof prefix, "http://127.0.0.1:%u/v1/", port);
    snprintf(header, sizeof header, "Authorization: Bearer %s\n", token);
    assert(pending_count == 1 && !strncmp(pending[0].url, prefix, strlen(prefix)));
    if (strstr(pending[0].url, "/v1/health")) {   /* asked once per start, before the first job, with the token */
        assert(!strcmp(pending[0].method, "GET") && strstr(pending[0].headers, header));
        reply(0, 1, 200, "{\"api_version\":1,\"service\":\"ProsperoTube\",\"mode\":\"youtube\",\"embedded\":true,\"features\":[\"search\",\"channels\","
              "\"dash\",\"captions\",\"locale\",\"library\",\"content_filters\",\"channel_pages\",\"embedded\"]}");
        assert(pending_count == 1 && !strncmp(pending[0].url, prefix, strlen(prefix)));
    }
    assert(!strstr(pending[0].url, "/v1/pair") && strstr(pending[0].url, "/v1/jobs") && !strcmp(pending[0].method, "POST"));
    assert(strstr(pending[0].headers, header));
}
static int settings_has(const char *id) {
    for (int page = 0; page < 8; ++page) {
        evo_provider_youtube.list_catalog("settings", page, items_cb, NULL);
        if (!last_ok || !last_count) break;
        if (find(id) >= 0) return 1;
    }
    return 0;
}

int main(void) {
    char dir[] = "/tmp/tube-embedded-XXXXXX", flag[600];
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE", dir, 1));
    snprintf(flag, sizeof flag, "%s/external-companion", dir);
    const evo_provider_t *p = &evo_provider_youtube;
    char source[512], token[128], profile[33];

    /* ---- The development switch: an external companion, exactly as before. ---- */
    FILE *f = fopen(flag, "w"); assert(f && !fclose(f));
    assert(!p->init() && !tube_embedded_mode() && !service_starts && !*tube_embedded_error());
    assert(!tube_embedded_start() && !service_starts);
    tube_embedded_stop(); assert(!service_stops);
    assert(tube_art_url_allowed("http://anything.example/a.jpg") && !tube_art_url_allowed(""));
    assert(p->search("cats", 0, items_cb, NULL) == 0 && !last_ok && !pending_count && strstr(error_text, "companion LAN address"));
    assert(!p->set_source("http://192.168.1.50:8765#123456"));
    assert(p->search("cats", 0, items_cb, NULL) == 0 && pending_count == 1 && strstr(pending[0].url, "http://192.168.1.50:8765/v1/pair"));
    reply(0, 1, 200, "{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    assert(pending_count == 1 && strstr(pending[0].url, "http://192.168.1.50:8765/v1/jobs"));
    poll_job(none); assert(last_ok && !pending_count);
    assert(settings_has("settings:remote"));
    assert(tube_voice_scope(source, sizeof source, token, sizeof token, profile, sizeof profile) && !strcmp(source, "http://192.168.1.50:8765"));
    p->shutdown();
    assert(!strcmp(stored("source"), "http://192.168.1.50:8765") && !strcmp(stored("token"), "abcdefghijklmnopqrstuvwxabcdefghijkl"));
    assert(!service_starts && !service_stops);

    /* ---- The built-in service: started at init, no pairing, its token on the first request. ---- */
    assert(!unlink(flag));
    assert(!p->init() && tube_embedded_mode() && service_starts == 1 && !strcmp(service_dir, dir));
    assert(!strcmp(p->get_source(), "http://127.0.0.1:23456") && !*tube_embedded_error());
    assert(service_sponsorblock);   /* segments, when the user turns them on, are answered by the service */
    assert(p->list_catalog("", 0, items_cb, NULL) == 0 && last_ok && find("settings:service-retry") < 0);
    assert(p->search("cats", 0, items_cb, NULL) == 0);
    sent_to_service(23456, TOKEN_A);
    assert(service_starts == 1);          /* running: the request did not start it again */
    poll_job(none); assert(last_ok && !pending_count);
    /* No address to set, no phone or voice rows, and their scope stays closed. */
    assert(p->set_source("http://192.168.1.60:8765#654321") < 0 && !strcmp(p->get_source(), "http://127.0.0.1:23456"));
    assert(!settings_has("settings:remote"));
    p->list_catalog("settings:remote:pair", 0, items_cb, NULL); assert(!pending_count);
    assert(!tube_voice_scope(source, sizeof source, token, sizeof token, profile, sizeof profile) && !*source && !*token);
    /* A change that saves: the external pairing is still in the store, the loopback one never is. */
    p->list_catalog("settings:ask-stream", 0, items_cb, NULL); assert(last_ok);
    assert(!strcmp(stored("source"), "http://192.168.1.50:8765") && !strcmp(stored("token"), "abcdefghijklmnopqrstuvwxabcdefghijkl"));

    /* Accounts are not offered: no sign-in, no profile or account rows, one explanation; Home stays. */
    p->list_catalog("discover", 0, items_cb, NULL);
    assert(last_ok && find("library") >= 0 && !strcmp(last_items[find("library")].title, "Accounts") &&
           strstr(last_items[find("library")].subtitle, "later version") && !strstr(last_items[find("library:home")].subtitle, "ompanion"));
    p->list_catalog("library", 0, items_cb, NULL);
    assert(last_ok && last_count == 3 && !pending_count && find("auth:start") < 0 && find("library:profiles") < 0 && find("library:history") < 0);
    assert(!strcmp(last_items[0].title, "Sign-in to YouTube arrives in a later version") && find("library:home") == 2);
    /* Nothing shown names a companion, whatever text reaches the screen. */
    assert(p->search("words", 0, items_cb, NULL) == 0); sent_to_service(23456, TOKEN_A);
    poll_job("{\"state\":\"failed\",\"error\":{\"code\":\"upstream\",\"message\":\"The companion could not read YouTube. Check the paired companion.\"}}");
    assert(!last_ok && !strcmp(error_text, "The built-in service could not read YouTube. Check the built-in service."));
    for (int page = 0; page < 8; ++page) {
        p->list_catalog("settings", page, items_cb, NULL);
        if (!last_ok || !last_count) break;
        for (int i = 0; i < last_count; ++i) assert(!strstr(last_items[i].title, "ompanion") && !strstr(last_items[i].subtitle, "ompanion"));
    }

    /* Artwork: the service's own address and YouTube's image hosts over https, nothing else. */
    static const char *const allowed[] = {"https://i.ytimg.com/vi/abcdefghijk/hqdefault.jpg", "https://i9.ytimg.com/vi/a/b.jpg?sqp=x&rs=y",
        "https://yt3.ggpht.com/abc=s176", "https://yt3.googleusercontent.com/abc=s900", "http://127.0.0.1:23456/art/abc"};
    static const char *const refused[] = {"", "http://i.ytimg.com/vi/a/b.jpg", "https://i.ytimg.com", "https://i.ytimg.com:444/vi/a.jpg",
        "https://i.ytimg.com.evil.example/vi/a.jpg", "https://evil.example/i.ytimg.com/a.jpg", "https://user@i.ytimg.com/a.jpg",
        "https://i.ytimg.com@evil.example/a.jpg", "https://I.YTIMG.COM/vi/a.jpg", "https://xi.ytimg.com/a.jpg", "https://ytimg.com/a.jpg",
        "https://i.ytimg.com/vi/a b.jpg", "https://i.ytimg.com\\@evil.example/a.jpg", "https://i.ytimg.com?x=/a.jpg", "https://i.ytimg.com#/a.jpg",
        "http://127.0.0.1:23457/art/abc", "http://127.0.0.1:23456", "http://192.168.1.50:8765/art/abc", "file:///data/a.jpg",
        "https://dearrow-thumb.ajay.app/api/v1/getThumbnail?videoID=a", "ftp://i.ytimg.com/a.jpg"};
    for (unsigned i = 0; i < sizeof allowed / sizeof *allowed; ++i) assert(tube_art_url_allowed(allowed[i]));
    for (unsigned i = 0; i < sizeof refused / sizeof *refused; ++i) assert(!tube_art_url_allowed(refused[i]));
    assert(!tube_art_url_allowed(NULL));

    /* A manifest from the service plays; the request headers it names reach the player, malformed ones do not. */
    assert(p->resolve("abcdefghijk", resolve_cb, NULL) == 0);
    sent_to_service(23456, TOKEN_A);
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"duration_sec\":100,\"choices\":["
             "{\"url\":\"https://rr1---sn-abc.googlevideo.com/videoplayback?id=1\",\"container\":\"mp4\"},"
             "{\"url\":\"http://127.0.0.1:23456/media/s1/manifest.mpd\",\"container\":\"dash\",\"video_codec\":\"avc\",\"audio_codec\":\"aac\","
             "\"headers\":\"User-Agent: com.example.player/1.0\\r\\nOrigin: https://www.youtube.com\\r\\n\"}]}}");
    assert(resolve_calls == 1 && last_ok && !strcmp(last_choice.url, "http://127.0.0.1:23456/media/s1/manifest.mpd"));
    assert(!strcmp(last_choice.headers, "User-Agent: com.example.player/1.0\r\nOrigin: https://www.youtube.com\r\n"));
    drop_pending();
    static const char *const bad_headers[] = {"User-Agent: x", "User-Agent: x\\r\\nno colon here\\r\\n", "X: a\\nInjected: b\\r\\n", ": empty name\\r\\n"};
    for (unsigned i = 0; i < sizeof bad_headers / sizeof *bad_headers; ++i) {
        char job[1024];
        snprintf(job, sizeof job, "{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"duration_sec\":100,\"choices\":["
                 "{\"url\":\"http://127.0.0.1:23456/media/s1/manifest.mpd\",\"container\":\"dash\",\"headers\":\"%s\"}]}}", bad_headers[i]);
        tube_cancel(); drop_pending(); usleep(1100000); tube_poll(); drop_pending();
        assert(p->resolve("abcdefghijk", resolve_cb, NULL) == 0 && pending_count >= 1);
        poll_job(job);
        assert(last_ok && !*last_choice.headers);
        drop_pending();
    }
    tube_cancel(); drop_pending();

    /* A refused token is asked for again from the service, never from the user. */
    assert(p->search("dogs", 0, items_cb, NULL) == 0); sent_to_service(23456, TOKEN_A);
    service_token = TOKEN_B;
    reply(0, 1, 401, "{\"error\":{\"message\":\"Sign-in to the service was refused.\"}}");
    assert(!last_ok && !pending_count);
    int starts = service_starts;
    assert(p->search("dogs", 0, items_cb, NULL) == 0); sent_to_service(23456, TOKEN_B);
    assert(service_starts == starts + 1);
    poll_job(none); assert(last_ok);

    /* ---- Stop (soft close): requests are cancelled, the service stops, nothing goes out afterwards. ---- */
    assert(p->search("birds", 0, items_cb, NULL) == 0 && pending_count == 1);
    int calls = item_calls; starts = service_starts;
    tube_embedded_stop();
    assert(service_stops == 1 && !service_up);
    drop_pending();                       /* a late answer changes nothing */
    assert(item_calls <= calls + 1);
    assert(!*p->get_source());
    error_text[0] = 0;
    assert(p->search("birds", 0, items_cb, NULL) == 0 && !last_ok && !pending_count && !*error_text);
    assert(p->resolve("abcdefghijk", resolve_cb, NULL) == 0 && !last_ok && !pending_count);
    p->list_catalog("library:home", 0, items_cb, NULL); assert(!pending_count);
    usleep(300000); tube_poll(); assert(!pending_count && service_starts == starts);
    assert(p->list_catalog("", 0, items_cb, NULL) == 0 && last_ok && find("settings:service-retry") < 0);   /* local rows still list */
    assert(!strcmp(stored("source"), "http://192.168.1.50:8765"));
    /* A resume starts it again. */
    assert(tube_embedded_start() && service_starts == starts + 1 && !strcmp(p->get_source(), "http://127.0.0.1:23456"));
    assert(p->search("birds", 0, items_cb, NULL) == 0); sent_to_service(23456, TOKEN_B);
    poll_job(none); assert(last_ok);
    p->shutdown();
    assert(service_stops == 2 && !strcmp(stored("source"), "http://192.168.1.50:8765") && !strcmp(stored("token"), "abcdefghijklmnopqrstuvwxabcdefghijkl"));

    /* ---- The service cannot start: the app opens, says why on its first row, and retries. ---- */
    service_fails = 1; service_port = 23999; starts = service_starts;
    assert(!p->init() && tube_embedded_mode() && service_starts == starts + 1);
    assert(!strcmp(tube_embedded_error(), "no free port between 20000 and 40000") && !*p->get_source());
    assert(p->list_catalog("", 0, items_cb, NULL) == 0 && last_ok && last_count > 3);
    assert(find("settings:service-retry") == 0 && !strcmp(last_items[0].subtitle, "no free port between 20000 and 40000"));
    assert(find("settings") > 0 && find("app:exit") > 0);
    assert(p->list_catalog("favorites", 0, items_cb, NULL) == 0 && last_ok);                 /* the local library works without it */
    error_text[0] = 0;
    assert(p->search("cats", 0, items_cb, NULL) == 0 && !last_ok && !pending_count);
    assert(strstr(error_text, "could not start") && strstr(error_text, "no free port") && !strstr(error_text, "air"));   /* no "pair", no "Pair" */
    error_text[0] = 0;
    assert(p->list_catalog("settings:service-retry", 0, items_cb, NULL) == 0 && last_ok && find("settings:service-retry") == 0);
    assert(strstr(error_text, "could not start"));
    service_fails = 0;
    assert(p->list_catalog("settings:service-retry", 0, items_cb, NULL) == 0 && last_ok && last_count > 3 && find("settings:service-retry") < 0);
    assert(!*tube_embedded_error() && !strcmp(p->get_source(), "http://127.0.0.1:23999"));
    assert(p->search("cats", 0, items_cb, NULL) == 0); sent_to_service(23999, TOKEN_B);
    poll_job(none); assert(last_ok);
    /* A service that answers without a usable token is a failed start too. */
    p->shutdown(); service_token = "short";
    assert(!p->init() && *tube_embedded_error() && !*p->get_source());
    assert(p->search("cats", 0, items_cb, NULL) == 0 && !last_ok && !pending_count);
    service_token = TOKEN_A; p->shutdown();

    /* ---- Back to the external companion: its stored pairing is used as it was. ---- */
    f = fopen(flag, "w"); assert(f && !fclose(f));
    starts = service_starts;
    assert(!p->init() && !tube_embedded_mode() && service_starts == starts && !strcmp(p->get_source(), "http://192.168.1.50:8765"));
    assert(p->search("cats", 0, items_cb, NULL) == 0 && pending_count == 1 && strstr(pending[0].url, "http://192.168.1.50:8765/v1/jobs"));
    assert(strstr(pending[0].headers, "Authorization: Bearer abcdefghijklmnopqrstuvwxabcdefghijkl\n"));
    poll_job(none); assert(last_ok);
    p->shutdown();
    puts("PASS: built-in service start without pairing, token on requests, kept external pairing, failure and retry, stop and resume, development switch, artwork hosts, stream headers");
    return 0;
}
