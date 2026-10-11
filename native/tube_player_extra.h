/* ProsperoTube per-profile player preferences that live outside the provider:
 * caption look, volume, audio delay, controls-bar labels, exit and D-pad
 * behaviour, seek preview, speed lists and Shorts rules, plus the per-video
 * facts (loudness, Shorts flag, chapters, storyboards) the player needs.
 * Main/UI thread only; the audio thread never calls into this module.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_PLAYER_EXTRA_H
#define TUBE_PLAYER_EXTRA_H
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include "tube_storyboard.h"
#ifdef __cplusplus
extern "C" {
#endif

struct cJSON;

/* One profile's preferences. Defaults reproduce the player without this module. */
typedef struct {
    int subtitle_style;      /* 0 = player default, 1..6 = SmartTube presets */
    int subtitle_scale;      /* percent, 10..200 step 10 */
    int subtitle_position;   /* -1 = player default, else 0..1000 per mille from the bottom, step 25 */
    int volume;              /* percent, 0..300 step 5 */
    int auto_volume;         /* 0/1: attenuate loud videos by their loudness value */
    int audio_delay_ms;      /* -2000..2000; positive = audio later */
    int audio_delay_enabled; /* 0/1 */
    int clock;               /* 0/1: wall clock in the controls bar */
    int quality_info;        /* 0/1: resolution, frame rate and codec in the controls bar */
    int quality_bitrate;     /* 0/1: add the bitrate to the quality label */
    int clock_12h;           /* 0 = 24-hour, 1 = 12-hour */
    int exit_mode;           /* TUBE_EXIT_MODE_* */
    int keys_lr;             /* TUBE_KEYS_* for Left/Right */
    int keys_ud;             /* TUBE_KEYS_* for Up/Down */
    int seek_preview;        /* 0 off, 1 single frame, 2 carousel (slow), 3 carousel (fast) */
    int speed_list;          /* 0 short, 1 long, 2 extra long */
    int loop_shorts;         /* 0/1 */
    int remember_short;      /* 0/1: resume videos shorter than six minutes */
} TubePlayerExtra;

void tube_player_extra_defaults(TubePlayerExtra *prefs);
/* Replace every out-of-range field with its default; returns how many changed. */
int tube_player_extra_validate(TubePlayerExtra *prefs);
/* One profile's JSON object. Missing, corrupt or out-of-range fields read as defaults. */
TubePlayerExtra tube_player_extra_read(const struct cJSON *object);
/* Add or replace this module's fields in a profile object. 1 = written. */
int tube_player_extra_write(struct cJSON *object, const TubePlayerExtra *prefs);
/* Make a profile's preferences current (NULL = defaults). Does not call the save hook. */
void tube_player_extra_select(const TubePlayerExtra *prefs);
const TubePlayerExtra *tube_player_extra_current(void);
/* Called after every accepted change; a zero result reports a failed save. */
typedef int (*tube_player_extra_save_fn)(const TubePlayerExtra *prefs, void *user);
void tube_player_extra_set_save_hook(tube_player_extra_save_fn save, void *user);
/* Changes with every accepted setter, selection or per-video update. */
unsigned tube_extra_revision(void);

/* Getters and validating setters (1 = accepted and saved, 0 = rejected or not saved). */
int tube_extra_subtitle_style_index(void);
int tube_extra_set_subtitle_style(int style);
int tube_extra_subtitle_scale(void);
int tube_extra_set_subtitle_scale(int percent);
int tube_extra_subtitle_position(void);
int tube_extra_set_subtitle_position(int per_mille);
int tube_extra_volume(void);
int tube_extra_set_volume(int percent);
int tube_extra_auto_volume(void);
int tube_extra_set_auto_volume(int enabled);
int tube_extra_audio_delay_ms(void);          /* stored value */
int tube_extra_set_audio_delay_ms(int ms);    /* also enables (ms != 0) or disables */
int tube_extra_audio_delay_enabled(void);
int tube_extra_set_audio_delay_enabled(int enabled);
int tube_extra_clock(void);
int tube_extra_set_clock(int enabled);
int tube_extra_quality_info(void);
int tube_extra_set_quality_info(int enabled);
int tube_extra_quality_bitrate(void);
int tube_extra_set_quality_bitrate(int enabled);
int tube_extra_clock_12h(void);
int tube_extra_set_clock_12h(int enabled);
int tube_extra_exit_mode(void);
int tube_extra_set_exit_mode(int mode);
int tube_extra_keys_lr(void);
int tube_extra_set_keys_lr(int mode);
int tube_extra_keys_ud(void);
int tube_extra_set_keys_ud(int mode);
int tube_extra_seek_preview(void);
int tube_extra_set_seek_preview(int mode);
int tube_extra_speed_list(void);
int tube_extra_set_speed_list(int mode);
int tube_extra_loop_shorts(void);
int tube_extra_set_loop_shorts(int enabled);
int tube_extra_remember_short(void);
int tube_extra_set_remember_short(int enabled);
/* Mute belongs to the running player, not to the saved profile. */
int tube_extra_muted(void);
void tube_extra_set_muted(int muted);

