/* ProsperoTube player preferences outside the provider; see tube_player_extra.h.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_player_extra.h"
#include "tube_audio_delay.h"
#include "tube_time_display.h"
#include "tube_volume.h"
#include "cJSON.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ---- Fields: one table drives defaults, validation and JSON. ---- */
#define NO_ALT INT_MIN
typedef struct {
    const char *name;
    size_t offset;
    int fallback, min, max, step;
    int alt;                 /* one extra accepted value outside the range */
} field_t;
#define FIELD(member, fallback, min, max, step, alt) \
    { #member, offsetof(TubePlayerExtra, member), fallback, min, max, step, alt }
enum { F_SUBTITLE_STYLE, F_SUBTITLE_SCALE, F_SUBTITLE_POSITION, F_VOLUME, F_AUTO_VOLUME,
       F_AUDIO_DELAY_MS, F_AUDIO_DELAY_ENABLED, F_CLOCK, F_QUALITY_INFO, F_QUALITY_BITRATE,
       F_CLOCK_12H, F_EXIT_MODE, F_KEYS_LR, F_KEYS_UD, F_SEEK_PREVIEW, F_SPEED_LIST,
       F_LOOP_SHORTS, F_REMEMBER_SHORT, F_BUTTONS, F_SUGGESTIONS, F_CHAPTER_NOTICE, F_ERROR_FIXER,
       F_REMEMBER_QUALITY, F_DIM_TIMEOUT, F_DIM_AMOUNT, F_SCREEN_OFF_TIMEOUT, F_SCREEN_OFF_AMOUNT, F_VIDEO_BUFFER, F_COUNT };
static const field_t fields[F_COUNT] = {
    FIELD(subtitle_style, 0, 0, 6, 1, NO_ALT),
    FIELD(subtitle_scale, 100, 10, 200, 10, NO_ALT),
    FIELD(subtitle_position, -1, 0, 1000, 25, -1),
    FIELD(volume, 100, 0, 300, 5, NO_ALT),
    FIELD(auto_volume, 0, 0, 1, 1, NO_ALT),
    FIELD(audio_delay_ms, 0, -TUBE_AUDIO_DELAY_MAX_MS, TUBE_AUDIO_DELAY_MAX_MS, 1, NO_ALT),
    FIELD(audio_delay_enabled, 0, 0, 1, 1, NO_ALT),
    FIELD(clock, 0, 0, 1, 1, NO_ALT),
    FIELD(quality_info, 0, 0, 1, 1, NO_ALT),
    FIELD(quality_bitrate, 0, 0, 1, 1, NO_ALT),
    FIELD(clock_12h, 0, 0, 1, 1, NO_ALT),
    FIELD(exit_mode, TUBE_EXIT_MODE_ASK, 0, 2, 1, NO_ALT),
    FIELD(keys_lr, TUBE_KEYS_DEFAULT, 0, TUBE_KEYS_VOLUME, 1, NO_ALT),
    FIELD(keys_ud, TUBE_KEYS_DEFAULT, 0, TUBE_KEYS_SUGGESTIONS, 1, NO_ALT),
    FIELD(seek_preview, 0, 0, 3, 1, NO_ALT),
    FIELD(speed_list, 1, 0, 2, 1, NO_ALT),
    FIELD(loop_shorts, 0, 0, 1, 1, NO_ALT),
    FIELD(remember_short, 1, 0, 1, 1, NO_ALT),
    FIELD(buttons, TUBE_BUTTONS_DEFAULT, 0, (1 << TUBE_BUTTON_COUNT) - 1, 1, NO_ALT),
    FIELD(suggestions, 1, 0, 1, 1, NO_ALT),
    FIELD(chapter_notice, 0, 0, 1, 1, NO_ALT),
    FIELD(error_fixer, 1, 0, 1, 1, NO_ALT),
    FIELD(remember_quality, 0, 0, 1, 1, NO_ALT),
    FIELD(dim_timeout, 0, 0, 900, 5, NO_ALT),
    FIELD(dim_amount, 80, 10, 100, 5, NO_ALT),
    FIELD(screen_off_timeout, 0, 0, 10800, 1, NO_ALT),
    FIELD(screen_off_amount, 100, 10, 100, 10, NO_ALT),
    FIELD(video_buffer, TUBE_BUFFER_MEDIUM, TUBE_BUFFER_LOW, TUBE_BUFFER_HIGHEST, 1, NO_ALT),
};
_Static_assert(TUBE_EXTRA_GROUPS == TUBE_EXTRA_BUTTON_FIRST + TUBE_BUTTON_COUNT, "one switch per player button");

static int *slot(TubePlayerExtra *prefs, const field_t *field) {
    return (int *)(void *)((char *)prefs + field->offset);
}
static int field_valid(const field_t *field, int value) {
    if (field->alt != NO_ALT && value == field->alt) return 1;
    return value >= field->min && value <= field->max && (value - field->min) % field->step == 0;
}

void tube_player_extra_defaults(TubePlayerExtra *prefs) {
    if (!prefs) return;
    for (int i = 0; i < F_COUNT; ++i) *slot(prefs, &fields[i]) = fields[i].fallback;
}

int tube_player_extra_validate(TubePlayerExtra *prefs) {
    int changed = 0;
    if (!prefs) return 0;
    for (int i = 0; i < F_COUNT; ++i) {
        int *value = slot(prefs, &fields[i]);
        if (!field_valid(&fields[i], *value)) { *value = fields[i].fallback; ++changed; }
    }
    return changed;
}

static int json_int(const cJSON *item, int *out) {
    if (cJSON_IsBool(item)) { *out = cJSON_IsTrue(item) ? 1 : 0; return 1; }
    if (!cJSON_IsNumber(item)) return 0;
    const double value = item->valuedouble;
    if (!(value >= -2147483647.0 && value <= 2147483647.0) || value != floor(value)) return 0;
    *out = (int)value;
    return 1;
}

TubePlayerExtra tube_player_extra_read(const cJSON *object) {
    TubePlayerExtra prefs;
    tube_player_extra_defaults(&prefs);
    if (!cJSON_IsObject(object)) return prefs;
    for (int i = 0; i < F_COUNT; ++i) {
        int value;
        if (json_int(cJSON_GetObjectItemCaseSensitive(object, fields[i].name), &value) &&
            field_valid(&fields[i], value)) *slot(&prefs, &fields[i]) = value;
    }
    return prefs;
}

int tube_player_extra_write(cJSON *object, const TubePlayerExtra *prefs) {
    if (!cJSON_IsObject(object) || !prefs) return 0;
    TubePlayerExtra checked = *prefs;
    tube_player_extra_validate(&checked);
    for (int i = 0; i < F_COUNT; ++i) {
        cJSON_DeleteItemFromObjectCaseSensitive(object, fields[i].name);
        if (!cJSON_AddNumberToObject(object, fields[i].name, *slot(&checked, &fields[i]))) return 0;
    }
    return 1;
}

/* ---- Current profile and per-video state. ---- */
typedef struct { double start; char title[TUBE_EXTRA_CHAPTER_TITLE]; } chapter_t;
static TubePlayerExtra g_prefs;
static int g_ready;
static unsigned g_revision;
static tube_player_extra_save_fn g_save;
static void *g_save_user;
static int g_muted, g_has_loudness, g_short;
static double g_loudness;
static chapter_t g_chapters[TUBE_EXTRA_CHAPTERS];
static int g_chapter_count;
static tube_storyboard_level g_storyboards[TUBE_STORYBOARD_LEVELS];
static int g_storyboard_count;
static int g_stream_height;
static double g_stream_fps;
static long long g_stream_bitrate;
static char g_stream_codec[24];
static uint64_t g_exit_armed;
static int g_chapter_announced = -1, g_picker_request;
static uint64_t g_dim_input_ms;
static int g_dim_started, g_dim_percent, g_screen_off;

static TubePlayerExtra *prefs(void) {
    if (!g_ready) { tube_player_extra_defaults(&g_prefs); g_ready = 1; }
    return &g_prefs;
}
static int saved(void) {
    ++g_revision;
    return g_save ? g_save(prefs(), g_save_user) != 0 : 1;
}
static int set_field(int index, int value) {
    if (!field_valid(&fields[index], value)) return 0;
    int *current = slot(prefs(), &fields[index]);
    if (*current == value) return 1;
    *current = value;
    return saved();
}

void tube_player_extra_select(const TubePlayerExtra *selected) {
    if (selected) { g_prefs = *selected; tube_player_extra_validate(&g_prefs); }
    else tube_player_extra_defaults(&g_prefs);
    g_ready = 1; g_exit_armed = 0; ++g_revision;
}
const TubePlayerExtra *tube_player_extra_current(void) { return prefs(); }
void tube_player_extra_set_save_hook(tube_player_extra_save_fn save, void *user) { g_save = save; g_save_user = user; }
unsigned tube_extra_revision(void) { return g_revision; }

#define ACCESSORS(getter, setter, member, index) \
    int getter(void) { return prefs()->member; } \
    int setter(int value) { return set_field(index, value); }
ACCESSORS(tube_extra_subtitle_style_index, tube_extra_set_subtitle_style, subtitle_style, F_SUBTITLE_STYLE)
ACCESSORS(tube_extra_subtitle_scale, tube_extra_set_subtitle_scale, subtitle_scale, F_SUBTITLE_SCALE)
ACCESSORS(tube_extra_subtitle_position, tube_extra_set_subtitle_position, subtitle_position, F_SUBTITLE_POSITION)
ACCESSORS(tube_extra_volume, tube_extra_set_volume, volume, F_VOLUME)
ACCESSORS(tube_extra_auto_volume, tube_extra_set_auto_volume, auto_volume, F_AUTO_VOLUME)
ACCESSORS(tube_extra_clock, tube_extra_set_clock, clock, F_CLOCK)
ACCESSORS(tube_extra_quality_info, tube_extra_set_quality_info, quality_info, F_QUALITY_INFO)
ACCESSORS(tube_extra_quality_bitrate, tube_extra_set_quality_bitrate, quality_bitrate, F_QUALITY_BITRATE)
ACCESSORS(tube_extra_clock_12h, tube_extra_set_clock_12h, clock_12h, F_CLOCK_12H)
ACCESSORS(tube_extra_exit_mode, tube_extra_set_exit_mode, exit_mode, F_EXIT_MODE)
ACCESSORS(tube_extra_keys_lr, tube_extra_set_keys_lr, keys_lr, F_KEYS_LR)
ACCESSORS(tube_extra_keys_ud, tube_extra_set_keys_ud, keys_ud, F_KEYS_UD)
ACCESSORS(tube_extra_seek_preview, tube_extra_set_seek_preview, seek_preview, F_SEEK_PREVIEW)
ACCESSORS(tube_extra_speed_list, tube_extra_set_speed_list, speed_list, F_SPEED_LIST)
ACCESSORS(tube_extra_loop_shorts, tube_extra_set_loop_shorts, loop_shorts, F_LOOP_SHORTS)
ACCESSORS(tube_extra_remember_short, tube_extra_set_remember_short, remember_short, F_REMEMBER_SHORT)
ACCESSORS(tube_extra_buttons, tube_extra_set_buttons, buttons, F_BUTTONS)
ACCESSORS(tube_extra_suggestions, tube_extra_set_suggestions, suggestions, F_SUGGESTIONS)
ACCESSORS(tube_extra_chapter_notice_enabled, tube_extra_set_chapter_notice, chapter_notice, F_CHAPTER_NOTICE)
ACCESSORS(tube_extra_error_fixer, tube_extra_set_error_fixer, error_fixer, F_ERROR_FIXER)
ACCESSORS(tube_extra_remember_quality, tube_extra_set_remember_quality, remember_quality, F_REMEMBER_QUALITY)
ACCESSORS(tube_extra_dim_timeout, tube_extra_set_dim_timeout, dim_timeout, F_DIM_TIMEOUT)
ACCESSORS(tube_extra_dim_amount, tube_extra_set_dim_amount, dim_amount, F_DIM_AMOUNT)
ACCESSORS(tube_extra_screen_off_timeout, tube_extra_set_screen_off_timeout, screen_off_timeout, F_SCREEN_OFF_TIMEOUT)
ACCESSORS(tube_extra_screen_off_amount, tube_extra_set_screen_off_amount, screen_off_amount, F_SCREEN_OFF_AMOUNT)
ACCESSORS(tube_extra_buffer, tube_extra_set_buffer, video_buffer, F_VIDEO_BUFFER)
int tube_extra_button(int button) {
    return button >= 0 && button < TUBE_BUTTON_COUNT && (prefs()->buttons >> button & 1);
}

int tube_extra_audio_delay_ms(void) { return prefs()->audio_delay_ms; }
int tube_extra_audio_delay_enabled(void) { return prefs()->audio_delay_enabled; }
int tube_extra_set_audio_delay_ms(int ms) {
    if (!field_valid(&fields[F_AUDIO_DELAY_MS], ms)) return 0;
    TubePlayerExtra *current = prefs();
    const int enabled = ms != 0;
    if (current->audio_delay_ms == ms && current->audio_delay_enabled == enabled) return 1;
    current->audio_delay_ms = ms; current->audio_delay_enabled = enabled;
    return saved();
}
/* As in SmartTube, the switch has nothing to enable while the value is zero. */
int tube_extra_set_audio_delay_enabled(int enabled) {
    if (enabled != 0 && enabled != 1) return 0;
    if (enabled && !prefs()->audio_delay_ms) return 0;
    return set_field(F_AUDIO_DELAY_ENABLED, enabled);
}
int tube_extra_effective_audio_delay_ms(void) {
    return prefs()->audio_delay_enabled ? prefs()->audio_delay_ms : 0;
}
int tube_extra_muted(void) { return g_muted; }
void tube_extra_set_muted(int muted) { muted = muted != 0; if (muted != g_muted) { g_muted = muted; ++g_revision; } }

/* ---- Captions. ---- */
tube_subtitle_style_t tube_subtitle_style_preset(int style) {
    /* SmartTube's presets: light grey or yellow text on a transparent,
     * semitransparent or black plate; a drop shadow replaces the outline when
     * there is no plate. */
    static const tube_subtitle_style_t presets[] = {
        {0, 0xffffff, 0x000000, 140, TUBE_SUBTITLE_EDGE_NONE},
        {1, 0xe4e4e4, 0x000000, 0, TUBE_SUBTITLE_EDGE_SHADOW},
        {1, 0xe4e4e4, 0x000000, 100, TUBE_SUBTITLE_EDGE_OUTLINE},
        {1, 0xe4e4e4, 0x000000, 255, TUBE_SUBTITLE_EDGE_OUTLINE},
        {1, 0xffff00, 0x000000, 0, TUBE_SUBTITLE_EDGE_SHADOW},
        {1, 0xffff00, 0x000000, 100, TUBE_SUBTITLE_EDGE_OUTLINE},
        {1, 0xffff00, 0x000000, 255, TUBE_SUBTITLE_EDGE_OUTLINE},
    };
    return presets[style >= 0 && style <= 6 ? style : 0];
}
tube_subtitle_style_t tube_extra_subtitle_style(void) { return tube_subtitle_style_preset(prefs()->subtitle_style); }

double tube_subtitle_baseline(int per_mille, double screen_height, double block_height, double line_height) {
    if (per_mille < 0) per_mille = 0;
    if (per_mille > 1000) per_mille = 1000;
    double baseline = screen_height * (1000 - per_mille) / 1000.0;
    double lowest = screen_height - line_height * 0.3, highest = block_height;
    if (highest > lowest) highest = lowest;
    if (baseline > lowest) baseline = lowest;
    if (baseline < highest) baseline = highest;
    return baseline;
}

/* ---- Volume. ---- */
int tube_extra_gain_for(const TubePlayerExtra *profile, int muted, int has_loudness, double loudness_db) {
    if (!profile || muted) return 0;
    long gain = (long)profile->volume * 10;
    /* SmartTube replaces a boost by the video's own level when auto volume is
     * on. Here the normalisation only ever attenuates and the chosen volume
     * still applies, so the switch cannot make a video louder than without it. */
    if (profile->auto_volume && has_loudness)
        gain = gain * tube_volume_normalization_milli(loudness_db) / TUBE_VOLUME_UNITY;
    if (gain < 0) gain = 0;
    if (gain > TUBE_VOLUME_MAX_MILLI) gain = TUBE_VOLUME_MAX_MILLI;
    return (int)gain;
}
int tube_extra_gain_milli(void) { return tube_extra_gain_for(prefs(), g_muted, g_has_loudness, g_loudness); }
int tube_extra_volume_step(int direction) {
    int volume = prefs()->volume + (direction > 0 ? 5 : direction < 0 ? -5 : 0);
    if (volume < 0) volume = 0;
    if (volume > 300) volume = 300;
    tube_extra_set_volume(volume);
    return prefs()->volume;
}

/* ---- Controls-bar labels. ---- */
void tube_extra_set_stream_info(int height, double fps, const char *codec, long long bitrate_bps) {
    if (height < 0 || height > 100000) height = 0;
    if (!isfinite(fps) || fps < 0 || fps > 1000) fps = 0;
    if (bitrate_bps < 0) bitrate_bps = 0;
    if (!codec) codec = "";
    if (height == g_stream_height && fps == g_stream_fps && bitrate_bps == g_stream_bitrate &&
        !strncmp(codec, g_stream_codec, sizeof g_stream_codec - 1)) return;
    g_stream_height = height; g_stream_fps = fps; g_stream_bitrate = bitrate_bps;
    snprintf(g_stream_codec, sizeof g_stream_codec, "%s", codec);
    ++g_revision;
}

int tube_extra_quality_text(char *out, size_t size, int height, double fps, const char *codec,
                            long long bitrate_bps, int with_bitrate) {
    if (!out || !size) return 0;
    out[0] = 0;
    if (height <= 0) return 0;
    size_t used;
    /* YouTube's own naming: the frame rate is spelled out only above 30. */
    if (isfinite(fps) && fps > 32 && fps <= 1000) snprintf(out, size, "%dp%d", height, (int)(fps + 0.5));
    else snprintf(out, size, "%dp", height);
    used = strlen(out);
    if (codec && *codec && used < size) { snprintf(out + used, size - used, " %s", codec); used = strlen(out); }
    if (with_bitrate && bitrate_bps > 0 && used < size) {
        if (bitrate_bps >= 1000000) snprintf(out + used, size - used, " %.1f Mbps", (double)bitrate_bps / 1e6);
        else snprintf(out + used, size - used, " %lld kbps", (bitrate_bps + 500) / 1000);
    }
    return 1;
}

int tube_extra_controls_info(char *out, size_t size, time_t now) {
    if (!out || !size) return 0;
    out[0] = 0;
    char part[64];
    size_t used = 0;
    if (g_muted) { snprintf(out, size, "MUTED"); used = strlen(out); }
    if (prefs()->quality_info && tube_extra_quality_text(part, sizeof part, g_stream_height, g_stream_fps,
            g_stream_codec, g_stream_bitrate, prefs()->quality_bitrate) && used < size) {
        snprintf(out + used, size - used, "%s%s", used ? "   " : "", part); used = strlen(out);
    }
    if (prefs()->clock && tube_clock_text(part, sizeof part, now, prefs()->clock_12h) && used < size) {
        snprintf(out + used, size - used, "%s%s", used ? "   " : "", part); used = strlen(out);
    }
    return used != 0;
}
int tube_extra_time_flags(void) { return prefs()->clock_12h ? TUBE_TIME_12H : 0; }

/* ---- Exit from the player. ---- */
int tube_extra_exit_decide(int mode, uint64_t now_ms, uint64_t *armed_ms) {
    if (mode == TUBE_EXIT_MODE_SINGLE) { if (armed_ms) *armed_ms = 0; return TUBE_EXIT_NOW; }
    if (mode != TUBE_EXIT_MODE_DOUBLE || !armed_ms) { if (armed_ms) *armed_ms = 0; return TUBE_EXIT_ASK; }
    if (*armed_ms && now_ms >= *armed_ms && now_ms - *armed_ms <= TUBE_EXIT_DOUBLE_MS) {
        *armed_ms = 0;
        return TUBE_EXIT_NOW;
    }
    *armed_ms = now_ms ? now_ms : 1;
    return TUBE_EXIT_ARMED;
}
int tube_extra_exit_press(uint64_t now_ms) { return tube_extra_exit_decide(prefs()->exit_mode, now_ms, &g_exit_armed); }
void tube_extra_exit_reset(void) { g_exit_armed = 0; }

/* ---- D-pad behaviour. ---- */
int tube_extra_key_action(int mode, int is_short, int controls_hidden) {
    switch (mode) {
    case TUBE_KEYS_SKIP_SHORTS: return controls_hidden && is_short ? TUBE_KEY_ACTION_SKIP : TUBE_KEY_ACTION_DEFAULT;
    case TUBE_KEYS_SKIP_VIDEOS: return controls_hidden && !is_short ? TUBE_KEY_ACTION_SKIP : TUBE_KEY_ACTION_DEFAULT;
    case TUBE_KEYS_SKIP_ALL: return controls_hidden ? TUBE_KEY_ACTION_SKIP : TUBE_KEY_ACTION_DEFAULT;
    case TUBE_KEYS_VOLUME: return TUBE_KEY_ACTION_VOLUME;
    case TUBE_KEYS_SPEED: return TUBE_KEY_ACTION_SPEED;
    case TUBE_KEYS_SUGGESTIONS: return controls_hidden ? TUBE_KEY_ACTION_DEFAULT : TUBE_KEY_ACTION_SUGGESTIONS;
    default: return TUBE_KEY_ACTION_DEFAULT;
    }
}
int tube_extra_dpad_action(int vertical, int controls_hidden) {
    const int action = tube_extra_key_action(vertical ? prefs()->keys_ud : prefs()->keys_lr, g_short, controls_hidden);
    return action == TUBE_KEY_ACTION_SUGGESTIONS && !prefs()->suggestions ? TUBE_KEY_ACTION_DEFAULT : action;
}

/* ---- Speed lists. ---- */
static const int speeds_short[] = {25,50,75,100,125,150,175,200,225,250,275,300,325,350,375,400};
static const int speeds_long[] = {25,50,75,80,85,90,95,100,105,110,115,120,125,130,135,140,145,150,160,175,
                                  180,200,225,250,275,300,325,350,375,400};
/* SmartTube's extra-long list starts at 0.05x; the PS5 tempo stage accepts
 * 0.25x to 4x, so the list keeps the same 0.05 steps inside that range. */
#define SPEED_EXTRA_MIN 25
#define SPEED_EXTRA_MAX 400
#define SPEED_EXTRA_STEP 5
int tube_speed_list_count(int list) {
    if (list == 0) return (int)(sizeof speeds_short / sizeof *speeds_short);
    if (list == 2) return (SPEED_EXTRA_MAX - SPEED_EXTRA_MIN) / SPEED_EXTRA_STEP + 1;
    return (int)(sizeof speeds_long / sizeof *speeds_long);
}
int tube_speed_list_at(int list, int index) {
    if (index < 0 || index >= tube_speed_list_count(list)) return 0;
    if (list == 0) return speeds_short[index];
    if (list == 2) return SPEED_EXTRA_MIN + SPEED_EXTRA_STEP * index;
    return speeds_long[index];
}
int tube_extra_speed_count(void) { return tube_speed_list_count(prefs()->speed_list); }
int tube_extra_speed_at(int index) { return tube_speed_list_at(prefs()->speed_list, index); }
int tube_extra_speed_valid(int percent) {
    for (int i = 0; i < tube_extra_speed_count(); ++i) if (tube_extra_speed_at(i) == percent) return 1;
    return 0;
}
int tube_extra_speed_step(int percent, int direction) {
    const int count = tube_extra_speed_count();
    if (direction > 0) {
        for (int i = 0; i < count; ++i) if (tube_extra_speed_at(i) > percent) return tube_extra_speed_at(i);
    } else if (direction < 0) {
        for (int i = count - 1; i >= 0; --i) if (tube_extra_speed_at(i) < percent) return tube_extra_speed_at(i);
    }
    return percent;
}

/* ---- Shorts and short videos. ---- */
int tube_extra_should_loop(int loop_shorts, int is_short) { return loop_shorts && is_short; }
int tube_extra_resume_decision(int remember_short, double duration_seconds) {
    if (remember_short) return 1;
    /* Unknown durations keep their position: only a known short video restarts. */
    return !(isfinite(duration_seconds) && duration_seconds > 0 && duration_seconds < TUBE_SHORT_VIDEO_SECONDS);
}
int tube_extra_loop_current(void) { return tube_extra_should_loop(prefs()->loop_shorts, g_short); }
int tube_extra_resume_allowed(double duration_seconds) {
    return tube_extra_resume_decision(prefs()->remember_short, duration_seconds);
}

/* ---- Per-video facts. ---- */
void tube_extra_set_loudness(int has_value, double loudness_db) {
    g_has_loudness = has_value && isfinite(loudness_db) && loudness_db >= -60 && loudness_db <= 60;
    g_loudness = g_has_loudness ? loudness_db : 0;
    ++g_revision;
}
void tube_extra_set_short(int is_short) { g_short = is_short != 0; ++g_revision; }
int tube_extra_is_short(void) { return g_short; }

/* Bounded copy that never splits a UTF-8 sequence; control bytes become spaces. */
static void copy_title(char *out, size_t size, const char *text) {
    size_t used = 0;
    if (!text) text = "";
    while (*text) {
        const unsigned char lead = (unsigned char)*text;
        size_t length = lead < 0x80 ? 1 : (lead & 0xe0) == 0xc0 ? 2 : (lead & 0xf0) == 0xe0 ? 3 :
                        (lead & 0xf8) == 0xf0 ? 4 : 0;
        size_t i = 1;
        while (length && i < length && ((unsigned char)text[i] & 0xc0) == 0x80) ++i;
        if (!length || i != length) { ++text; continue; }   /* drop malformed bytes */
        if (used + length + 1 > size) break;
        if (length == 1) out[used++] = lead < 0x20 || lead == 0x7f ? ' ' : (char)lead;
        else { memcpy(out + used, text, length); used += length; }
        text += length;
    }
    out[used] = 0;
}

static char g_playing_channel[193], g_playing_channel_id[25], g_playing_art[512];
static char g_playing_views[17], g_playing_published[65];

const char *tube_playing_fact(const char *name) {
    if (!name) return "";
    if (!strcmp(name, "channel")) return g_playing_channel;
    if (!strcmp(name, "channel_id")) return g_playing_channel_id;
    if (!strcmp(name, "channel_art_url")) return g_playing_art;
    if (!strcmp(name, "views")) return g_playing_views;
    if (!strcmp(name, "published")) return g_playing_published;
    return "";
}

static const char *fact_string(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : "";
}

static int channel_id_valid(const char *id) {
    if (strlen(id) != 24 || id[0] != 'U' || id[1] != 'C') return 0;
    for (const char *at = id + 2; *at; ++at)
        if (!((*at >= 'A' && *at <= 'Z') || (*at >= 'a' && *at <= 'z') ||
              (*at >= '0' && *at <= '9') || *at == '_' || *at == '-')) return 0;
    return 1;
}

static int views_valid(const char *value) {
    size_t n = strlen(value);
    if (!n || n > 16) return 0;
    for (const char *at = value; *at; ++at) if (*at < '0' || *at > '9') return 0;
    return 1;
}

static int add_chapter(double start, const char *title) {
    if (!isfinite(start) || start < 0 || start > 1e9 || g_chapter_count >= TUBE_EXTRA_CHAPTERS) return 0;
    int at = g_chapter_count;
    while (at > 0 && g_chapters[at - 1].start > start) { g_chapters[at] = g_chapters[at - 1]; --at; }
    g_chapters[at].start = start;
    copy_title(g_chapters[at].title, sizeof g_chapters[at].title, title);
    ++g_chapter_count;
    return 1;
}

int tube_extra_set_chapters(const cJSON *chapters) {
    g_chapter_count = 0;
    const cJSON *row;
    if (cJSON_IsArray(chapters)) cJSON_ArrayForEach(row, chapters) {
        const cJSON *start = cJSON_GetObjectItemCaseSensitive(row, "start");
        const cJSON *title = cJSON_GetObjectItemCaseSensitive(row, "title");
        if (cJSON_IsNumber(start)) add_chapter(start->valuedouble, cJSON_IsString(title) ? title->valuestring : "");
    }
    ++g_revision;
    return g_chapter_count;
}

int tube_extra_set_chapter_list(const double *starts, const char *const *titles, int count) {
    g_chapter_count = 0;
    for (int i = 0; starts && i < count; ++i) add_chapter(starts[i], titles ? titles[i] : "");
    ++g_revision;
    return g_chapter_count;
}

int tube_extra_chapter_count(void) { return g_chapter_count; }
double tube_extra_chapter_start(int index) {
    return index >= 0 && index < g_chapter_count ? g_chapters[index].start : -1;
}
const char *tube_extra_chapter_title(int index) {
    return index >= 0 && index < g_chapter_count ? g_chapters[index].title : "";
}
int tube_extra_chapter_at(double position) {
    int found = -1;
    if (!isfinite(position)) return -1;
    for (int i = 0; i < g_chapter_count && g_chapters[i].start <= position + 0.001; ++i) found = i;
    return found;
}
const char *tube_extra_chapter_title_at(double position) {
    return tube_extra_chapter_title(tube_extra_chapter_at(position));
}
int tube_extra_chapter_label(int index, char *out, size_t size) {
    if (!out || !size) return 0;
    out[0] = 0;
    if (index < 0 || index >= g_chapter_count) return 0;
    char at[40];
    tube_duration_text(at, sizeof at, g_chapters[index].start);
    if (g_chapters[index].title[0]) snprintf(out, size, "%s  %s", at, g_chapters[index].title);
    else snprintf(out, size, "%s  Chapter %d", at, index + 1);
    return 1;
}

int tube_extra_set_storyboards(const cJSON *storyboards) {
    g_storyboard_count = 0;
    const cJSON *row;
    if (cJSON_IsArray(storyboards)) cJSON_ArrayForEach(row, storyboards) {
        if (g_storyboard_count >= TUBE_STORYBOARD_LEVELS) break;
        tube_storyboard_level level;
        memset(&level, 0, sizeof level);
        const cJSON *url = cJSON_GetObjectItemCaseSensitive(row, "url");
        if (!cJSON_IsString(url) || strlen(url->valuestring) >= sizeof level.url) continue;
        if (!json_int(cJSON_GetObjectItemCaseSensitive(row, "width"), &level.width) ||
            !json_int(cJSON_GetObjectItemCaseSensitive(row, "height"), &level.height) ||
            !json_int(cJSON_GetObjectItemCaseSensitive(row, "columns"), &level.columns) ||
            !json_int(cJSON_GetObjectItemCaseSensitive(row, "rows"), &level.rows) ||
            !json_int(cJSON_GetObjectItemCaseSensitive(row, "count"), &level.count) ||
            !json_int(cJSON_GetObjectItemCaseSensitive(row, "sheets"), &level.sheets) ||
            !json_int(cJSON_GetObjectItemCaseSensitive(row, "interval_ms"), &level.interval_ms)) continue;
        memcpy(level.url, url->valuestring, strlen(url->valuestring) + 1);
        if (tube_storyboard_valid(&level)) g_storyboards[g_storyboard_count++] = level;
    }
    ++g_revision;
    return g_storyboard_count;
}
int tube_extra_storyboard_count(void) { return g_storyboard_count; }
const tube_storyboard_level *tube_extra_storyboard(int index) {
    return index >= 0 && index < g_storyboard_count ? &g_storyboards[index] : NULL;
}
int tube_extra_seek_preview_tile(double seconds, double duration, int target_width, int target_height,
                                 tube_storyboard_tile *tile, char *url, size_t url_size) {
    if (url && url_size) url[0] = 0;
    if (!prefs()->seek_preview || !tile) return 0;
    const int level = tube_storyboard_choose(g_storyboards, g_storyboard_count, target_width, target_height);
    if (level < 0 || !tube_storyboard_tile_at(&g_storyboards[level], seconds, duration, tile)) return 0;
    return !url || tube_storyboard_sheet_url(&g_storyboards[level], tile->sheet, url, url_size);
}

void tube_extra_video_end(void) {
    g_playing_channel[0] = g_playing_channel_id[0] = g_playing_art[0] = 0;
    g_playing_views[0] = g_playing_published[0] = 0;
    g_has_loudness = 0; g_loudness = 0; g_short = 0;
    g_chapter_count = 0; g_storyboard_count = 0;
    g_stream_height = 0; g_stream_fps = 0; g_stream_bitrate = 0; g_stream_codec[0] = 0;
    g_exit_armed = 0; g_chapter_announced = -1;
    ++g_revision;
}

void tube_extra_video_begin(const cJSON *resolve_result) {
    tube_extra_video_end();
    if (!cJSON_IsObject(resolve_result)) return;
    copy_title(g_playing_channel, sizeof g_playing_channel, fact_string(resolve_result, "channel"));
    const char *id = fact_string(resolve_result, "channel_id");
    if (channel_id_valid(id)) copy_title(g_playing_channel_id, sizeof g_playing_channel_id, id);
    const char *art = fact_string(resolve_result, "channel_art_url");
    if (strlen(art) < sizeof g_playing_art) copy_title(g_playing_art, sizeof g_playing_art, art);
    const char *views = fact_string(resolve_result, "views");
    if (views_valid(views)) copy_title(g_playing_views, sizeof g_playing_views, views);
    copy_title(g_playing_published, sizeof g_playing_published, fact_string(resolve_result, "published"));
    const cJSON *loudness = cJSON_GetObjectItemCaseSensitive(resolve_result, "loudness_db");
    tube_extra_set_loudness(cJSON_IsNumber(loudness), cJSON_IsNumber(loudness) ? loudness->valuedouble : 0);
    tube_extra_set_short(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(resolve_result, "is_short")));
    tube_extra_set_chapters(cJSON_GetObjectItemCaseSensitive(resolve_result, "chapters"));
    tube_extra_set_storyboards(cJSON_GetObjectItemCaseSensitive(resolve_result, "storyboards"));
}

