/* End to end on the host, as close to the console as a PC gets: the shipping
 * provider in built-in mode, EVO's own HTTP client, and the real service
 * (engine, bundle, listener) in this process. YouTube is played by the
 * service's canned answers (TUBE_JS_FIXTURES); no request leaves the machine.
 *
 *   start -> health -> search -> resolve -> the manifest and the captions
 *   fetched through the provider's HTTP path -> stop
 * TUBE_E2E_LIVE=1 with TUBE_E2E_TYPING=<text> (never a test) asks YouTube, as
 * Guest, only what Afterglow's search asks while that text is typed: its
 * completions and its matches. Two requests, then the service stops.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_provider.h"
#include "tube_remote.h"
#include "tube_service.h"
#include "evo_net.h"
#include "evo_data_path.h"
#include <assert.h>
#include <math.h>
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
    host.lounge_base = getenv("TUBE_E2E_LOUNGE");   /* the stand-in for YouTube's phone connection service; unset = YouTube */
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

    if (live && getenv("TUBE_E2E_TYPING")) {
        for (int step = 0; step < 2; ++step) {
            char catalog[200]; int before = item_calls, videos = 0; double asked = seconds();
            snprintf(catalog, sizeof catalog, "%s:%.120s", step ? "search-preview" : "search-suggest", getenv("TUBE_E2E_TYPING"));
            assert(p->list_catalog(catalog, 0, items_cb, NULL) == 0);
            run_until(&item_calls, before + 1, 30, catalog);
            for (int i = 0; i < last_count; ++i) videos += !last_items[i].is_folder && strlen(last_items[i].id) == 11;
            printf("%s: %s, %d rows (%d videos) in %.0f ms", catalog, last_ok ? "ok" : "failed", last_count, videos, (seconds() - asked) * 1000);
            for (int i = 0; last_ok && i < last_count && i < 4; ++i) printf("%s\"%.40s\"", i ? ", " : "; ", last_items[i].title);
            puts("");
        }
        p->shutdown(); assert(!tube_service_running()); evo_net_shutdown();
        return 0;
    }

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

    /* SponsorBlock from an alternative server: its address is typed on the console and goes with the segments request. */
    if (!live) {
        tube_playlist_prompt_t prompt; char opened[80];
        p->list_catalog("settings:sponsor", 0, items_cb, NULL); p->list_catalog("settings:sb:alt", 0, items_cb, NULL);
        p->list_catalog("settings:sb:server", 0, items_cb, NULL);
        assert(last_ok && tube_take_playlist_prompt(&prompt) && !tube_submit_playlist_name(prompt.ticket, "sb.example.org", opened, sizeof opened));
        p->list_catalog(opened, 0, items_cb, NULL);
        int server_row = -1;
        for (int i = 0; i < last_count; ++i) if (!strcmp(last_items[i].id, "settings:sb:server")) server_row = i;
        assert(last_ok && server_row > 0 && !strcmp(last_items[server_row].title, "Alternative server: sb.example.org") &&
               !strcmp(last_items[server_row - 1].title, "Use alternative server: on"));
    }

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
    /* The segments are the alternative server's (3 to 9 s), not SponsorBlock's own (12.5 to 40 s and more). */
    tube_sponsor_marker_t marks[8]; int marked = 0;
    for (int i = 0; i < 600 && !(marked = tube_sponsor_markers(635, marks, 8)); ++i) { evo_net_poll(); tube_poll(); usleep(5000); }
    assert(marked == 1 && fabs(marks[0].start * 635 - 3) < 1e-6 && fabs(marks[0].end * 635 - 9) < 1e-6 && !marks[0].point);
    puts("sponsor: one segment, from the alternative server named in the request");

    /* Dislikes: off, a details page lists none and Return YouTube Dislike is not asked; turned on for this profile, the
     * service asks it (here: its canned answer) and the page gains the row. */
    calls = item_calls;
    assert(p->list_catalog("details:fixtureVID1", 0, items_cb, NULL) == 0);
    run_until(&item_calls, calls + 1, 30, "video details");
    assert(last_ok && last_count >= 3);
    const int plain = last_count;
    for (int i = 0; i < last_count; ++i) assert(strcmp(last_items[i].id, "account:notice:dislikes"));
    p->list_catalog("settings:dislikes", 0, items_cb, NULL); assert(last_ok);
    calls = item_calls;
    assert(p->list_catalog("details:fixtureVID1", 0, items_cb, NULL) == 0);
    run_until(&item_calls, calls + 1, 30, "video details with dislikes");
    assert(last_ok && last_count == plain + 1 && !strcmp(last_items[plain].id, "account:notice:dislikes"));
    assert(!strcmp(last_items[plain].title, "7 dislikes") && strstr(last_items[plain].subtitle, "Return YouTube Dislike"));
    printf("dislikes: \"%s\" on the details page of a profile that turned them on\n", last_items[plain].title);
    p->list_catalog("settings:dislikes", 0, items_cb, NULL);   /* off again */

    /* Subscriptions import: the user's export file goes to the service as the request body and comes back as channels. */
    p->list_catalog("settings:backup", 0, items_cb, NULL);
    int import_row = 0;
    for (int i = 0; i < last_count; ++i) if (!strcmp(last_items[i].id, "settings:backup:import")) import_row = 1;
    assert(last_ok && import_row);
    p->list_catalog("settings:backup:import", 0, items_cb, NULL);   /* creates the folder */
    snprintf(path, sizeof path, "%s/subs.json", evo_data_path("import"));
    FILE *export = fopen(path, "wb"); assert(export);
    fputs("{\"subscriptions\":[{\"service_id\":0,\"url\":\"https://www.youtube.com/channel/UCaaaaaaaaaaaaaaaaaaaaaa\",\"name\":\"Alpha\"},"
          "{\"service_id\":0,\"url\":\"https://www.youtube.com/@handle\",\"name\":\"No identity\"},", export);
    for (int i = 0; i < 400; ++i) fprintf(export, "{\"service_id\":3,\"url\":\"https://example.org/%040d\",\"name\":\"Elsewhere\"},", i);   /* well over 8 KiB */
    fputs("{\"service_id\":0,\"url\":\"https://www.youtube.com/channel/UCbbbbbbbbbbbbbbbbbbbbbb\",\"name\":\"Beta\"}]}", export);
    assert(!fclose(export));
    calls = item_calls;
    assert(p->list_catalog("settings:backup:import:file:subs.json", 0, items_cb, NULL) == 0);
    run_until(&item_calls, calls + 1, 30, "subscriptions import");
    assert(last_ok && last_count == 2 && !strcmp(last_items[0].title, "2 channels found in subs.json") && strstr(last_items[0].subtitle, "newpipe"));
    assert(!strcmp(last_items[1].title, "Add 2 channels to Followed channels"));
    printf("import: \"%s\" (%s)\n", last_items[0].title, last_items[0].subtitle);

    /* Phone: "Link a phone" registers the receiver through the service (here with a stand-in for YouTube's
     * Lounge service whose back channel stays open), the TV code comes back, the phone's commands reach the
     * driver in order and are acknowledged, the player's state goes out, and turning it off ends the channel. */
    if (getenv("TUBE_E2E_LOUNGE")) {
        int remote_row = 0;
        for (int page = 0; page < 8 && !remote_row; ++page) {
            p->list_catalog("settings", page, items_cb, NULL);
            for (int i = 0; i < last_count; ++i) if (!strcmp(last_items[i].id, "settings:remote")) remote_row = 1;
        }
        assert(remote_row);
        p->list_catalog("settings:remote:pair", 0, items_cb, NULL); assert(last_ok);
        for (double end = seconds() + 20; seconds() < end && !*tube_remote_code();) { evo_net_poll(); tube_poll(); usleep(5000); }
        assert(!strcmp(tube_remote_code(), "123456789012") && tube_remote_state() == TUBE_REMOTE_LISTENING);
        assert(tube_remote_identity_valid(tube_remote_device()) && !strcmp(tube_remote_screen(), "screen-1"));
        p->list_catalog("settings:remote", 0, items_cb, NULL);
        assert(last_ok && !strcmp(last_items[0].title, "Phone connection: ready") && !strcmp(last_items[1].title, "TV code: 123 456 789 012"));
        static tube_remote_command command;
        static const int expected[] = {TUBE_REMOTE_QUERY, TUBE_REMOTE_PLAYLIST, TUBE_REMOTE_PAUSE};
        for (int i = 0; i < 3; ++i) {
            int have = 0;
            for (double end = seconds() + 20; seconds() < end && !(have = tube_remote_take(&command));) { evo_net_poll(); tube_poll(); usleep(5000); }
            assert(have && command.sequence == i + 1 && command.action == expected[i]);
            if (i == 1) assert(!strcmp(command.video, "fixtureVID1") && command.count == 2 && !strcmp(command.videos[1], "zyxwvutsrqp") &&
                               command.position == 12.5 && !strcmp(command.ctt, "opaque") && !command.has_window);
            assert(tube_remote_finish(command.owner, command.sequence, 1));
        }
        printf("phone: TV code %s, commands query, playlist %s (+%d) at %.1f s, pause\n", tube_remote_code(), "fixtureVID1", 1, 12.5);
        tube_remote_player playing = {.video = "fixtureVID1", .ctt = "opaque", .state = 2, .volume = 80, .position = 12.5, .duration = 60, .next = 1};
        tube_remote_report(&playing);
        char calls_url[160], lounge_origin[96];
        snprintf(lounge_origin, sizeof lounge_origin, "%s", getenv("TUBE_E2E_LOUNGE"));
        *strstr(lounge_origin, "/api/lounge") = 0;
        snprintf(calls_url, sizeof calls_url, "%s/_calls", lounge_origin);
        int reported = 0;
        for (double end = seconds() + 20; seconds() < end && !reported;) {
            for (int i = 0; i < 60; ++i) { evo_net_poll(); tube_poll(); usleep(5000); }
            fetch(calls_url, 0);
            reported = fetch_ok && strstr(fetched, "\"req0__sc\": \"nowPlaying\"") && strstr(fetched, "\"req0_videoId\": \"fixtureVID1\"") &&
                       strstr(fetched, "\"req2_volume\": \"80\"") && strstr(fetched, "\"req3_hasNext\": \"true\"");
        }
        assert(reported && strstr(fetched, "\"errors\": []") && !strstr(fetched, "\"TYPE\": \"terminate\""));
        assert(tube_remote_state() == TUBE_REMOTE_LISTENING);
        p->list_catalog("settings:remote:toggle", 0, items_cb, NULL);   /* off */
        int ended = 0;
        for (double end = seconds() + 20; seconds() < end && !ended;) {
            for (int i = 0; i < 40; ++i) { evo_net_poll(); tube_poll(); usleep(5000); }
            fetch(calls_url, 0);
            ended = fetch_ok && strstr(fetched, "\"TYPE\": \"terminate\"") != NULL;
        }
        assert(ended && strstr(fetched, "\"errors\": []") && tube_remote_state() == TUBE_REMOTE_OFF);
        puts("phone: state reported, receiver closed at the service and ended upstream");
    }

    /* A Guest's Home: a topic as one shelf of videos, from the one request that lists the topic's rows. */
    calls = item_calls;
    assert(p->list_catalog("home-topic:music", 0, items_cb, NULL) == 0);
    run_until(&item_calls, calls + 1, 30, "the videos of a topic");
    assert(last_ok && last_count == 6);
    for (int i = 0; i < last_count; ++i) assert(!last_items[i].is_folder && strlen(last_items[i].id) == 11 && *last_items[i].title);
    printf("home: a topic as a shelf of %d videos, first \"%s\"\n", last_count, last_items[0].title);

    /* A search while it is typed (Afterglow): YouTube's completions of the text, as YouTube words them, and the first
     * videos that match it as cards, without the two rows that lead a search a person ran. Neither is kept as a search. */
    calls = item_calls;
    assert(p->list_catalog("search-suggest:canned", 0, items_cb, NULL) == 0);
    run_until(&item_calls, calls + 1, 30, "completions");
    assert(last_ok && last_count == 3 && !strcmp(last_items[0].title, "canned heat") && !strcmp(last_items[2].title, "canned laughter"));
    for (int i = 0; i < last_count; ++i) assert(last_items[i].is_folder && !last_items[i].tube_interface);
    calls = item_calls;
    assert(p->list_catalog("search-preview:canned heat", 0, items_cb, NULL) == 0);
    run_until(&item_calls, calls + 1, 30, "matches");
    assert(last_ok && last_count >= 2 && strcmp(last_items[0].id, "settings:search") && strlen(last_items[0].id) == 11);
    printf("typing: 3 completions, %d matches, first \"%s\"\n", last_count, last_items[0].title);
    p->list_catalog("search-history", 0, items_cb, NULL);
    assert(last_ok);
    for (int i = 0; i < last_count; ++i) assert(strcmp(last_items[i].title, "canned heat"));

    /* The saved state never names the service. */
    p->list_catalog("settings:ask-stream", 0, items_cb, NULL);
    FILE *state = fopen(evo_data_path("youtube.json"), "rb"); assert(state);
    static char text[1 << 16]; got = fread(text, 1, sizeof text - 1, state); fclose(state); text[got] = 0;
    assert(!strstr(text, "127.0.0.1") && strstr(text, "\"token\":\"\"") && !strstr(text, "123456789012") && !strstr(text, "private-"));

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
    puts("PASS: provider on the real built-in service: start, search, resolve, manifest, captions, alternative SponsorBlock server, dislikes, import, phone, a topic's shelf, completions and matches while typing, stop and restart");
    return 0;
}
