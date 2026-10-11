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
/* Startup preferences for the UI host; main thread, after provider init.
 * Section: a catalog id to open instead of the main menu ("" = main menu). */
int tube_select_account_on_boot(void);
const char *tube_boot_section(void);
const char *tube_boot_section_title(void);
/* Touchpad on a folder row: the catalog id of its section menu, "" for none.
 * The title names a channel or playlist the menu may pin. Main thread only;
 * the result is valid until the next call. */
const char *tube_folder_menu(const char *id, const char *title);
/* Catalog id that lists the last typed search again with the current options,
 * "" when this profile has not searched in the current browsing scope. */
const char *tube_search_folder(void);
int tube_phone_close_on_disconnect(void);
/* Current successful choice must still belong to this source and profile. */
int tube_phone_video(const char *url, char video[12]);
unsigned tube_phone_selection(void);
#ifndef TUBE_PHONE_NEXT_T
#define TUBE_PHONE_NEXT_T
/* Next video with its optional YouTube playlist occurrence (empty = none). */
typedef struct { char video[12], playlist[101]; int index; } tube_phone_next_t;
#endif
/* Remote-only asynchronous next metadata and verified suggestion traversal. */
int tube_phone_recommendation(unsigned owner,unsigned selection,const char *video,const char *playlist,int index,tube_phone_next_t *next);
void tube_phone_recommendation_cancel(void);
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
int tube_queue_play_next(const char *id);
int tube_mark_watched(const char *id);
int tube_remove_history(const char *id);
/* Browser occurrence is one-shot and scoped to the current source/profile epoch.
 * Group keys are YouTube playlist IDs or explicit channel:UC... catalog keys. */
int tube_select_playlist_context(const char *id, const char *playlist, int index, unsigned scope);
int tube_playback_playlist_context(char *playlist, size_t capacity, int *index);
/* Bounded playlist WatchNext or channel Videos window. Main thread only.
 * begin: 1=started, 0=unavailable/busy. status: 0=pending, 1=ready, -1=retired/failed.
 * Internal directional traversal may return 2 for an unavailable-only scan step.
 * A window edge does not establish the end of the upstream playlist. */
typedef struct {
    char playlist[101], ids[128][12], channels[128][25], channel_names[128][256];
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
/* The same prompt also takes a search inside a channel and profile passwords.
 * After a refused text: why, for the host's notice ("" = the playlist name
 * rule). After an accepted one: the title of the folder that opens. */
const char *tube_prompt_error(void);
const char *tube_prompt_title(const char *parent);
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
/* ---- In-player actions. Main/UI thread only. ----
 * The video actions menu and the lists behind it (related videos, queue,
 * playlist, description, links, confirmations), listed for the playing video
 * while it keeps playing. Rows stay valid until the next call. */
typedef struct {
    char id[EVO_PROVIDER_MAX_ITEM_ID], title[160], detail[160], playlist[101];
    int folder, index;
    unsigned scope;
} tube_player_menu_row_t;
enum { TUBE_PLAYER_ACTIONS = 0, TUBE_PLAYER_LIKE, TUBE_PLAYER_DISLIKE, TUBE_PLAYER_SUBSCRIBE, TUBE_PLAYER_PLAYLIST_ADD,
       TUBE_PLAYER_QUEUE, TUBE_PLAYER_SUGGESTIONS, TUBE_PLAYER_DESCRIPTION, TUBE_PLAYER_SHARE, TUBE_PLAYER_SPONSOR,
       TUBE_PLAYER_CHANNEL, TUBE_PLAYER_PLAYLIST, TUBE_PLAYER_ACTION_COUNT };
/* 1 = the actions menu offers this for the playing video and profile; the
 * label and detail are its row there. */
int tube_player_action(int action, char *label, size_t label_size, char *detail, size_t detail_size);
enum { TUBE_MENU_NOTHING = 0,   /* an inert row, or nothing to open */
       TUBE_MENU_OPENED = 1,    /* a list was requested: poll tube_player_menu_status() */
       TUBE_MENU_LEAVE = 2 };   /* a video or a channel was chosen: stop and return to the browser */
int tube_player_menu_begin(int action);
int tube_player_menu_open(int row);
int tube_player_menu_status(void);              /* 0 pending, 1 ready, -1 failed or closed */
int tube_player_menu_count(void);
const tube_player_menu_row_t *tube_player_menu_row(int index);
const char *tube_player_menu_title(void);
const char *tube_player_menu_message(void);     /* why the list failed; "" = the last reported error */
int tube_player_menu_root(void);                /* 1 = the list is the actions menu itself */
/* Ends the lists and puts the browser's own paging state back. */
void tube_player_menu_close(void);
int tube_player_menu_calling(void);
/* After playback stopped on a row's request: the video to start, or the
 * folder to open (1) or the search prompt to show (2) in the browser. */
int tube_player_launch_pending(void);
int tube_player_take_launch(evo_provider_item_t *item);
void tube_player_request_search(void);
int tube_player_take_browse(char *parent, size_t parent_size, char *title, size_t title_size);
/* Quality limits of the selected profile: height, frame rate (0 = any) and
 * codec (0 automatic, 1 AVC, 2 VP9, 3 AV1, 4 HEVC). A negative value keeps. */
void tube_player_quality(int *height, int *fps, int *codec);
int tube_player_set_quality(int height, int fps, int codec);
int tube_player_remember_height(int height);
/* Reopen the playing video at this position with the current limits. */
int tube_player_reload(double position, int live, int subtitles);
/* Error fixer (tube_player_extra.h): arms the bounded recovery for one error
 * and returns the TUBE_FIX_* action taken, 0 for none. While a reload asks
 * for another quality, the host takes its stream from tube_error_fix_choice(). */
int tube_error_fix_begin(int error, double position, int live, int subtitles);
int tube_error_fix_relaxed(void);
int tube_error_fix_choice(const evo_stream_choice_t *choices, int count);
#ifdef __cplusplus
}
#endif
#endif
