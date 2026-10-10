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
void tube_cancel(void);
void tube_auth_cancel(void);
unsigned tube_auth_revision(void);
unsigned tube_accounts_revision(void);
int tube_toggle_favorite(const char *id);
int tube_toggle_queue(const char *id);
int tube_toggle_subscription(const char *id);
const char *tube_caption_media_path(void);
double tube_skip_target(double position);
const char *tube_sponsor_prompt(double position);
double tube_sponsor_confirm(double position, int accept);
int tube_sponsor_show_message(void);
double tube_chapter_target(double position, int direction);
int tube_playback_at_end(double position, double duration);
int tube_finish_drained_and_next(double position);
int tube_finish_and_next(double position, double duration);
int tube_take_next(evo_provider_item_t *item);
int tube_autoplay_enabled(void);
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
void tube_chat_stop(void);
typedef struct { unsigned ticket; char label[64], initial[601]; } tube_playlist_prompt_t;
int tube_take_playlist_prompt(tube_playlist_prompt_t *prompt);
int tube_submit_playlist_name(unsigned ticket, const char *text, char *parent, size_t capacity);
typedef struct { unsigned ticket; char label[64], initial[801]; } tube_chat_prompt_t;
int tube_take_chat_prompt(tube_chat_prompt_t *prompt);
int tube_submit_chat_message(unsigned ticket, const char *text, char *parent, size_t capacity);
void tube_cancel_chat_prompt(unsigned ticket);
void tube_set_error(const char *message);
#ifdef __cplusplus
}
#endif
#endif
