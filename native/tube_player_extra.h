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
    int buttons;             /* bit set of TUBE_BUTTON_*: action rows the in-player options show */
    int suggestions;         /* 0/1: related videos can be listed during playback */
    int chapter_notice;      /* 0/1: announce a chapter when playback enters it */
    int error_fixer;         /* 0/1: reload or lower the quality after playback errors */
    int remember_quality;    /* 0/1: a quality chosen in the player becomes the profile's maximum */
    int dim_timeout;         /* seconds without input before dimming while paused or browsing; 0 = never */
    int dim_amount;          /* percent of black while dimmed, 10..100 */
    int screen_off_timeout;  /* seconds without input before the screen goes off in the player; 0 = never */
    int screen_off_amount;   /* percent of black while the screen is off, 10..100 */
    int video_buffer;        /* TUBE_BUFFER_*: how far ahead of playback a stream is read */
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
    TUBE_EXTRA_SUGGESTIONS, TUBE_EXTRA_CHAPTER_NOTICE, TUBE_EXTRA_ERROR_FIXER,
    TUBE_EXTRA_VIDEO_BUFFER,
    TUBE_EXTRA_DIM_TIMEOUT, TUBE_EXTRA_DIM_AMOUNT, TUBE_EXTRA_SCREEN_OFF_TIMEOUT, TUBE_EXTRA_SCREEN_OFF_AMOUNT,
    TUBE_EXTRA_BUTTON_FIRST,       /* one switch per TUBE_BUTTON_*, in that order */
    TUBE_EXTRA_GROUPS = TUBE_EXTRA_BUTTON_FIRST + 21
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
       TUBE_KEYS_SKIP_ALL = 3, TUBE_KEYS_VOLUME = 4, TUBE_KEYS_SPEED = 5, TUBE_KEYS_SUGGESTIONS = 6 };
/* TUBE_KEY_ACTION_SUGGESTIONS belongs to Down alone and only while the
 * controls are visible, as SmartTube lists its rows under the controls. */
enum { TUBE_KEY_ACTION_DEFAULT = 0, TUBE_KEY_ACTION_SKIP = 1, TUBE_KEY_ACTION_VOLUME = 2, TUBE_KEY_ACTION_SPEED = 3,
       TUBE_KEY_ACTION_SUGGESTIONS = 4 };
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
/* A bounded snapshot of the active resolve result; empty when unavailable or stopped. */
const char *tube_playing_fact(const char *name);
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

/* Player buttons: which action rows the in-player options show. SmartTube's
 * button flags, for the rows that exist here. */
enum { TUBE_BUTTON_LIKE, TUBE_BUTTON_DISLIKE, TUBE_BUTTON_SUBSCRIBE, TUBE_BUTTON_PLAYLIST_ADD, TUBE_BUTTON_QUEUE,
       TUBE_BUTTON_CHANNEL, TUBE_BUTTON_SEARCH, TUBE_BUTTON_VIDEO_INFO, TUBE_BUTTON_SHARE, TUBE_BUTTON_CONTENT_BLOCK,
       TUBE_BUTTON_CHAT, TUBE_BUTTON_SCREEN_OFF, TUBE_BUTTON_SOUND, TUBE_BUTTON_SPEED, TUBE_BUTTON_ZOOM,
       TUBE_BUTTON_REPEAT, TUBE_BUTTON_NEXT, TUBE_BUTTON_PREVIOUS, TUBE_BUTTON_SEEK_INTERVAL, TUBE_BUTTON_ROTATE,
       TUBE_BUTTON_FLIP, TUBE_BUTTON_COUNT };
/* Rows the player already had stay shown; of the new ones SmartTube hides the
 * queue, the share link and SponsorBlock until they are switched on. */
#define TUBE_BUTTONS_DEFAULT (((1 << TUBE_BUTTON_COUNT) - 1) & \
    ~((1 << TUBE_BUTTON_QUEUE) | (1 << TUBE_BUTTON_SHARE) | (1 << TUBE_BUTTON_CONTENT_BLOCK)))
int tube_extra_buttons(void);
int tube_extra_set_buttons(int mask);
int tube_extra_button(int button);               /* 1 = its rows are shown */
int tube_extra_suggestions(void);
int tube_extra_set_suggestions(int enabled);
int tube_extra_chapter_notice_enabled(void);
int tube_extra_set_chapter_notice(int enabled);
int tube_extra_error_fixer(void);
int tube_extra_set_error_fixer(int enabled);
int tube_extra_remember_quality(void);
int tube_extra_set_remember_quality(int enabled);
int tube_extra_dim_timeout(void);
int tube_extra_set_dim_timeout(int seconds);
int tube_extra_dim_amount(void);
int tube_extra_set_dim_amount(int percent);
int tube_extra_screen_off_timeout(void);
int tube_extra_set_screen_off_timeout(int seconds);
int tube_extra_screen_off_amount(void);
int tube_extra_set_screen_off_amount(int percent);
/* 0 = this group is not a row of the in-player options: a button switch, or a
 * preference whose button is switched off. */
int tube_extra_group_in_player(int group);
/* Settings screen a group belongs to: 0 captions, 1 sound, 2 player, 3 player
 * buttons, 4 screen dimming. */
