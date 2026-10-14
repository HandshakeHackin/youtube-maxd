# Original ProsperoTube interface sounds

Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later.

All 34 WAV assets are original procedural synthesis using Python's standard
`math`, `struct` and `wave` modules. No sampled or recorded instruments,
downloads or external generation services were used. The voice uses soft
attacks, a decaying fundamental and two quieter glass-like partials, with
notes from D minor pentatonic. Short repeats have no trailing sustain.

Run `python3 scripts/generate_afterglow_sounds.py` to reproduce them, or add
`--check` to compare every byte with the committed set. The manifest records
SHA-256, duration, RMS target and measured PCM levels. Assets are mono 48 kHz
16-bit PCM; the existing mixer supplies stereo placement and variation.
`--sampler build/afterglow/sound-sampler.wav` creates a labeled JSON timeline
beside a sampler with 350 ms of silence between cues, in cue-sheet order.
The sampler presents source levels before volume preferences and playback's
existing 6 dB attenuation. Hearing comfort and console output need owner review.
