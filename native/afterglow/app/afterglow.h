/* ProsperoTube Afterglow - what the app's own code sees of the interface.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The mount (mount.cpp) runs the Afterglow frame inside EVO's provider screen
 * for the YouTube provider. scripts/afterglow_overlay.py wires three places of
 * EVO to these functions: where the provider screen is drawn, where it takes
 * the pad, and where EVO's kit screens report what is on display. Everything
 * is called on the main thread. */
#ifndef TUBE_AFTERGLOW_H
#define TUBE_AFTERGLOW_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* 1 = the profile's Interface preference is Afterglow, or the file
 * afterglow.on exists in the app's data folder (the console session's
 * override; afterglow.off there keeps the classic browser whatever the
 * preference says). The files are looked at once a second. */
int tube_afterglow_enabled(void);
/* 1 = the interface drew the last frame of the provider screen: the pad is
 * its, and the classic browser underneath must not act. */
int tube_afterglow_shown(void);
/* The pad of this frame, in EVO's PadButtons bits. */
void tube_afterglow_input(uint32_t pressed, uint32_t held);
/* 1 = a video was chosen: its row, title, whether it is live and where to
 * resume. The caller hands them to the player the classic browser uses. */
int tube_afterglow_take_play(char *item, size_t item_size, char *title, size_t title_size, int *is_live, long long *resume_sec);
/* 1 = Circle was pressed on the last screen: the profile's rule for Circle on
 * the main menu applies. */
int tube_afterglow_take_exit(void);
/* Milliseconds of a monotonic clock, for that rule. */
uint64_t tube_afterglow_now_ms(void);
/* The interface's state as one JSON object (Frame::snapshot_json), "" while it
 * is not shown. Valid until the next call. */
const char *tube_afterglow_snapshot(void);
/* The provider screen closed: pictures and textures are given back. */
void tube_afterglow_close(void);

#ifdef __cplusplus
}
#endif
#endif
