/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MAXD_EXPERIENCE_H
#define MAXD_EXPERIENCE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
int tube_maxd_queue_current(void);
int tube_maxd_family_tick(long long now,int playing);
int tube_maxd_transcript_begin(void);
int tube_maxd_transcript_state(void); /* 0 loading, 1 ready, -1 unavailable */
int tube_maxd_transcript_count(void);
const char *tube_maxd_transcript_text(int index);
double tube_maxd_transcript_seconds(int index);
void tube_maxd_transcript_clear(void);
int tube_maxd_bookmark_add(unsigned selection,double seconds,const char *label);
int tube_maxd_bookmark_count(void);
const char *tube_maxd_bookmark_label(int index);
double tube_maxd_bookmark_seconds(int index);
int tube_maxd_bookmark_remove(int index);
#ifdef __cplusplus
}
#endif
#endif
