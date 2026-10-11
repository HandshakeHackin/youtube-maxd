/* Reuse the actual provider HTTP harness and also run its original regression.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define main native_regression_main
#include "test_native.c"
#undef main

static evo_stream_choice_t recovered;
static int quality_changed;
static void recovery_resolved(int ok, const evo_stream_choice_t *choices, int count, void *ud) {
    (void)ud; last_ok = ok;
    assert(ok && count == 1); recovered = choices[0];
}

static void resolution(const evo_provider_t *p, int fresh) {
    assert(p->resolve("abcdefghijk", recovery_resolved, NULL) == 0);
    assert(strstr(pending[0].body, fresh ? "\"fresh\":true" : "\"fresh\":false"));
    const int original = fresh || !quality_changed;
    assert(strstr(pending[0].body,original ? "\"max_height\":1080" : "\"max_height\":2160"));
    assert(strstr(pending[0].body,original ? "\"max_fps\":30" : "\"max_fps\":60"));
    assert(strstr(pending[0].body,original ? "\"codec\":\"avc\"" : "\"codec\":\"vp9\""));
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"title\":\"Recovery fixture\",\"duration_sec\":100,\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/fresh/d0.mpd\",\"container\":\"dash\",\"width\":1920,\"height\":1080,\"fps\":29.97,\"video_codec\":\"avc\",\"audio_codec\":\"aac\",\"audio_channels\":2}]}}");
    if (!quality_changed) {
        /* Change next-video preferences after resolution, before the native choice is bound. */
        p->list_catalog("settings:quality:2160",0,items_cb,NULL);
        p->list_catalog("settings:frame-rate:60",0,items_cb,NULL);
        p->list_catalog("settings:codec",0,items_cb,NULL);quality_changed=1;
    }
}

int main(void) {
    char temporary[] = "/tmp/prosperotube-recovery-XXXXXX";
    assert(mkdtemp(temporary)); assert(setenv("EVO_DATA_DIR_OVERRIDE", temporary, 1) == 0);
    const evo_provider_t *p = &evo_provider_youtube;
    assert(p->init() == 0);
    assert(p->set_source("http://127.0.0.1:8765#123456") == 0);
    assert(p->search("fixture", 0, items_cb, NULL) == 0);
    reply(0, 1, 200, "{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[]}}");
    p->list_catalog("settings:captions", 0, items_cb, NULL);
    p->list_catalog("settings:frame-rate:30",0,items_cb,NULL);
    p->list_catalog("settings:codec",0,items_cb,NULL);
    resolution(p, 0); assert(recovered.fps==29.97); p->stream_chosen("abcdefghijk", &recovered);
    unsigned selection = tube_phone_selection(); assert(selection);
    assert(tube_toggle_queue("abcdefghijk") == 1);
    assert(!tube_queue_recovery(NAN, 0, 1));
    assert(!tube_queue_recovery(-1, 0, 1));
    assert(!tube_queue_recovery(1000001, 0, 1));
    assert(!tube_queue_recovery(1, 1, 1));
    evo_provider_item_t item; double position; int subtitles;
    for (int attempt = 0; attempt < 3; ++attempt) {
        assert(tube_queue_recovery(37.625, 0, attempt % 2));
        assert(tube_recovery_pending());
        assert(!tube_queue_recovery(40, 0, 1));
        assert(tube_take_recovery(&item, &position, &subtitles));
        assert(!strcmp(item.id, "abcdefghijk") && position == 37.625 && subtitles == attempt % 2);
        assert(tube_recovery_active() && !tube_take_recovery(&item, &position, &subtitles));
        resolution(p, 1);
        evo_stream_choice_t choices[2] = {recovered, recovered};
        assert(tube_recovery_choice(choices, 2) < 0); // ambiguous
        choices[0].height = 720;
        assert(tube_recovery_choice(choices, 2) == 1); // retains the chosen quality
        choices[1].fps = 60; assert(tube_recovery_choice(choices,2)<0);
        choices[1].fps = 0; assert(tube_recovery_choice(choices,2)<0);
        choices[1].fps = 30; assert(tube_recovery_choice(choices,2)<0);
        choices[1].fps = NAN; assert(tube_recovery_choice(choices,2)<0);
        choices[1].fps = recovered.fps;
        choices[1].audio_channels = 6;
        assert(tube_recovery_choice(choices, 2) < 0);
        assert(tube_recovery_choice(&recovered, 1) == 0);
        p->stream_chosen("abcdefghijk", &recovered); // a reopen must not reset the retry budget
        assert(tube_phone_selection() == selection);
        tube_recovery_finish();
    }
    assert(!tube_queue_recovery(38, 0, 1));
    p->list_catalog("queue", 0, items_cb, NULL);
    assert(last_count == 1 && !strcmp(last_items[0].id, "abcdefghijk") && !last_items[0].played);
    resolution(p,0); // The next explicit resolution adopts the new profile preferences.
    p->stream_chosen("abcdefghijk", &recovered); // explicit selection starts a new budget
    assert(tube_phone_selection() != selection);
    assert(tube_queue_recovery(39, 0, 0)); tube_cancel();
    assert(!tube_recovery_pending() && !tube_take_recovery(&item, &position, &subtitles));
    assert(tube_queue_recovery(39, 0, 0));
    p->list_catalog("library:profile:another", 0, items_cb, NULL);
    assert(!tube_take_recovery(&item, &position, &subtitles) && !tube_queue_recovery(39, 0, 0));
    p->list_catalog("library:profile:guest", 0, items_cb, NULL);
    p->stream_chosen("abcdefghijk", &recovered);
    assert(tube_queue_recovery(39, 0, 0));
    assert(p->set_source("http://127.0.0.1:9000#123456") == 0);
    assert(!tube_take_recovery(&item, &position, &subtitles) && !tube_queue_recovery(39, 0, 0));
    assert(p->set_source("http://127.0.0.1:8765#123456") == 0);
    // Re-pair and resolve before a new selected stream can recover.
    assert(p->resolve("abcdefghijk", recovery_resolved, NULL) == 0);
    reply(0, 1, 200, "{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"title\":\"Live fixture\",\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/live/h0.m3u8\",\"container\":\"hls\",\"is_live\":true}]}}");
    p->stream_chosen("abcdefghijk", &recovered);
    assert(tube_queue_recovery(500, 1, 1));
    assert(tube_take_recovery(&item, &position, &subtitles) && item.is_live && position == 0);
    tube_recovery_finish(); p->shutdown();
    printf("PASS: shipping recovery scope, fractional VOD resume, live edge, stream matching, budget and cancellation\n");
    return 0;
}
