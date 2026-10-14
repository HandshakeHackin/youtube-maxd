/* Player preferences outside the provider: storage, validation, option
 * groups, per-video facts and the pure decisions the player overlays use. */
#include "tube_player_extra.h"
#include "tube_time_display.h"
#include "cJSON.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int saves, save_result = 1;
static TubePlayerExtra last_saved;
static int save(const TubePlayerExtra *prefs, void *user) {
    assert(user == &saves);
    last_saved = *prefs; ++saves;
    return save_result;
}

static void test_storage(void) {
    TubePlayerExtra defaults;
    tube_player_extra_defaults(&defaults);
    /* Defaults leave today's player unchanged. */
    assert(defaults.subtitle_style == 0 && defaults.subtitle_scale == 100 && defaults.subtitle_position == -1);
    assert(defaults.volume == 100 && !defaults.auto_volume && !defaults.audio_delay_ms && !defaults.audio_delay_enabled);
    assert(!defaults.clock && !defaults.quality_info && !defaults.quality_bitrate && !defaults.clock_12h);
    assert(defaults.exit_mode == TUBE_EXIT_MODE_ASK && !defaults.keys_lr && !defaults.keys_ud && !defaults.seek_preview);
    assert(defaults.speed_list == 1 && !defaults.loop_shorts && defaults.remember_short == 1);
    /* Rows the options already had stay shown; nothing dims and nothing is announced. */
    assert(defaults.buttons == TUBE_BUTTONS_DEFAULT && defaults.suggestions == 1 && !defaults.chapter_notice);
    assert(defaults.error_fixer == 1 && !defaults.remember_quality && !defaults.dim_timeout && defaults.dim_amount == 80);
    assert(!defaults.screen_off_timeout && defaults.screen_off_amount == 100);
    /* Medium is the player's own read-ahead. */
    assert(defaults.video_buffer == TUBE_BUFFER_MEDIUM && tube_buffer_seconds(defaults.video_buffer, 30.0, 1) == 30.0 &&
           tube_buffer_seconds(defaults.video_buffer, 10.0, 0) == 10.0);

    TubePlayerExtra read = tube_player_extra_read(NULL);
    assert(!memcmp(&read, &defaults, sizeof read));
    cJSON *array = cJSON_CreateArray();
    read = tube_player_extra_read(array);
    assert(!memcmp(&read, &defaults, sizeof read));
    assert(!tube_player_extra_write(array, &defaults) && !tube_player_extra_write(NULL, &defaults));
    cJSON_Delete(array);

    /* A written profile reads back identically and leaves foreign fields alone. */
    TubePlayerExtra chosen = defaults;
    chosen.subtitle_style = 5; chosen.subtitle_scale = 150; chosen.subtitle_position = 125; chosen.volume = 235;
    chosen.auto_volume = 1; chosen.audio_delay_ms = -340; chosen.audio_delay_enabled = 1; chosen.clock = 1;
    chosen.quality_info = 1; chosen.quality_bitrate = 1; chosen.clock_12h = 1; chosen.exit_mode = TUBE_EXIT_MODE_DOUBLE;
    chosen.keys_lr = TUBE_KEYS_VOLUME; chosen.keys_ud = TUBE_KEYS_SPEED; chosen.seek_preview = 3; chosen.speed_list = 2;
    chosen.loop_shorts = 1; chosen.remember_short = 0;
    chosen.buttons = 1 << TUBE_BUTTON_SHARE; chosen.suggestions = 0; chosen.chapter_notice = 1; chosen.error_fixer = 0;
    chosen.remember_quality = 1; chosen.dim_timeout = 300; chosen.dim_amount = 85; chosen.screen_off_timeout = 7;
    chosen.screen_off_amount = 40; chosen.video_buffer = TUBE_BUFFER_HIGHEST;
    cJSON *object = cJSON_CreateObject();
    cJSON_AddStringToObject(object, "name", "Profile");
    cJSON_AddNumberToObject(object, "volume", 5);          /* replaced, not duplicated */
    assert(tube_player_extra_write(object, &chosen));
    assert(cJSON_GetArraySize(object) == 29 && cJSON_GetObjectItem(object, "video_buffer")->valueint == TUBE_BUFFER_HIGHEST);
    assert(!strcmp(cJSON_GetObjectItem(object, "name")->valuestring, "Profile"));
    read = tube_player_extra_read(object);
    assert(!memcmp(&read, &chosen, sizeof read));
    char *text = cJSON_PrintUnformatted(object);
    cJSON *parsed = cJSON_Parse(text);
    read = tube_player_extra_read(parsed);
    assert(!memcmp(&read, &chosen, sizeof read));
    cJSON_Delete(parsed); free(text); cJSON_Delete(object);

    /* Corrupt and out-of-range values fall back field by field. */
    parsed = cJSON_Parse("{\"subtitle_style\":7,\"subtitle_scale\":105,\"subtitle_position\":-2,\"volume\":\"loud\","
                         "\"auto_volume\":true,\"audio_delay_ms\":2001,\"audio_delay_enabled\":2,\"clock\":1.5,"
                         "\"quality_info\":1e300,\"quality_bitrate\":null,\"clock_12h\":[1],\"exit_mode\":3,"
                         "\"keys_lr\":5,\"keys_ud\":5,\"seek_preview\":-1,\"speed_list\":0,\"loop_shorts\":{},"
                         "\"remember_short\":false,\"buttons\":2097152,\"suggestions\":2,\"chapter_notice\":-1,\"error_fixer\":\"no\","
                         "\"remember_quality\":3,\"dim_timeout\":7,\"dim_amount\":5,\"screen_off_timeout\":10801,"
                         "\"screen_off_amount\":95,\"video_buffer\":4}");
    assert(parsed);
    read = tube_player_extra_read(parsed);
    TubePlayerExtra expected = defaults;
    expected.auto_volume = 1; expected.keys_ud = TUBE_KEYS_SPEED; expected.speed_list = 0; expected.remember_short = 0;
    assert(!memcmp(&read, &expected, sizeof read));
    cJSON_Delete(parsed);
    parsed = cJSON_Parse("{\"subtitle_position\":1000,\"subtitle_scale\":10,\"volume\":300,\"audio_delay_ms\":-2000}");
    read = tube_player_extra_read(parsed);
    assert(read.subtitle_position == 1000 && read.subtitle_scale == 10 && read.volume == 300 && read.audio_delay_ms == -2000);
    cJSON_Delete(parsed);
    parsed = cJSON_Parse("{\"subtitle_position\":12,\"volume\":302,\"subtitle_scale\":210,\"video_buffer\":-1}");
    read = tube_player_extra_read(parsed);
    assert(!memcmp(&read, &defaults, sizeof read));
    cJSON_Delete(parsed);
    /* A file from before the video buffer existed reads as Medium; each saved choice reads back. */
    for (int buffer = TUBE_BUFFER_LOW; buffer <= TUBE_BUFFER_HIGHEST; ++buffer) {
        char saved[32]; snprintf(saved, sizeof saved, "{\"video_buffer\":%d}", buffer);
        parsed = cJSON_Parse(saved); read = tube_player_extra_read(parsed); cJSON_Delete(parsed);
        assert(read.video_buffer == buffer);
    }
    parsed = cJSON_Parse("{\"volume\":150}"); read = tube_player_extra_read(parsed); cJSON_Delete(parsed);
    assert(read.video_buffer == TUBE_BUFFER_MEDIUM && read.volume == 150);

    TubePlayerExtra broken = chosen;
    broken.volume = 301; broken.keys_lr = -1;
    assert(tube_player_extra_validate(&broken) == 2 && broken.volume == 100 && broken.keys_lr == 0 && broken.subtitle_style == 5);
    assert(!tube_player_extra_validate(&broken));
    /* Writing validates as well. */
    broken.subtitle_scale = 7;
    object = cJSON_CreateObject();
    assert(tube_player_extra_write(object, &broken));
    assert(cJSON_GetObjectItem(object, "subtitle_scale")->valueint == 100);
    cJSON_Delete(object);
}