int tube_extra_group_screen(int group);
/* One-shot request from the player screen for the list the options open on. */
enum { TUBE_PICKER_OPTIONS = 0, TUBE_PICKER_SUGGESTIONS = 1 };
void tube_extra_picker_request(int list);
int tube_extra_take_picker_request(void);

/* Chapter notification: the index of the chapter to announce, once, when
 * playback enters it with the controls hidden; -1 = nothing to announce. */
int tube_chapter_notice_decide(int enabled, int announced, int current, int busy, int controls_visible);
int tube_extra_chapter_notice(double position, int busy, int controls_visible);

/* Screen dimming and screen off. One idle clock restarted by every input:
 * the picture dims while paused or browsing, and goes off in the player after
 * its own timeout or at once on request. Any input brings it back. */
enum { TUBE_DIM_NONE = 0,      /* a screen that never dims: text entry, sign-in, pickers */
       TUBE_DIM_BROWSING = 1, TUBE_DIM_PAUSED = 2, TUBE_DIM_PLAYING = 3 };
/* Percent of black, 0..100. */
int tube_dim_decide(const TubePlayerExtra *prefs, int context, uint64_t idle_ms, int screen_off);
void tube_extra_dim_reset(uint64_t now_ms);
/* One frame of input. 1 = the press only brought the picture back and must
 * not reach the screen. While the screen is off, Left and Right alone keep it
 * off so that seeking stays possible, as in SmartTube. */
int tube_extra_dim_input(uint64_t now_ms, int pressed, int seek_only);
int tube_extra_dim_tick(uint64_t now_ms, int context);
int tube_extra_dim_percent(void);                /* the last tick's result */
void tube_extra_screen_off(void);              /* until the next input; the player only */
int tube_extra_screen_is_off(void);
/* Black at the given percentage over straight-alpha 0xAARRGGBB pixels. */
void tube_dim_pixels(uint32_t *pixels, size_t count, int percent);

/* Error fixer: SmartTube's recovery ladder as one decision. */
enum { TUBE_CODEC_AUTO = 0, TUBE_CODEC_AVC = 1, TUBE_CODEC_VP9 = 2, TUBE_CODEC_AV1 = 3, TUBE_CODEC_HEVC = 4 };
enum { TUBE_ERROR_SOURCE = 0, TUBE_ERROR_VIDEO_DECODER = 1, TUBE_ERROR_AUDIO_DECODER = 2, TUBE_ERROR_BUFFERING = 3 };
enum { TUBE_FIX_NONE = 0,          /* the player's own handling applies */
       TUBE_FIX_RELOAD = 1,        /* the same format again */
       TUBE_FIX_LOWER_QUALITY = 2, /* the next lower resolution */
       TUBE_FIX_SAFE_VIDEO = 3 };  /* AVC at up to 1080p30 after a failed video decoder */
#define TUBE_FIX_ACTION 0xff
#define TUBE_FIX_SUBTITLES_OFF 0x100   /* added to an action: captions are switched off first */
#define TUBE_FIX_ATTEMPTS 3
typedef struct { int enabled, error, live, subtitles, height, fps, codec, attempts; } tube_fix_input;
int tube_error_fix_decide(const tube_fix_input *input);
/* The quality a fix asks for. 1 = it differs from the one given. */
int tube_error_fix_quality(int action, int *height, int *fps, int *codec);
int tube_error_fix_lower_height(int height);     /* 0 = nothing lower is offered */
const char *tube_error_fix_message(int error, int action);
/* Long buffering: 20 s of waiting inside a 60 s window. */
#define TUBE_BUFFERING_LIMIT_MS 20000
#define TUBE_BUFFERING_WINDOW_MS 60000
typedef struct { uint64_t window_ms, last_ms; uint32_t waited_ms; int started; } tube_buffering_t;
void tube_buffering_reset(tube_buffering_t *state);
/* 1 = the limit was reached now; the detector starts over. */
int tube_buffering_tick(tube_buffering_t *state, uint64_t now_ms, int buffering);

/* Video buffer: SmartTube's four choices as the depth of the demuxer's
 * read-ahead on a network stream, in seconds of media kept ahead of playback.
 * Medium is the player's own depth, whatever that is, so the default changes
 * nothing. The depth is read when a stream opens: a change applies from the
 * next video. */
enum { TUBE_BUFFER_LOW = 0, TUBE_BUFFER_MEDIUM = 1, TUBE_BUFFER_HIGH = 2, TUBE_BUFFER_HIGHEST = 3 };
int tube_extra_buffer(void);
int tube_extra_set_buffer(int buffer);
/* `base`: the player's depth for the packet store in use. `ring` = 0 when the
 * queued packets live in the flexible memory pool, whose ceilings hold no more
 * than `base`: a choice can shorten the read-ahead there, never deepen it. */
double tube_buffer_seconds(int buffer, double base, int ring);
double tube_extra_read_ahead_seconds(double base, int ring);

/* Video presets: one resolution, frame rate and codec at a time. Entry 0
 * switches presets off (any frame rate and codec; height 0 = keep the limit). */
int tube_preset_count(void);
int tube_preset_at(int index, int *height, int *fps, int *codec);
int tube_preset_label(int index, char *out, size_t size);
int tube_preset_find(int height, int fps, int codec);   /* -1 = not a preset */
#ifdef __cplusplus
}
#endif
#endif