/* Preference groups: one table for the settings screen and the in-player
 * picker. A switch is a group with the two choices Off and On. */
enum {
    TUBE_EXTRA_SUBTITLE_STYLE, TUBE_EXTRA_SUBTITLE_SIZE, TUBE_EXTRA_SUBTITLE_POSITION,
    TUBE_EXTRA_VOLUME, TUBE_EXTRA_MUTE, TUBE_EXTRA_AUTO_VOLUME,
    TUBE_EXTRA_AUDIO_DELAY, TUBE_EXTRA_AUDIO_DELAY_ENABLED,
    TUBE_EXTRA_CLOCK, TUBE_EXTRA_QUALITY_INFO, TUBE_EXTRA_QUALITY_BITRATE, TUBE_EXTRA_TIME_FORMAT,
    TUBE_EXTRA_EXIT, TUBE_EXTRA_KEYS_LR, TUBE_EXTRA_KEYS_UD, TUBE_EXTRA_SEEK_PREVIEW,
    TUBE_EXTRA_SPEED_LIST, TUBE_EXTRA_LOOP_SHORTS, TUBE_EXTRA_REMEMBER_SHORT,
    TUBE_EXTRA_GROUPS
};
const char *tube_extra_group_id(int group);     /* "settings:subtitle-style", ... */
const char *tube_extra_group_title(int group);
const char *tube_extra_group_detail(int group);
int tube_extra_group_count(int group);
int tube_extra_group_label(int group, int choice, char *out, size_t size);
int tube_extra_group_selected(int group);       /* -1 when the stored value is not listed */
int tube_extra_group_select(int group, int choice);
int tube_extra_group_find(const char *id);      /* -1 = unknown */

/* Captions. */
enum { TUBE_SUBTITLE_EDGE_NONE = 0, TUBE_SUBTITLE_EDGE_OUTLINE = 1, TUBE_SUBTITLE_EDGE_SHADOW = 2 };
typedef struct {
    int custom;               /* 0 = draw the player's own caption plate */
    unsigned text_rgb, background_rgb;
    int background_alpha;     /* 0..255 */
    int edge;                 /* TUBE_SUBTITLE_EDGE_* */
} tube_subtitle_style_t;
tube_subtitle_style_t tube_subtitle_style_preset(int style);
tube_subtitle_style_t tube_extra_subtitle_style(void);
/* Baseline of the last caption line for a position in per mille from the
 * bottom, kept inside the screen for a block of the given height. */
double tube_subtitle_baseline(int per_mille, double screen_height, double block_height, double line_height);

/* Volume: linear gain in thousandths applied after the phone volume. */
int tube_extra_gain_for(const TubePlayerExtra *prefs, int muted, int has_loudness, double loudness_db);
int tube_extra_gain_milli(void);
/* Audio delay actually in effect (0 while disabled). */
int tube_extra_effective_audio_delay_ms(void);

/* Controls-bar labels. */
void tube_extra_set_stream_info(int height, double fps, const char *codec, long long bitrate_bps);
int tube_extra_quality_text(char *out, size_t size, int height, double fps, const char *codec,
                            long long bitrate_bps, int with_bitrate);
/* Quality label and clock by preference; empty when both are off. */
int tube_extra_controls_info(char *out, size_t size, time_t now);
/* Flag for tube_time_display(): TUBE_TIME_12H or 0. */
int tube_extra_time_flags(void);