static void test_setters(void) {
    TubePlayerExtra defaults;
    tube_player_extra_defaults(&defaults);
    assert(!memcmp(tube_player_extra_current(), &defaults, sizeof defaults));
    /* Without a hook a valid change is accepted in memory. */
    unsigned revision = tube_extra_revision();
    assert(tube_extra_set_volume(150) && tube_extra_volume() == 150 && tube_extra_revision() != revision);
    assert(!tube_extra_set_volume(152) && !tube_extra_set_volume(305) && !tube_extra_set_volume(-5) && tube_extra_volume() == 150);

    tube_player_extra_set_save_hook(save, &saves);
    tube_player_extra_select(NULL);
    assert(!saves && tube_extra_volume() == 100);
    assert(tube_extra_set_subtitle_style(6) && saves == 1 && last_saved.subtitle_style == 6);
    assert(tube_extra_set_subtitle_style(6) && saves == 1);          /* unchanged: nothing to save */
    assert(!tube_extra_set_subtitle_style(7) && saves == 1 && tube_extra_subtitle_style_index() == 6);
    assert(tube_extra_set_subtitle_scale(200) && !tube_extra_set_subtitle_scale(0) && tube_extra_subtitle_scale() == 200);
    assert(tube_extra_set_subtitle_position(0) && tube_extra_set_subtitle_position(975) && tube_extra_set_subtitle_position(-1));
    assert(!tube_extra_set_subtitle_position(10) && !tube_extra_set_subtitle_position(1025) && tube_extra_subtitle_position() == -1);
    assert(tube_extra_set_auto_volume(1) && !tube_extra_set_auto_volume(2) && tube_extra_auto_volume());
    assert(tube_extra_set_clock(1) && tube_extra_set_quality_info(1) && tube_extra_set_quality_bitrate(1) && tube_extra_set_clock_12h(1));
    assert(tube_extra_clock() && tube_extra_quality_info() && tube_extra_quality_bitrate() && tube_extra_clock_12h());
    assert(tube_extra_set_exit_mode(2) && !tube_extra_set_exit_mode(3) && tube_extra_exit_mode() == 2);
    assert(tube_extra_set_keys_lr(4) && !tube_extra_set_keys_lr(5) && tube_extra_set_keys_ud(6) && !tube_extra_set_keys_ud(7) && tube_extra_set_keys_ud(5));
    assert(tube_extra_keys_lr() == 4 && tube_extra_keys_ud() == 5);
    assert(tube_extra_set_seek_preview(3) && !tube_extra_set_seek_preview(4) && tube_extra_seek_preview() == 3);
    assert(tube_extra_set_speed_list(2) && !tube_extra_set_speed_list(3) && tube_extra_speed_list() == 2);
    assert(tube_extra_set_loop_shorts(1) && tube_extra_set_remember_short(0) && tube_extra_loop_shorts() && !tube_extra_remember_short());

    /* The delay value and its switch move together as in SmartTube. */
    int before = saves;
    assert(!tube_extra_set_audio_delay_enabled(1) && saves == before);         /* nothing to enable at 0 ms */
    assert(tube_extra_set_audio_delay_ms(-120) && saves == before + 1);
    assert(tube_extra_audio_delay_ms() == -120 && tube_extra_audio_delay_enabled() && tube_extra_effective_audio_delay_ms() == -120);
    assert(tube_extra_set_audio_delay_enabled(0) && tube_extra_audio_delay_ms() == -120 && !tube_extra_effective_audio_delay_ms());
    assert(tube_extra_set_audio_delay_enabled(1) && tube_extra_effective_audio_delay_ms() == -120);
    assert(!tube_extra_set_audio_delay_ms(2001) && !tube_extra_set_audio_delay_enabled(2));
    assert(tube_extra_set_audio_delay_ms(0) && !tube_extra_audio_delay_enabled());

    /* A failed save is reported, and selecting a profile replaces everything. */
    save_result = 0;
    assert(!tube_extra_set_volume(45) && tube_extra_volume() == 45);
    save_result = 1;
    TubePlayerExtra other = defaults;
    other.volume = 20; other.exit_mode = 9;
    before = saves; revision = tube_extra_revision();
    tube_player_extra_select(&other);
    assert(saves == before && tube_extra_revision() != revision);
    assert(tube_extra_volume() == 20 && tube_extra_exit_mode() == TUBE_EXIT_MODE_ASK && !tube_extra_loop_shorts());
    tube_player_extra_set_save_hook(NULL, NULL);
    tube_player_extra_select(NULL);
}

static void test_groups(void) {
    char label[96];
    int total = 0;
    tube_player_extra_select(NULL);
    for (int group = 0; group < TUBE_EXTRA_GROUPS; ++group) {
        assert(!strncmp(tube_extra_group_id(group), "settings:", 9) && tube_extra_group_title(group)[0] && tube_extra_group_detail(group)[0]);
        assert(tube_extra_group_find(tube_extra_group_id(group)) == group);
        const int count = tube_extra_group_count(group);
        assert(count >= 2);
        total += count;
        /* Every choice has a label, is accepted, and reads back as selected. */
        const int initial = tube_extra_group_selected(group);
        assert(initial >= 0);
        for (int choice = 0; choice < count; ++choice) {
            assert(tube_extra_group_label(group, choice, label, sizeof label) && label[0]);
            if (group == TUBE_EXTRA_AUDIO_DELAY_ENABLED && choice && !tube_extra_audio_delay_ms()) {
                assert(!tube_extra_group_select(group, choice));
                continue;
            }
            assert(tube_extra_group_select(group, choice) && tube_extra_group_selected(group) == choice);
        }
        assert(tube_extra_group_select(group, initial));
        assert(!tube_extra_group_select(group, count) && !tube_extra_group_select(group, -1));
        assert(!tube_extra_group_label(group, count, label, sizeof label) && !label[0]);
    }
    assert(total == 7 + 20 + 42 + 61 + 2 + 2 + 29 + 2 + 2 + 2 + 2 + 2 + 3 + 5 + 7 + 4 + 3 + 2 + 2 +
                    2 + 2 + 2 + 2 + 4 + 19 + 12 + 17 + 10 + 2 * TUBE_BUTTON_COUNT);
    /* Video buffer: SmartTube's four names, a row of the player screen and of the in-player options; a change is saved
     * and read by the next stream that opens. */
    static const char *const buffers[] = { "Low", "Medium", "High", "Highest" };
    assert(!strcmp(tube_extra_group_id(TUBE_EXTRA_VIDEO_BUFFER), "settings:video-buffer") && tube_extra_group_count(TUBE_EXTRA_VIDEO_BUFFER) == 4);
    assert(!strcmp(tube_extra_group_title(TUBE_EXTRA_VIDEO_BUFFER), "Video buffer") && strstr(tube_extra_group_detail(TUBE_EXTRA_VIDEO_BUFFER), "next video"));
    assert(tube_extra_group_screen(TUBE_EXTRA_VIDEO_BUFFER) == 2 && tube_extra_group_in_player(TUBE_EXTRA_VIDEO_BUFFER));
    assert(tube_extra_group_selected(TUBE_EXTRA_VIDEO_BUFFER) == TUBE_BUFFER_MEDIUM && tube_extra_buffer() == TUBE_BUFFER_MEDIUM);
    assert(tube_extra_read_ahead_seconds(30.0, 1) == 30.0 && tube_extra_read_ahead_seconds(10.0, 0) == 10.0);
    for (int choice = 0; choice < 4; ++choice) {
        assert(tube_extra_group_label(TUBE_EXTRA_VIDEO_BUFFER, choice, label, sizeof label) && !strcmp(label, buffers[choice]));
        assert(tube_extra_group_select(TUBE_EXTRA_VIDEO_BUFFER, choice) && tube_extra_buffer() == choice);
    }
    assert(tube_extra_read_ahead_seconds(30.0, 1) == 100.0 && tube_extra_read_ahead_seconds(10.0, 0) == 10.0);
    assert(tube_extra_set_buffer(TUBE_BUFFER_LOW) && tube_extra_read_ahead_seconds(30.0, 1) == 5.0 && tube_extra_read_ahead_seconds(10.0, 0) == 5.0);
    assert(!tube_extra_set_buffer(4) && !tube_extra_set_buffer(-1) && tube_extra_buffer() == TUBE_BUFFER_LOW);
    assert(tube_extra_set_buffer(TUBE_BUFFER_MEDIUM));
    assert(tube_extra_group_find("settings:nothing") == -1 && tube_extra_group_find(NULL) == -1);
    assert(!tube_extra_group_count(-1) && !tube_extra_group_count(TUBE_EXTRA_GROUPS) && tube_extra_group_selected(99) == -1);
    TubePlayerExtra defaults;
    tube_player_extra_defaults(&defaults);
    assert(!memcmp(tube_player_extra_current(), &defaults, sizeof defaults) && !tube_extra_muted());

    /* SmartTube's lists. */
    assert(tube_extra_group_label(TUBE_EXTRA_SUBTITLE_SIZE, 0, label, sizeof label) && !strcmp(label, "0.1x"));
    assert(tube_extra_group_label(TUBE_EXTRA_SUBTITLE_SIZE, 9, label, sizeof label) && !strcmp(label, "1.0x"));
    assert(tube_extra_group_label(TUBE_EXTRA_SUBTITLE_SIZE, 19, label, sizeof label) && !strcmp(label, "2.0x"));
    assert(tube_extra_group_selected(TUBE_EXTRA_SUBTITLE_SIZE) == 9);
    assert(tube_extra_group_label(TUBE_EXTRA_SUBTITLE_POSITION, 0, label, sizeof label) && !strcmp(label, "Default"));
    assert(tube_extra_group_label(TUBE_EXTRA_SUBTITLE_POSITION, 1, label, sizeof label) && !strcmp(label, "0%"));
    assert(tube_extra_group_label(TUBE_EXTRA_SUBTITLE_POSITION, 2, label, sizeof label) && !strcmp(label, "2.5%"));
    assert(tube_extra_group_label(TUBE_EXTRA_SUBTITLE_POSITION, 41, label, sizeof label) && !strcmp(label, "100%"));
    assert(tube_extra_group_select(TUBE_EXTRA_SUBTITLE_POSITION, 5) && tube_extra_subtitle_position() == 100);
    assert(tube_extra_group_label(TUBE_EXTRA_VOLUME, 0, label, sizeof label) && !strcmp(label, "0%"));
    assert(tube_extra_group_label(TUBE_EXTRA_VOLUME, 60, label, sizeof label) && !strcmp(label, "300%"));
    assert(tube_extra_group_selected(TUBE_EXTRA_VOLUME) == 20);
    assert(tube_extra_group_label(TUBE_EXTRA_AUDIO_DELAY, 14, label, sizeof label) && !strcmp(label, "No delay"));
    assert(tube_extra_group_label(TUBE_EXTRA_AUDIO_DELAY, 0, label, sizeof label) && !strcmp(label, "-2000 ms (audio earlier)"));
    assert(tube_extra_group_label(TUBE_EXTRA_AUDIO_DELAY, 28, label, sizeof label) && !strcmp(label, "+2000 ms (audio later)"));
    /* A stored delay outside the list has no selected row; disabled reads as none. */
    assert(tube_extra_set_audio_delay_ms(333) && tube_extra_group_selected(TUBE_EXTRA_AUDIO_DELAY) == -1);
    assert(tube_extra_set_audio_delay_enabled(0) && tube_extra_group_selected(TUBE_EXTRA_AUDIO_DELAY) == 14);
    assert(tube_extra_group_label(TUBE_EXTRA_LOOP_SHORTS, 1, label, sizeof label) && !strcmp(label, "On"));
    char tiny[4];
    assert(tube_extra_group_label(TUBE_EXTRA_SUBTITLE_STYLE, 6, tiny, sizeof tiny) && !strcmp(tiny, "Yel"));
    assert(!tube_extra_group_label(0, 0, NULL, 0));
    tube_player_extra_select(NULL);
}

