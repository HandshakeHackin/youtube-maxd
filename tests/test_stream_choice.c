/* Per-video quality and audio track on the shipping provider: the qualities a resolve listed
 * (one row per resolution, frame rate and range), the audio tracks of a dubbed video, and a
 * choice that reopens the video at its position with exactly that stream.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define main native_regression_main
#include "test_native.c"
#undef main

#define CHOICE(n, w, h, fps, codec, audio) "{\"url\":\"http://127.0.0.1:8765/media/s/d" #n ".mpd\",\"label\":\"" #h "p " #fps " fps " codec "\",\"container\":\"dash\"," \
    "\"width\":" #w ",\"height\":" #h ",\"fps\":" #fps ",\"video_codec\":\"" codec "\",\"audio_codec\":\"" audio "\",\"audio_channels\":2,\"video_range\":\"SDR\"}"
static const char *answer(const char *audio) {
    static char text[4096];
    snprintf(text, sizeof text, "{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"title\":\"Dubbed\",\"duration_sec\":100,\"choices\":["
        CHOICE(0, 1920, 1080, 60, "h264", "aac") "," CHOICE(1, 1920, 1080, 60, "vp9", "aac") "," CHOICE(2, 1280, 720, 30, "h264", "aac") ","
        CHOICE(3, 854, 480, 30, "h264", "aac") "],\"audio_track\":\"%s\",\"audio_tracks\":["
        "{\"id\":\"en-US.4\",\"language\":\"en-US\",\"name\":\"English (US) original\",\"original\":true},"
        "{\"id\":\"de.3\",\"language\":\"de\",\"name\":\"German\",\"dubbed\":true},"
        "{\"id\":\"en-US.2\",\"language\":\"en-US\",\"name\":\"English (US) audio description\",\"descriptive\":true},"
        "{\"id\":\"bad id\",\"name\":\"Ignored\"}]}}", audio);
    return text;
}
static evo_stream_choice_t listed[8]; static int listed_count;
static void resolved_cb(int ok, const evo_stream_choice_t *choices, int count, void *ud) {
    (void)ud; last_ok = ok; listed_count = ok ? count : 0;
    if (ok) memcpy(listed, choices, (size_t)(count < 8 ? count : 8) * sizeof *choices);
}
static void resolve_video(const char *audio, const char *expect_audio_field) {
    assert(evo_provider_youtube.resolve("abcdefghijk", resolved_cb, NULL) == 0);
    if (expect_audio_field) assert(strstr(pending[0].body, expect_audio_field));
    else assert(!strstr(pending[0].body, "audio_track"));
    poll_job(answer(audio));
    assert(last_ok && listed_count == 4);
}
#ifndef MAXD_STREAM_HELPERS_ONLY
int main(void) {
    char temporary[] = "/tmp/prosperotube-streams-XXXXXX";
    assert(mkdtemp(temporary)); assert(setenv("EVO_DATA_DIR_OVERRIDE", temporary, 1) == 0);
    const evo_provider_t *p = &evo_provider_youtube;
    assert(p->init() == 0);
    assert(p->set_source("http://127.0.0.1:8765#123456") == 0);
    assert(p->search("fixture", 0, items_cb, NULL) == 0);
    reply(0, 1, 200, "{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[]}}");
    assert(!tube_player_quality_count() && !tube_player_audio_count());
    resolve_video("en-US.4", NULL);
    p->stream_chosen("abcdefghijk", &listed[0]);
    /* Qualities: one row per resolution and frame rate, best first; Auto is the first. */
    assert(tube_player_quality_count() == 3);
    char label[48];
    assert(tube_player_quality_label(0, label, sizeof label) && !strcmp(label, "1080p60"));
    assert(tube_player_quality_label(1, label, sizeof label) && !strcmp(label, "720p"));
    assert(tube_player_quality_label(2, label, sizeof label) && !strcmp(label, "480p"));
    assert(!tube_player_quality_label(3, label, sizeof label) && !*label);
    assert(!strcmp(tube_player_quality_detail(1), "720p 30 fps h264"));
    assert(tube_player_quality_playing() == 0 && tube_player_quality_pinned() == -1);
    /* Audio tracks: the bad identity is dropped; the playing one is YouTube's default. */
    assert(tube_player_audio_count() == 3 && tube_player_audio_playing() == 0);
    assert(!strcmp(tube_player_audio_name(1), "German") && !strcmp(tube_player_audio_language(1), "de"));
    assert(tube_player_audio_kind(0) == TUBE_AUDIO_ORIGINAL && tube_player_audio_kind(1) == TUBE_AUDIO_DUBBED &&
           tube_player_audio_kind(2) == TUBE_AUDIO_DESCRIPTIVE);
    assert(tube_player_audio_select(0, 10, 0, 1) == 2 && tube_player_quality_select(-1, 10, 0, 1) == 2);   /* already playing */
    assert(!tube_player_quality_select(3, 10, 0, 1) && !tube_player_audio_select(5, 10, 0, 1));

    /* 720p: the video reopens at its position with exactly that stream; the request stays the same. */
    assert(tube_player_quality_select(1, 42.5, 0, 1) == 1 && tube_recovery_pending() && tube_player_quality_pinned() == 1);
    assert(!tube_player_quality_select(2, 42.5, 0, 1));   /* one reopen at a time */
    evo_provider_item_t item; double position; int subtitles;
    assert(tube_take_recovery(&item, &position, &subtitles) && position == 42.5 && subtitles == 1);
    resolve_video("en-US.4", NULL);
    assert(!tube_error_fix_relaxed());
    int chosen = tube_recovery_choice(listed, listed_count);
    assert(chosen == 2);
    p->stream_chosen("abcdefghijk", &listed[chosen]); tube_recovery_finish();
    assert(tube_player_quality_playing() == 1 && tube_player_quality_pinned() == 1);
    /* A later ordinary recovery (an expired link) keeps the stream that plays. */
    assert(tube_queue_recovery(50, 0, 0) && tube_take_recovery(&item, &position, &subtitles));
    resolve_video("en-US.4", NULL);
    assert(tube_recovery_choice(listed, listed_count) == 2); tube_recovery_finish();

    /* German: the same picture, the service asked for that audio track. */
    assert(tube_player_audio_select(1, 61, 0, 0) == 1);
    assert(tube_take_recovery(&item, &position, &subtitles) && position == 61 && !subtitles);
    resolve_video("de.3", "\"audio_track\":\"de.3\"");
    chosen = tube_recovery_choice(listed, listed_count);
    assert(chosen == 2);
    p->stream_chosen("abcdefghijk", &listed[chosen]); tube_recovery_finish();
    assert(tube_player_audio_playing() == 1 && tube_player_quality_playing() == 1);
    /* Auto returns to the best stream and keeps the audio track. */
    assert(tube_player_quality_select(-1, 70, 0, 0) == 1 && tube_take_recovery(&item, &position, &subtitles));
    resolve_video("de.3", "\"audio_track\":\"de.3\"");
    assert(tube_recovery_choice(listed, listed_count) == 0); p->stream_chosen("abcdefghijk", &listed[0]); tube_recovery_finish();
    assert(tube_player_quality_pinned() == -1 && tube_player_quality_playing() == 0);
    /* A stream that is no longer listed cannot be guessed. */
    assert(tube_player_quality_select(2, 80, 0, 0) == 1 && tube_take_recovery(&item, &position, &subtitles));
    resolve_video("de.3", "\"audio_track\":\"de.3\"");
    assert(tube_recovery_choice(listed, 2) == -1); tube_recovery_finish();

    /* The next selection of a video starts at Auto with YouTube's default audio track. */
    resolve_video("en-US.4", NULL);
    p->stream_chosen("abcdefghijk", &listed[0]);
    assert(tube_player_quality_pinned() == -1 && tube_player_audio_playing() == 0);
    p->shutdown();
    puts("PASS: per-video qualities and audio tracks, reopen at the position with the stream chosen, Auto, nothing kept for the next video");
    return 0;
}
#endif