/* ---- Preference groups for the settings screen and the in-player picker. ---- */
enum { G_LABELS, G_SWITCH, G_SCALE, G_POSITION, G_VOLUME, G_DELAY, G_SECONDS, G_PERCENT, G_BUTTON };
typedef struct {
    const char *id, *title, *detail;
    int kind, field, count;       /* field: index into fields, or -1 for mute */
    const char *const *labels;
    const int *values;            /* G_SECONDS and G_PERCENT choices */
    int button;                   /* G_BUTTON: its TUBE_BUTTON_* */
} group_t;
static const char *const style_labels[] = {
    "Default", "White on transparent background", "White on semitransparent background",
    "White on black background", "Yellow on transparent background",
    "Yellow on semitransparent background", "Yellow on black background" };
static const char *const time_labels[] = { "24-hour clock", "12-hour clock" };
static const char *const exit_labels[] = { "Ask before stopping", "Press Circle twice to exit", "Press Circle once to exit" };
static const char *const keys_lr_labels[] = {
    "Seek", "Previous / next for Shorts", "Previous / next for videos",
    "Previous / next for Shorts and videos", "Volume down / up" };
static const char *const keys_ud_labels[] = {
    "Captions on or off / caption list", "Previous / next for Shorts", "Previous / next for videos",
    "Previous / next for Shorts and videos", "Volume up / down", "Speed up / down",
    "Captions on or off / suggestions" };