static void test_captions(void) {
    tube_subtitle_style_t style = tube_extra_subtitle_style();
    assert(!style.custom);
    assert(!tube_subtitle_style_preset(-1).custom && !tube_subtitle_style_preset(7).custom);
    style = tube_subtitle_style_preset(1);
    assert(style.custom && style.text_rgb == 0xe4e4e4 && !style.background_alpha && style.edge == TUBE_SUBTITLE_EDGE_SHADOW);
    style = tube_subtitle_style_preset(2);
    assert(style.text_rgb == 0xe4e4e4 && style.background_alpha == 100 && style.edge == TUBE_SUBTITLE_EDGE_OUTLINE);
    style = tube_subtitle_style_preset(3);
    assert(style.background_alpha == 255 && !style.background_rgb);
    style = tube_subtitle_style_preset(4);
    assert(style.text_rgb == 0xffff00 && !style.background_alpha && style.edge == TUBE_SUBTITLE_EDGE_SHADOW);
    assert(tube_subtitle_style_preset(5).background_alpha == 100 && tube_subtitle_style_preset(6).background_alpha == 255);
    assert(tube_extra_set_subtitle_style(5) && tube_extra_subtitle_style().text_rgb == 0xffff00);

    /* 10 % from the bottom of a 1080-line screen; the block never leaves it. */
    assert(tube_subtitle_baseline(100, 1080, 125, 62.5) == 972);
    assert(tube_subtitle_baseline(500, 1080, 125, 62.5) == 540);
    assert(tube_subtitle_baseline(0, 1080, 125, 62.5) == 1080 - 62.5 * 0.3);
    assert(tube_subtitle_baseline(1000, 1080, 125, 62.5) == 125);
    assert(tube_subtitle_baseline(2000, 1080, 250, 62.5) == 250 && tube_subtitle_baseline(-5, 1080, 125, 62.5) == 1080 - 62.5 * 0.3);
    assert(tube_subtitle_baseline(1000, 1080, 5000, 62.5) == 1080 - 62.5 * 0.3);   /* taller than the screen */
    tube_player_extra_select(NULL);
}

static void test_volume(void) {
    TubePlayerExtra prefs;
    tube_player_extra_defaults(&prefs);
    assert(tube_extra_gain_for(&prefs, 0, 0, 0) == 1000 && tube_extra_gain_for(&prefs, 1, 0, 0) == 0 && tube_extra_gain_for(NULL, 0, 0, 0) == 0);
    assert(tube_extra_gain_for(&prefs, 0, 1, 12) == 1000);                  /* auto volume off */
    prefs.volume = 300; assert(tube_extra_gain_for(&prefs, 0, 0, 0) == 3000);
    prefs.volume = 0; assert(tube_extra_gain_for(&prefs, 0, 0, 0) == 0);
    prefs.volume = 100; prefs.auto_volume = 1;
    assert(tube_extra_gain_for(&prefs, 0, 0, 12) == 1000);                  /* no loudness known */
    assert(tube_extra_gain_for(&prefs, 0, 1, 6.0206) == 500 && tube_extra_gain_for(&prefs, 0, 1, -9) == 1000);
    prefs.volume = 50; assert(tube_extra_gain_for(&prefs, 0, 1, 6.0206) == 250);
    prefs.volume = 200; assert(tube_extra_gain_for(&prefs, 0, 1, 6.0206) == 1000 && tube_extra_gain_for(&prefs, 0, 1, -3) == 2000);

    assert(tube_extra_gain_milli() == 1000);
    tube_extra_set_muted(1);
    assert(tube_extra_muted() && tube_extra_gain_milli() == 0 && tube_extra_group_selected(TUBE_EXTRA_MUTE) == 1);
    tube_extra_set_muted(0);
    assert(tube_extra_set_auto_volume(1));
    tube_extra_set_loudness(1, 20);
    assert(tube_extra_gain_milli() == 100);
    tube_extra_set_loudness(1, NAN); assert(tube_extra_gain_milli() == 1000);
    tube_extra_set_loudness(1, 61); assert(tube_extra_gain_milli() == 1000);  /* outside the contract: ignored */
    tube_extra_set_loudness(0, 20); assert(tube_extra_gain_milli() == 1000);

    assert(tube_extra_volume_step(1) == 105 && tube_extra_volume_step(-1) == 100 && tube_extra_volume_step(0) == 100);
    assert(tube_extra_set_volume(300) && tube_extra_volume_step(1) == 300);
    assert(tube_extra_set_volume(0) && tube_extra_volume_step(-1) == 0);
    tube_player_extra_select(NULL);
}

