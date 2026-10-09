/* Host regression tests exercise the shipping C provider with queued HTTP mocks.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_provider.h"
#include "evo_net.h"
#include "evo_data_path.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct { char method[16], url[2048], body[2048]; evo_net_cb cb; void *ud; } Pending;
static Pending pending[32];
static int pending_count, item_calls, resolve_calls, last_ok;
static evo_provider_item_t last_items[24];
static int last_count;
static char error_text[256];

int evo_net_request_async(const char *method, const char *url, const char *post,
                         const char **headers, int header_count, evo_net_cb cb, void *ud) {
    (void)headers; (void)header_count;
    assert(pending_count < 32);
    Pending *p = &pending[pending_count++];
    snprintf(p->method, sizeof p->method, "%s", method);
    snprintf(p->url, sizeof p->url, "%s", url);
    snprintf(p->body, sizeof p->body, "%s", post ? post : "");
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

static void items_cb(int ok, const evo_provider_item_t *items, int count, int more, void *ud) {
    (void)more; (void)ud; ++item_calls; last_ok = ok; last_count = count;
    assert(count <= 24);
    if (items) memcpy(last_items, items, (size_t)count * sizeof *items);
}

static void resolve_cb(int ok, const evo_stream_choice_t *choices, int count, void *ud) {
    (void)ud; ++resolve_calls; last_ok = ok;
    if (ok) { assert(count == 1); assert(strstr(choices[0].url, "http://127.0.0.1:8765/media/") == choices[0].url); }
}

static void poll_job(const char *response) {
    reply(0, 1, 200, "{\"job\":\"fixture_job\",\"state\":\"pending\"}");
    usleep(300000); tube_poll();
    assert(pending_count >= 1 && strstr(pending[0].url, "/v1/jobs/fixture_job"));
    reply(0, 1, 200, response);
}

int main(void) {
    char temporary[] = "/tmp/prosperotube-native-XXXXXX";
    assert(mkdtemp(temporary)); assert(setenv("EVO_DATA_DIR_OVERRIDE", temporary, 1) == 0);
    const evo_provider_t *p = &evo_provider_youtube;
    assert(p->init() == 0);
    assert(evo_provider_mgr_init() == 1);
    assert(evo_provider_count() == 1 && !strcmp(evo_provider_at(0)->id, "youtube"));
    assert(p->list_catalog("", 0, items_cb, NULL) == 0 && last_ok && last_count == 8);
    assert(p->set_source("http://127.0.0.1:8765#123456") == 0);
    assert(p->set_source("http://user:pass@127.0.0.1:8765") < 0);
    assert(p->set_source("http://127.0.0.1:8765#badcode") < 0);
    assert(p->search("fixture", 0, items_cb, NULL) == 0);
    assert(strstr(pending[0].url, "/v1/pair"));
    reply(0, 1, 200, "{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    assert(strstr(pending[0].url, "/v1/jobs") && strstr(pending[0].body, "fixture"));
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"Stable fixture video\",\"subtitle\":\"Fixture channel\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"duration_sec\":100}],\"has_more\":false}}");
    assert(last_ok && last_count == 1 && !strcmp(last_items[0].id, "abcdefghijk"));
    assert(tube_toggle_favorite("abcdefghijk") == 1);
    assert(tube_toggle_queue("abcdefghijk") == 1);
    assert(tube_toggle_subscription("abcdefghijk") == 1);
    p->report_progress("abcdefghijk", 37, 100, EVO_PROVIDER_PLAY_STOP);
    assert(p->list_catalog("favorites", 0, items_cb, NULL) == 0);
    assert(last_count == 1 && last_items[0].resume_pos_sec == 37);
    assert(p->init() == 0); /* reload the same persisted file */
    p->list_catalog("history", 0, items_cb, NULL);
    assert(last_count == 1 && last_items[0].resume_pos_sec == 37);
    p->list_catalog("subscriptions", 0, items_cb, NULL);
    assert(last_count == 1 && last_items[0].is_folder && !strncmp(last_items[0].id, "channel:UC", 10));

    p->list_catalog("settings:sponsor", 0, items_cb, NULL);
    p->list_catalog("settings:captions", 0, items_cb, NULL); /* avoid a caption request in this fixture */
    assert(p->resolve("invalid", resolve_cb, NULL) < 0);
    assert(p->resolve("abcdefghijk", resolve_cb, NULL) == 0);
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"title\":\"Fixture\",\"duration_sec\":100,\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/capability/d0.mpd\",\"label\":\"1080p\",\"container\":\"dash\",\"height\":1080,\"video_codec\":\"h264\",\"audio_codec\":\"aac\"}],\"chapters\":[{\"start\":0},{\"start\":20},{\"start\":60}]}}");
    assert(resolve_calls == 1 && last_ok && pending_count == 1);
    reply(0, 1, 200, "{\"segments\":[{\"start\":10,\"end\":20},{\"start\":15,\"end\":30},{\"start\":29,\"end\":40}]}");
    assert(tube_skip_target(12) == 40);
    assert(tube_skip_target(16) < 0); /* no repeated seeks during a pending skip */
    assert(tube_skip_target(5) < 0);  /* a deliberate rewind re-arms */
    assert(tube_skip_target(12) == 40);
    assert(tube_chapter_target(21, 1) == 60);
    assert(tube_chapter_target(25, -1) == 20);

    /* Cancellation must complete exactly once even when the old HTTP reply lands. */
    int before = item_calls;
    p->search("cancel this", 0, items_cb, NULL);
    tube_cancel(); assert(item_calls == before);
    reply(0, 1, 200, "{\"job\":\"cancelled_job\"}");
    assert(item_calls == before + 1 && !last_ok);

    /* Media links from another origin are refused instead of reaching the player. */
    p->resolve("abcdefghijk", resolve_cb, NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"choices\":[{\"url\":\"http://evil.test/media/track\"}]}}");
    assert(resolve_calls == 2 && !last_ok);
    tube_set_error("https://private.test/signed?token=secret");
    assert(!strstr(error_text, "secret"));
    /* Remote library navigation is asynchronous and retains only opaque cursors. */
    p->list_catalog("discover", 0, items_cb, NULL);
    assert(last_count == 8 && !strcmp(last_items[0].id, "library:home"));
    p->list_catalog("library:profiles", 0, items_cb, NULL);
    assert(strstr(pending[0].body, "\"action\":\"library\""));
    poll_job("{\"state\":\"done\",\"result\":{\"profiles\":[{\"id\":\"guest\",\"name\":\"Guest\"},{\"id\":\"fixture\",\"name\":\"Fixture\",\"account_configured\":true},{\"id\":\"../../bad\"}]}}");
    assert(last_ok && last_count == 2 && !strcmp(last_items[1].id, "library:profile:fixture"));
    p->list_catalog(last_items[1].id, 0, items_cb, NULL);
    assert(last_ok && last_count == 6 && strstr(last_items[0].title, "fixture"));
    p->list_catalog("library:playlists", 0, items_cb, NULL);
    assert(strstr(pending[0].body, "\"profile\":\"fixture\""));
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"library:playlist:PLabcdefghijk\",\"title\":\"Account playlist\",\"is_folder\":true}],\"has_more\":true,\"cursor\":\"fixture_cursor\"}}");
    assert(last_ok && last_count == 1 && last_items[0].is_folder);
    p->list_catalog("library:playlists", 1, items_cb, NULL);
    assert(strstr(pending[0].body, "\"action\":\"continue\"") && strstr(pending[0].body, "fixture_cursor"));
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[],\"has_more\":false}}");
    assert(last_ok && last_count == 0);
    p->list_catalog("library:playlist:PLabcdefghijk", 0, items_cb, NULL);
    assert(strstr(pending[0].body, "\"action\":\"playlist\"") && strstr(pending[0].body, "PLabcdefghijk"));
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"Account video\"}],\"has_more\":false}}");
    assert(last_ok && last_count == 1);
    p->list_catalog("library:home", 1, items_cb, NULL);
    assert(!last_ok && pending_count == 0); /* unrelated cursor cannot cross levels */
    p->shutdown();
    printf("PASS: native provider, local state, stable IDs, pairing, jobs, cancellation, origins, SponsorBlock, chapters\n");
    return 0;
}