/* Exit from the player with Circle. */
enum { TUBE_EXIT_MODE_ASK = 0, TUBE_EXIT_MODE_DOUBLE = 1, TUBE_EXIT_MODE_SINGLE = 2 };
enum { TUBE_EXIT_ASK = 0, TUBE_EXIT_NOW = 1, TUBE_EXIT_ARMED = 2 };
#define TUBE_EXIT_DOUBLE_MS 2000
int tube_extra_exit_decide(int mode, uint64_t now_ms, uint64_t *armed_ms);
int tube_extra_exit_press(uint64_t now_ms);
void tube_extra_exit_reset(void);

/* D-pad behaviour. */
enum { TUBE_KEYS_DEFAULT = 0, TUBE_KEYS_SKIP_SHORTS = 1, TUBE_KEYS_SKIP_VIDEOS = 2,
       TUBE_KEYS_SKIP_ALL = 3, TUBE_KEYS_VOLUME = 4, TUBE_KEYS_SPEED = 5 };
enum { TUBE_KEY_ACTION_DEFAULT = 0, TUBE_KEY_ACTION_SKIP = 1, TUBE_KEY_ACTION_VOLUME = 2, TUBE_KEY_ACTION_SPEED = 3 };
/* Quick skip applies only while the controls are hidden, as in SmartTube. */
int tube_extra_key_action(int mode, int is_short, int controls_hidden);
int tube_extra_dpad_action(int vertical, int controls_hidden);
/* Step the saved volume by one 5 % notch; returns the new percentage. */
int tube_extra_volume_step(int direction);

/* Speed lists in percent. */
int tube_speed_list_count(int list);
int tube_speed_list_at(int list, int index);
int tube_extra_speed_count(void);
int tube_extra_speed_at(int index);
int tube_extra_speed_valid(int percent);
/* Next slower (-1) or faster (+1) entry of the current list; the input at an end. */
int tube_extra_speed_step(int percent, int direction);

/* Shorts and short videos. */
#define TUBE_SHORT_VIDEO_SECONDS 360.0
int tube_extra_should_loop(int loop_shorts, int is_short);
int tube_extra_resume_decision(int remember_short, double duration_seconds);
int tube_extra_loop_current(void);
int tube_extra_resume_allowed(double duration_seconds);

/* Per-video facts from a resolve result. tube_extra_video_begin() reads
 * loudness_db, is_short, chapters and storyboards from the result object;
 * tube_extra_video_end() forgets them and the stream info. Mute stays until
 * it is switched back. */
void tube_extra_video_begin(const struct cJSON *resolve_result);
void tube_extra_video_end(void);
void tube_extra_set_loudness(int has_value, double loudness_db);
void tube_extra_set_short(int is_short);
int tube_extra_is_short(void);
#define TUBE_EXTRA_CHAPTERS 128
#define TUBE_EXTRA_CHAPTER_TITLE 385   /* 96 UTF-8 characters and the terminator */
/* Chapters sorted by start; invalid rows are dropped. Returns the count kept. */
int tube_extra_set_chapters(const struct cJSON *chapters);
int tube_extra_set_chapter_list(const double *starts, const char *const *titles, int count);
int tube_extra_chapter_count(void);
double tube_extra_chapter_start(int index);
const char *tube_extra_chapter_title(int index);
int tube_extra_chapter_at(double position);            /* -1 before the first chapter */
const char *tube_extra_chapter_title_at(double position); /* "" when untitled or none */
int tube_extra_chapter_label(int index, char *out, size_t size);
/* Seek-preview storyboards (at most four levels). Returns the count kept. */
int tube_extra_set_storyboards(const struct cJSON *storyboards);
int tube_extra_storyboard_count(void);
const tube_storyboard_level *tube_extra_storyboard(int index);
/* Tile for a seek target when the preference allows a preview. 1 = found;
 * url receives the sheet address. */
int tube_extra_seek_preview_tile(double seconds, double duration, int target_width, int target_height,
                                 tube_storyboard_tile *tile, char *url, size_t url_size);
#ifdef __cplusplus
}
#endif
#endif
