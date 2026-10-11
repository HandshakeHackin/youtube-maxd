/* ProsperoTube native provider. Copyright (C) 2026 BlackBearReloaded.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_PROVIDER_H
#define TUBE_PROVIDER_H
#include "evo_provider.h"
#ifdef __cplusplus
extern "C" {
#endif
extern const evo_provider_t evo_provider_youtube;
void tube_poll(void);
unsigned tube_phone_revision(void);
/* Synchronize UI scope before callbacks can start a deferred phone launch. */
void tube_phone_sync_scope(void);
/* Copy the validated paired scope for native voice; never partial credentials. */
int tube_voice_scope(char *source, size_t source_capacity, char *token,
                     size_t token_capacity, char *profile, size_t profile_capacity);
int tube_phone_stop_on_disconnect(void);
int tube_phone_close_on_disconnect(void);
/* Current successful choice must still belong to this source and profile. */
int tube_phone_video(const char *url, char video[12]);
unsigned tube_phone_selection(void);
const char *tube_phone_caption_language(void);
/* Ephemeral tracks for the active source/profile/video. Main thread only. */
int tube_caption_track_count(void);
const char *tube_caption_track_language(int index);
const char *tube_caption_track_name(int index);
int tube_caption_track_automatic(int index);
unsigned tube_caption_revision(void);
/* Current validated source/profile/video selection; zero after scope retirement. */
unsigned tube_caption_selection(void);
/* 2=pending, 0=rejected. owner/sequence=0 for a local picker operation. */
int tube_caption_switch_begin(const char *language, unsigned owner, int sequence);
int tube_caption_secondary_begin(const char *language);
int tube_caption_switch_secondary(void);
const char *tube_caption_switch_language(void);
const char *tube_caption_secondary_language(void);
const char *tube_caption_secondary_media_path(void);
void tube_caption_secondary_clear(void);
/* 0=pending, 1=ready, -1=failed/retired. Only apply ready files on the main thread. */
int tube_caption_switch_status(const char **media_path);
void tube_caption_switch_finish(int applied);
void tube_caption_switch_cancel(void);
int tube_caption_switch_local(void);
/* Selected on-demand video speed in percent; main/UI thread only. */
int tube_playback_speed(void);
int tube_speed_count(void);
int tube_speed_at(int index);
int tube_set_playback_speed(int percent);
int tube_playback_speed_for(const char *video_id);
int tube_remember_playback_speed(const char *video_id, int percent);
int tube_last_playback_speed(void);
void tube_cancel(void);
void tube_auth_cancel(void);
unsigned tube_auth_revision(void);
unsigned tube_accounts_revision(void);
int tube_toggle_favorite(const char *id);
int tube_toggle_queue(const char *id);
int tube_toggle_subscription(const char *id);
const char *tube_caption_media_path(void);
double tube_skip_target(double position);
double tube_skip_target_at_rate(double position, double rate);
/* Explicit point jump; highlights never become automatic skip intervals. */
double tube_highlight_target(double position, double duration);
void tube_highlight_seek_failed(double target);
/* New explicit intent after the caller verifies idle seek/discard/prebuffer state. */
void tube_highlight_retry_idle(void);
const char *tube_sponsor_prompt(double position);
double tube_sponsor_confirm(double position, int accept);
int tube_sponsor_show_message(void);
/* Main/UI thread, after successful playback start; consumes at most once. */
const char *tube_take_paid_content_notice(void);
int tube_sponsor_finish(double target, double duration);
typedef struct { double start, end; unsigned rgb; int point; } tube_sponsor_marker_t;
int tube_sponsor_markers(double duration, tube_sponsor_marker_t *markers, int capacity);
double tube_chapter_target(double position, int direction);
int tube_playback_at_end(double position, double duration);
int tube_finish_drained_and_next(double position);
int tube_finish_and_next(double position, double duration);
int tube_take_next(evo_provider_item_t *item);
/* Explicit user navigation does not mark the current video watched or change
 * playback mode. status: 0=pending, 1=ready, -1=absent/failed/cancelled. */
int tube_manual_navigation_begin(int direction);
int tube_manual_navigation_status(void);
int tube_manual_navigation_pending(void);
/* Browser occurrence is one-shot and scoped to the current source/profile epoch.
 * Group keys are YouTube playlist IDs or explicit channel:UC... catalog keys. */
int tube_select_playlist_context(const char *id, const char *playlist, int index, unsigned scope);
int tube_playback_playlist_context(char *playlist, size_t capacity, int *index);
/* Bounded playlist WatchNext or channel Videos window. Main thread only.
 * begin: 1=started, 0=unavailable/busy. status: 0=pending, 1=ready, -1=retired/failed.
 * Internal directional traversal may return 2 for an unavailable-only scan step.
 * A window edge does not establish the end of the upstream playlist. */
