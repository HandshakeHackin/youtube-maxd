/* Shipping provider with the same queued HTTP harness as its broad regression.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define main existing_provider_regression
#include "test_native.c"
#undef main

static const char *resolved_fixture = "{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"duration_sec\":100,\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/test/a.mpd\",\"video_codec\":\"h264\",\"audio_codec\":\"aac\"}]}}";
static const evo_provider_t *provider;
static void segments(const char *body) {
    assert(pending_count == 0);
    assert(provider->resolve("abcdefghijk", resolve_cb, NULL) == 0);
    poll_job(resolved_fixture);
    assert(pending_count == 1 && strstr(pending[0].url, "/v1/sponsor/abcdefghijk?categories="));
    assert(strstr(pending[0].url, "intro") && strstr(pending[0].url, "filler") && strstr(pending[0].url, "poi_highlight"));
    reply(0, 1, 200, body);
}
static void action(const char *category, int desired) {
    char id[80]; snprintf(id, sizeof id, "settings:sb:action:%s", category);
    for (int i = 0; i < 4; ++i) {
        cJSON *root = read_store();
        cJSON *settings = cJSON_GetObjectItemCaseSensitive(root, "sponsor_settings");
        int current = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(settings, "actions"), category)->valueint;
        cJSON_Delete(root);
        if (current == desired) return;
        provider->list_catalog(id, 0, items_cb, NULL); assert(last_ok);
    }
    assert(0);
}
int main(void) {
    char temporary[] = "/tmp/tube-sponsor-XXXXXX";
    assert(mkdtemp(temporary) && setenv("EVO_DATA_DIR_OVERRIDE", temporary, 1) == 0);
    provider = &evo_provider_youtube; assert(provider->init() == 0);
    assert(provider->set_source("http://127.0.0.1:8765#123456") == 0);
    provider->search("fixture", 0, items_cb, NULL);
    reply(0, 1, 200, "{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"duration_sec\":100}],\"has_more\":false}}");
    provider->list_catalog("settings:sponsor", 0, items_cb, NULL);
    provider->list_catalog("settings:captions", 0, items_cb, NULL);
    provider->list_catalog("settings:sb", 0, items_cb, NULL); assert(last_ok && last_count == 13);
    segments("{\"segments\":[{\"start\":10,\"end\":20,\"category\":\"sponsor\"},{\"start\":15,\"end\":40,\"category\":\"intro\"},{\"start\":35,\"end\":50,\"category\":\"filler\"}]}");
    assert(tube_skip_target(12) == 40 && tube_sponsor_show_message());
    assert(tube_skip_target(60) < 0 && tube_skip_target(38) < 0); // Remaining skip is shorter than default five seconds.
    for (int i = 0; i < 5; ++i) provider->list_catalog("settings:sb:minimum", 0, items_cb, NULL);
    action("sponsor", 0);
    segments("{\"segments\":[{\"start\":10,\"end\":20,\"category\":\"sponsor\"},{\"start\":15,\"end\":40,\"category\":\"intro\"}]}");
    assert(tube_skip_target(12) == 20 && !tube_sponsor_show_message());
    assert(tube_skip_target(21) == 40 && tube_sponsor_show_message());
    action("sponsor", 2);
    action("intro", 2);
    segments("{\"segments\":[{\"start\":10,\"end\":20,\"category\":\"sponsor\"},{\"start\":15,\"end\":30,\"category\":\"intro\"}]}");
    assert(tube_skip_target(12) < 0 && tube_sponsor_prompt(12));
    assert(tube_sponsor_confirm(12, 0) < 0 && !tube_sponsor_prompt(12));
    assert(tube_skip_target(16) < 0 && !tube_sponsor_prompt(16));
    assert(tube_skip_target(40) < 0 && tube_skip_target(12) < 0 && tube_sponsor_prompt(12));
    assert(tube_sponsor_confirm(12, 1) == 30 && tube_sponsor_confirm(12, 1) < 0);
    assert(!tube_sponsor_prompt(31));
    action("sponsor", 0);
    provider->list_catalog("settings:sb:repeat", 0, items_cb, NULL);
    segments("{\"segments\":[{\"start\":10,\"end\":30,\"category\":\"sponsor\"}]}");
    assert(tube_skip_target(12) == 30 && tube_skip_target(40) < 0 && tube_skip_target(12) < 0);
    provider->list_catalog("settings:sb:exclude:UCaaaaaaaaaaaaaaaaaaaaaa", 0, items_cb, NULL);
    assert(last_ok && !tube_sponsor_prompt(12));
    assert(provider->init() == 0);
    provider->resolve("abcdefghijk", resolve_cb, NULL); poll_job(resolved_fixture);
    assert(pending_count == 0 && tube_skip_target(12) < 0); // Exclusion survived reload and prevented the fetch.
    provider->list_catalog("settings:sb:exclude:UCaaaaaaaaaaaaaaaaaaaaaa", 0, items_cb, NULL);
    provider->list_catalog("settings:sb:repeat", 0, items_cb, NULL);
    provider->resolve("abcdefghijk", resolve_cb, NULL); poll_job(resolved_fixture);
    assert(pending_count == 1);
    provider->resolve("abcdefghijk", resolve_cb, NULL);
    reply(1, 1, 200, "{\"job\":\"new_job\",\"state\":\"pending\"}");
    usleep(300000); tube_poll();
    reply(1, 1, 200, resolved_fixture); assert(pending_count == 2);
    reply(0, 1, 200, "{\"segments\":[{\"start\":0,\"end\":90,\"category\":\"sponsor\"}]}");
    assert(tube_skip_target(12) < 0); // Old request cannot fill the new video's segment list.
    reply(0, 1, 200, "{\"segments\":[{\"start\":10,\"end\":25,\"category\":\"sponsor\"},{\"start\":0,\"end\":99,\"category\":\"unknown\"},{\"start\":-1,\"end\":90,\"category\":\"intro\"}]}");
    assert(tube_skip_target(12) == 25);
    /* A skip to the known end completes deliberately, without claiming a
     * natural drain or asking the decoder for an impossible endpoint frame. */
    action("sponsor", 1);
    segments("{\"segments\":[{\"start\":80,\"end\":105,\"category\":\"sponsor\"}]}");
    evo_stream_choice_t selected = {0};
    snprintf(selected.url, sizeof selected.url, "http://127.0.0.1:8765/media/test/a.mpd");
    provider->stream_chosen("abcdefghijk", &selected);
    provider->report_progress("abcdefghijk", 80, 100, EVO_PROVIDER_PLAY_START);
    assert(!tube_sponsor_finish(105, 100)); // No acknowledged skip yet.
    double target = tube_skip_target(80);
    assert(target == 105 && tube_sponsor_show_message());
    assert(!tube_sponsor_finish(target, NAN) && !tube_sponsor_finish(target, 0));
    assert(!tube_sponsor_finish(NAN, 100) && !tube_sponsor_finish(100, 100));
    assert(!tube_sponsor_finish(target, 110)); // In-video targets must seek instead.
    assert(tube_sponsor_finish(target, 100));
    assert(!tube_sponsor_finish(target, 100)); // One completion, even without autoplay.
    provider->report_progress("abcdefghijk", 80, 100, EVO_PROVIDER_PLAY_STOP);
    provider->list_catalog("history", 0, items_cb, NULL);
    assert(last_ok && last_count == 1 && last_items[0].played && !last_items[0].resume_pos_sec);
    assert(!tube_take_next(&last_items[0]));
    // Confirmed end skips use the same queue transition as normal completion.
    provider->list_catalog("settings:autoplay", 0, items_cb, NULL);
    assert(tube_toggle_queue("abcdefghijk") == 1 && tube_toggle_queue("12345678901") == 1);
    action("sponsor", 2);
    segments("{\"segments\":[{\"start\":80,\"end\":100,\"category\":\"sponsor\"}]}");
    provider->stream_chosen("abcdefghijk", &selected);
    assert(tube_skip_target(80) < 0 && tube_sponsor_prompt(80));
    assert(tube_sponsor_confirm(80, 0) < 0 && !tube_sponsor_finish(100, 100));
    assert(tube_skip_target(10) < 0 && tube_skip_target(80) < 0 && tube_sponsor_prompt(80));
    target = tube_sponsor_confirm(80, 1); assert(target == 100);
    assert(tube_sponsor_finish(target, 100));
    evo_provider_item_t next;
    assert(tube_take_next(&next) && !strcmp(next.id, "12345678901"));
    assert(!tube_take_next(&next));
    provider->report_progress("abcdefghijk", 80, 100, EVO_PROVIDER_PLAY_STOP);
    provider->list_catalog("queue", 0, items_cb, NULL);
    assert(last_count == 1 && !strcmp(last_items[0].id, "12345678901"));
    // Resolving again invalidates completion authorization from the old instance.
    action("sponsor", 0);
    segments("{\"segments\":[{\"start\":80,\"end\":100,\"category\":\"sponsor\"}]}");
    provider->stream_chosen("abcdefghijk", &selected);
    assert(tube_skip_target(80) == 100);
    segments("{\"segments\":[{\"start\":80,\"end\":100,\"category\":\"sponsor\"}]}");
    assert(!tube_sponsor_finish(100, 100));
    assert(tube_skip_target(80) == 100 && !tube_sponsor_finish(100, 100));
    // A stale or excluded playing item cannot advance a new video's queue.
    provider->stream_chosen("12345678901", &selected);
    assert(!tube_sponsor_finish(100, 100));
    provider->stream_chosen("abcdefghijk", &selected);
    assert(tube_skip_target(80) == 100);
    provider->list_catalog("settings:sb:exclude:UCaaaaaaaaaaaaaaaaaaaaaa", 0, items_cb, NULL);
    assert(!tube_sponsor_finish(100, 100));
    provider->list_catalog("settings:sb:exclude:UCaaaaaaaaaaaaaaaaaaaaaa", 0, items_cb, NULL);
    // Minimum skip duration is the playable remainder, even if metadata overruns.
    for (int i = 0; i < 3; ++i) provider->list_catalog("settings:sb:minimum", 0, items_cb, NULL);
    segments("{\"segments\":[{\"start\":80,\"end\":105,\"category\":\"sponsor\"}]}");
    provider->stream_chosen("abcdefghijk", &selected);
    assert(tube_skip_target(97) < 0); // Three playable seconds, not eight.
    assert(tube_skip_target(90) == 105);
    provider->list_catalog("settings:sponsor", 0, items_cb, NULL);
    assert(!tube_sponsor_finish(105, 100));
    provider->list_catalog("settings:sponsor", 0, items_cb, NULL);
    assert(!tube_sponsor_finish(105, 100)); // Re-enabling cannot revive an old authorization.
    provider->list_catalog("settings:sb:colors", 0, items_cb, NULL);
    assert(last_ok && last_count == 9);
    segments("{\"segments\":[{\"start\":10,\"end\":20,\"category\":\"sponsor\"},{\"start\":30,\"end\":40,\"category\":\"filler\"},{\"start\":50,\"end\":50,\"category\":\"poi_highlight\"},{\"start\":90,\"end\":105,\"category\":\"outro\"},{\"start\":101,\"end\":101,\"category\":\"poi_highlight\"},{\"start\":1,\"end\":2,\"category\":\"poi_highlight\"}]}");
    tube_sponsor_marker_t markers[128];
    assert(!tube_sponsor_markers(100, markers, 128)); // Resolution alone is not a playing instance.
    provider->stream_chosen("abcdefghijk", &selected);
    assert(tube_sponsor_markers(100, markers, 128) == 4);
    assert(markers[0].start == .1 && markers[0].end == .2 && markers[0].rgb == 0x00ff00);
    assert(markers[1].rgb == 0x7300ff && !markers[1].point);
    assert(markers[2].point && markers[2].start == .5 && markers[2].end == .5 && markers[2].rgb == 0xff1684);
    assert(markers[3].start == .9 && markers[3].end == 1);
    assert(tube_sponsor_markers(100, markers, 1) == 1);
    assert(!tube_sponsor_markers(NAN, markers, 128) && !tube_sponsor_markers(0, markers, 128));
    assert(!tube_sponsor_markers(100, NULL, 128) && !tube_sponsor_markers(100, markers, 0));
    assert(tube_skip_target(32) < 0 && tube_skip_target(50) < 0); // Markers never create skip intervals.
    provider->list_catalog("settings:sb:color:filler", 0, items_cb, NULL);
    assert(tube_sponsor_markers(100, markers, 128) == 3); // Browsing cancellation is not playback cancellation.
    cJSON *persisted = read_store();
    assert(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(persisted, "sponsor_settings"), "color_markers"), "filler")));
    cJSON_Delete(persisted);
    assert(provider->init() == 0 && !tube_sponsor_markers(100, markers, 128));
    provider->list_catalog("settings:sb:colors", 0, items_cb, NULL);
    assert(last_count == 9 && strstr(last_items[7].title, "off"));
    provider->list_catalog("settings:sb:color:invalid", 0, items_cb, NULL); assert(!last_ok);
    provider->resolve("abcdefghijk", resolve_cb, NULL); poll_job(resolved_fixture);
    assert(pending_count == 1 && !strstr(pending[0].url, "filler") && strstr(pending[0].url, "poi_highlight"));
    tube_cancel(); // Cancel browsing while this playing video's optional metadata is in flight.
    reply(0, 1, 200, "{\"segments\":[{\"start\":30,\"end\":40,\"category\":\"filler\"},{\"start\":50,\"end\":50,\"category\":\"poi_highlight\"}]}");
    provider->stream_chosen("abcdefghijk", &selected);
    assert(tube_sponsor_markers(100, markers, 128) == 1 && markers[0].point);
    tube_cancel();
    assert(tube_sponsor_markers(100, markers, 128) == 1); // Keep valid playback data across menu/chat request cancellation.
    provider->list_catalog("library:profile:another", 0, items_cb, NULL);
    assert(!tube_sponsor_markers(100, markers, 128));
    provider->list_catalog("library:profile:guest", 0, items_cb, NULL);
    assert(!tube_sponsor_markers(100, markers, 128)); // Switching back cannot revive the old instance.
    provider->resolve("abcdefghijk", resolve_cb, NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"duration_sec\":0,\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/live/a.m3u8\",\"video_codec\":\"h264\",\"audio_codec\":\"aac\",\"is_live\":true}]}}");
    assert(pending_count == 0 && tube_skip_target(12) < 0 && !tube_sponsor_prompt(12));
    provider->list_catalog("settings:sb:action:invalid", 0, items_cb, NULL); assert(!last_ok);
    assert(provider->list_catalog("settings:sb:exclusions", 1001, items_cb, NULL) < 0);
    puts("PASS: shipping SponsorBlock actions, confirmation, end skips, watched state, queue transitions, exclusions, persistence and stale replies");
    return 0;
}
