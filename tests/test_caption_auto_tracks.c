/* Automatic (speech recognition) tracks and machine translations on the shipping provider:
 * listed with their target languages, found for the interface language, and an automatic
 * track starts by itself only when "Automatic captions" allows it.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define main native_provider_regression_main
#include "test_native.c"
#undef main
#include <time.h>
static int64_t caption_clock = 100000;
int tube_test_clock_gettime(clockid_t clock, struct timespec *value) {
    assert(clock == CLOCK_MONOTONIC);
    value->tv_sec = caption_clock / 1000; value->tv_nsec = caption_clock % 1000 * 1000000; return 0;
}
#define TRACK(language, extra) "{\"language\":\"" language "\",\"url\":\"http://127.0.0.1:8765/media/session/" language ".srt\"" extra "}"
static const char *answer =
    "{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/session/video\"}],\"captions\":["
    TRACK("en", ",\"name\":\"English (auto-generated)\",\"automatic\":true") ","
    TRACK("fr-en", ",\"name\":\"French\",\"automatic\":true,\"translated\":true,\"target\":\"fr\"") ","
    TRACK("pt-en", ",\"name\":\"Portuguese\",\"automatic\":true,\"translated\":true,\"target\":\"pt\"") ","
    TRACK("zh-Hans-en", ",\"name\":\"Chinese (Simplified)\",\"automatic\":true,\"translated\":true,\"target\":\"zh-Hans\"") ","
    TRACK("de-en", ",\"name\":\"German\",\"automatic\":true,\"translated\":true,\"target\":\"../de\"") "]}}";
static void resolve_with_tracks(void) {
    last_ok = 0;
    evo_provider_youtube.resolve("abcdefghijk", resolve_cb, NULL);
    reply(0, 1, 200, "{\"job\":\"fixture_job\",\"state\":\"pending\"}");
    caption_clock += 500; tube_poll();
    reply(0, 1, 200, answer);
}
static void choose(void) {
    evo_stream_choice_t choice = {0}; strcpy(choice.url, "http://127.0.0.1:8765/media/session/video");
    evo_provider_youtube.stream_chosen("abcdefghijk", &choice);
}
static void start(const char *dir, int automatic) {
    char seed_text[256];
    snprintf(seed_text, sizeof seed_text, "{\"version\":2,\"source\":\"http://127.0.0.1:8765\",\"token\":\"synthetic-token\","
             "\"captions\":true,\"language\":\"en\",\"automatic_captions\":%s,\"records\":[]}", automatic ? "true" : "false");
    (void)dir;
    cJSON *seed = cJSON_Parse(seed_text); assert(seed); write_store(seed); cJSON_Delete(seed);
    assert(!evo_provider_youtube.init());
}
int main(void) {
    char dir[] = "/tmp/prosperotube-caption-auto-XXXXXX";
    assert(mkdtemp(dir)); assert(!setenv("EVO_DATA_DIR_OVERRIDE", dir, 1));
    start(dir, 0);
    /* Captions are on and the English track is automatic: with "Automatic captions" off
     * nothing is fetched by itself, and the video starts at once. */
    resolve_with_tracks();
    assert(last_ok && !pending_count);
    choose();
    assert(tube_caption_track_count() == 5 && !*tube_phone_caption_language());
    assert(tube_caption_track_automatic(0) && !tube_caption_track_translated(0) && !*tube_caption_track_target(0));
    assert(tube_caption_track_translated(1) && !strcmp(tube_caption_track_target(1), "fr"));
    /* The translation into the interface language: exact, then the same language first named. */
    assert(tube_caption_track_translation_for("fr") == 1);
    assert(tube_caption_track_translation_for("pt-BR") == 2 && tube_caption_track_translation_for("pt-PT") == 2);
    assert(tube_caption_track_translation_for("zh-Hans") == 3 && tube_caption_track_translation_for("ZH-hans") == 3);
    assert(tube_caption_track_translation_for("en") == -1 && tube_caption_track_translation_for("") == -1);
    assert(tube_caption_track_translation_for(NULL) == -1);
    /* A target that is not a language tag is dropped, the track stays selectable. */
    assert(tube_caption_track_translated(4) && !*tube_caption_track_target(4) && tube_caption_track_translation_for("de") == -1);
    /* A translation is switched to like any other track. */
    assert(tube_caption_switch_begin("fr-en", 0, 0) == 2);
    assert(pending_count == 1 && strstr(pending[0].url, "/media/session/fr-en.srt"));
    reply(0, 1, 200, "1\n00:00:00,000 --> 00:00:02,000\nBonjour\n\n");
    const char *media = ""; assert(tube_caption_switch_status(&media) == 1); tube_caption_switch_finish(1);
    assert(!strcmp(tube_phone_caption_language(), "fr-en"));
    evo_provider_youtube.shutdown();

    /* "Automatic captions: allowed": the automatic English track starts by itself. */
    start(dir, 1);
    resolve_with_tracks();
    assert(!last_ok && pending_count == 1 && strstr(pending[0].url, "/media/session/en.srt"));
    reply(0, 1, 200, "1\n00:00:00,000 --> 00:00:02,000\nHello\n\n");
    assert(last_ok && !pending_count);
    evo_provider_youtube.shutdown();
    char path[512];
    const char *files[] = {"youtube.json", "active.srt", "caption-a.srt", "caption-b.srt"};
    for (unsigned i = 0; i < sizeof files / sizeof *files; ++i) { snprintf(path, sizeof path, "%s/%s", dir, files[i]); unlink(path); }
    puts("PASS: automatic tracks and translations listed with targets, interface-language translation, automatic start only when allowed");
    return 0;
}