static void test_labels(void) {
    char text[96];
    setenv("TZ", "UTC", 1); tzset();
    const time_t afternoon = 1704067200 + 14 * 3600 + 5 * 60;   /* 14:05 UTC */
    tube_extra_video_end();
    assert(!tube_extra_controls_info(text, sizeof text, afternoon) && !text[0]);
    assert(tube_extra_set_clock(1));
    assert(tube_extra_controls_info(text, sizeof text, afternoon) && !strcmp(text, "14:05"));
    assert(tube_extra_set_clock_12h(1) && tube_extra_time_flags() == TUBE_TIME_12H);
    assert(tube_extra_controls_info(text, sizeof text, afternoon) && !strcmp(text, "2:05 PM"));
    assert(!tube_extra_controls_info(text, sizeof text, -1) && !text[0]);

    /* The quality label needs a known stream. */
    assert(tube_extra_set_quality_info(1));
    assert(tube_extra_controls_info(text, sizeof text, afternoon) && !strcmp(text, "2:05 PM"));
    tube_extra_set_stream_info(1080, 59.94, "VP9", 2449000);
    assert(tube_extra_controls_info(text, sizeof text, afternoon) && !strcmp(text, "1080p60 VP9   2:05 PM"));
    assert(tube_extra_set_quality_bitrate(1));
    assert(tube_extra_controls_info(text, sizeof text, afternoon) && !strcmp(text, "1080p60 VP9 2.4 Mbps   2:05 PM"));
    tube_extra_set_muted(1);
    assert(tube_extra_set_clock(0) && tube_extra_controls_info(text, sizeof text, afternoon) && !strcmp(text, "MUTED   1080p60 VP9 2.4 Mbps"));
    tube_extra_set_muted(0);
    unsigned revision = tube_extra_revision();
    tube_extra_set_stream_info(1080, 59.94, "VP9", 2449000);
    assert(tube_extra_revision() == revision);                    /* unchanged facts do not wake the HUD */
    tube_extra_video_end();
    assert(!tube_extra_controls_info(text, sizeof text, afternoon));

    assert(tube_extra_quality_text(text, sizeof text, 720, 30, "AVC", 0, 1) && !strcmp(text, "720p AVC"));
    assert(tube_extra_quality_text(text, sizeof text, 2160, 50, "AV1", 812345, 1) && !strcmp(text, "2160p50 AV1 812 kbps"));
    assert(tube_extra_quality_text(text, sizeof text, 480, NAN, NULL, 900000, 0) && !strcmp(text, "480p"));
    assert(!tube_extra_quality_text(text, sizeof text, 0, 30, "AVC", 1, 1) && !text[0]);
    char tiny[6];
    assert(tube_extra_quality_text(tiny, sizeof tiny, 1080, 60, "VP9", 1, 1) && !strcmp(tiny, "1080p"));

    /* The same 12-hour preference reaches the existing finish-time label. */
    tube_time_display(text, sizeof text, 2 | tube_extra_time_flags(), 40, 100, 1, 0, afternoon);
    assert(!strcmp(text, "Ends 2:06 PM / 01:40"));
    tube_time_display(text, sizeof text, 2, 40, 100, 1, 0, afternoon);
    assert(!strcmp(text, "Ends 14:06 / 01:40"));
    tube_time_display(text, sizeof text, 1 | TUBE_TIME_12H, 40, 100, 1, 0, afternoon);
    assert(!strcmp(text, "-01:00 / 01:40"));
    tube_clock_parts(text, sizeof text, 0, 7, 1); assert(!strcmp(text, "12:07 AM"));
    tube_clock_parts(text, sizeof text, 12, 0, 1); assert(!strcmp(text, "12:00 PM"));
    tube_clock_parts(text, sizeof text, 23, 59, 1); assert(!strcmp(text, "11:59 PM"));
    tube_clock_parts(text, sizeof text, 0, 7, 0); assert(!strcmp(text, "00:07"));
    tube_player_extra_select(NULL);
}

static void test_exit_and_keys(void) {
    uint64_t armed = 0;
    assert(tube_extra_exit_decide(TUBE_EXIT_MODE_ASK, 1000, &armed) == TUBE_EXIT_ASK && !armed);
    assert(tube_extra_exit_decide(TUBE_EXIT_MODE_SINGLE, 1000, &armed) == TUBE_EXIT_NOW);
    assert(tube_extra_exit_decide(TUBE_EXIT_MODE_DOUBLE, 1000, &armed) == TUBE_EXIT_ARMED && armed == 1000);
    assert(tube_extra_exit_decide(TUBE_EXIT_MODE_DOUBLE, 1000 + TUBE_EXIT_DOUBLE_MS, &armed) == TUBE_EXIT_NOW && !armed);
    assert(tube_extra_exit_decide(TUBE_EXIT_MODE_DOUBLE, 5000, &armed) == TUBE_EXIT_ARMED);
    assert(tube_extra_exit_decide(TUBE_EXIT_MODE_DOUBLE, 5001 + TUBE_EXIT_DOUBLE_MS, &armed) == TUBE_EXIT_ARMED);   /* too late: armed again */
    assert(tube_extra_exit_decide(TUBE_EXIT_MODE_DOUBLE, 100, &armed) == TUBE_EXIT_ARMED);                          /* clock went back */
    assert(tube_extra_exit_decide(TUBE_EXIT_MODE_DOUBLE, 0, &armed) == TUBE_EXIT_ARMED && armed == 1);
    assert(tube_extra_exit_decide(TUBE_EXIT_MODE_DOUBLE, 1000, NULL) == TUBE_EXIT_ASK);
    assert(tube_extra_exit_decide(77, 1000, &armed) == TUBE_EXIT_ASK && !armed);

    assert(tube_extra_exit_press(1000) == TUBE_EXIT_ASK);
    assert(tube_extra_set_exit_mode(TUBE_EXIT_MODE_DOUBLE));
    assert(tube_extra_exit_press(1000) == TUBE_EXIT_ARMED && tube_extra_exit_press(1500) == TUBE_EXIT_NOW);
    assert(tube_extra_exit_press(2000) == TUBE_EXIT_ARMED);
    tube_extra_exit_reset();                                    /* entering the player forgets an old press */
    assert(tube_extra_exit_press(2100) == TUBE_EXIT_ARMED);
    assert(tube_extra_set_exit_mode(TUBE_EXIT_MODE_SINGLE) && tube_extra_exit_press(1) == TUBE_EXIT_NOW);

    for (int hidden = 0; hidden < 2; ++hidden) for (int is_short = 0; is_short < 2; ++is_short) {
        assert(tube_extra_key_action(TUBE_KEYS_DEFAULT, is_short, hidden) == TUBE_KEY_ACTION_DEFAULT);
        assert(tube_extra_key_action(TUBE_KEYS_SKIP_SHORTS, is_short, hidden) == (hidden && is_short ? TUBE_KEY_ACTION_SKIP : TUBE_KEY_ACTION_DEFAULT));
        assert(tube_extra_key_action(TUBE_KEYS_SKIP_VIDEOS, is_short, hidden) == (hidden && !is_short ? TUBE_KEY_ACTION_SKIP : TUBE_KEY_ACTION_DEFAULT));
        assert(tube_extra_key_action(TUBE_KEYS_SKIP_ALL, is_short, hidden) == (hidden ? TUBE_KEY_ACTION_SKIP : TUBE_KEY_ACTION_DEFAULT));
        assert(tube_extra_key_action(TUBE_KEYS_VOLUME, is_short, hidden) == TUBE_KEY_ACTION_VOLUME);
        assert(tube_extra_key_action(TUBE_KEYS_SPEED, is_short, hidden) == TUBE_KEY_ACTION_SPEED);
        assert(tube_extra_key_action(42, is_short, hidden) == TUBE_KEY_ACTION_DEFAULT);
    }
    tube_player_extra_select(NULL);
    assert(tube_extra_dpad_action(0, 1) == TUBE_KEY_ACTION_DEFAULT && tube_extra_dpad_action(1, 1) == TUBE_KEY_ACTION_DEFAULT);
    assert(tube_extra_set_keys_lr(TUBE_KEYS_SKIP_SHORTS) && tube_extra_set_keys_ud(TUBE_KEYS_SKIP_VIDEOS));
    assert(tube_extra_dpad_action(0, 1) == TUBE_KEY_ACTION_DEFAULT && tube_extra_dpad_action(1, 1) == TUBE_KEY_ACTION_SKIP);
    tube_extra_set_short(1);
    assert(tube_extra_dpad_action(0, 1) == TUBE_KEY_ACTION_SKIP && tube_extra_dpad_action(1, 1) == TUBE_KEY_ACTION_DEFAULT);
    assert(tube_extra_dpad_action(0, 0) == TUBE_KEY_ACTION_DEFAULT);
    tube_extra_video_end();
    tube_player_extra_select(NULL);
}

