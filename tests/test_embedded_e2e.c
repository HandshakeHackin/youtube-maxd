/* End to end on the host, as close to the console as a PC gets: the shipping
 * provider in built-in mode, EVO's own HTTP client, and the real service
 * (engine, bundle, listener) in this process. YouTube is played by the
 * service's canned answers (TUBE_JS_FIXTURES); no request leaves the machine.
 *
 *   start -> health -> search -> resolve -> the manifest and the captions
 *   fetched through the provider's HTTP path -> stop
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_provider.h"
#include "tube_service.h"
#include "evo_net.h"
#include "evo_data_path.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* The provider configures the service as the console build does (embedded
 * bundle and roots); here the bundle is a file, added to what it asked for. */
void __real_tube_service_configure(const tube_service_options *options);
void __wrap_tube_service_configure(const tube_service_options *options) {
    static tube_service_options host;
    host = *options;
    host.bundle_path = getenv("TUBE_E2E_BUNDLE");
    __real_tube_service_configure(&host);
}

static int item_calls, last_ok, last_count, resolve_calls, fetch_calls, fetch_ok, fetch_status;
static evo_provider_item_t last_items[26];
static evo_stream_choice_t choice;
static char fetched[1 << 16];

static void items_cb(int ok, const evo_provider_item_t *items, int count, int more, void *ud) {
    (void)ud; (void)more; ++item_calls; last_ok = ok; last_count = count;
    assert(count <= 26);
    if (items) memcpy(last_items, items, (size_t)count * sizeof *items);
}
static void resolve_cb(int ok, const evo_stream_choice_t *choices, int count, void *ud) {
    (void)ud; ++resolve_calls; last_ok = ok;
    if (ok && count) choice = choices[0];
}
static void fetch_cb(int ok, int status, const char *body, size_t size, void *ud) {
    (void)ud; ++fetch_calls; fetch_ok = ok; fetch_status = status;
    snprintf(fetched, sizeof fetched, "%.*s", (int)(size < sizeof fetched - 1 ? size : sizeof fetched - 1), body ? body : "");
}
static double seconds(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (double)t.tv_sec + (double)t.tv_nsec / 1e9; }
/* The app's frame loop in small: network completions, then the provider's poll. */
static void run_until(const int *counter, int target, double limit, const char *what) {
    double end = seconds() + limit;
    while (*counter < target) {
        if (seconds() > end) { fprintf(stderr, "timed out waiting for %s\n", what); abort(); }
        evo_net_poll(); tube_poll(); usleep(5000);
    }
}
static void fetch(const char *url, int authorized) {
    char source[512], token[128], profile[33], auth[160];
    const char *headers[1] = {auth};
    (void)source; (void)token; (void)profile;
    int calls = fetch_calls;
    snprintf(auth, sizeof auth, "Authorization: Bearer %s", "none");
    assert(evo_net_request_async("GET", url, NULL, headers, authorized ? 1 : 0, fetch_cb, NULL) == 0);
    run_until(&fetch_calls, calls + 1, 20, url);
}