static const char *const preview_labels[] = { "Disabled", "Single frame", "Carousel (slow)", "Carousel (fast)" };
static const char *const speed_list_labels[] = { "Short speed list", "Long speed list", "Extra long speed list" };
static const char *const buffer_labels[] = { "Low", "Medium", "High", "Highest" };
static const int delay_values[] = { -2000, -1500, -1000, -750, -500, -400, -300, -250, -200, -150, -100, -75, -50, -25,
                                    0, 25, 50, 75, 100, 150, 200, 250, 300, 400, 500, 750, 1000, 1500, 2000 };
#define DELAY_COUNT ((int)(sizeof delay_values / sizeof *delay_values))
/* SmartTube's lists: never, 5 to 30 s, 1 to 15 min; 10 to 80 % and 85 to 100 %. */
static const int dim_timeout_values[] = { 0, 5, 15, 30, 60, 120, 180, 240, 300, 360, 420, 480, 540, 600, 660, 720, 780, 840, 900 };
static const int dim_amount_values[] = { 10, 20, 30, 40, 50, 60, 70, 80, 85, 90, 95, 100 };
/* Never, 1 to 10 s, 30 to 180 min. */
static const int screen_off_timeout_values[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1800, 3600, 5400, 7200, 9000, 10800 };
static const int screen_off_amount_values[] = { 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
#define VALUES(list) (int)(sizeof list / sizeof *list), NULL, list, 0
#define BUTTON_DETAIL "Show this action in the in-player options"
static const group_t groups[TUBE_EXTRA_GROUPS] = {
    { "settings:subtitle-style", "Subtitle style", "Text colour, background and edge of captions", G_LABELS, F_SUBTITLE_STYLE, 7, style_labels, NULL, 0 },
    { "settings:subtitle-size", "Subtitle size", "Caption text scale", G_SCALE, F_SUBTITLE_SCALE, 20, NULL, NULL, 0 },
    { "settings:subtitle-position", "Subtitle position", "Height of captions above the bottom edge", G_POSITION, F_SUBTITLE_POSITION, 42, NULL, NULL, 0 },
    { "settings:player-volume", "Player volume", "Above 100% the sound is boosted and may distort", G_VOLUME, F_VOLUME, 61, NULL, NULL, 0 },
    { "settings:player-mute", "Mute", "Silence the player until switched back", G_SWITCH, -1, 2, NULL, NULL, 0 },
    { "settings:auto-volume", "Auto volume", "Turn loud videos down to YouTube's reference level", G_SWITCH, F_AUTO_VOLUME, 2, NULL, NULL, 0 },
    { "settings:audio-delay", "Audio delay", "Shift the sound against the picture", G_DELAY, F_AUDIO_DELAY_MS, DELAY_COUNT, NULL, NULL, 0 },
    { "settings:audio-delay-enabled", "Use audio delay", "Keep the chosen delay but switch it on or off", G_SWITCH, F_AUDIO_DELAY_ENABLED, 2, NULL, NULL, 0 },
    { "settings:player-clock", "Clock in controls", "Show the time of day in the controls bar", G_SWITCH, F_CLOCK, 2, NULL, NULL, 0 },
    { "settings:quality-info", "Quality info in controls", "Show resolution, frame rate and codec in the controls bar", G_SWITCH, F_QUALITY_INFO, 2, NULL, NULL, 0 },
    { "settings:quality-bitrate", "Bitrate in quality info", "Add the stream bitrate to the quality info", G_SWITCH, F_QUALITY_BITRATE, 2, NULL, NULL, 0 },
    { "settings:time-format", "Time format", "Clock and estimated finish time", G_LABELS, F_CLOCK_12H, 2, time_labels, NULL, 0 },
    { "settings:player-exit", "Exit from the player", "What Circle does during playback", G_LABELS, F_EXIT_MODE, 3, exit_labels, NULL, 0 },
    { "settings:keys-left-right", "Left / Right", "Quick skip works while the controls are hidden", G_LABELS, F_KEYS_LR, 5, keys_lr_labels, NULL, 0 },
    { "settings:keys-up-down", "Up / Down", "Quick skip works while the controls are hidden", G_LABELS, F_KEYS_UD, 7, keys_ud_labels, NULL, 0 },
    { "settings:seek-preview", "Preview while seeking", "Thumbnails from YouTube storyboards", G_LABELS, F_SEEK_PREVIEW, 4, preview_labels, NULL, 0 },
    { "settings:speed-list", "Speed list", "Which speeds the speed picker offers", G_LABELS, F_SPEED_LIST, 3, speed_list_labels, NULL, 0 },
    { "settings:loop-shorts", "Loop Shorts", "Repeat a Short until another video is chosen", G_SWITCH, F_LOOP_SHORTS, 2, NULL, NULL, 0 },
    { "settings:remember-short-position", "Remember position of short videos", "Off: videos under six minutes start from the beginning", G_SWITCH, F_REMEMBER_SHORT, 2, NULL, NULL, 0 },
    { "settings:player-suggestions", "Suggestions in the player", "List related videos, the queue and the playlist during playback", G_SWITCH, F_SUGGESTIONS, 2, NULL, NULL, 0 },
    { "settings:chapter-notification", "Chapter notification", "Announce a chapter when it starts; R1 skips it", G_SWITCH, F_CHAPTER_NOTICE, 2, NULL, NULL, 0 },
    { "settings:error-fixer", "Fix playback errors", "Reload the stream or lower its quality after decoder errors and long buffering", G_SWITCH, F_ERROR_FIXER, 2, NULL, NULL, 0 },
    { "settings:remember-quality", "Remember chosen quality", "A quality chosen in the player becomes the maximum for this profile", G_SWITCH, F_REMEMBER_QUALITY, 2, NULL, NULL, 0 },
    { "settings:video-buffer", "Video buffer", "How far ahead of playback a video is loaded; applies from the next video", G_LABELS, F_VIDEO_BUFFER, 4, buffer_labels, NULL, 0 },
    { "settings:dim-timeout", "Screen dimming", "Dim the picture while paused or browsing after this time without input", G_SECONDS, F_DIM_TIMEOUT, VALUES(dim_timeout_values) },
    { "settings:dim-amount", "Screen dimming amount", "How dark the dimmed picture is", G_PERCENT, F_DIM_AMOUNT, VALUES(dim_amount_values) },
    { "settings:screen-off-timeout", "Screen off timeout", "Turn the picture off in the player after this time without input; sound keeps playing", G_SECONDS, F_SCREEN_OFF_TIMEOUT, VALUES(screen_off_timeout_values) },
    { "settings:screen-off-amount", "Screen off amount", "How dark the picture is while the screen is off", G_PERCENT, F_SCREEN_OFF_AMOUNT, VALUES(screen_off_amount_values) },
    { "settings:button-like", "Button: Like", BUTTON_DETAIL, G_BUTTON, F_BUTTONS, 2, NULL, NULL, TUBE_BUTTON_LIKE },
    { "settings:button-dislike", "Button: Dislike", BUTTON_DETAIL, G_BUTTON, F_BUTTONS, 2, NULL, NULL, TUBE_BUTTON_DISLIKE },
    { "settings:button-subscribe", "Button: Subscribe", BUTTON_DETAIL, G_BUTTON, F_BUTTONS, 2, NULL, NULL, TUBE_BUTTON_SUBSCRIBE },
    { "settings:button-playlist", "Button: Add to playlist", BUTTON_DETAIL, G_BUTTON, F_BUTTONS, 2, NULL, NULL, TUBE_BUTTON_PLAYLIST_ADD },
    { "settings:button-queue", "Button: Playback queue", BUTTON_DETAIL, G_BUTTON, F_BUTTONS, 2, NULL, NULL, TUBE_BUTTON_QUEUE },
    { "settings:button-channel", "Button: Open channel", BUTTON_DETAIL, G_BUTTON, F_BUTTONS, 2, NULL, NULL, TUBE_BUTTON_CHANNEL },
    { "settings:button-search", "Button: Search", BUTTON_DETAIL, G_BUTTON, F_BUTTONS, 2, NULL, NULL, TUBE_BUTTON_SEARCH },
    { "settings:button-description", "Button: Description", BUTTON_DETAIL, G_BUTTON, F_BUTTONS, 2, NULL, NULL, TUBE_BUTTON_VIDEO_INFO },
    { "settings:button-share", "Button: Share link", BUTTON_DETAIL, G_BUTTON, F_BUTTONS, 2, NULL, NULL, TUBE_BUTTON_SHARE },
    { "settings:button-sponsorblock", "Button: SponsorBlock", BUTTON_DETAIL, G_BUTTON, F_BUTTONS, 2, NULL, NULL, TUBE_BUTTON_CONTENT_BLOCK },
    { "settings:button-chat", "Button: Live chat", BUTTON_DETAIL, G_BUTTON, F_BUTTONS, 2, NULL, NULL, TUBE_BUTTON_CHAT },
    { "settings:button-screen-off", "Button: Screen off", BUTTON_DETAIL, G_BUTTON, F_BUTTONS, 2, NULL, NULL, TUBE_BUTTON_SCREEN_OFF },
    { "settings:button-sound", "Button: Mute and volume", BUTTON_DETAIL, G_BUTTON, F_BUTTONS, 2, NULL, NULL, TUBE_BUTTON_SOUND },
    { "settings:button-speed", "Button: Video speed", BUTTON_DETAIL, G_BUTTON, F_BUTTONS, 2, NULL, NULL, TUBE_BUTTON_SPEED },
    { "settings:button-zoom", "Button: Video zoom", BUTTON_DETAIL, G_BUTTON, F_BUTTONS, 2, NULL, NULL, TUBE_BUTTON_ZOOM },
    { "settings:button-repeat", "Button: Playback mode", BUTTON_DETAIL, G_BUTTON, F_BUTTONS, 2, NULL, NULL, TUBE_BUTTON_REPEAT },
    { "settings:button-next", "Button: Next video", BUTTON_DETAIL, G_BUTTON, F_BUTTONS, 2, NULL, NULL, TUBE_BUTTON_NEXT },
    { "settings:button-previous", "Button: Previous video", BUTTON_DETAIL, G_BUTTON, F_BUTTONS, 2, NULL, NULL, TUBE_BUTTON_PREVIOUS },
    { "settings:button-seek-interval", "Button: Seek interval", BUTTON_DETAIL, G_BUTTON, F_BUTTONS, 2, NULL, NULL, TUBE_BUTTON_SEEK_INTERVAL },
    { "settings:button-rotate", "Button: Rotate video", BUTTON_DETAIL, G_BUTTON, F_BUTTONS, 2, NULL, NULL, TUBE_BUTTON_ROTATE },
    { "settings:button-mirror", "Button: Mirror video", BUTTON_DETAIL, G_BUTTON, F_BUTTONS, 2, NULL, NULL, TUBE_BUTTON_FLIP },
};

static const group_t *group_at(int group) { return group >= 0 && group < TUBE_EXTRA_GROUPS ? &groups[group] : NULL; }
static int group_value(const group_t *group, int choice) {
    switch (group->kind) {
    case G_SCALE: return 10 + 10 * choice;
    case G_POSITION: return choice ? (choice - 1) * 25 : -1;
    case G_VOLUME: return choice * 5;
    case G_DELAY: return delay_values[choice];
    case G_SECONDS: case G_PERCENT: return group->values[choice];
    default: return choice;
    }
}
const char *tube_extra_group_id(int group) { return group_at(group) ? groups[group].id : ""; }
const char *tube_extra_group_title(int group) { return group_at(group) ? groups[group].title : ""; }
const char *tube_extra_group_detail(int group) { return group_at(group) ? groups[group].detail : ""; }
int tube_extra_group_count(int group) { return group_at(group) ? groups[group].count : 0; }
int tube_extra_group_find(const char *id) {
    for (int i = 0; id && i < TUBE_EXTRA_GROUPS; ++i) if (!strcmp(id, groups[i].id)) return i;
    return -1;
}
int tube_extra_group_label(int group, int choice, char *out, size_t size) {
    const group_t *entry = group_at(group);
    if (!out || !size) return 0;
    out[0] = 0;
    if (!entry || choice < 0 || choice >= entry->count) return 0;
    const int value = group_value(entry, choice);
    switch (entry->kind) {
    case G_LABELS: snprintf(out, size, "%s", entry->labels[choice]); break;
    case G_SWITCH: snprintf(out, size, "%s", choice ? "On" : "Off"); break;
    case G_SCALE: snprintf(out, size, "%d.%dx", value / 100, value / 10 % 10); break;
    case G_POSITION:
        if (value < 0) snprintf(out, size, "Default");
        else if (value % 10) snprintf(out, size, "%d.%d%%", value / 10, value % 10);
        else snprintf(out, size, "%d%%", value / 10);
        break;
    case G_VOLUME: case G_PERCENT: snprintf(out, size, "%d%%", value); break;
    case G_BUTTON: snprintf(out, size, "%s", choice ? "On" : "Off"); break;
    case G_SECONDS:
        if (!value) snprintf(out, size, "Never");
        else if (value < 60) snprintf(out, size, "%d sec", value);
        else snprintf(out, size, "%d min", value / 60);
        break;
    default:
        if (!value) snprintf(out, size, "No delay");
        else snprintf(out, size, "%+d ms (audio %s)", value, value > 0 ? "later" : "earlier");
        break;
    }
    return 1;
}
int tube_extra_group_selected(int group) {
    const group_t *entry = group_at(group);
    if (!entry) return -1;
    int value = entry->field < 0 ? g_muted : *slot(prefs(), &fields[entry->field]);
    if (entry->kind == G_DELAY) value = tube_extra_effective_audio_delay_ms();
    if (entry->kind == G_BUTTON) value = value >> entry->button & 1;
    for (int choice = 0; choice < entry->count; ++choice) if (group_value(entry, choice) == value) return choice;
    return -1;
}
int tube_extra_group_select(int group, int choice) {
    const group_t *entry = group_at(group);
    if (!entry || choice < 0 || choice >= entry->count) return 0;
    const int value = group_value(entry, choice);
    if (entry->field < 0) { tube_extra_set_muted(value); return 1; }
    if (entry->kind == G_DELAY) return tube_extra_set_audio_delay_ms(value);
    if (entry->field == F_AUDIO_DELAY_ENABLED) return tube_extra_set_audio_delay_enabled(value);
    if (entry->kind == G_BUTTON)
        return set_field(F_BUTTONS, value ? prefs()->buttons | 1 << entry->button : prefs()->buttons & ~(1 << entry->button));
    return set_field(entry->field, value);
}

/* ---- Player buttons and where a group is listed. ---- */
int tube_extra_group_in_player(int group) {
    if (!group_at(group) || groups[group].kind == G_BUTTON) return 0;
    if (group == TUBE_EXTRA_VOLUME || group == TUBE_EXTRA_MUTE) return tube_extra_button(TUBE_BUTTON_SOUND);
    if (group >= TUBE_EXTRA_DIM_TIMEOUT && group <= TUBE_EXTRA_SCREEN_OFF_AMOUNT) return tube_extra_button(TUBE_BUTTON_SCREEN_OFF);
    return 1;
}
int tube_extra_group_screen(int group) {
    if (!group_at(group)) return -1;
    if (groups[group].kind == G_BUTTON) return 3;
    if (group >= TUBE_EXTRA_DIM_TIMEOUT && group <= TUBE_EXTRA_SCREEN_OFF_AMOUNT) return 4;
    return group <= TUBE_EXTRA_SUBTITLE_POSITION ? 0 : group <= TUBE_EXTRA_AUDIO_DELAY_ENABLED ? 1 : 2;
}
void tube_extra_picker_request(int list) { g_picker_request = list; }
int tube_extra_take_picker_request(void) { const int list = g_picker_request; g_picker_request = TUBE_PICKER_OPTIONS; return list; }

/* ---- Chapter notification. ---- */
int tube_chapter_notice_decide(int enabled, int announced, int current, int busy, int controls_visible) {
    /* The controls already show the chapter title; a seek in flight has no
     * settled position yet. Either way the chapter is announced afterwards. */
    if (!enabled || current < 0 || busy || controls_visible || current == announced) return -1;
    return current;
}
int tube_extra_chapter_notice(double position, int busy, int controls_visible) {
    const int chapter = tube_chapter_notice_decide(prefs()->chapter_notice, g_chapter_announced,
        tube_extra_chapter_at(position), busy, controls_visible);
    if (chapter >= 0) g_chapter_announced = chapter;
    return chapter;
}

/* ---- Screen dimming and screen off. ---- */
int tube_dim_decide(const TubePlayerExtra *profile, int context, uint64_t idle_ms, int screen_off) {
    int percent = 0;
    if (!profile || context == TUBE_DIM_NONE) return 0;
    const int player = context == TUBE_DIM_PAUSED || context == TUBE_DIM_PLAYING;
    if (player && (screen_off || (profile->screen_off_timeout > 0 &&
                                  idle_ms >= (uint64_t)profile->screen_off_timeout * 1000)))
        percent = profile->screen_off_amount;
    if (context != TUBE_DIM_PLAYING && profile->dim_timeout > 0 &&
        idle_ms >= (uint64_t)profile->dim_timeout * 1000 && profile->dim_amount > percent)
        percent = profile->dim_amount;
    return percent < 0 ? 0 : percent > 100 ? 100 : percent;
}
void tube_extra_dim_reset(uint64_t now_ms) {
    g_dim_input_ms = now_ms; g_dim_started = 1; g_screen_off = 0;
    if (g_dim_percent) { g_dim_percent = 0; ++g_revision; }
}
int tube_extra_dim_input(uint64_t now_ms, int pressed, int seek_only) {
    if (!pressed) return 0;
    const int dimmed = g_dim_percent > 0;
    /* Seeking blind keeps a screen that is off as it is; a dimmed one wakes. */
    if (dimmed && seek_only && g_dim_percent >= 100) return 0;
    tube_extra_dim_reset(now_ms);
    return dimmed;
}
int tube_extra_dim_tick(uint64_t now_ms, int context) {
    if (!g_dim_started || now_ms < g_dim_input_ms) { g_dim_input_ms = now_ms; g_dim_started = 1; }
    /* Screen off belongs to the player: the browser brings the picture back. */
    if (context == TUBE_DIM_BROWSING) g_screen_off = 0;
    /* A screen that never dims also restarts the wait for the next one. */
    if (context == TUBE_DIM_NONE) g_dim_input_ms = now_ms;
    const int percent = tube_dim_decide(prefs(), context, now_ms - g_dim_input_ms, g_screen_off);
    if (percent != g_dim_percent) { g_dim_percent = percent; ++g_revision; }
    return percent;
}
int tube_extra_dim_percent(void) { return g_dim_percent; }
void tube_extra_screen_off(void) { g_screen_off = 1; }
int tube_extra_screen_is_off(void) { return g_screen_off; }
void tube_dim_pixels(uint32_t *pixels, size_t count, int percent) {
    if (!pixels || percent <= 0) return;
    if (percent > 100) percent = 100;
    const unsigned black = (unsigned)percent * 255u / 100u, keep = 255u - black;
    for (size_t i = 0; i < count; ++i) {
        const uint32_t pixel = pixels[i];
        const unsigned alpha = pixel >> 24, covered = alpha * keep / 255u, out = black + covered;
        /* Black over the pixel: its colours keep only their own share of the result. */
        const unsigned scale = out ? covered * 255u / out : 0;
        const unsigned r = (pixel >> 16 & 0xffu) * scale / 255u, g = (pixel >> 8 & 0xffu) * scale / 255u,
                       b = (pixel & 0xffu) * scale / 255u;
        pixels[i] = (uint32_t)out << 24 | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
    }
}

/* ---- Error fixer. ---- */
int tube_error_fix_lower_height(int height) {
    /* SmartTube steps down the format list but never below 720p. */
    static const int steps[] = { 2160, 1440, 1080, 720 };
    for (int i = 0; i + 1 < (int)(sizeof steps / sizeof *steps); ++i) if (height > steps[i + 1]) return steps[i + 1];
    return 0;
}
int tube_error_fix_decide(const tube_fix_input *input) {
    if (!input || !input->enabled || input->attempts < 0 || input->attempts >= TUBE_FIX_ATTEMPTS) return TUBE_FIX_NONE;
    const int lower = tube_error_fix_lower_height(input->height) ? TUBE_FIX_LOWER_QUALITY : TUBE_FIX_NONE;
    switch (input->error) {
    case TUBE_ERROR_SOURCE: return TUBE_FIX_RELOAD;
    case TUBE_ERROR_VIDEO_DECODER: {
        /* First the format every decoder path handles, then smaller pictures. */
        const int safe = input->codec == TUBE_CODEC_AVC && input->height <= 1080 && input->fps <= 30;
        return !input->attempts && !safe ? TUBE_FIX_SAFE_VIDEO : lower;
    }
    case TUBE_ERROR_BUFFERING: {
        /* Captions go first, as in SmartTube; a repeated stall also lowers the quality. */
        const int action = input->attempts && lower ? lower : TUBE_FIX_RELOAD;
        return action | (input->subtitles && !input->live ? TUBE_FIX_SUBTITLES_OFF : 0);
    }
    default: return TUBE_FIX_NONE;    /* no other audio format can be requested */
    }
}
int tube_error_fix_quality(int action, int *height, int *fps, int *codec) {
    if (!height || !fps || !codec) return 0;
    const int old_height = *height, old_fps = *fps, old_codec = *codec;
    switch (action & TUBE_FIX_ACTION) {
    case TUBE_FIX_LOWER_QUALITY:
        if (tube_error_fix_lower_height(*height)) *height = tube_error_fix_lower_height(*height);
        break;
    case TUBE_FIX_SAFE_VIDEO:
        if (*height > 1080) *height = 1080;
        *fps = 30; *codec = TUBE_CODEC_AVC;
        break;
    default: break;
    }
    return *height != old_height || *fps != old_fps || *codec != old_codec;
}
const char *tube_error_fix_message(int error, int action) {
    const int off = action & TUBE_FIX_SUBTITLES_OFF;
    switch (action & TUBE_FIX_ACTION) {
    case TUBE_FIX_SAFE_VIDEO: return "Video decoder error. Reloading as AVC at up to 1080p30...";
    case TUBE_FIX_LOWER_QUALITY:
        return error == TUBE_ERROR_BUFFERING ? off ? "Still buffering. Captions off; reloading at a lower quality..."
                                                   : "Still buffering. Reloading at a lower quality..."
                                             : "Video decoder error. Reloading at a lower quality...";
    case TUBE_FIX_RELOAD:
        return error == TUBE_ERROR_BUFFERING ? off ? "Buffering too long. Captions off; reloading this stream..."
                                                   : "Buffering too long. Reloading this stream..."
                                             : "Refreshing this stream and restoring playback...";
    default:
        return error == TUBE_ERROR_VIDEO_DECODER ? "Video decoder error. Select the video to retry."
             : error == TUBE_ERROR_AUDIO_DECODER ? "Audio decoding or output failed. Select the video to retry."
             : error == TUBE_ERROR_BUFFERING ? "Buffering too long. Check the connection or choose a lower quality."
             : "Recovery is unavailable. Stop and select the video again.";
    }
}
void tube_buffering_reset(tube_buffering_t *state) { if (state) memset(state, 0, sizeof *state); }
int tube_buffering_tick(tube_buffering_t *state, uint64_t now_ms, int buffering) {
    if (!state) return 0;
    if (!state->started || now_ms < state->last_ms) {
        state->started = 1; state->window_ms = state->last_ms = now_ms; state->waited_ms = 0;
        return 0;
    }
    /* A frame gap longer than a second is a stopped clock, not a wait. */
    const uint64_t step = now_ms - state->last_ms;
    state->last_ms = now_ms;
    if (now_ms - state->window_ms > TUBE_BUFFERING_WINDOW_MS) { state->window_ms = now_ms; state->waited_ms = 0; }
    if (buffering && step <= 1000) state->waited_ms += (uint32_t)step;
    if (state->waited_ms < TUBE_BUFFERING_LIMIT_MS) return 0;
    state->window_ms = now_ms; state->waited_ms = 0;
    return 1;
}

/* ---- Video buffer. ---- */
double tube_buffer_seconds(int buffer, double base, int ring) {
    /* SmartTube's load control (ExoPlayerInitializer): 5 s, 30 s, 50 s and up to 100 s of media ahead. */
    static const double seconds[] = { 5.0, 30.0, 50.0, 100.0 };
    if (buffer == TUBE_BUFFER_MEDIUM || buffer < TUBE_BUFFER_LOW || buffer > TUBE_BUFFER_HIGHEST || !(base > 0)) return base;
    if (buffer == TUBE_BUFFER_LOW) return seconds[buffer] < base ? seconds[buffer] : base;   /* never deeper than Medium */
    return ring && seconds[buffer] > base ? seconds[buffer] : base;                          /* never shallower; the pool goes no deeper */
}
double tube_extra_read_ahead_seconds(double base, int ring) { return tube_buffer_seconds(prefs()->video_buffer, base, ring); }

/* ---- Video presets. ---- */
static const int preset_heights[] = { 2160, 1440, 1080, 720, 480, 360, 240, 144 };
/* SmartTube's table without its HDR rows: AVC up to 1080p, 60 fps from 360p. */
static int preset_walk(int wanted, int *height, int *fps, int *codec) {
    int index = 1;
    for (int h = 0; h < (int)(sizeof preset_heights / sizeof *preset_heights); ++h)
        for (int rate = 60; rate >= 30; rate -= 30) {
            if (rate == 60 && preset_heights[h] < 360) continue;
            for (int kind = TUBE_CODEC_AVC; kind <= TUBE_CODEC_AV1; ++kind) {
                if (kind == TUBE_CODEC_AVC && preset_heights[h] > 1080) continue;
                if (index == wanted) { *height = preset_heights[h]; *fps = rate; *codec = kind; }
                ++index;
            }
        }
    return index;
}
int tube_preset_count(void) { int h, f, c; return preset_walk(-1, &h, &f, &c); }
int tube_preset_at(int index, int *height, int *fps, int *codec) {
    if (!height || !fps || !codec || index < 0 || index >= tube_preset_count()) return 0;
    *height = 0; *fps = 0; *codec = TUBE_CODEC_AUTO;
    if (index) preset_walk(index, height, fps, codec);
    return 1;
}
int tube_preset_label(int index, char *out, size_t size) {
    int height, fps, codec;
    if (!out || !size) return 0;
    out[0] = 0;
    if (!tube_preset_at(index, &height, &fps, &codec)) return 0;
    if (!index) snprintf(out, size, "Disabled");
    else snprintf(out, size, "%dp %d fps %s", height, fps, codec == TUBE_CODEC_AVC ? "AVC" : codec == TUBE_CODEC_VP9 ? "VP9" : "AV1");
    return 1;
}
int tube_preset_find(int height, int fps, int codec) {
    if (!fps && codec == TUBE_CODEC_AUTO) return 0;
    for (int i = 1; i < tube_preset_count(); ++i) {
        int h, f, c;
        tube_preset_at(i, &h, &f, &c);
        if (h == height && f == fps && c == codec) return i;
    }
    return -1;
}