static void test_speeds_and_shorts(void) {
    /* SmartTube's short and long lists; the default equals today's 30 speeds. */
    static const int long_list[] = {25,50,75,80,85,90,95,100,105,110,115,120,125,130,135,140,145,150,160,175,180,200,225,250,275,300,325,350,375,400};
    assert(tube_extra_speed_count() == 30);
    for (int i = 0; i < 30; ++i) assert(tube_extra_speed_at(i) == long_list[i]);
    assert(!tube_extra_speed_at(30) && !tube_extra_speed_at(-1));
    assert(tube_speed_list_count(0) == 16 && tube_speed_list_at(0, 0) == 25 && tube_speed_list_at(0, 3) == 100 && tube_speed_list_at(0, 15) == 400);
    assert(tube_speed_list_count(2) == 76 && tube_speed_list_at(2, 0) == 25 && tube_speed_list_at(2, 26) == 155 && tube_speed_list_at(2, 75) == 400);
    assert(tube_speed_list_count(9) == 30);
    for (int list = 0; list < 3; ++list) {
        int has_normal = 0;
        for (int i = 0; i < tube_speed_list_count(list); ++i) {
            const int speed = tube_speed_list_at(list, i);
            assert(speed >= 25 && speed <= 400 && (!i || speed > tube_speed_list_at(list, i - 1)));
            has_normal |= speed == 100;
        }
        assert(has_normal);
    }
    assert(tube_extra_speed_valid(160) && !tube_extra_speed_valid(155) && !tube_extra_speed_valid(0));
    assert(tube_extra_speed_step(100, 1) == 105 && tube_extra_speed_step(100, -1) == 95 && tube_extra_speed_step(400, 1) == 400);
    assert(tube_extra_speed_step(25, -1) == 25 && tube_extra_speed_step(152, 1) == 160 && tube_extra_speed_step(152, -1) == 150);
    assert(tube_extra_speed_step(100, 0) == 100);
    assert(tube_extra_set_speed_list(0));
    assert(tube_extra_speed_count() == 16 && !tube_extra_speed_valid(105) && tube_extra_speed_step(100, 1) == 125 && tube_extra_speed_step(105, -1) == 100);
    assert(tube_extra_set_speed_list(2));
    assert(tube_extra_speed_valid(155) && tube_extra_speed_step(155, 1) == 160);

    assert(!tube_extra_should_loop(0, 1) && !tube_extra_should_loop(1, 0) && tube_extra_should_loop(1, 1));
    assert(tube_extra_resume_decision(1, 30) && tube_extra_resume_decision(1, 5000));
    assert(!tube_extra_resume_decision(0, 30) && !tube_extra_resume_decision(0, 359.9) && tube_extra_resume_decision(0, 360));
    assert(tube_extra_resume_decision(0, 0) && tube_extra_resume_decision(0, NAN) && tube_extra_resume_decision(0, -1));
    tube_player_extra_select(NULL);
    assert(!tube_extra_loop_current() && tube_extra_resume_allowed(30));
    tube_extra_set_short(1);
    assert(!tube_extra_loop_current() && tube_extra_set_loop_shorts(1) && tube_extra_loop_current());
    tube_extra_set_short(0);
    assert(!tube_extra_loop_current() && tube_extra_set_remember_short(0) && !tube_extra_resume_allowed(30) && tube_extra_resume_allowed(900));
    tube_player_extra_select(NULL);
}

static void test_video(void) {
    char text[512];
    cJSON *result = cJSON_Parse("{\"channel\":\"Fixture channel\",\"channel_id\":\"UCffffffffffffffffffffff\","
        "\"views\":\"12345\",\"published\":\"2024-01-01\",\"loudness_db\":6.0206,\"is_short\":true,"
        "\"chapters\":[{\"title\":\"Second & last\",\"start\":95.5,\"end\":200},{\"title\":\"Intro\",\"start\":0,\"end\":95.5},"
        "{\"title\":7,\"start\":3725},{\"start\":\"x\"},{\"title\":\"bad\",\"start\":-1},7,{\"title\":\"A\\u0001B\\u00e9\",\"start\":200}],"
        "\"storyboards\":[{\"width\":48,\"height\":27,\"columns\":10,\"rows\":10,\"count\":100,\"sheets\":1,\"interval_ms\":0,\"url\":\"http://h/media/s/sb0-$M.jpg\"},"
        "{\"width\":160,\"height\":90,\"columns\":5,\"rows\":5,\"count\":120,\"sheets\":5,\"interval_ms\":1000,\"url\":\"http://h/media/s/sb1-$M.jpg\"},"
        "{\"width\":160,\"height\":90,\"columns\":5,\"rows\":5,\"count\":120,\"sheets\":4,\"interval_ms\":1000,\"url\":\"http://h/media/s/sb2-$M.jpg\"},"
        "{\"width\":160.5,\"height\":90,\"columns\":5,\"rows\":5,\"count\":120,\"sheets\":5,\"interval_ms\":1000,\"url\":\"http://h/media/s/sb3-$M.jpg\"},"
        "{\"width\":320,\"height\":180,\"columns\":3,\"rows\":3,\"count\":120,\"sheets\":14,\"interval_ms\":1000,\"url\":7},\"x\"]}");
    assert(result);
    assert(tube_extra_set_auto_volume(1));
    tube_extra_video_begin(result);
    cJSON_Delete(result);                                       /* nothing keeps pointers into the result */
    assert(tube_extra_is_short() && tube_extra_gain_milli() == 500);
    assert(!strcmp(tube_playing_fact("channel"), "Fixture channel"));
    assert(!strcmp(tube_playing_fact("channel_id"), "UCffffffffffffffffffffff"));
    assert(!strcmp(tube_playing_fact("views"), "12345"));
    assert(!strcmp(tube_playing_fact("published"), "2024-01-01"));
    assert(!*tube_playing_fact("channel_art_url") && !*tube_playing_fact("unknown") && !*tube_playing_fact(NULL));

    /* Chapters arrive sorted with bounded, cleaned titles. */
    assert(tube_extra_chapter_count() == 4);
    assert(tube_extra_chapter_start(0) == 0 && !strcmp(tube_extra_chapter_title(0), "Intro"));
    assert(tube_extra_chapter_start(1) == 95.5 && !strcmp(tube_extra_chapter_title(1), "Second & last"));
    assert(tube_extra_chapter_start(2) == 200 && !strcmp(tube_extra_chapter_title(2), "A B\xc3\xa9"));
    assert(tube_extra_chapter_start(3) == 3725 && !tube_extra_chapter_title(3)[0]);
    assert(tube_extra_chapter_start(4) == -1 && !tube_extra_chapter_title(4)[0] && !tube_extra_chapter_title(-1)[0]);
    assert(tube_extra_chapter_at(0) == 0 && tube_extra_chapter_at(95.4) == 0 && tube_extra_chapter_at(95.5) == 1);
    assert(tube_extra_chapter_at(1e9) == 3 && tube_extra_chapter_at(-1) == -1 && tube_extra_chapter_at(NAN) == -1);
    assert(!strcmp(tube_extra_chapter_title_at(120), "Second & last") && !tube_extra_chapter_title_at(-5)[0]);
    assert(tube_extra_chapter_label(1, text, sizeof text) && !strcmp(text, "01:35  Second & last"));
    assert(tube_extra_chapter_label(3, text, sizeof text) && !strcmp(text, "1:02:05  Chapter 4"));
    assert(!tube_extra_chapter_label(4, text, sizeof text) && !text[0]);

    /* Only storyboard levels that satisfy the contract are kept. */
    assert(tube_extra_storyboard_count() == 2);
    assert(tube_extra_storyboard(0)->width == 48 && tube_extra_storyboard(1)->sheets == 5 && !tube_extra_storyboard(2));
    tube_storyboard_tile tile;
    assert(!tube_extra_seek_preview_tile(57, 120, 160, 90, &tile, text, sizeof text) && !text[0]);   /* preference off */
    assert(tube_extra_set_seek_preview(1));
    assert(tube_extra_seek_preview_tile(57, 120, 160, 90, &tile, text, sizeof text));
    assert(tile.index == 57 && tile.sheet == 2 && tile.x == 320 && tile.y == 90 && !strcmp(text, "http://h/media/s/sb1-2.jpg"));
    assert(tube_extra_seek_preview_tile(60, 120, 40, 20, &tile, text, sizeof text) && tile.index == 50 && !strcmp(text, "http://h/media/s/sb0-0.jpg"));
    assert(tube_extra_seek_preview_tile(60, 120, 40, 20, &tile, NULL, 0));
    assert(!tube_extra_seek_preview_tile(60, 120, 40, 20, NULL, text, sizeof text));
    char tiny[8];
    assert(!tube_extra_seek_preview_tile(60, 120, 40, 20, &tile, tiny, sizeof tiny));

    /* The next video starts without the previous one's facts. */
    tube_extra_video_begin(NULL);
    assert(!tube_extra_is_short() && tube_extra_gain_milli() == 1000 && !tube_extra_chapter_count() && !tube_extra_storyboard_count());
    assert(!*tube_playing_fact("channel") && !*tube_playing_fact("views"));
    assert(!tube_extra_seek_preview_tile(57, 120, 160, 90, &tile, text, sizeof text));
    result = cJSON_Parse("{\"loudness_db\":null,\"is_short\":1,\"channel_id\":\"bad\",\"views\":\"1.2M\",\"chapters\":{},\"storyboards\":null}");
    tube_extra_video_begin(result);
    cJSON_Delete(result);
    assert(!tube_extra_is_short() && tube_extra_gain_milli() == 1000 && !tube_extra_chapter_count());
    assert(!*tube_playing_fact("channel_id") && !*tube_playing_fact("views"));

    /* The provider can also hand over the chapter starts it already keeps. */
    const double starts[] = { 30, 0, NAN, 10 };
    const char *titles[] = { "C", "A", "bad", "B" };
    assert(tube_extra_set_chapter_list(starts, titles, 4) == 3 && !strcmp(tube_extra_chapter_title(1), "B"));
    assert(tube_extra_set_chapter_list(starts, NULL, 2) == 2 && !tube_extra_chapter_title(0)[0]);
    assert(tube_extra_set_chapter_list(NULL, NULL, 4) == 0);

    /* More rows than the bound and over-long titles stay inside it. */
    cJSON *many = cJSON_CreateArray();
    char title[1200];
    memset(title, 'x', sizeof title - 1); title[sizeof title - 1] = 0;
    memcpy(title + 382, "\xe2\x82\xac", 3);                     /* a three-byte character across the limit */
    for (int i = 0; i < 200; ++i) {
        cJSON *row = cJSON_CreateObject();
        cJSON_AddNumberToObject(row, "start", 200 - i);
        cJSON_AddStringToObject(row, "title", title);
        cJSON_AddItemToArray(many, row);
    }
    assert(tube_extra_set_chapters(many) == TUBE_EXTRA_CHAPTERS);
    cJSON_Delete(many);
    assert(strlen(tube_extra_chapter_title(0)) == 382 && tube_extra_chapter_start(0) < tube_extra_chapter_start(127));
    tube_extra_video_end();
    tube_player_extra_select(NULL);
}