typedef struct {
    char playlist[101], ids[128][12];
    int indices[128], count, current, playlist_size; /* size 0 = unknown */
    unsigned scope, selection;
} tube_playlist_window_t;
int tube_playlist_window_begin(void);
/* Locate a different occurrence while retaining the active playback owner. */
int tube_playlist_window_at(int index);
int tube_playlist_window_status(tube_playlist_window_t *out);
void tube_playlist_window_cancel(void);
int tube_playlist_eof_pending(void);
void tube_playlist_eof_cancel(void);
int tube_autoplay_enabled(void);
/* SmartTube mode identities. */
enum { TUBE_MODE_PAUSE=0, TUBE_MODE_CLOSE=1, TUBE_MODE_ALL=2, TUBE_MODE_ONE=3,
       TUBE_MODE_SHUFFLE=4, TUBE_MODE_LIST=5, TUBE_MODE_REVERSE=6 };
int tube_playback_mode(void);
int tube_queue_respects_playback_mode(void);
int tube_set_playback_mode(int mode);
enum { TUBE_COMPLETION_PAUSE=1, TUBE_COMPLETION_REPEAT=2 };
int tube_take_completion_action(void);
void tube_completion_playback_state(int ended,int busy);
int tube_sponsor_completed(void);
void tube_completion_seek_started(double target);
void tube_completion_seek_observe(double position,int busy);
int tube_choose_stream_enabled(void);
int tube_queue_recovery(double position, int live, int subtitles);
int tube_take_recovery(evo_provider_item_t *item, double *position, int *subtitles);
int tube_recovery_active(void);
int tube_recovery_pending(void);
int tube_recovery_choice(const evo_stream_choice_t *choices, int count);
void tube_recovery_finish(void);
const char *tube_comment_text(const char *id);
const char *tube_comment_rating(const char *id);
int tube_chat_due(const char *parent);
double tube_chat_reader_position(const char *parent);
int tube_chat_reader_seek(const char *parent, double position_sec);
/* Advance a stationary replay through remaining messages at the selected time. */
int tube_chat_reader_next(const char *parent);
int tube_sleep_minutes(void);
int tube_sleep_set_minutes(int minutes);
void tube_sleep_activity(void);
int tube_sleep_tick(int playback_active);
int tube_player_ok_mode(void);
int tube_player_set_ok_mode(int mode);
int tube_player_hide_ms(void);
int tube_player_set_hide_seconds(int seconds);
int tube_player_rotation_degrees(void);
int tube_player_mirror_enabled(void);
int tube_player_set_rotation_degrees(int degrees);
int tube_player_set_mirror_enabled(int enabled);
int tube_player_aspect_mode(void);
int tube_player_pixel_mode(void);
int tube_player_set_aspect_mode(int mode);
int tube_player_set_pixel_mode(int mode);
int tube_player_resize_mode(void);
int tube_player_zoom_percent(void);
int tube_player_set_resize_mode(int mode);
int tube_player_set_zoom_percent(int percent);
unsigned tube_player_video_geometry(void);
void tube_video_geometry_enable(int enabled);
int tube_player_time_mode(void);
int tube_player_set_time_mode(int mode);
int tube_player_seek_mode(void);
int tube_player_seek_seconds(void);
int tube_player_set_seek_seconds(int seconds);
int tube_player_set_seek_mode(int mode);
void tube_chat_stop(void);
/* Main/UI thread: request-driven live-chat sidebar, independent of controls fade. */
typedef struct { char author[128], message[1024]; } tube_player_chat_row_t;
void tube_player_chat_tick(int visible);
void tube_player_chat_tick_at(int visible, double position_sec);
const char *tube_player_chat_title(void);
int tube_player_chat_mode(void); /* active sidebar: 0 off, 1 left, 2 right */
int tube_player_chat_preference(void);
int tube_player_chat_set_mode(int mode);
int tube_player_chat_rows(const tube_player_chat_row_t **rows);
const char *tube_player_chat_status(void);
typedef struct { unsigned ticket; char label[64], initial[601]; } tube_playlist_prompt_t;
int tube_take_playlist_prompt(tube_playlist_prompt_t *prompt);
int tube_submit_playlist_name(unsigned ticket, const char *text, char *parent, size_t capacity);
typedef struct { unsigned ticket; char label[64], initial[801]; } tube_chat_prompt_t;
int tube_take_chat_prompt(tube_chat_prompt_t *prompt);
int tube_submit_chat_message(unsigned ticket, const char *text, char *parent, size_t capacity);
void tube_cancel_chat_prompt(unsigned ticket);
/* Current request's allowlisted unplayability; not transport/auth/engine failure. */
int tube_resolve_unavailable(const char *video_id);
int tube_queue_skip_unavailable(const char *video_id);
int tube_queue_retry_pending(void);
void tube_queue_retry_cancel(void);
void tube_set_error(const char *message);
#ifdef __cplusplus
}
#endif
#endif
