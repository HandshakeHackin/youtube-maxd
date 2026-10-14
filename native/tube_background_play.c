/* Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_background_play.h"
#include <stdio.h>
#include <string.h>
static unsigned owner;
static char profile_id[33], title_text[1024];
static int paused_state, resume_browser;
int tube_background_play_begin(unsigned selection,const char *profile) {
    if (!selection || !profile || !*profile || strlen(profile)>=sizeof profile_id) return 0;
    owner=selection;resume_browser=1;snprintf(profile_id,sizeof profile_id,"%s",profile);return 1;
}
int tube_background_play_active(void) { return owner!=0; }
int tube_background_play_take_browser_resume(void) { int resume=owner && resume_browser;resume_browser=0;return resume; }
int tube_background_play_matches(unsigned selection,const char *profile) {
    return owner && selection==owner && profile && !strcmp(profile_id,profile);
}
void tube_background_play_clear(void) { owner=0;profile_id[0]=title_text[0]=0;paused_state=resume_browser=0; }
void tube_background_play_status(const char *title,int paused) {
    snprintf(title_text,sizeof title_text,"%s",title?title:"");paused_state=paused!=0;
}
const char *tube_background_play_title(void) { return title_text; }
int tube_background_play_paused(void) { return paused_state; }