static void test_buttons(void) {
    char label[96];
    tube_player_extra_select(NULL);
    /* SmartTube hides the queue, the share link and SponsorBlock until asked for. */
    for (int button = 0; button < TUBE_BUTTON_COUNT; ++button)
        assert(tube_extra_button(button) == (button != TUBE_BUTTON_QUEUE && button != TUBE_BUTTON_SHARE && button != TUBE_BUTTON_CONTENT_BLOCK));
    assert(!tube_extra_button(-1) && !tube_extra_button(TUBE_BUTTON_COUNT));
    assert(!tube_extra_set_buttons(-1) && !tube_extra_set_buttons(1 << TUBE_BUTTON_COUNT) && tube_extra_set_buttons(0));
    assert(!tube_extra_button(TUBE_BUTTON_LIKE) && tube_extra_set_buttons(TUBE_BUTTONS_DEFAULT));
    /* One switch per button, each moving its own bit only. */
    for (int button = 0; button < TUBE_BUTTON_COUNT; ++button) {
        const int group = TUBE_EXTRA_BUTTON_FIRST + button, mask = tube_extra_buttons();
        assert(tube_extra_group_count(group) == 2 && !strncmp(tube_extra_group_title(group), "Button: ", 8));
        assert(tube_extra_group_selected(group) == tube_extra_button(button));
        assert(tube_extra_group_select(group, !tube_extra_button(button)) && tube_extra_buttons() == (mask ^ 1 << button));
        assert(tube_extra_group_select(group, mask >> button & 1) && tube_extra_buttons() == mask);
        assert(!tube_extra_group_in_player(group) && tube_extra_group_screen(group) == 3);
    }
    assert(!strcmp(tube_extra_group_id(TUBE_EXTRA_BUTTON_FIRST + TUBE_BUTTON_FLIP), "settings:button-mirror"));
    assert(tube_extra_group_label(TUBE_EXTRA_BUTTON_FIRST, 1, label, sizeof label) && !strcmp(label, "On"));
    /* A switched-off button also hides the preferences behind it. */
    assert(tube_extra_group_in_player(TUBE_EXTRA_VOLUME) && tube_extra_group_in_player(TUBE_EXTRA_MUTE));
    assert(tube_extra_group_in_player(TUBE_EXTRA_DIM_TIMEOUT) && tube_extra_group_in_player(TUBE_EXTRA_SCREEN_OFF_AMOUNT));
    assert(tube_extra_set_buttons(TUBE_BUTTONS_DEFAULT & ~(1 << TUBE_BUTTON_SOUND) & ~(1 << TUBE_BUTTON_SCREEN_OFF)));
    assert(!tube_extra_group_in_player(TUBE_EXTRA_VOLUME) && !tube_extra_group_in_player(TUBE_EXTRA_MUTE));
    assert(!tube_extra_group_in_player(TUBE_EXTRA_DIM_AMOUNT) && !tube_extra_group_in_player(TUBE_EXTRA_SCREEN_OFF_TIMEOUT));
    assert(tube_extra_group_in_player(TUBE_EXTRA_AUTO_VOLUME) && tube_extra_group_in_player(TUBE_EXTRA_CHAPTER_NOTICE));
    assert(!tube_extra_group_in_player(-1) && tube_extra_group_screen(TUBE_EXTRA_GROUPS) == -1);
    assert(tube_extra_group_screen(TUBE_EXTRA_SUBTITLE_SIZE) == 0 && tube_extra_group_screen(TUBE_EXTRA_AUDIO_DELAY) == 1);
    assert(tube_extra_group_screen(TUBE_EXTRA_ERROR_FIXER) == 2 && tube_extra_group_screen(TUBE_EXTRA_DIM_AMOUNT) == 4);
    /* SmartTube's dimming lists. */
    assert(tube_extra_group_label(TUBE_EXTRA_DIM_TIMEOUT, 0, label, sizeof label) && !strcmp(label, "Never"));
    assert(tube_extra_group_label(TUBE_EXTRA_DIM_TIMEOUT, 1, label, sizeof label) && !strcmp(label, "5 sec"));
    assert(tube_extra_group_label(TUBE_EXTRA_DIM_TIMEOUT, 18, label, sizeof label) && !strcmp(label, "15 min"));
    assert(tube_extra_group_label(TUBE_EXTRA_DIM_AMOUNT, 8, label, sizeof label) && !strcmp(label, "85%"));
    assert(tube_extra_group_selected(TUBE_EXTRA_DIM_AMOUNT) == 7 && !tube_extra_group_selected(TUBE_EXTRA_DIM_TIMEOUT));
    assert(tube_extra_group_label(TUBE_EXTRA_SCREEN_OFF_TIMEOUT, 10, label, sizeof label) && !strcmp(label, "10 sec"));
    assert(tube_extra_group_label(TUBE_EXTRA_SCREEN_OFF_TIMEOUT, 16, label, sizeof label) && !strcmp(label, "180 min"));
    assert(tube_extra_group_select(TUBE_EXTRA_SCREEN_OFF_TIMEOUT, 11) && tube_extra_screen_off_timeout() == 1800);
    assert(tube_extra_set_dim_timeout(10) && tube_extra_group_selected(TUBE_EXTRA_DIM_TIMEOUT) == -1);   /* valid, not listed */
    assert(!tube_extra_set_dim_timeout(7) && !tube_extra_set_dim_amount(5) && !tube_extra_set_screen_off_amount(95));
    /* Down lists suggestions only with visible controls and the switch on. */
    assert(tube_extra_key_action(TUBE_KEYS_SUGGESTIONS, 0, 0) == TUBE_KEY_ACTION_SUGGESTIONS);
    assert(tube_extra_key_action(TUBE_KEYS_SUGGESTIONS, 0, 1) == TUBE_KEY_ACTION_DEFAULT);
    assert(tube_extra_set_keys_ud(TUBE_KEYS_SUGGESTIONS) && tube_extra_dpad_action(1, 0) == TUBE_KEY_ACTION_SUGGESTIONS);
    assert(tube_extra_dpad_action(0, 0) == TUBE_KEY_ACTION_DEFAULT && tube_extra_set_suggestions(0));
    assert(tube_extra_dpad_action(1, 0) == TUBE_KEY_ACTION_DEFAULT);
    /* The request for the opening list is taken once. */
    assert(tube_extra_take_picker_request() == TUBE_PICKER_OPTIONS);
    tube_extra_picker_request(TUBE_PICKER_SUGGESTIONS);
    assert(tube_extra_take_picker_request() == TUBE_PICKER_SUGGESTIONS && tube_extra_take_picker_request() == TUBE_PICKER_OPTIONS);
    tube_player_extra_select(NULL);
}

