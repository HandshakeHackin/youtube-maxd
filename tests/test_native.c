/* Host regression tests exercise the shipping C provider with queued HTTP mocks.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_provider.h"
#include "evo_net.h"
#include "evo_data_path.h"
#include "cJSON.h"
#include <assert.h>
#include <math.h>
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
static int last_more;

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
    (void)ud; ++item_calls; last_ok = ok; last_count = count; last_more = more;
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

static cJSON *read_store(void) {
    FILE *f = fopen(evo_data_path("youtube.json"), "rb"); assert(f);
    assert(fseek(f, 0, SEEK_END) == 0); long n = ftell(f); assert(n > 0); rewind(f);
    char *text = calloc(1, (size_t)n + 1); assert(text);
    assert(fread(text, 1, (size_t)n, f) == (size_t)n); assert(fclose(f) == 0);
    cJSON *root = cJSON_Parse(text); free(text); assert(root); return root;
}

static void write_store(cJSON *root) {
    char *text = cJSON_PrintUnformatted(root); assert(text);
    FILE *f = fopen(evo_data_path("youtube.json"), "wb"); assert(f);
    assert(fwrite(text, 1, strlen(text), f) == strlen(text)); assert(fclose(f) == 0); free(text);
}

int main(void) {
    char temporary[] = "/tmp/prosperotube-native-XXXXXX";
    assert(mkdtemp(temporary)); assert(setenv("EVO_DATA_DIR_OVERRIDE", temporary, 1) == 0);
    const evo_provider_t *p = &evo_provider_youtube;
    assert(p->init() == 0);
    assert(tube_playback_speed() == 100);
    assert(tube_speed_count() == 30 && tube_speed_at(0) == 25 && tube_speed_at(29) == 400);
    assert(tube_speed_at(-1) == 0 && tube_speed_at(30) == 0);
    assert(!tube_set_playback_speed(126) && tube_playback_speed() == 100);
    assert(tube_set_playback_speed(125) && p->init() == 0 && tube_playback_speed() == 125);
    assert(tube_set_playback_speed(100));
    p->list_catalog("settings:speed", 0, items_cb, NULL);
    assert(last_ok && last_count == 24 && last_more);
    assert(!strcmp(last_items[0].id, "settings:speed:25"));
    p->list_catalog("settings:speed", 1, items_cb, NULL);
    assert(last_ok && last_count == 6 && !last_more);
    assert(!strcmp(last_items[5].id, "settings:speed:400"));
    assert(p->list_catalog("settings:speed", 2147483647, items_cb, NULL) < 0);
    p->list_catalog("settings:speed", 1000, items_cb, NULL);
    assert(last_ok && last_count == 0 && !last_more);
    p->list_catalog("settings:speed:125", 0, items_cb, NULL);
    assert(last_ok && tube_playback_speed() == 125);
    assert(!strcmp(last_items[12].title, "Selected: 1.25x"));
    assert(p->init() == 0 && tube_playback_speed() == 125);
    p->list_catalog("settings:speed:126", 0, items_cb, NULL);
    assert(!last_ok && tube_playback_speed() == 125);
    p->list_catalog("settings:speed:125junk", 0, items_cb, NULL);
    assert(!last_ok && tube_playback_speed() == 125);
    p->list_catalog("settings:speed:100", 0, items_cb, NULL);
    assert(last_ok && tube_playback_speed() == 100);
    assert(!tube_choose_stream_enabled());
    assert(!tube_playback_at_end(100, 0)); /* no known duration or active item */
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
    for(int i=0;i<3;++i)p->list_catalog("settings:codec",0,items_cb,NULL);
    p->list_catalog("settings:frame-rate:30",0,items_cb,NULL);
    assert(p->resolve("invalid", resolve_cb, NULL) < 0);
    assert(p->resolve("abcdefghijk", resolve_cb, NULL) == 0);
    assert(strstr(pending[0].body, "\"profile\":\"guest\""));
    assert(strstr(pending[0].body, "\"codec\":\"av1\""));
    assert(strstr(pending[0].body, "\"max_fps\":30"));
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"title\":\"Fixture\",\"duration_sec\":100,\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/capability/d0.mpd\",\"label\":\"1080p\",\"container\":\"dash\",\"height\":1080,\"video_codec\":\"av1\",\"audio_codec\":\"aac\"}],\"chapters\":[{\"start\":0},{\"start\":20},{\"start\":60}]}}");
    assert(resolve_calls == 1 && last_ok && pending_count == 1);
    reply(0, 1, 200, "{\"segments\":[{\"start\":10,\"end\":20},{\"start\":15,\"end\":30},{\"start\":29,\"end\":40}]}");
    assert(tube_skip_target(12) == 20);
    assert(tube_skip_target(16) < 0); /* no repeated seeks during a pending skip */
    assert(tube_skip_target(5) < 0);  /* a deliberate rewind re-arms */
    assert(tube_skip_target(12) == 20);
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
    assert(last_count == 12 && !strcmp(last_items[0].id, "library:home"));
    assert(!strcmp(last_items[4].id, "section:gaming") && !strcmp(last_items[5].id, "section:news") &&
           !strcmp(last_items[6].id, "section:music"));
    assert(!strcmp(last_items[9].id, "section:sports") && !strcmp(last_items[10].id, "section:live") &&
           !strcmp(last_items[11].id, "section:movies"));
    p->list_catalog("library:profiles", 0, items_cb, NULL);
    assert(strstr(pending[0].body, "\"action\":\"library\""));
    poll_job("{\"state\":\"done\",\"result\":{\"profiles\":[{\"id\":\"guest\",\"name\":\"Guest\"},{\"id\":\"fixture\",\"name\":\"Fixture\",\"account_configured\":true},{\"id\":\"../../bad\"}]}}");
    assert(last_ok && last_count == 2 && !strcmp(last_items[1].id, "library:profile:fixture"));
    p->list_catalog(last_items[1].id, 0, items_cb, NULL);
    assert(last_ok && last_count == 9 && strstr(last_items[0].title, "fixture"));
    assert(!strcmp(last_items[8].id, "account:remove"));
    p->list_catalog("settings:captions",0,items_cb,NULL); /* this fixture excludes sidecar downloads */
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
    /* Native discussion records never enter the playable-video library. */
    assert(p->resolve("abcdefghijk", resolve_cb, NULL) == 0);
    assert(strstr(pending[0].body, "\"profile\":\"fixture\""));
    tube_cancel();
    reply(0, 1, 200, "{\"job\":\"cancelled-profile-resolve\",\"state\":\"pending\"}");
    assert(!last_ok && !pending_count);
    assert(p->search("profile-search", 0, items_cb, NULL) == 0);
    assert(strstr(pending[0].body, "\"profile\":\"fixture\""));
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[],\"has_more\":false}}");
    p->list_catalog("discussion:top:abcdefghijk", 0, items_cb, NULL);
    assert(strstr(pending[0].body, "\"action\":\"comments\"") &&
        strstr(pending[0].body, "\"profile\":\"fixture\"") && strstr(pending[0].body, "\"sort\":\"top\""));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"comments\",\"items\":[{\"id\":\"upstream-comment\",\"author\":\"Author\",\"content\":\"Literal <b>comment</b> & text\\nSecond line\",\"published\":\"Today\",\"likes\":\"10\",\"replies\":\"2\",\"pinned\":true,\"owner\":true,\"reply_cursor\":\"opaque_reply\"},{\"id\":\"no-replies\",\"content\":\"Leaf comment\"},{\"id\":\"invalid-thread\",\"reply_cursor\":\"bad/cursor\"}],\"has_more\":true,\"cursor\":\"comments_next\"}}");
    assert(last_ok && last_count == 2 && last_more && last_items[0].is_folder);
    assert(!strcmp(last_items[0].id, "discussion:replies:opaque_reply"));
    assert(strstr(last_items[0].subtitle, "Pinned") && strstr(last_items[0].subtitle, "Creator"));
    assert(!strcmp(tube_comment_text(last_items[0].id), "Literal <b>comment</b> & text\nSecond line"));
    assert(!strncmp(last_items[1].id, "discussion:leaf:", 16));
    assert(!strcmp(tube_comment_text(last_items[1].id), "Leaf comment"));
    p->list_catalog("discussion:top:abcdefghijk", 1, items_cb, NULL);
    assert(strstr(pending[0].body, "\"action\":\"continue\"") && strstr(pending[0].body, "comments_next"));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"comments\",\"items\":[],\"has_more\":false}}");
    assert(last_ok && last_count == 0 && !last_more);
    assert(!*tube_comment_text("discussion:replies:opaque_reply"));
    p->list_catalog("discussion:replies:opaque_reply", 0, items_cb, NULL);
    assert(strstr(pending[0].body, "\"action\":\"replies\"") && strstr(pending[0].body, "opaque_reply"));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"comments\",\"items\":[{\"id\":\"reply\",\"author\":\"Replier\",\"content\":\"Reply text\"}],\"has_more\":false}}");
    assert(last_ok && last_count == 1 && !strcmp(tube_comment_text(last_items[0].id), "Reply text"));
    p->list_catalog("discussion:newest:abcdefghijk", 0, items_cb, NULL);
    assert(strstr(pending[0].body, "\"sort\":\"newest\""));
    char long_text[6001]; memset(long_text, 'x', sizeof long_text - 1); long_text[6000] = 0;
    char comment_response[6400];
    snprintf(comment_response, sizeof comment_response,
        "{\"state\":\"done\",\"result\":{\"kind\":\"comments\",\"items\":[{\"id\":\"long\",\"content\":\"%s\"}],\"has_more\":false}}", long_text);
    poll_job(comment_response);
    assert(last_ok && strlen(tube_comment_text(last_items[0].id)) == 6000);
    char previous_comment[160]; snprintf(previous_comment, sizeof previous_comment, "%s", last_items[0].id);
    p->list_catalog("library:profile:guest", 0, items_cb, NULL);
    assert(!*tube_comment_text(previous_comment));
    p->list_catalog("discussion:newest:abcdefghijk", 1, items_cb, NULL);
    assert(!last_ok && !pending_count); /* profile switch invalidates page cursors */
    p->list_catalog("discussion:replies:bad/cursor", 0, items_cb, NULL);
    assert(!last_ok && !pending_count);
    p->list_catalog("discussion:top:bad-video", 0, items_cb, NULL);
    assert(!last_ok && !pending_count);
    p->list_catalog("discussion:top:abcdefghijk", 0, items_cb, NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"comments\",\"items\":[],\"has_more\":true,\"cursor\":\"invalid/cursor\"}}");
    assert(!last_ok);
    p->list_catalog("discussion:top:abcdefghijk", 0, items_cb, NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":false}}");
    assert(!last_ok);
    /* Account actions require a deliberate confirmation and submit only once. */
    p->list_catalog("account:menu:abcdefghijk", 0, items_cb, NULL);
    assert(last_ok && last_count == 4 && strstr(last_items[0].title, "profile first"));
    assert(!pending_count);
    p->list_catalog("library:profile:fixture", 0, items_cb, NULL);
    /* Displayed comment capabilities are profile-bound and require review. */
    p->list_catalog("discussion:top:abcdefghijk", 0, items_cb, NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"comments\",\"items\":[{\"id\":\"vote\",\"content\":\"Literal <b>comment</b> café\",\"rating_cursor\":\"rating_reference\",\"rating_like\":true,\"rating_dislike\":true}],\"has_more\":false}}");
    char rated_comment[160]; snprintf(rated_comment, sizeof rated_comment, "%s", last_items[0].id);
    assert(!strcmp(tube_comment_rating(rated_comment), "rating_reference"));
    p->list_catalog("comment:menu:rating_reference", 0, items_cb, NULL);
    assert(last_ok && last_count == 2 && !pending_count);
    p->list_catalog("comment:confirm:none:rating_reference", 0, items_cb, NULL);
    assert(!last_ok && !pending_count);
    p->list_catalog("comment:confirm:like:rating_reference", 0, items_cb, NULL);
    assert(last_ok && last_count == 1 && !pending_count);
    assert(strstr(tube_comment_text(last_items[0].id), "Literal <b>comment</b> café"));
    char rating_apply[160]; snprintf(rating_apply, sizeof rating_apply, "%s", last_items[0].id);
    assert(!strcmp(tube_comment_text(rated_comment), "Literal <b>comment</b> café"));
    p->list_catalog(rating_apply, 0, items_cb, NULL);
    assert(strstr(pending[0].body, "\"action\":\"comment_rate\"") && strstr(pending[0].body, "\"cursor\":\"rating_reference\"") &&
           strstr(pending[0].body, "\"rating\":\"like\""));
    p->list_catalog(rating_apply, 0, items_cb, NULL); assert(pending_count == 1);
    poll_job("{\"state\":\"done\",\"result\":{\"success\":true,\"rating\":\"dislike\"}}");
    assert(!last_ok); p->list_catalog(rating_apply, 0, items_cb, NULL); assert(!pending_count);
    p->list_catalog("library:profile:guest", 0, items_cb, NULL);
    assert(!*tube_comment_rating(rated_comment));
    p->list_catalog("comment:menu:rating_reference", 0, items_cb, NULL); assert(!last_ok && !pending_count);
    p->list_catalog(rating_apply, 0, items_cb, NULL); assert(!last_ok && !pending_count);
    p->list_catalog("library:profile:fixture", 0, items_cb, NULL);
    p->list_catalog("discussion:top:abcdefghijk", 0, items_cb, NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"comments\",\"items\":[{\"id\":\"vote\",\"content\":\"Rated comment\",\"rating_cursor\":\"new_rating_reference\",\"rating_remove\":true}],\"has_more\":false}}");
    p->list_catalog("comment:menu:new_rating_reference", 0, items_cb, NULL); assert(last_ok && last_count == 1);
    p->list_catalog(last_items[0].id, 0, items_cb, NULL); assert(last_ok && !pending_count);
    snprintf(rating_apply, sizeof rating_apply, "%s", last_items[0].id);
    p->list_catalog(rating_apply, 0, items_cb, NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"success\":true,\"rating\":\"none\"}}"); assert(last_ok);
    p->list_catalog(rating_apply, 0, items_cb, NULL); assert(!pending_count);
    p->search("account fixture", 0, items_cb, NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"Account fixture\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\"}],\"has_more\":false}}");
    p->list_catalog("account:menu:abcdefghijk", 0, items_cb, NULL);
    assert(last_ok && last_count == 14 && !pending_count);
    char account_choices[7][160];
    for (int i = 0; i < 7; ++i) snprintf(account_choices[i], sizeof account_choices[i], "%s", last_items[i].id);
    const char *operations[] = {"like", "dislike", "unrate", "watch_later_add", "watch_later_remove", "subscribe", "unsubscribe"};
    for (int i = 0; i < 7; ++i) {
        p->list_catalog(account_choices[i], 0, items_cb, NULL);
        assert(last_ok && last_count == 1 && !pending_count && strstr(last_items[0].subtitle, "fixture"));
        char apply[160]; snprintf(apply, sizeof apply, "%s", last_items[0].id);
        p->list_catalog(apply, 1, items_cb, NULL);
        assert(!last_ok && !pending_count);
        p->list_catalog(apply, 0, items_cb, NULL);
        char operation[80]; snprintf(operation, sizeof operation, "\"action\":\"%s\"", operations[i]);
        assert(pending_count == 1 && strstr(pending[0].body, operation) && strstr(pending[0].body, "\"profile\":\"fixture\""));
        assert(strstr(pending[0].body, i >= 5 ? "UCaaaaaaaaaaaaaaaaaaaaaa" : "abcdefghijk"));
        p->list_catalog(apply, 0, items_cb, NULL); /* a repeated catalog read never submits again */
        assert(last_ok && pending_count == 1 && strstr(last_items[0].title, "pending"));
        poll_job("{\"state\":\"done\",\"result\":{\"success\":true}}");
        assert(last_ok && last_count == 1 && strstr(last_items[0].title, "updated"));
        p->list_catalog(apply, 0, items_cb, NULL);
        assert(last_ok && !pending_count && strstr(last_items[0].title, "updated"));
    }
    p->list_catalog(account_choices[0], 0, items_cb, NULL);
    char account_apply[160]; snprintf(account_apply, sizeof account_apply, "%s", last_items[0].id);
    p->list_catalog("library:profile:guest", 0, items_cb, NULL);
    p->list_catalog(account_apply, 0, items_cb, NULL);
    assert(!last_ok && !pending_count); /* confirmation cannot cross profiles */
    p->list_catalog("library:profile:fixture", 0, items_cb, NULL);
    p->list_catalog(account_apply, 0, items_cb, NULL);
    assert(!last_ok && !pending_count); /* switching back cannot revive the old confirmation */
    p->list_catalog(account_choices[0], 0, items_cb, NULL);
    snprintf(account_apply, sizeof account_apply, "%s", last_items[0].id);
    p->list_catalog(account_apply, 0, items_cb, NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"success\":false}}");
    assert(!last_ok);
    p->list_catalog(account_apply, 0, items_cb, NULL);
    assert(last_ok && !pending_count && strstr(last_items[0].title, "not confirmed"));
    p->list_catalog("account:apply:999999999", 0, items_cb, NULL);
    assert(!last_ok && !pending_count);
    p->list_catalog("account:confirm:unknown:abcdefghijk", 0, items_cb, NULL);
    assert(!last_ok && !pending_count);
    p->list_catalog("account:confirm:subscribe:bad-channel", 0, items_cb, NULL);
    assert(!last_ok && !pending_count);
    /* Playlist picker pages preserve video identity and selected profile. */
    p->list_catalog("playlist:pick:add:abcdefghijk", 0, items_cb, NULL);
    assert(strstr(pending[0].body, "\"action\":\"playlists\"") && strstr(pending[0].body, "fixture"));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"library:playlist:PLabcdefghijk\",\"title\":\"First playlist\",\"is_folder\":true},{\"id\":\"library:playlist:PLabcdefghijk\",\"title\":\"Duplicate\",\"is_folder\":true},{\"id\":\"abcdefghijk\",\"title\":\"Not a playlist\"}],\"has_more\":true,\"cursor\":\"playlist_next\"}}");
    assert(last_ok && last_count == 1 && last_more);
    assert(!strcmp(last_items[0].id, "playlist:confirm:add:abcdefghijk:PLabcdefghijk"));
    p->list_catalog("playlist:pick:add:abcdefghijk", 1, items_cb, NULL);
    assert(strstr(pending[0].body, "\"action\":\"continue\"") && strstr(pending[0].body, "playlist_next"));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":false}}");
    assert(last_ok && !last_more);
    const char *playlist_changes[] = {"playlist:confirm:add:abcdefghijk:PLabcdefghijk",
        "playlist:confirm:remove:abcdefghijk:PLabcdefghijk", "playlist:confirm:delete:PLabcdefghijk"};
    const char *playlist_operations[] = {"playlist_add", "playlist_remove", "playlist_delete"};
    for (int i = 0; i < 3; ++i) {
        p->list_catalog(playlist_changes[i], 0, items_cb, NULL);
        assert(last_ok && last_count == 1 && !pending_count);
        snprintf(account_apply, sizeof account_apply, "%s", last_items[0].id);
        p->list_catalog(account_apply, 0, items_cb, NULL);
        cJSON *job = cJSON_Parse(pending[0].body);
        cJSON *operation = cJSON_GetObjectItem(job, "library");
        assert(!strcmp(cJSON_GetObjectItem(operation, "action")->valuestring, playlist_operations[i]));
        assert(!strcmp(cJSON_GetObjectItem(operation, "playlist")->valuestring, "PLabcdefghijk"));
        assert(!strcmp(cJSON_GetObjectItem(operation, "id")->valuestring, i == 2 ? "" : "abcdefghijk"));
        cJSON_Delete(job);
        p->list_catalog(account_apply, 0, items_cb, NULL); assert(pending_count == 1);
        poll_job("{\"state\":\"done\",\"result\":{\"success\":true}}"); assert(last_ok);
        p->list_catalog(account_apply, 0, items_cb, NULL); assert(!pending_count);
    }
    p->list_catalog("playlist:menu:PLabcdefghijk", 0, items_cb, NULL);
    assert(last_ok && last_count == 2 && strstr(last_items[0].id, "rename"));
    p->list_catalog("playlist:prompt:rename:PLabcdefghijk", 0, items_cb, NULL);
    tube_playlist_prompt_t prompt;
    assert(tube_take_playlist_prompt(&prompt) && strstr(prompt.label, "name"));
    assert(!tube_take_playlist_prompt(&prompt));
    char review[80];
    assert(tube_submit_playlist_name(prompt.ticket, "  Name \"&\" café  ", review, sizeof review) == 0 && !pending_count);
    p->list_catalog(review, 0, items_cb, NULL); assert(last_ok && last_count == 1 && !pending_count);
    assert(strstr(tube_comment_text(last_items[0].id), "Name \"&\" café"));
    snprintf(account_apply, sizeof account_apply, "%s", last_items[0].id);
    p->list_catalog(account_apply, 0, items_cb, NULL);
    cJSON *job = cJSON_Parse(pending[0].body), *operation = cJSON_GetObjectItem(job, "library");
    assert(!strcmp(cJSON_GetObjectItem(operation, "action")->valuestring, "playlist_rename"));
    assert(!strcmp(cJSON_GetObjectItem(operation, "title")->valuestring, "Name \"&\" café"));
    cJSON_Delete(job); poll_job("{\"state\":\"done\",\"result\":{\"success\":true}}");
    p->list_catalog("playlist:prompt:create:abcdefghijk", 0, items_cb, NULL);
    assert(tube_take_playlist_prompt(&prompt));
    assert(tube_submit_playlist_name(prompt.ticket, "", review, sizeof review) < 0);
    assert(tube_submit_playlist_name(prompt.ticket, "x\ny", review, sizeof review) < 0);
    assert(tube_submit_playlist_name(prompt.ticket, "\xed\xa0\x80", review, sizeof review) < 0);
    char oversized[152]; memset(oversized, 'a', 151); oversized[151] = 0;
    assert(tube_submit_playlist_name(prompt.ticket, oversized, review, sizeof review) < 0);
    char unicode[601]; for (int i = 0; i < 150; ++i) memcpy(unicode + i*4, "\xf0\x9f\x8c\x8d", 4); unicode[600] = 0;
    assert(tube_submit_playlist_name(prompt.ticket, unicode, review, sizeof review) == 0);
    assert(tube_submit_playlist_name(prompt.ticket, "Duplicate", review, sizeof review) < 0);
    p->list_catalog(review, 0, items_cb, NULL); assert(last_ok && !pending_count);
    assert(strlen(tube_comment_text(last_items[0].id)) > 600);
    snprintf(account_apply, sizeof account_apply, "%s", last_items[0].id);
    p->list_catalog(account_apply, 0, items_cb, NULL);
    job = cJSON_Parse(pending[0].body); operation = cJSON_GetObjectItem(job, "library");
    assert(!strcmp(cJSON_GetObjectItem(operation, "action")->valuestring, "playlist_create"));
    assert(!strcmp(cJSON_GetObjectItem(operation, "id")->valuestring, "abcdefghijk"));
    assert(!strcmp(cJSON_GetObjectItem(operation, "title")->valuestring, unicode));
    cJSON_Delete(job);
    poll_job("{\"state\":\"done\",\"result\":{\"success\":true,\"playlist_id\":\"PLcreatedfixture\"}}");
    assert(last_ok && !strcmp(last_items[0].id, "library:playlist:PLcreatedfixture"));
    p->list_catalog("playlist:prompt:create:", 0, items_cb, NULL); assert(tube_take_playlist_prompt(&prompt));
    p->list_catalog("library:profile:guest", 0, items_cb, NULL);
    assert(tube_submit_playlist_name(prompt.ticket, "Stale", review, sizeof review) < 0 && !pending_count);
    p->list_catalog("library:profile:fixture", 0, items_cb, NULL);
    p->list_catalog("playlist:menu:WL", 0, items_cb, NULL); assert(!last_ok && !pending_count);
    p->list_catalog("playlist:confirm:delete:LL", 0, items_cb, NULL); assert(!last_ok && !pending_count);
    p->list_catalog(account_choices[0], 0, items_cb, NULL);
    snprintf(account_apply, sizeof account_apply, "%s", last_items[0].id);
    p->list_catalog(account_apply, 0, items_cb, NULL);
    assert(pending_count == 1);
    tube_cancel();
    reply(0, 1, 200, "{\"job\":\"possibly-executed\",\"state\":\"pending\"}");
    assert(!last_ok && !pending_count);
    p->list_catalog(account_apply, 0, items_cb, NULL);
    assert(last_ok && !pending_count && strstr(last_items[0].title, "not confirmed"));
    p->list_catalog(account_choices[0], 0, items_cb, NULL);
    snprintf(account_apply, sizeof account_apply, "%s", last_items[0].id);
    assert(p->set_source("http://127.0.0.1:8765#123456") == 0);
    p->list_catalog(account_apply, 0, items_cb, NULL);
    assert(!last_ok && !pending_count); /* re-pairing invalidates an unsubmitted intent */
    /* The YouTube playback prompt is opt-in and survives a reload in both states. */
    p->list_catalog("settings:ask-stream", 0, items_cb, NULL);
    assert(tube_choose_stream_enabled());
    assert(p->init() == 0 && tube_choose_stream_enabled());
    p->list_catalog("settings:ask-stream", 0, items_cb, NULL);
    assert(!tube_choose_stream_enabled());
    assert(p->init() == 0 && !tube_choose_stream_enabled());
    p->list_catalog("library:home", 1, items_cb, NULL);
    assert(!last_ok && pending_count == 0); /* unrelated cursor cannot cross levels */
    /* Demux can exhaust a short clip while its audio/video are still playing.
     * Early EOF must retain the current queue item and its unwatched state. */
    p->list_catalog("library:profile:guest", 0, items_cb, NULL);
    p->report_progress("abcdefghijk", 0, 8, EVO_PROVIDER_PLAY_START);
    p->report_progress("12345678901", 0, 30, EVO_PROVIDER_PLAY_START);
    assert(tube_toggle_queue("12345678901") == 1);
    evo_stream_choice_t selected = {0};
    p->stream_chosen("abcdefghijk", &selected);
    p->list_catalog("settings:autoplay", 0, items_cb, NULL);
    assert(tube_autoplay_enabled());
    assert(!tube_playback_at_end(NAN, 8));
    assert(!tube_playback_at_end(8, INFINITY));
    assert(!tube_finish_and_next(0.6, 8));
    assert(!tube_finish_and_next(1.62, 8));
    p->list_catalog("queue", 0, items_cb, NULL);
    assert(last_count == 2 && !last_items[0].played);
    assert(tube_playback_at_end(7.96, 0)); /* use resolved duration when needed */
    assert(tube_finish_and_next(7.96, 8));
    evo_provider_item_t next;
    assert(tube_take_next(&next) && !strcmp(next.id, "12345678901"));
    assert(!tube_take_next(&next) && !tube_finish_and_next(8, 8));
    p->list_catalog("favorites", 0, items_cb, NULL);
    assert(last_count == 1 && last_items[0].played && last_items[0].resume_pos_sec == 0);
    /* Clean decoder/output drain can finish media with inaccurate or absent
     * duration metadata; the UI calls this only after the full engine proof. */
    p->stream_chosen("12345678901", &selected);
    assert(!tube_finish_drained_and_next(NAN));
    assert(!tube_finish_drained_and_next(-1));
    assert(!tube_playback_at_end(2, 0)); /* stored duration remains 30 seconds */
    (void)tube_finish_drained_and_next(2);
    assert(!pending_count && !tube_playlist_eof_pending()); /* Re-pairing above retired credentials. */
    p->list_catalog("queue", 0, items_cb, NULL);
    assert(last_count == 0); /* drained completion removed the final queued item */
    /* History reports bind to a resolved account and never block browsing. */
    p->list_catalog("settings:history-sync", 0, items_cb, NULL); /* default on -> off */
    assert(p->init() == 0);
    int history_row=-1,history_page=0;
    for(;;++history_page) {
        p->list_catalog("settings",history_page,items_cb,NULL);assert(last_ok);
        for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].id,"settings:history-sync"))history_row=i;
        if(history_row>=0 || !last_more)break;
    }
    assert(history_row >= 0 && strstr(last_items[history_row].title, "off"));
    p->list_catalog("settings:history-sync", 0, items_cb, NULL);
    p->list_catalog("settings:sponsor", 0, items_cb, NULL); /* disable extra sponsor request */
    p->list_catalog("library:profile:fixture", 0, items_cb, NULL);
    assert(p->resolve("abcdefghijk", resolve_cb, NULL) == 0);
    assert(strstr(pending[0].url, "/v1/pair"));
    reply(0, 1, 200, "{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"title\":\"History fixture\",\"duration_sec\":100,\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/capability/p0\",\"label\":\"360p\",\"container\":\"mp4\"}]}}");
    assert(last_ok && !pending_count);
    p->stream_chosen("abcdefghijk", &selected);
    p->report_progress("abcdefghijk", 0, 100, EVO_PROVIDER_PLAY_START);
    p->report_progress("abcdefghijk", 0, 100, EVO_PROVIDER_PLAY_UPDATE);
    assert(!pending_count); /* opening/zero progress must not write history */
    p->report_progress("abcdefghijk", 1, 100, EVO_PROVIDER_PLAY_UPDATE);
    assert(pending_count == 1 && strstr(pending[0].url, "/v1/library"));
    assert(strstr(pending[0].body, "\"profile\":\"fixture\"") && strstr(pending[0].body, "watch_progress"));
    char watch_session[65];
    job = cJSON_Parse(pending[0].body);
    snprintf(watch_session, sizeof watch_session, "%s", cJSON_GetObjectItem(job, "session")->valuestring);
    cJSON_Delete(job);
    p->list_catalog("favorites", 0, items_cb, NULL);
    assert(pending_count == 1); /* no DELETE/cancellation from browsing */
    p->report_progress("abcdefghijk", 4, 100, EVO_PROVIDER_PLAY_UPDATE);
    assert(pending_count == 1); /* coalesce while pending */
    reply(0, 1, 200, "{\"success\":true,\"position_sec\":1}");
    tube_poll(); assert(!pending_count); /* 30-second wall-clock throttle */
    usleep(30050000);
    tube_poll();
    assert(pending_count == 1 && strstr(pending[0].body, "\"position_sec\":4"));
    reply(0, 1, 200, "{\"success\":true,\"position_sec\":4}");
    p->report_progress("abcdefghijk", 8, 100, EVO_PROVIDER_PLAY_STOP);
    assert(pending_count == 1 && strstr(pending[0].body, "\"state\":\"stopped\"") && strstr(pending[0].body, watch_session));
    reply(0, 1, 200, "{\"success\":true,\"position_sec\":8}");
    p->report_progress("abcdefghijk", 9, 100, EVO_PROVIDER_PLAY_UPDATE);
    assert(!pending_count); /* stopped sessions stay closed */
    p->stream_chosen("abcdefghijk", &selected);
    p->report_progress("abcdefghijk", 2, 100, EVO_PROVIDER_PLAY_UPDATE);
    assert(pending_count == 1 && !strstr(pending[0].body, watch_session));
    reply(0, 1, 503, "{\"error\":{\"message\":\"Unconfirmed history\"}}");
    p->report_progress("abcdefghijk", 90, 100, EVO_PROVIDER_PLAY_STOP);
    tube_poll(); assert(!pending_count); /* uncertain results never retry */
    p->list_catalog("settings", history_page, items_cb, NULL);
    assert(strstr(last_items[history_row].subtitle, "unconfirmed"));
    p->stream_chosen("abcdefghijk", &selected);
    p->report_progress("abcdefghijk", 3, 100, EVO_PROVIDER_PLAY_UPDATE);
    p->list_catalog("library:profile:guest", 0, items_cb, NULL);
    reply(0, 1, 401, "{\"error\":{\"message\":\"Stale account result\"}}");
    p->report_progress("abcdefghijk", 5, 100, EVO_PROVIDER_PLAY_STOP);
    assert(!pending_count); /* changed profile cannot submit to a new account */
    p->list_catalog("library:profile:fixture", 0, items_cb, NULL);
    assert(p->resolve("abcdefghijk", resolve_cb, NULL) == 0);
    assert(strstr(pending[0].url, "/v1/jobs")); /* stale 401 did not clear pairing */
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"title\":\"Fixture\",\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/capability/p0\"}]}}");
    p->stream_chosen("abcdefghijk", &selected);
    p->report_progress("abcdefghijk", 1, 100, EVO_PROVIDER_PLAY_UPDATE);
    p->report_progress("abcdefghijk", 6, 100, EVO_PROVIDER_PLAY_STOP);
    assert(pending_count == 1);
    p->stream_chosen("abcdefghijk", &selected);
    assert(pending_count == 2 && strstr(pending[1].body, "\"state\":\"stopped\""));
    reply(0, 1, 200, "{\"success\":true,\"position_sec\":1}");
    reply(0, 1, 200, "{\"success\":true,\"position_sec\":6}");
    p->report_progress("abcdefghijk", 1, 100, EVO_PROVIDER_PLAY_UPDATE);
    assert(pending_count == 1);
    p->list_catalog("settings:history-sync", 0, items_cb, NULL);
    reply(0, 1, 200, "{\"success\":true,\"position_sec\":1}");
    p->report_progress("abcdefghijk", 8, 100, EVO_PROVIDER_PLAY_STOP);
    assert(!pending_count); /* disabling sync prevents future reports */
    /* Local state is keyed by profile as well as the stable video identity. */
    assert(p->search("local-profiles", 0, items_cb, NULL) == 0);
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"Profile video\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"subtitle\":\"Profile channel\",\"duration_sec\":100}],\"has_more\":false}}");
    assert(tube_toggle_favorite("abcdefghijk") == 1);
    assert(tube_toggle_queue("abcdefghijk") == 1);
    assert(tube_toggle_subscription("abcdefghijk") == 1);
    p->report_progress("abcdefghijk", 19, 100, EVO_PROVIDER_PLAY_STOP);
    p->list_catalog("library:profile:guest", 0, items_cb, NULL);
    p->list_catalog("favorites", 0, items_cb, NULL);
    assert(last_count == 1 && last_items[0].resume_pos_sec == 0 && last_items[0].played);
    assert(tube_toggle_subscription("abcdefghijk") == 0); /* only Guest unfollows */
    p->list_catalog("settings:clear-history", 0, items_cb, NULL);
    p->list_catalog("settings:clear-queue", 0, items_cb, NULL);
    p->list_catalog("history", 0, items_cb, NULL); assert(last_count == 0);
    p->list_catalog("queue", 0, items_cb, NULL); assert(last_count == 0);
    p->list_catalog("library:profile:fixture", 0, items_cb, NULL);
    p->list_catalog("favorites", 0, items_cb, NULL);
    assert(last_count == 1 && last_items[0].resume_pos_sec == 19 && !last_items[0].played);
    p->list_catalog("queue", 0, items_cb, NULL); assert(last_count == 1);
    p->list_catalog("subscriptions", 0, items_cb, NULL); assert(last_count == 1);
    assert(p->init() == 0);
    p->list_catalog("favorites", 0, items_cb, NULL); assert(last_count == 1 && last_items[0].resume_pos_sec == 19);
    p->stream_chosen("abcdefghijk", &selected);
    p->list_catalog("library:profile:guest", 0, items_cb, NULL);
    p->report_progress("abcdefghijk", 11, 100, EVO_PROVIDER_PLAY_STOP);
    p->list_catalog("history", 0, items_cb, NULL); assert(last_count == 0);
    assert(!tube_finish_and_next(100, 100)); /* finish old profile, no cross-profile autoplay */
    p->list_catalog("library:profile:fixture", 0, items_cb, NULL);
    p->list_catalog("favorites", 0, items_cb, NULL);
    assert(last_count == 1 && last_items[0].played && last_items[0].resume_pos_sec == 0);
    p->list_catalog("queue", 0, items_cb, NULL); assert(last_count == 0);
    assert(tube_toggle_queue("12345678901") == 1);
    p->list_catalog("settings:autoplay",0,items_cb,NULL); /* opt in for this profile */
    p->stream_chosen("abcdefghijk", &selected);
    assert(tube_finish_and_next(100, 100));
    p->list_catalog("library:profile:guest", 0, items_cb, NULL);
    assert(!tube_take_next(&next)); /* a staged next item cannot cross profiles */
    cJSON *stored = read_store();
    assert(cJSON_GetObjectItem(stored, "version")->valueint == 2);
    int guest_records = 0, fixture_records = 0; cJSON *entry;
    cJSON_ArrayForEach(entry, cJSON_GetObjectItem(stored, "records")) {
        if (strcmp(cJSON_GetObjectItem(entry, "id")->valuestring, "abcdefghijk")) continue;
        if (!strcmp(cJSON_GetObjectItem(entry, "profile")->valuestring, "guest")) ++guest_records;
        if (!strcmp(cJSON_GetObjectItem(entry, "profile")->valuestring, "fixture")) ++fixture_records;
    }
    assert(guest_records == 1 && fixture_records == 1); cJSON_Delete(stored);
    /* A v1 global library migrates to Guest, never the selected account. */
    stored = cJSON_Parse("{\"version\":1,\"library_profile\":\"fixture\",\"records\":[{\"id\":\"abcdefghijk\",\"title\":\"Legacy favorite\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"position\":37,\"duration\":100,\"updated\":123,\"favorite\":true,\"queued\":true,\"subscribed\":true}]}");
    assert(stored); write_store(stored); cJSON_Delete(stored); assert(p->init() == 0);
    p->list_catalog("favorites", 0, items_cb, NULL); assert(last_count == 0);
    p->list_catalog("library:profile:guest", 0, items_cb, NULL);
    p->list_catalog("favorites", 0, items_cb, NULL); assert(last_count == 1 && last_items[0].resume_pos_sec == 37);
    p->list_catalog("queue", 0, items_cb, NULL); assert(last_count == 1);
    p->list_catalog("subscriptions", 0, items_cb, NULL); assert(last_count == 1);
    stored = read_store(); assert(cJSON_GetObjectItem(stored, "version")->valueint == 2);
    cJSON *rows = cJSON_GetObjectItem(stored, "records"); entry = cJSON_GetArrayItem(rows, 0);
    assert(!strcmp(cJSON_GetObjectItem(entry, "profile")->valuestring, "guest"));
    cJSON *duplicate = cJSON_Duplicate(entry, 1); cJSON_ReplaceItemInObject(duplicate, "position", cJSON_CreateNumber(99));
    cJSON_AddItemToArray(rows, duplicate);
    cJSON *invalid = cJSON_Duplicate(entry, 1); cJSON_ReplaceItemInObject(invalid, "profile", cJSON_CreateString("../bad"));
    cJSON_AddItemToArray(rows, invalid);
    cJSON *missing = cJSON_Duplicate(entry, 1); cJSON_DeleteItemFromObject(missing, "profile"); cJSON_AddItemToArray(rows, missing);
    write_store(stored); cJSON_Delete(stored); assert(p->init() == 0);
    p->list_catalog("favorites", 0, items_cb, NULL); assert(last_count == 1 && last_items[0].resume_pos_sec == 37);
    p->shutdown();
    stored = read_store();
    cJSON *future = cJSON_Duplicate(stored, 1);
    cJSON_ReplaceItemInObject(future, "version", cJSON_CreateNumber(2.5)); write_store(future); cJSON_Delete(future);
    assert(p->init() < 0); p->shutdown();
    future = read_store(); assert(cJSON_GetObjectItem(future, "version")->valuedouble == 2.5); cJSON_Delete(future);
    future = cJSON_Duplicate(stored, 1);
    cJSON_ReplaceItemInObject(future, "records", cJSON_CreateString("malformed")); write_store(future); cJSON_Delete(future);
    assert(p->init() < 0); p->shutdown();
    future = read_store(); assert(cJSON_IsString(cJSON_GetObjectItem(future, "records"))); cJSON_Delete(future);
    write_store(stored); cJSON_Delete(stored); assert(p->init() == 0); p->shutdown();
    printf("PASS: native provider, local state, stable IDs, pairing, jobs, cancellation, origins, SponsorBlock, chapters, discussions, account intents\n");
    return 0;
}
