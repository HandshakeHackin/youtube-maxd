/* ProsperoTube Afterglow - interface sound and vibration, as the app's own code sees them.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The Afterglow interface plays its cues on an AudioOut port of its own and
 * vibrates the pad the app opened (native/afterglow/sound). The app tells it
 * three things: which pad, when it parks, when it exits; and whoever keeps the
 * profile's preferences tells it what Settings > Interface > Sounds says.
 * Everything is called on the main thread; nothing blocks except the exit. */
#ifndef TUBE_AFTERGLOW_SOUND_H
#define TUBE_AFTERGLOW_SOUND_H
#ifdef __cplusplus
extern "C" {
#endif

/* Settings > Interface > Sounds (docs/afterglow/sound-design.md, section 6). */
typedef struct TubeAfterglowSound {
    int sounds;         /* Interface sounds: 0 off, 1 on */
    int volume;         /* Interface volume: 0..10 */
    int playback;       /* Sounds during playback: 0 all, 1 notices only, 2 off */
    int quiet_from;     /* Quiet hours: minutes after midnight, -1 for none */
    int quiet_until;
    int vibration;      /* Vibration: 0 off, 1 on */
    int reduced_motion; /* Reduce motion: 0 off, 1 on */
} TubeAfterglowSound;

/* The preferences of the profile in use; call when they are read and whenever
 * one changes. Until the first call the defaults of the design apply: on,
 * volume 7, notices only, no quiet hours, vibration on. */
void tube_afterglow_sound_options(const TubeAfterglowSound *options);
/* The handle of the pad the app opened (scePadOpen), negative for none. */
void tube_afterglow_sound_pad(int pad_handle);
/* The app parks (soft close): the parting sound, then the port is given back
 * and vibration stops, within a second and without further calls. */
void tube_afterglow_sound_park(void);
/* The app exits: everything stops now. Waits for the audio thread, a few
 * milliseconds. */
void tube_afterglow_sound_close(void);

#ifdef __cplusplus
}
#endif
#endif