static void test_chapter_notice(void) {
    assert(tube_chapter_notice_decide(0, -1, 2, 0, 0) == -1);                 /* switched off */
    assert(tube_chapter_notice_decide(1, -1, -1, 0, 0) == -1);                /* before the first chapter */
    assert(tube_chapter_notice_decide(1, -1, 0, 0, 0) == 0 && tube_chapter_notice_decide(1, 0, 0, 0, 0) == -1);
    assert(tube_chapter_notice_decide(1, 0, 1, 1, 0) == -1 && tube_chapter_notice_decide(1, 0, 1, 0, 1) == -1);
    assert(tube_chapter_notice_decide(1, 0, 1, 0, 0) == 1 && tube_chapter_notice_decide(1, 3, 1, 0, 0) == 1);
    const double starts[] = {0, 60, 600};
    const char *titles[] = {"Intro", "Middle", "End"};
    tube_player_extra_select(NULL);
    assert(tube_extra_set_chapter_list(starts, titles, 3) == 3);
    assert(tube_extra_chapter_notice(10, 0, 0) == -1);                          /* default: off */
    assert(tube_extra_set_chapter_notice(1) && tube_extra_chapter_notice_enabled());
    assert(tube_extra_chapter_notice(10, 0, 1) == -1 && tube_extra_chapter_notice(10, 1, 0) == -1);
    assert(tube_extra_chapter_notice(10, 0, 0) == 0 && tube_extra_chapter_notice(30, 0, 0) == -1);   /* once per chapter */
    assert(tube_extra_chapter_notice(61, 0, 0) == 1 && tube_extra_chapter_notice(700, 0, 0) == 2);
    assert(tube_extra_chapter_notice(20, 0, 0) == 0);                           /* a seek back announces again */
    tube_extra_video_end();
    assert(tube_extra_set_chapter_list(starts, titles, 3) == 3 && tube_extra_chapter_notice(20, 0, 0) == 0);
    tube_extra_video_end();
    assert(tube_extra_chapter_notice(20, 0, 0) == -1);                          /* no chapters */
    tube_player_extra_select(NULL);
}

static void test_dimming(void) {
    TubePlayerExtra prefs;
    tube_player_extra_defaults(&prefs);
    /* Defaults never darken the picture by themselves. */
    for (int context = TUBE_DIM_NONE; context <= TUBE_DIM_PLAYING; ++context)
        assert(!tube_dim_decide(&prefs, context, 100000000, 0));
    assert(tube_dim_decide(&prefs, TUBE_DIM_PLAYING, 0, 1) == 100 && tube_dim_decide(&prefs, TUBE_DIM_PAUSED, 0, 1) == 100);
    assert(!tube_dim_decide(&prefs, TUBE_DIM_BROWSING, 0, 1) && !tube_dim_decide(&prefs, TUBE_DIM_NONE, 0, 1));
    assert(!tube_dim_decide(NULL, TUBE_DIM_PAUSED, 0, 1));
    prefs.dim_timeout = 60; prefs.dim_amount = 80;
    assert(!tube_dim_decide(&prefs, TUBE_DIM_BROWSING, 59999, 0) && tube_dim_decide(&prefs, TUBE_DIM_BROWSING, 60000, 0) == 80);
    assert(tube_dim_decide(&prefs, TUBE_DIM_PAUSED, 60000, 0) == 80 && !tube_dim_decide(&prefs, TUBE_DIM_PLAYING, 60000, 0));
    assert(!tube_dim_decide(&prefs, TUBE_DIM_NONE, 60000, 0));
    prefs.screen_off_timeout = 5; prefs.screen_off_amount = 50;
    assert(tube_dim_decide(&prefs, TUBE_DIM_PLAYING, 5000, 0) == 50 && !tube_dim_decide(&prefs, TUBE_DIM_PLAYING, 4999, 0));
    assert(tube_dim_decide(&prefs, TUBE_DIM_PAUSED, 5000, 0) == 50 && tube_dim_decide(&prefs, TUBE_DIM_PAUSED, 60000, 0) == 80);
    assert(!tube_dim_decide(&prefs, TUBE_DIM_BROWSING, 5000, 0));

    tube_player_extra_select(&prefs);
    tube_extra_dim_reset(1000);
    unsigned revision = tube_extra_revision();
    assert(!tube_extra_dim_tick(1000, TUBE_DIM_PLAYING) && !tube_extra_dim_tick(5999, TUBE_DIM_PLAYING) && !tube_extra_dim_percent());
    assert(tube_extra_revision() == revision);
    assert(tube_extra_dim_tick(6000, TUBE_DIM_PLAYING) == 50 && tube_extra_dim_percent() == 50 && tube_extra_revision() != revision);
    assert(!tube_extra_dim_input(6100, 0, 0) && tube_extra_dim_percent() == 50);       /* no press */
    /* The waking press is swallowed; the next one reaches the player. */
    assert(tube_extra_dim_input(6200, 1, 1) == 1 && !tube_extra_dim_percent() && !tube_extra_dim_tick(6200, TUBE_DIM_PLAYING));
    assert(!tube_extra_dim_input(6300, 1, 0));
    /* Screen off on request: at once, kept through blind seeking, ended by another button. */
    assert(tube_extra_set_screen_off_amount(100));
    tube_extra_screen_off();
    assert(tube_extra_screen_is_off() && tube_extra_dim_tick(7000, TUBE_DIM_PLAYING) == 100);
    assert(!tube_extra_dim_input(7100, 1, 1) && tube_extra_dim_tick(7100, TUBE_DIM_PLAYING) == 100);
    assert(tube_extra_dim_input(7200, 1, 0) == 1 && !tube_extra_screen_is_off() && !tube_extra_dim_tick(7200, TUBE_DIM_PLAYING));
    /* Leaving the player, or a screen that never dims, ends it too. */
    tube_extra_screen_off();
    assert(tube_extra_dim_tick(8000, TUBE_DIM_PAUSED) == 100 && !tube_extra_dim_tick(8000, TUBE_DIM_NONE) && tube_extra_screen_is_off());
    assert(tube_extra_dim_tick(8000, TUBE_DIM_PLAYING) == 100 && !tube_extra_dim_tick(8001, TUBE_DIM_BROWSING) && !tube_extra_screen_is_off());
    assert(tube_extra_dim_tick(68001, TUBE_DIM_BROWSING) == 80 && !tube_extra_dim_tick(68002, TUBE_DIM_NONE));
    assert(!tube_extra_dim_tick(68003, TUBE_DIM_BROWSING) && tube_extra_dim_tick(128002, TUBE_DIM_BROWSING) == 80);
    assert(!tube_extra_dim_tick(100, TUBE_DIM_BROWSING));                                 /* a clock that went back */
    tube_extra_dim_reset(0);
    tube_player_extra_select(NULL);

    uint32_t pixels[] = {0x00000000u, 0xffffffffu, 0xff804020u, 0x80ffffffu};
    tube_dim_pixels(pixels, 4, 0);
    assert(pixels[1] == 0xffffffffu);
    tube_dim_pixels(pixels, 4, 100);
    for (int i = 0; i < 4; ++i) assert(pixels[i] == 0xff000000u);
    uint32_t half[] = {0x00000000u, 0xffffffffu, 0xff804020u};
    tube_dim_pixels(half, 3, 50);
    assert(half[0] == 0x7f000000u);                       /* over nothing: black at half strength */
    assert(half[1] >> 24 == 0xff && (half[1] & 0xff) >= 0x7f && (half[1] & 0xff) <= 0x81);
    assert((half[2] >> 16 & 0xff) >= 0x3f && (half[2] >> 16 & 0xff) <= 0x41 && (half[2] & 0xff) >= 0x0f && (half[2] & 0xff) <= 0x11);
    tube_dim_pixels(NULL, 4, 50);
    tube_dim_pixels(half, 3, 250);
    assert(half[1] == 0xff000000u);
}

