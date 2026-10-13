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
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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
static int service_sponsorblock, service_dislikes;
static const char *service_sb_server = "unset";
void tube_service_configure(const tube_service_options *options) {
    service_sponsorblock = options && options->sponsorblock; service_dislikes = options && options->return_youtube_dislike;
    service_sb_server = options ? options->sponsorblock_server : "unset";
}

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
static int service_accounts;   /* the stand-in service lists sign-in, as the real one does */
static void sent_to_service(unsigned port, const char *token) {
    char prefix[64], header[128];
    snprintf(prefix, sizeof prefix, "http://127.0.0.1:%u/v1/", port);
    snprintf(header, sizeof header, "Authorization: Bearer %s\n", token);
    assert(pending_count == 1 && !strncmp(pending[0].url, prefix, strlen(prefix)));
    if (strstr(pending[0].url, "/v1/health")) {   /* asked once per start, before the first job, with the token */
        assert(!strcmp(pending[0].method, "GET") && strstr(pending[0].headers, header));
        reply(0, 1, 200, service_accounts ?
              "{\"api_version\":1,\"service\":\"ProsperoTube\",\"mode\":\"youtube\",\"embedded\":true,\"account_playback\":false,\"features\":[\"search\","
              "\"channels\",\"dash\",\"captions\",\"locale\",\"library\",\"content_filters\",\"channel_pages\",\"device_auth\",\"subscriptions_import\",\"remote\","
              "\"video_details\",\"sponsorblock_server\",\"dislikes\",\"embedded\"]}" :
              "{\"api_version\":1,\"service\":\"ProsperoTube\",\"mode\":\"youtube\",\"embedded\":true,\"features\":[\"search\",\"channels\","
              "\"dash\",\"captions\",\"locale\",\"library\",\"content_filters\",\"channel_pages\",\"embedded\"]}");
        assert(pending_count == 1 && !strncmp(pending[0].url, prefix, strlen(prefix)));
    }
    assert(!strstr(pending[0].url, "/v1/pair") && strstr(pending[0].url, "/v1/jobs") && !strcmp(pending[0].method, "POST"));
    assert(strstr(pending[0].headers, header));
}
/* The built-in service has no proxy: no settings screen offers one, in a title, a detail or a longer text. */
static void no_proxy_offer(void) {
    static const char *const screens[] = {"", "discover", "library", "settings", "settings:sb", "settings:dearrow", "settings:extra:captions",
        "settings:extra:audio", "settings:extra:player", "settings:extra:buttons", "settings:extra:dimming", "settings:search", "settings:sections",
        "settings:ui", "settings:hide", "settings:thumbnails", "settings:experimental", "settings:accounts", "settings:backup", "settings:about",
        "settings:content-language", "settings:channel-sort", "settings:history", "settings:playback-mode"};
    for (unsigned s = 0; s < sizeof screens / sizeof *screens; ++s) for (int page = 0; page < 8; ++page) {
        evo_provider_youtube.list_catalog(screens[s], page, items_cb, NULL);
        if (!last_ok || !last_count) break;
        for (int i = 0; i < last_count; ++i)
            assert(!strstr(last_items[i].title, "roxy") && !strstr(last_items[i].subtitle, "roxy") && !strstr(last_items[i].overview, "roxy"));
    }
    assert(!pending_count);
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
    /* With a companion the alternative SponsorBlock server is its host's choice: one switch, no address to type. */
    p->list_catalog("settings:sb", 0, items_cb, NULL);
    assert(last_ok && find("settings:sb:alt") >= 0 && find("settings:sb:server") < 0 && strstr(last_items[find("settings:sb:alt")].subtitle, "companion host chooses"));
    p->list_catalog("settings:sb:server", 0, items_cb, NULL);
    tube_playlist_prompt_t unasked; assert(last_ok && !tube_take_playlist_prompt(&unasked));
    assert(tube_voice_scope(source, sizeof source, token, sizeof token, profile, sizeof profile) && !strcmp(source, "http://192.168.1.50:8765"));
    p->shutdown();
    assert(!strcmp(stored("source"), "http://192.168.1.50:8765") && !strcmp(stored("token"), "abcdefghijklmnopqrstuvwxabcdefghijkl"));
    assert(!service_starts && !service_stops);

    /* ---- The built-in service: started at init, no pairing, its token on the first request. ---- */
    assert(!unlink(flag));
    assert(!p->init() && tube_embedded_mode() && service_starts == 1 && !strcmp(service_dir, dir));
    assert(!strcmp(p->get_source(), "http://127.0.0.1:23456") && !*tube_embedded_error());
    assert(service_sponsorblock);   /* segments, when the user turns them on, are answered by the service */
    /* So are dislike counts, for a profile that turned them on. No alternative server is named at the start: the one typed
     * in the settings goes with each request, and a change needs no restart. */
    assert(service_dislikes && !service_sb_server);
    assert(p->list_catalog("", 0, items_cb, NULL) == 0 && last_ok && find("settings:service-retry") < 0);
    /* Before the service said what it offers, the account rows are there: the service the app carries has sign-in. */
    p->list_catalog("library", 0, items_cb, NULL);
    assert(last_ok && !pending_count && find("auth:start") >= 0 && find("library:profiles") >= 0 && find("account:notice") < 0);
    assert(p->search("cats", 0, items_cb, NULL) == 0);
    sent_to_service(23456, TOKEN_A);
    assert(service_starts == 1);          /* running: the request did not start it again */
    poll_job(none); assert(last_ok && !pending_count);
    /* No address to set. This service bundle lists no phone receiver: no phone rows, and nothing is asked of it.
     * Voice search is never part of the built-in service: its scope stays closed. */
    assert(p->set_source("http://192.168.1.60:8765#654321") < 0 && !strcmp(p->get_source(), "http://127.0.0.1:23456"));
    assert(!settings_has("settings:remote"));
    p->list_catalog("settings:remote:pair", 0, items_cb, NULL); assert(!pending_count);
    assert(!tube_voice_scope(source, sizeof source, token, sizeof token, profile, sizeof profile) && !*source && !*token);
    /* A change that saves: the external pairing is still in the store, the loopback one never is. */
    p->list_catalog("settings:ask-stream", 0, items_cb, NULL); assert(last_ok);
    assert(!strcmp(stored("source"), "http://192.168.1.50:8765") && !strcmp(stored("token"), "abcdefghijklmnopqrstuvwxabcdefghijkl"));

    /* A service that does not list sign-in (an older service bundle): no sign-in, no profile or account rows, one explanation; Home stays.
     * Nor does it list the subscriptions import: that row is gone as well. */
    p->list_catalog("settings:backup", 0, items_cb, NULL); assert(last_ok && find("settings:backup:create") >= 0 && find("settings:backup:import") < 0);
    /* It lists neither the dislike relay nor a server named by the request: no row that would do nothing. */
    p->list_catalog("settings:extra:player", 0, items_cb, NULL); assert(last_ok && last_count > 3 && find("settings:dislikes") < 0);
    p->list_catalog("settings:sb", 0, items_cb, NULL);
    assert(last_ok && find("settings:sb:repeat") >= 0 && find("settings:sb:alt") < 0 && find("settings:sb:server") < 0);
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
    no_proxy_offer();

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
    service_accounts = 1;
    assert(tube_embedded_start() && service_starts == starts + 1 && !strcmp(p->get_source(), "http://127.0.0.1:23456"));
    assert(p->search("birds", 0, items_cb, NULL) == 0); sent_to_service(23456, TOKEN_B);
    poll_job(none); assert(last_ok);
    /* The service lists sign-in: accounts, profiles and the account rows are offered, in this console's words. */
    p->list_catalog("discover", 0, items_cb, NULL);
    assert(last_ok && find("library") >= 0 && strcmp(last_items[find("library")].title, "Accounts") && !strstr(last_items[find("library")].subtitle, "later version"));
    p->list_catalog("library", 0, items_cb, NULL);
    assert(last_ok && !pending_count && find("account:notice") < 0 && find("auth:start") >= 0 && find("library:profiles") >= 0 &&
           find("library:subscriptions") >= 0 && find("library:history") >= 0 && find("library:watch_later") >= 0 && find("library:playlists") >= 0 &&
           find("library:channels") >= 0 && find("library:notifications") >= 0 && find("section:my_videos") >= 0);
    assert(strstr(last_items[find("library:profiles")].subtitle, "signed in on this PS5"));
    for (int i = 0; i < last_count; ++i) assert(!strstr(last_items[i].title, "ompanion") && !strstr(last_items[i].subtitle, "ompanion"));
    /* The service lists the phone receiver: its rows are back, and linking a phone asks the service itself, with its token. */
    assert(settings_has("settings:remote"));
    p->list_catalog("settings:experimental", 0, items_cb, NULL);
    assert(last_ok && strstr(last_items[2].subtitle, "Phone pairing") && !strstr(last_items[2].subtitle, "oice"));
    p->list_catalog("settings:remote:pair", 0, items_cb, NULL); assert(last_ok && !pending_count);
    tube_poll();
    assert(pending_count == 1 && !strcmp(pending[0].url, "http://127.0.0.1:23456/v1/remote") && !strcmp(pending[0].method, "POST"));
    assert(strstr(pending[0].headers, "Authorization: Bearer " TOKEN_B "\n") && !strstr(pending[0].body, "voice_enabled"));
    cJSON *sent = cJSON_Parse(pending[0].body); assert(sent && !strcmp(cJSON_GetObjectItem(sent, "action")->valuestring, "start"));
    char device[40], answer[512]; snprintf(device, sizeof device, "%s", cJSON_GetObjectItem(sent, "device_id")->valuestring);
    assert(strlen(device) == 36 && !strcmp(stored("remote_device"), device) && !*cJSON_GetObjectItem(sent, "screen_id")->valuestring);
    cJSON_Delete(sent);
    snprintf(answer, sizeof answer, "{\"api_version\":1,\"session\":\"0123456789abcdef0123456789abcdef\",\"device_id\":\"%s\","
             "\"screen_id\":\"screen-123\",\"name\":\"ProsperoTube PS5\",\"state\":\"listening\",\"error\":\"\"}", device);
    reply(0, 1, 200, answer);
    tube_poll();
    assert(pending_count == 1 && strstr(pending[0].body, "\"action\":\"pair\"") && !strcmp(stored("remote_screen"), "screen-123"));
    reply(0, 1, 200, "{\"api_version\":1,\"session\":\"0123456789abcdef0123456789abcdef\",\"state\":\"listening\",\"error\":\"\",\"pairing_code\":\"123456789012\"}");
    p->list_catalog("settings:remote", 0, items_cb, NULL);
    assert(last_ok && !strcmp(last_items[0].title, "Phone connection: ready") && !strcmp(last_items[1].title, "TV code: 123 456 789 012"));
    for (int i = 0; i < last_count; ++i) assert(!strstr(last_items[i].title, "ompanion") && !strstr(last_items[i].subtitle, "ompanion"));
    assert(!tube_voice_scope(source, sizeof source, token, sizeof token, profile, sizeof profile));   /* voice search stays out */
    /* The saved state keeps the receiver's identity and nothing of the session, the code or the service. */
    assert(!strcmp(stored("source"), "http://192.168.1.50:8765") && !strcmp(stored("remote_device"), device));
    p->list_catalog("settings:remote:toggle", 0, items_cb, NULL);   /* off: the receiver is closed at the service, the identity is kept */
    assert(pending_count == 1 && strstr(pending[0].body, "\"action\":\"stop\"") && strstr(pending[0].url, "http://127.0.0.1:23456/v1/remote"));
    reply(0, 1, 200, "{\"api_version\":1,\"state\":\"stopped\"}");
    tube_poll(); assert(!pending_count && !strcmp(stored("remote_device"), device) && !strcmp(stored("remote_screen"), "screen-123"));
    p->list_catalog("settings:backup", 0, items_cb, NULL); assert(last_ok && find("settings:backup:import") >= 0);   /* the service reads exports */

    /* The service lists the dislike relay: the row says what turning it on discloses, and Return YouTube Dislike is asked,
     * through the service, only for a profile whose switch is on: once per opened details page. */
    static const char *details = "{\"state\":\"done\",\"result\":{\"api_version\":1,\"kind\":\"video_details\",\"id\":\"abcdefghijk\",\"title\":\"Premiere\","
        "\"channel\":{\"id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"name\":\"Channel A\",\"subscribers\":\"\",\"art_url\":\"\"},\"published\":\"2026-10-01\","
        "\"views\":\"12\",\"like_count\":\"4\",\"description\":\"\",\"description_truncated\":false,\"duration_sec\":125,\"badges\":[],\"has_related\":false}}";
    tube_cancel(); drop_pending();
    p->list_catalog("settings:extra:player", 0, items_cb, NULL);
    int row = find("settings:dislikes");
    assert(last_ok && row >= 0 && !strcmp(last_items[row].title, "Show dislikes: off"));
    assert(!strcmp(last_items[row].subtitle, "In descriptions and details; Return YouTube Dislike then learns which videos you open them for"));
    assert(p->list_catalog("details:abcdefghijk", 0, items_cb, NULL) == 0); sent_to_service(23456, TOKEN_B); poll_job(details);
    assert(last_ok && !pending_count && last_count > 2);            /* off: the page is shown and nothing more is asked */
    const int plain = last_count;
    p->list_catalog("settings:dislikes", 0, items_cb, NULL);
    assert(last_ok && !strcmp(last_items[find("settings:dislikes")].title, "Show dislikes: on") && !pending_count);   /* the switch itself asks nothing */
    calls = item_calls;
    assert(p->list_catalog("details:abcdefghijk", 0, items_cb, NULL) == 0); sent_to_service(23456, TOKEN_B); poll_job(details);
    assert(pending_count == 1 && !strcmp(pending[0].url, "http://127.0.0.1:23456/v1/dislikes/abcdefghijk") && !strcmp(pending[0].method, "GET"));
    assert(strstr(pending[0].headers, "Authorization: Bearer " TOKEN_B "\n") && item_calls == calls);
    reply(0, 1, 200, "{\"api_version\":1,\"enabled\":true,\"id\":\"abcdefghijk\",\"available\":true,\"likes\":4,\"dislikes\":3}");
    assert(last_ok && !pending_count && last_count == plain + 1 && !strcmp(last_items[plain].title, "3 dislikes"));
    /* No count, or no answer: the page has no dislike row and no message. */
    error_text[0] = 0;
    assert(p->list_catalog("details:abcdefghijk", 0, items_cb, NULL) == 0); sent_to_service(23456, TOKEN_B); poll_job(details);
    assert(pending_count == 1); reply(0, 1, 200, "{\"api_version\":1,\"enabled\":true,\"id\":\"abcdefghijk\",\"available\":false}");
    assert(last_ok && !pending_count && last_count == plain && !*error_text);
    assert(p->list_catalog("details:abcdefghijk", 0, items_cb, NULL) == 0); sent_to_service(23456, TOKEN_B); poll_job(details);
    assert(pending_count == 1); reply(0, 0, 0, "");
    assert(last_ok && !pending_count && last_count == plain && !*error_text);
    p->list_catalog("settings:dislikes", 0, items_cb, NULL);   /* off again */
    assert(p->list_catalog("details:abcdefghijk", 0, items_cb, NULL) == 0); sent_to_service(23456, TOKEN_B); poll_job(details);
    assert(last_ok && !pending_count && last_count == plain);

    /* The alternative SponsorBlock server: no host chooses one here, so its address is typed on the console, kept as a
     * plain https origin and sent with the segments request of a profile that turned the switch on. */
    p->list_catalog("settings:sb", 0, items_cb, NULL);
    assert(last_ok && find("settings:sb:alt") >= 0 && find("settings:sb:server") == find("settings:sb:alt") + 1);
    assert(!strcmp(last_items[find("settings:sb:server")].title, "Alternative server: not set") && strstr(last_items[find("settings:sb:alt")].subtitle, "below first"));
    tube_playlist_prompt_t prompt; char opened[80];
    p->list_catalog("settings:sb:server", 0, items_cb, NULL);       /* the same screen, with the keyboard over it */
    assert(last_ok && find("settings:sb:server") >= 0 && tube_take_playlist_prompt(&prompt));
    assert(!strcmp(prompt.label, "Alternative SponsorBlock server") && !strcmp(prompt.initial, "https://"));
    static const char *const no_origin[] = {"http://sb.example.org", "https://sb.example.org:8443", "https://user@sb.example.org", "https://sb.example.org/api",
        "https://sb.example.org?x=1", "https://sb.example.org/#a", "https://sb..example.org", "https://-sb.example.org", "https://sb.example.org.", "https://",
        "sb example.org", "ftp://sb.example.org", "https://sb.example.org//", "https://s\xc3\xa9.example.org"};
    for (unsigned i = 0; i < sizeof no_origin / sizeof *no_origin; ++i) {
        assert(tube_submit_playlist_name(prompt.ticket, no_origin[i], opened, sizeof opened) < 0 && strstr(tube_prompt_error(), "https address"));
        assert(!strcmp(stored("sponsorblock_server"), "") || !strcmp(stored("sponsorblock_server"), "<absent>"));
    }
    assert(!tube_submit_playlist_name(prompt.ticket, "  HTTPS://SB.Example.org/ ", opened, sizeof opened) && !*tube_prompt_error());
    assert(!strcmp(opened, "settings:sb") && !strcmp(tube_prompt_title(opened), "SponsorBlock actions"));
    assert(!strcmp(stored("sponsorblock_server"), "https://sb.example.org"));
    assert(tube_submit_playlist_name(prompt.ticket, "https://other.example.org", opened, sizeof opened) < 0);   /* one text per prompt */
    p->list_catalog("settings:sb", 0, items_cb, NULL);
    assert(!strcmp(last_items[find("settings:sb:server")].title, "Alternative server: sb.example.org") && strstr(last_items[find("settings:sb:alt")].subtitle, "server below"));
    for (int i = 0; i < last_count; ++i) assert(!strstr(last_items[i].title, "ompanion") && !strstr(last_items[i].subtitle, "ompanion"));
    p->list_catalog("settings:sb:server", 0, items_cb, NULL);       /* changing it starts from the saved address; the scheme may be left out */
    assert(tube_take_playlist_prompt(&prompt) && !strcmp(prompt.initial, "https://sb.example.org"));
    assert(!tube_submit_playlist_name(prompt.ticket, "mirror.example.net", opened, sizeof opened) && !strcmp(stored("sponsorblock_server"), "https://mirror.example.net"));
    static const char *const play = "{\"state\":\"done\",\"result\":{\"id\":\"%s\",\"duration_sec\":100,\"choices\":["
        "{\"url\":\"http://127.0.0.1:23456/media/s2/manifest.mpd\",\"container\":\"dash\"}]}}";
    p->list_catalog("settings:sponsor", 0, items_cb, NULL);         /* segments on; the alternative server still off */
    for (int pass = 0; pass < 2; ++pass) {
        char job[512], id[12]; int asked = -1;
        snprintf(id, sizeof id, "%cbcdefghijk", 'b' + pass); snprintf(job, sizeof job, play, id);
        tube_cancel(); drop_pending();
        assert(p->resolve(id, resolve_cb, NULL) == 0); sent_to_service(23456, TOKEN_B); poll_job(job);
        for (int i = 0; i < pending_count; ++i) if (strstr(pending[i].url, "http://127.0.0.1:23456/v1/sponsor/") == pending[i].url) asked = i;
        assert(last_ok && asked >= 0 && strstr(pending[asked].url, id) && strstr(pending[asked].headers, "Authorization: Bearer " TOKEN_B "\n"));
        if (!pass) assert(!strstr(pending[asked].url, "alt=") && !strstr(pending[asked].url, "server="));
        else assert(strstr(pending[asked].url, "&alt=true&server=https%3A%2F%2Fmirror.example.net") && !strstr(pending[asked].url, "sb.example.org"));
        drop_pending();
        if (!pass) p->list_catalog("settings:sb:alt", 0, items_cb, NULL);   /* on */
    }
    p->list_catalog("settings:sb:alt", 0, items_cb, NULL); p->list_catalog("settings:sponsor", 0, items_cb, NULL);   /* both off again */
    tube_cancel(); drop_pending();

    /* Backups and sign-ins: the service keeps the accounts' tokens in a file of its own. A backup is the state file
     * alone, the screen says so in this console's words, and a restore leaves that file and the running service alone. */
    static const char secret[] = "{\"version\":1,\"profiles\":[{\"id\":\"oauth_0123456789abcdef\",\"name\":\"Alice\",\"cookie\":\"\","
        "\"oauth\":{\"access_token\":\"ya29.SECRET-ACCESS\",\"refresh_token\":\"1//SECRET-REFRESH\"}}]}\n";
    char accounts[640], name[64], entry[160], copy_path[700], kept[sizeof secret + 8];
    snprintf(accounts, sizeof accounts, "%s/accounts", dir); assert(!mkdir(accounts, 0700));
    snprintf(accounts, sizeof accounts, "%s/accounts/oauth-profiles.json", dir);
    f = fopen(accounts, "wb"); assert(f && fputs(secret, f) >= 0 && !fclose(f) && !chmod(accounts, 0600));
    p->list_catalog("library:profile:oauth_0123456789abcdef", 0, items_cb, NULL); assert(last_ok);   /* the account's profile is the selected one */
    assert(!strcmp(stored("library_profile"), "oauth_0123456789abcdef"));
    p->list_catalog("settings:backup", 0, items_cb, NULL);
    assert(last_ok && !pending_count && find("settings:backup:create") == 0 && find("account:notice:backup-accounts") == last_count - 1);
    assert(!strcmp(last_items[0].subtitle, "A dated copy of settings and library in the app data folder; sign-ins to YouTube are not included"));
    row = find("account:notice:backup-accounts");
    assert(!strcmp(last_items[row].title, "Sign-ins to YouTube are not part of a backup"));
    assert(!strcmp(last_items[row].subtitle, "Restoring a backup neither adds nor removes an account on this PS5"));
    assert(strstr(last_items[row].overview, "starts as a new profile.") && strlen(last_items[row].overview) < sizeof last_items[row].overview - 1);
    for (int i = 0; i < last_count; ++i) assert(!strstr(last_items[i].title, "ompanion") && !strstr(last_items[i].subtitle, "ompanion") &&
        !strstr(last_items[i].subtitle, "pair") && !strstr(last_items[i].overview, "ompanion"));
    p->list_catalog("settings:backup:create", 0, items_cb, NULL);
    assert(last_ok && !strcmp(last_items[0].title, "Backup created") && find("account:notice:backup-accounts") == last_count - 1);
    snprintf(name, sizeof name, "%.39s", last_items[0].subtitle);
    /* The backup folder holds that one file, and nothing of the accounts is in it. */
    snprintf(copy_path, sizeof copy_path, "%s/backup", dir);
    DIR *folder = opendir(copy_path); assert(folder);
    int files = 0;
    for (struct dirent *e; (e = readdir(folder)) != NULL;) if (e->d_name[0] != '.') { ++files; assert(!strcmp(e->d_name, name)); }
    closedir(folder); assert(files == 1);
    snprintf(copy_path, sizeof copy_path, "%s/backup/%s", dir, name);
    static char copied[1 << 16];
    f = fopen(copy_path, "rb"); assert(f); copied[fread(copied, 1, sizeof copied - 1, f)] = 0; fclose(f);
    assert(strstr(copied, "\"library_profile\":\t\"oauth_0123456789abcdef\"") || strstr(copied, "\"library_profile\":\"oauth_0123456789abcdef\""));
    assert(!strstr(copied, "SECRET") && !strstr(copied, "refresh_token") && !strstr(copied, "ya29") && !strstr(copied, "oauth-profiles"));
    /* The review of a restore names no pairing here; it says what happens to the accounts. */
    snprintf(entry, sizeof entry, "settings:backup:entry:%s", name);
    p->list_catalog(entry, 0, items_cb, NULL);
    assert(last_ok && last_count == 2 && !strcmp(last_items[0].title, "Restore this backup") && strstr(last_items[0].overview, " UTC. Sign-ins to YouTube are not part of it"));
    assert(strstr(last_items[0].overview, "stay as they are. Cross restores") && !strstr(last_items[0].overview, "pair") && !strstr(last_items[0].overview, "ompanion"));
    /* The account signs out (here: its file loses it) and Guest is selected; then the backup is restored. */
    p->list_catalog("library:profile:guest", 0, items_cb, NULL); assert(last_ok && !strcmp(stored("library_profile"), "guest"));
    const int stops = service_stops;
    char restore[160];
    p->list_catalog(entry, 0, items_cb, NULL); assert(last_ok);
    snprintf(restore, sizeof restore, "%s", last_items[0].id);
    p->list_catalog(restore, 0, items_cb, NULL);
    assert(last_ok && !strcmp(last_items[0].title, "Backup restored") && !pending_count);
    assert(service_stops == stops && service_up && !strcmp(p->get_source(), "http://127.0.0.1:23456"));   /* the service ran on: no session of it was retired */
    f = fopen(accounts, "rb"); assert(f); kept[fread(kept, 1, sizeof kept - 1, f)] = 0; fclose(f);
    struct stat mode; assert(!strcmp(kept, secret) && !stat(accounts, &mode) && (mode.st_mode & 0777) == 0600);
    p->list_catalog("settings:ask-stream", 0, items_cb, NULL); p->list_catalog("settings:ask-stream", 0, items_cb, NULL);   /* something saves */
    assert(!strcmp(stored("library_profile"), "oauth_0123456789abcdef"));   /* the backup's selection came back with it */
    /* That profile's sign-in is not on this console any more: the next profile list says so, and browsing continues as Guest. */
    assert(p->list_catalog("library:profiles", 0, items_cb, NULL) == 0); sent_to_service(23456, TOKEN_B);
    poll_job("{\"state\":\"done\",\"result\":{\"api_version\":1,\"profiles\":[{\"id\":\"guest\",\"name\":\"Guest\",\"account_configured\":false}]}}");
    assert(last_ok && last_count == 1 && !strcmp(stored("library_profile"), "guest"));
    no_proxy_offer();
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
    puts("PASS: built-in service start without pairing, token on requests, kept external pairing, failure and retry, stop and resume, development switch, artwork hosts, stream headers, "
         "dislikes asked only with the profile's switch on, typed SponsorBlock server, no proxy offer, backups without the sign-ins");
    return 0;
}
