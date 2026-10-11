/* Normal-speed audio rate for host tests that link evo_playback.c without the
 * audio output; the first presented frame copies it into the playback clock.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later
 */
double evo_audio_rate(void);
double evo_audio_rate(void) { return 1.0; }
