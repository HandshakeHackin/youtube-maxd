# YouTube Max’d 1.2.0 — first standalone preview

The first YouTube Max’d preview combines a standalone PS5 YouTube client with local voice search, Smart playlists, Auto Max quality and the approved red-to-white Max’d identity. Intended setup: firmware 11.00 with ShadowMount+.

## Added

- Local multilingual voice search: hold L3, speak after Listening, then release. Whisper recognition runs on the console; Circle cancels and capture stops after six seconds.
- Profile-owned Smart playlists with Home, Subscriptions, Favorites or Play queue sources, plus title, creator, duration, unwatched and live filters.
- Optional Auto Max quality responds to sustained buffering/dropped frames and tries guarded upgrades after stable playback. Manual choices and saved resolution/FPS/codec limits take priority. Off by default.
- Per-profile maximum-quality settings from 144p to 2160p.
- Red-to-white M/play artwork and matching DualSense browsing, loading, sign-in, voice, play, pause and error effects.
- Separate launcher/helper identity PPSA99991 and data under /data/youtube-maxd.

## Included foundation

- ProsperoTube/EVO browsing, subscriptions, library, device-code sign-in and native media playback. No companion PC/NAS or Google Cloud project is required.
- Per-video formats and audio tracks; AVC, VP9 and HEVC native decoder paths, with AV1 software decoding. Returned streams determine available formats.
- Optional SponsorBlock, DeArrow and dislike counts; captions, translations, chapters, seek previews, history, resume, queue and backups.
- Classic and Afterglow interfaces. Classic remains the default. Casting and background audio remain experimental.

## Install

Download the app ZIP, extract it and copy the complete PPSA99991 folder to the location scanned by ShadowMount+, commonly /data/homebrew/PPSA99991. Close the previous app before replacement; keep all assets and helpers, including the local voice model. Back up app data before an update.

Native startup needs your console-side ELF loader on TCP 9021 or a compatible resident Lapy service. Content version is 01.002.000. Avoid collisions with other homebrew using this development title ID. Automatic updates are disabled for this preview.

[Download the app ZIP](https://github.com/HandshakeHackin/youtube-maxd/raw/refs/heads/downloads/YouTube-Maxd-PS5.zip). See the repository README for sign-in, controls, checksums and matching source. The icon and GitHub Source code ZIP are not installable app bundles.

## Validation and limits

Native compilation, executable/package checks, embedded service/account fixtures, local voice-model host tests, Smart playlist/Auto Max checks, controller-bridge checks and interface fixtures passed during development. This exact integrated build has not been tested on a PS5.

Firmware 11.00 boot, live playback/sign-in, 4K/HDR/AV1 throughput, microphone routing/speed, physical lighting, casting and real Auto Max behavior need console acceptance. Auto Max can briefly rebuffer when changing format. Account-required playback is off by default. Ad removal is not guaranteed; internal YouTube APIs and community services may change.

See the README and MAXD_VALIDATION.md for the evidence and remaining tests. Report the build, firmware, launcher, steps and sanitized logs; never include tokens, cookies, passwords or account files.

## Credits and source

Based on ProsperoTube, copyright (C) 2026 BlackBearReloaded, pinned at 3446f364, and EVO Player pinned at 9715f6ee. The on-console service, account/UI/player foundation and existing integrations derive from their work. Additional credits include YouTube.js, QuickJS-NG, whisper.cpp/OpenAI Whisper, SponsorBlock/DeArrow by Ajay Ramachandran and community, the public PS5 toolchain, and Nuvio Max’d as the artwork reference.

GPL-3.0-or-later; dependency licenses and corresponding sources are retained. The initial app corresponds to source commit 014adc2d901518f47fb4d1fbc99eab15b41b79aa. New documentation does not change the compiled app. Independent homebrew, unaffiliated with Google, YouTube or Sony.

## Store delivery

ProsperoStore catalog preparation is separate from the GitHub preview. It requires a public repository, an attached GitHub release ZIP and maintainer review/merge/deployment. No store listing or submission is claimed by these release notes.
