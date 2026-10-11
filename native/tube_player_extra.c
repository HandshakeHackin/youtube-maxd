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
       F_LOOP_SHORTS, F_REMEMBER_SHORT, F_COUNT };
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
    FIELD(keys_ud, TUBE_KEYS_DEFAULT, 0, TUBE_KEYS_SPEED, 1, NO_ALT),
    FIELD(seek_preview, 0, 0, 3, 1, NO_ALT),
    FIELD(speed_list, 1, 0, 2, 1, NO_ALT),
    FIELD(loop_shorts, 0, 0, 1, 1, NO_ALT),
    FIELD(remember_short, 1, 0, 1, 1, NO_ALT),
};

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
    default: return TUBE_KEY_ACTION_DEFAULT;
    }
}
int tube_extra_dpad_action(int vertical, int controls_hidden) {
    return tube_extra_key_action(vertical ? prefs()->keys_ud : prefs()->keys_lr, g_short, controls_hidden);
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
    g_has_loudness = 0; g_loudness = 0; g_short = 0;
    g_chapter_count = 0; g_storyboard_count = 0;
    g_stream_height = 0; g_stream_fps = 0; g_stream_bitrate = 0; g_stream_codec[0] = 0;
    g_exit_armed = 0;
    ++g_revision;
}

void tube_extra_video_begin(const cJSON *resolve_result) {
    tube_extra_video_end();
    if (!cJSON_IsObject(resolve_result)) return;
    const cJSON *loudness = cJSON_GetObjectItemCaseSensitive(resolve_result, "loudness_db");
    tube_extra_set_loudness(cJSON_IsNumber(loudness), cJSON_IsNumber(loudness) ? loudness->valuedouble : 0);
    tube_extra_set_short(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(resolve_result, "is_short")));
    tube_extra_set_chapters(cJSON_GetObjectItemCaseSensitive(resolve_result, "chapters"));
    tube_extra_set_storyboards(cJSON_GetObjectItemCaseSensitive(resolve_result, "storyboards"));
}

/* ---- Preference groups for the settings screen and the in-player picker. ---- */
enum { G_LABELS, G_SWITCH, G_SCALE, G_POSITION, G_VOLUME, G_DELAY };
typedef struct {
    const char *id, *title, *detail;
    int kind, field, count;       /* field: index into fields, or -1 for mute */
    const char *const *labels;
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
    "Previous / next for Shorts and videos", "Volume up / down", "Speed up / down" };
static const char *const preview_labels[] = { "Disabled", "Single frame", "Carousel (slow)", "Carousel (fast)" };
static const char *const speed_list_labels[] = { "Short speed list", "Long speed list", "Extra long speed list" };
static const int delay_values[] = { -2000, -1500, -1000, -750, -500, -400, -300, -250, -200, -150, -100, -75, -50, -25,
                                    0, 25, 50, 75, 100, 150, 200, 250, 300, 400, 500, 750, 1000, 1500, 2000 };
#define DELAY_COUNT ((int)(sizeof delay_values / sizeof *delay_values))
static const group_t groups[TUBE_EXTRA_GROUPS] = {
    { "settings:subtitle-style", "Subtitle style", "Text colour, background and edge of captions", G_LABELS, F_SUBTITLE_STYLE, 7, style_labels },
    { "settings:subtitle-size", "Subtitle size", "Caption text scale", G_SCALE, F_SUBTITLE_SCALE, 20, NULL },
    { "settings:subtitle-position", "Subtitle position", "Height of captions above the bottom edge", G_POSITION, F_SUBTITLE_POSITION, 42, NULL },
    { "settings:player-volume", "Player volume", "Above 100% the sound is boosted and may distort", G_VOLUME, F_VOLUME, 61, NULL },
    { "settings:player-mute", "Mute", "Silence the player until switched back", G_SWITCH, -1, 2, NULL },
    { "settings:auto-volume", "Auto volume", "Turn loud videos down to YouTube's reference level", G_SWITCH, F_AUTO_VOLUME, 2, NULL },
    { "settings:audio-delay", "Audio delay", "Shift the sound against the picture", G_DELAY, F_AUDIO_DELAY_MS, DELAY_COUNT, NULL },
    { "settings:audio-delay-enabled", "Use audio delay", "Keep the chosen delay but switch it on or off", G_SWITCH, F_AUDIO_DELAY_ENABLED, 2, NULL },
    { "settings:player-clock", "Clock in controls", "Show the time of day in the controls bar", G_SWITCH, F_CLOCK, 2, NULL },
    { "settings:quality-info", "Quality info in controls", "Show resolution, frame rate and codec in the controls bar", G_SWITCH, F_QUALITY_INFO, 2, NULL },
    { "settings:quality-bitrate", "Bitrate in quality info", "Add the stream bitrate to the quality info", G_SWITCH, F_QUALITY_BITRATE, 2, NULL },
    { "settings:time-format", "Time format", "Clock and estimated finish time", G_LABELS, F_CLOCK_12H, 2, time_labels },
    { "settings:player-exit", "Exit from the player", "What Circle does during playback", G_LABELS, F_EXIT_MODE, 3, exit_labels },
    { "settings:keys-left-right", "Left / Right", "Quick skip works while the controls are hidden", G_LABELS, F_KEYS_LR, 5, keys_lr_labels },
    { "settings:keys-up-down", "Up / Down", "Quick skip works while the controls are hidden", G_LABELS, F_KEYS_UD, 6, keys_ud_labels },
    { "settings:seek-preview", "Preview while seeking", "Thumbnails from YouTube storyboards", G_LABELS, F_SEEK_PREVIEW, 4, preview_labels },
    { "settings:speed-list", "Speed list", "Which speeds the speed picker offers", G_LABELS, F_SPEED_LIST, 3, speed_list_labels },
    { "settings:loop-shorts", "Loop Shorts", "Repeat a Short until another video is chosen", G_SWITCH, F_LOOP_SHORTS, 2, NULL },
    { "settings:remember-short-position", "Remember position of short videos", "Off: videos under six minutes start from the beginning", G_SWITCH, F_REMEMBER_SHORT, 2, NULL },
};

static const group_t *group_at(int group) { return group >= 0 && group < TUBE_EXTRA_GROUPS ? &groups[group] : NULL; }
static int group_value(const group_t *group, int choice) {
    switch (group->kind) {
    case G_SCALE: return 10 + 10 * choice;
    case G_POSITION: return choice ? (choice - 1) * 25 : -1;
    case G_VOLUME: return choice * 5;
    case G_DELAY: return delay_values[choice];
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
    case G_VOLUME: snprintf(out, size, "%d%%", value); break;
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
    return set_field(entry->field, value);
}
