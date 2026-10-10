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
    assert(strstr(pending[0].url, "intro") && !strstr(pending[0].url, "filler"));
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
    provider->list_catalog("settings:sb", 0, items_cb, NULL); assert(last_ok && last_count == 11);
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
    provider->resolve("abcdefghijk", resolve_cb, NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"duration_sec\":0,\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/live/a.m3u8\",\"video_codec\":\"h264\",\"audio_codec\":\"aac\",\"is_live\":true}]}}");
    assert(pending_count == 0 && tube_skip_target(12) < 0 && !tube_sponsor_prompt(12));
    provider->list_catalog("settings:sb:action:invalid", 0, items_cb, NULL); assert(!last_ok);
    assert(provider->list_catalog("settings:sb:exclusions", 1001, items_cb, NULL) < 0);
    puts("PASS: shipping SponsorBlock category policy, merging, minimum duration, silent/message/confirmation actions, rewind guards, exclusions, persistence and stale replies");
    return 0;
}
