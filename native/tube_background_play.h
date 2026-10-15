/* Main-thread continuation of the current YouTube player while Afterglow browses.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_BACKGROUND_PLAY_H
#define TUBE_BACKGROUND_PLAY_H
#ifdef __cplusplus
extern "C" {
#endif
int tube_background_play_begin(unsigned selection,const char *profile);
int tube_background_play_active(void);
int tube_background_play_take_browser_resume(void);
int tube_background_play_matches(unsigned selection,const char *profile);
void tube_background_play_clear(void);
/* Only a pause issued by hidden EOF may be released automatically on return. */
void tube_background_play_end_pause(int paused);
int tube_background_play_take_end_pause(void);
void tube_background_play_status(const char *title,int paused);
const char *tube_background_play_title(void);
int tube_background_play_paused(void);
#ifdef __cplusplus
}
#endif
#endif
