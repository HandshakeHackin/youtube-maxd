/* Shipping HTTP/provider path; SPDX-License-Identifier: GPL-3.0-or-later */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const char *paid = "{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"duration_sec\":100,\"paid_content_text\":\"Contains paid promotion\",\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/test/a.mpd\"}]}}";
static void resolve_paid(const evo_provider_t *p) {
    assert(p->resolve("abcdefghijk", resolve_cb, NULL) == 0);
    assert(!tube_take_paid_content_notice());
    poll_job(paid); assert(last_ok && !pending_count);
}
int main(void) {
    char temporary[] = "/tmp/tube-paid-XXXXXX";
    assert(mkdtemp(temporary) && setenv("EVO_DATA_DIR_OVERRIDE", temporary, 1) == 0);
    const evo_provider_t *p = &evo_provider_youtube; assert(p->init() == 0);
    assert(p->set_source("http://127.0.0.1:8765#123456") == 0);
    p->search("fixture", 0, items_cb, NULL);
    reply(0, 1, 200, "{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\"}],\"has_more\":false}}");
    p->list_catalog("settings:captions", 0, items_cb, NULL);
    evo_stream_choice_t selected = {0};
    snprintf(selected.url, sizeof selected.url, "http://127.0.0.1:8765/media/test/a.mpd");
    resolve_paid(p); p->stream_chosen("abcdefghijk", &selected);
    assert(!tube_take_paid_content_notice()); /* default off, SponsorBlock also off */
    p->list_catalog("settings:sb:paid", 0, items_cb, NULL);
    assert(p->init() == 0); /* opt-in survives reload */
    resolve_paid(p); assert(!tube_take_paid_content_notice()); /* not selected yet */
    p->stream_chosen("12345678901", &selected); assert(!tube_take_paid_content_notice());
    p->stream_chosen("abcdefghijk", &selected);
    const char *notice = tube_take_paid_content_notice(); assert(notice && !strcmp(notice, "Contains paid promotion"));
    assert(!tube_take_paid_content_notice());
    assert(tube_queue_recovery(10, 0, 0));
    evo_provider_item_t item; double position; int subtitles;
    assert(tube_take_recovery(&item, &position, &subtitles));
    resolve_paid(p); p->stream_chosen("abcdefghijk", &selected); tube_recovery_finish();
    assert(!tube_take_paid_content_notice()); /* recovery never repeats an already shown notice */
    resolve_paid(p); p->stream_chosen("abcdefghijk", &selected);
    p->list_catalog("library:profile:another", 0, items_cb, NULL); assert(!tube_take_paid_content_notice());
    p->list_catalog("library:profile:guest", 0, items_cb, NULL);
    resolve_paid(p); p->stream_chosen("abcdefghijk", &selected);
    assert(p->set_source("http://127.0.0.1:9000#123456") == 0); assert(!tube_take_paid_content_notice());
    p->list_catalog("settings:sb:paid", 0, items_cb, NULL); assert(p->init() == 0);
    cJSON *saved=read_store(); assert(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(saved,"paid_content_notifications"))); cJSON_Delete(saved);
    puts("PASS: shipping paid-content opt-in persistence, default off, independent metadata, once-only start, recovery and stale scopes");
    return 0;
}
