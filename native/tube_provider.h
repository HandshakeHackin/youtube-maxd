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
int tube_toggle_favorite(const char *id);
int tube_toggle_queue(const char *id);
int tube_toggle_subscription(const char *id);
const char *tube_caption_media_path(void);
double tube_skip_target(double position);
double tube_chapter_target(double position, int direction);
int tube_finish_and_next(void);
int tube_take_next(evo_provider_item_t *item);
int tube_autoplay_enabled(void);
int tube_choose_stream_enabled(void);
void tube_set_error(const char *message);
#ifdef __cplusplus
}
#endif
#endif