int main(void) {
    char dir[] = "/tmp/tube-e2e-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE", dir, 1));
    /* TUBE_E2E_LIVE=1 (scripts/probe_live_embedded.sh, never a test): the same
     * flow against YouTube as Guest, then the service is kept up while a player
     * outside this process reads the manifest. */
    const int live = getenv("TUBE_E2E_LIVE") != NULL;
    const char *query = live ? getenv("TUBE_E2E_QUERY") : "canned", *wanted = live ? getenv("TUBE_E2E_VIDEO") : "fixtureVID1";
    assert((live || getenv("TUBE_JS_FIXTURES")) && getenv("TUBE_E2E_BUNDLE") && query && wanted);
    const evo_provider_t *p = &evo_provider_youtube;
    assert(evo_net_init() == 0);

    /* Start: the service is up before the first request, on loopback, in the console's port range. */
    double started = seconds();
    assert(!p->init() && tube_embedded_mode() && tube_service_running() && !*tube_embedded_error());
    unsigned port = 0;
    assert(sscanf(tube_embedded_address(), "127.0.0.1:%u", &port) == 1 && port >= 20000 && port <= 39999);
    printf("service started in %.0f ms on 127.0.0.1:%u\n", (seconds() - started) * 1000, port);

    /* Without the token the service answers nothing but a refusal. */
    char url[512];
    snprintf(url, sizeof url, "http://127.0.0.1:%u/v1/search?q=canned", port);
    fetch(url, 0);
    assert(fetch_status == 401);

    /* Search: health first (the feature list), then the job, then rows. */
    int calls = item_calls;
    assert(p->search(query, 0, items_cb, NULL) == 0);
    run_until(&item_calls, calls + 1, 30, "search results");
    assert(last_ok && last_count >= 2);
    int video = -1;
    for (int i = 0; i < last_count; ++i) if (!last_items[i].is_folder && strlen(last_items[i].id) == 11 && video < 0) video = i;
    assert(video >= 0 && *last_items[video].title);
    char id[16]; snprintf(id, sizeof id, "%s", last_items[video].id);
    printf("search: %d rows, first video %s \"%s\", art %s\n", last_count, id, last_items[video].title, last_items[video].art_url);
    /* The artwork is YouTube's own and passes the allowlist. */
    assert(!*last_items[video].art_url || tube_art_url_allowed(last_items[video].art_url));
    assert(tube_art_url_allowed("https://i.ytimg.com/vi/x/hqdefault.jpg") && !tube_art_url_allowed("https://example.org/a.jpg"));

    /* Open the result (captions are on by default): the choice is a manifest on the service. */
    calls = resolve_calls;
    assert(p->resolve(wanted, resolve_cb, NULL) == 0);
    run_until(&resolve_calls, calls + 1, 30, "resolve");
    assert(last_ok);
    char prefix[64]; snprintf(prefix, sizeof prefix, "http://127.0.0.1:%u/media/", port);
    assert(!strncmp(choice.url, prefix, strlen(prefix)) && strstr(choice.url, ".mpd"));
    printf("resolve: %s %s %dx%d %s/%s headers=\"%s\"\n", choice.label, choice.container, choice.width, choice.height,
           choice.video_codec, choice.audio_codec, choice.headers);
    for (const char *c = choice.headers; *c; ++c) assert(*c == '\r' || *c == '\n' || (*c >= 32 && *c < 127));

    /* The manifest, fetched the way the player's first request does: no token on /media. */
    fetch(choice.url, 0);
    assert(fetch_ok && fetch_status == 200 && strstr(fetched, "<MPD") && strstr(fetched, "googlevideo.com"));
    assert(!strstr(fetched, "127.0.0.1"));   /* media is never relayed */
    printf("manifest: %zu bytes, direct media addresses\n", strlen(fetched));

    /* Captions: the track list arrived with the resolve, and the provider wrote the chosen track for the player. */
    if (live) {
        FILE *out = fopen(getenv("TUBE_E2E_URL_FILE"), "w"); assert(out);
        fprintf(out, "%s\n", choice.url); fclose(out);
        for (double end = seconds() + 150; seconds() < end && access(getenv("TUBE_E2E_DONE_FILE"), F_OK);) { evo_net_poll(); tube_poll(); usleep(20000); }
        p->shutdown(); assert(!tube_service_running()); evo_net_shutdown();
        puts("live: service stopped");
        return 0;
    }
    p->stream_chosen("fixtureVID1", &choice);   /* what the host does before playback opens */
    for (int i = 0; i < 400 && !tube_caption_track_count(); ++i) { evo_net_poll(); tube_poll(); usleep(5000); }
    assert(tube_caption_track_count() >= 1);
    char path[600]; snprintf(path, sizeof path, "%s", evo_data_path("active.srt"));
    FILE *srt = NULL;
    for (int i = 0; i < 600 && !(srt = fopen(path, "rb")); ++i) { evo_net_poll(); tube_poll(); usleep(5000); }
    assert(srt);
    char line[128] = ""; size_t got = fread(line, 1, sizeof line - 1, srt); fclose(srt); line[got] = 0;
    assert(got > 10 && strstr(line, "-->"));
    printf("captions: %d track(s), first \"%s\", SubRip written for the player\n", tube_caption_track_count(), tube_caption_track_language(0));

    /* The saved state never names the service. */
    p->list_catalog("settings:ask-stream", 0, items_cb, NULL);
    FILE *state = fopen(evo_data_path("youtube.json"), "rb"); assert(state);
    static char text[1 << 16]; got = fread(text, 1, sizeof text - 1, state); fclose(state); text[got] = 0;
    assert(!strstr(text, "127.0.0.1") && strstr(text, "\"token\":\"\""));

    /* Stop: joined and silent; the old port no longer answers; a request fails without going out. */
    tube_embedded_stop();
    assert(!tube_service_running());
    calls = fetch_calls;
    fetch(choice.url, 0);
    assert(!fetch_ok);
    calls = item_calls;
    p->search("canned", 0, items_cb, NULL);
    assert(item_calls == calls + 1 && !last_ok);
    /* And it comes back on request. */
    assert(tube_embedded_start() && tube_service_running());
    calls = item_calls;
    assert(p->search("canned", 0, items_cb, NULL) == 0);
    run_until(&item_calls, calls + 1, 30, "search after restart");
    assert(last_ok && last_count >= 2);
    p->shutdown();
    assert(!tube_service_running());
    evo_net_shutdown();
    puts("PASS: provider on the real built-in service: start, search, resolve, manifest, captions, stop and restart");
    return 0;
}