static void test_error_fixer(void) {
    assert(tube_error_fix_lower_height(2160) == 1440 && tube_error_fix_lower_height(1440) == 1080);
    assert(tube_error_fix_lower_height(1080) == 720 && !tube_error_fix_lower_height(720) && !tube_error_fix_lower_height(144));
    assert(tube_error_fix_lower_height(4320) == 1440 && tube_error_fix_lower_height(900) == 720);
    tube_fix_input in = {1, TUBE_ERROR_VIDEO_DECODER, 0, 1, 2160, 60, TUBE_CODEC_VP9, 0};
    assert(tube_error_fix_decide(&in) == TUBE_FIX_SAFE_VIDEO);
    in.attempts = 1; assert(tube_error_fix_decide(&in) == TUBE_FIX_LOWER_QUALITY);
    in.attempts = TUBE_FIX_ATTEMPTS; assert(tube_error_fix_decide(&in) == TUBE_FIX_NONE);
    in.attempts = 0; in.enabled = 0; assert(tube_error_fix_decide(&in) == TUBE_FIX_NONE);
    in.enabled = 1; in.height = 1080; in.fps = 30; in.codec = TUBE_CODEC_AVC;
    assert(tube_error_fix_decide(&in) == TUBE_FIX_LOWER_QUALITY);            /* already the safe format */
    in.height = 720; assert(tube_error_fix_decide(&in) == TUBE_FIX_NONE);
    in.fps = 60; assert(tube_error_fix_decide(&in) == TUBE_FIX_SAFE_VIDEO);
    in.error = TUBE_ERROR_AUDIO_DECODER; assert(tube_error_fix_decide(&in) == TUBE_FIX_NONE);
    in.error = TUBE_ERROR_SOURCE; assert(tube_error_fix_decide(&in) == TUBE_FIX_RELOAD);
    in.error = 99; assert(tube_error_fix_decide(&in) == TUBE_FIX_NONE && tube_error_fix_decide(NULL) == TUBE_FIX_NONE);
    /* Long buffering: captions off first, then the same stream, then a smaller picture. */
    in = (tube_fix_input){1, TUBE_ERROR_BUFFERING, 0, 1, 1440, 30, TUBE_CODEC_VP9, 0};
    assert(tube_error_fix_decide(&in) == (TUBE_FIX_RELOAD | TUBE_FIX_SUBTITLES_OFF));
    in.subtitles = 0; assert(tube_error_fix_decide(&in) == TUBE_FIX_RELOAD);
    in.attempts = 1; assert(tube_error_fix_decide(&in) == TUBE_FIX_LOWER_QUALITY);
    in.height = 720; assert(tube_error_fix_decide(&in) == TUBE_FIX_RELOAD);
    in.live = 1; in.subtitles = 1; assert(tube_error_fix_decide(&in) == TUBE_FIX_RELOAD);   /* live keeps its captions */
    in.attempts = -1; assert(tube_error_fix_decide(&in) == TUBE_FIX_NONE);

    int height = 2160, fps = 60, codec = TUBE_CODEC_AV1;
    assert(tube_error_fix_quality(TUBE_FIX_SAFE_VIDEO, &height, &fps, &codec) && height == 1080 && fps == 30 && codec == TUBE_CODEC_AVC);
    assert(!tube_error_fix_quality(TUBE_FIX_SAFE_VIDEO, &height, &fps, &codec));
    assert(tube_error_fix_quality(TUBE_FIX_LOWER_QUALITY | TUBE_FIX_SUBTITLES_OFF, &height, &fps, &codec) && height == 720);
    assert(!tube_error_fix_quality(TUBE_FIX_LOWER_QUALITY, &height, &fps, &codec) && height == 720 && fps == 30);
    assert(!tube_error_fix_quality(TUBE_FIX_RELOAD, &height, &fps, &codec) && !tube_error_fix_quality(TUBE_FIX_RELOAD, NULL, &fps, &codec));
    height = 480; fps = 0; codec = TUBE_CODEC_AUTO;
    assert(tube_error_fix_quality(TUBE_FIX_SAFE_VIDEO, &height, &fps, &codec) && height == 480 && fps == 30);
    for (int error = 0; error <= TUBE_ERROR_BUFFERING; ++error)
        for (int action = 0; action <= TUBE_FIX_SAFE_VIDEO; ++action) {
            assert(tube_error_fix_message(error, action)[0] && tube_error_fix_message(error, action | TUBE_FIX_SUBTITLES_OFF)[0]);
            assert(strlen(tube_error_fix_message(error, action | TUBE_FIX_SUBTITLES_OFF)) < 96);
        }
    assert(strstr(tube_error_fix_message(TUBE_ERROR_BUFFERING, TUBE_FIX_RELOAD | TUBE_FIX_SUBTITLES_OFF), "Captions off"));
    assert(strstr(tube_error_fix_message(TUBE_ERROR_VIDEO_DECODER, TUBE_FIX_SAFE_VIDEO), "AVC"));

    /* 20 s of waiting inside 60 s; smooth playback in between lets the window expire. */
    tube_buffering_t state;
    tube_buffering_reset(&state);
    uint64_t now = 5000;
    assert(!tube_buffering_tick(&state, now, 1));
    for (int i = 0; i < 1200; ++i) { now += 16; assert(!tube_buffering_tick(&state, now, 1)); }   /* 19.2 s */
    for (int i = 0; i < 49; ++i) { now += 16; assert(!tube_buffering_tick(&state, now, 1)); }
    now += 16; assert(tube_buffering_tick(&state, now, 1));                                      /* 20 s reached */
    now += 16; assert(!tube_buffering_tick(&state, now, 1));                                     /* started over */
    tube_buffering_reset(&state); now = 100000;
    tube_buffering_tick(&state, now, 0);
    for (int i = 0; i < 950; ++i) { now += 20; assert(!tube_buffering_tick(&state, now, 1)); }    /* 19 s */
    for (int i = 0; i < 2100; ++i) { now += 20; assert(!tube_buffering_tick(&state, now, 0)); }   /* 42 s playing: new window */
    for (int i = 0; i < 950; ++i) { now += 20; assert(!tube_buffering_tick(&state, now, 1)); }
    for (int i = 0; i < 49; ++i) { now += 20; assert(!tube_buffering_tick(&state, now, 1)); }
    now += 20; assert(tube_buffering_tick(&state, now, 1));
    /* A stopped clock (the app was suspended) is not a wait, and time never runs back. */
    tube_buffering_reset(&state);
    assert(!tube_buffering_tick(&state, 1000, 1) && !tube_buffering_tick(&state, 31000, 1) && !state.waited_ms);
    assert(!tube_buffering_tick(&state, 500, 1) && !state.waited_ms && !tube_buffering_tick(NULL, 0, 1));
}

static void test_presets(void) {
    char label[48];
    int height, fps, codec, seen = 0;
    assert(tube_preset_count() == 39);
    assert(tube_preset_at(0, &height, &fps, &codec) && !height && !fps && codec == TUBE_CODEC_AUTO);
    assert(tube_preset_label(0, label, sizeof label) && !strcmp(label, "Disabled"));
    assert(tube_preset_label(1, label, sizeof label) && !strcmp(label, "2160p 60 fps VP9"));
    assert(tube_preset_label(38, label, sizeof label) && !strcmp(label, "144p 30 fps AV1"));
    assert(!tube_preset_at(39, &height, &fps, &codec) && !tube_preset_at(-1, &height, &fps, &codec) && !tube_preset_at(1, NULL, &fps, &codec));
    assert(!tube_preset_label(39, label, sizeof label) && !label[0]);
    for (int i = 1; i < tube_preset_count(); ++i) {
        assert(tube_preset_at(i, &height, &fps, &codec) && (fps == 30 || fps == 60) && codec >= TUBE_CODEC_AVC && codec <= TUBE_CODEC_AV1);
        assert(!(codec == TUBE_CODEC_AVC && height > 1080) && !(fps == 60 && height < 360));   /* SmartTube's table */
        assert(tube_preset_find(height, fps, codec) == i);
        seen += height == 1080;
    }
    assert(seen == 6);
    assert(!tube_preset_find(1080, 0, TUBE_CODEC_AUTO) && tube_preset_find(1080, 0, TUBE_CODEC_VP9) == -1);
    assert(tube_preset_find(1080, 60, TUBE_CODEC_HEVC) == -1 && tube_preset_find(2160, 30, TUBE_CODEC_AVC) == -1);
}

int main(void) {
    test_storage();
    test_setters();
    test_groups();
    test_captions();
    test_volume();
    test_labels();
    test_exit_and_keys();
    test_speeds_and_shorts();
    test_video();
    test_buttons();
    test_chapter_notice();
    test_dimming();
    test_error_fixer();
    test_presets();
    puts("PASS: player extra preferences, option groups, per-video facts and player decisions");
    return 0;
}
