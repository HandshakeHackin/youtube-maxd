# Changelog

YouTube Max’d integration changes, followed by retained upstream release notes.

## 1.003.000

Released as **v1.3.0-preview.1** (experimental preview, 2026-10-18).

- Transcript search with timestamp seeking and local, profile-owned bookmarks.
- Optional in-app picture-in-picture in Afterglow. **Off by default**, enabled through R2 → Picture-in-picture for the current session; the choice is not saved.
- Local push-to-talk playback commands for pause, resume, seek, captions, quality and queue additions.
- Session builder selects unwatched videos to fit a chosen 15–120-minute budget.
- Four independent signed-out family profiles with parent PIN, approved-channel playback, optional search restrictions and daily viewing limits.
- PS5-hosted LAN party queue with phone QR joining, expiring invitations, guest votes and host approval.
- Content version raised to `01.003.000`; title ID and data location retained.
- Native compilation and host checks pass. PS5/live account testing remains outstanding; linked Google child-profile import remains research.

See [release notes](docs/releases/v1.3.0-preview.1.md) for installation, usage and limits.

## 1.002.000

- YouTube Max’d now uses the pinned standalone ProsperoTube/EVO foundation, keeping its own identity, icon and coral/pink/violet palette.
- Embedded YouTube.js and TV activation-code login replace the prototype's Google Cloud configuration requirement.
- Native AVC/VP9/HEVC and HDR paths, software AV1, captions, chapters, quality/audio switching and live DVR are included from upstream.
- Saved maximum quality per profile: 144p, 240p, 360p, 480p, 720p, 1080p, 1440p and 2160p.
- Smooth native controller effects follow browsing, loading, playback, pause, sign-in and errors, using the existing pad and feedback switch.
- State and helper identity are isolated to YouTube Max’d; upstream automatic updates are disabled for this fork.
- Exact firmware 11.00 / ShadowMount+ playback, sign-in and physical-controller acceptance still require PS5 testing.

## 1.000.000

First public release. ProsperoTube runs on the console by itself: the YouTube
service is built into the app, as Guest or with YouTube accounts signed in on
the console. Guest browsing and playback have passed sessions on PS5 consoles;
accounts, phone casting and the Afterglow interface are still being validated.

- Self-sufficient: YouTube.js runs inside the app on QuickJS-NG, and video streams straight from YouTube's servers to the PS5 decoders. No computer, companion or API key.
- Playback in AVC, VP9 and HEVC up to 2160p through the hardware decoders and AV1 in software, with HDR (PQ and HLG) and automatic frame-rate switching.
- Per-video quality and audio track (dubs, original, described audio) in the playback options; switching keeps the position.
- Captions including YouTube's auto-generated and auto-translated tracks, a second language at once and caption styles.
- Live streams with rewind and a remembered position; Up next, chapters, seek preview, speed from 0.25x to 4x, a sleep timer and recovery after stream failures.
- Search with suggestions while typing, Home with real video shelves for Guests, Discover, channels with their sections, playlists, comments and live chat. Feeds load more as you scroll.
- Sign in to YouTube from the main menu with a code entered on a phone or computer: subscriptions, history, Watch later, playlists and notifications.
- Two interfaces: Classic (default) and Afterglow (preview) with four looks and its own sounds; the interface in 21 languages besides English.
- Profiles with their own history, resume points, favorites, followed channels and an ordered queue; backups and subscriptions import from NewPipe, PocketTube or GrayJay.
- SponsorBlock with skip review and Undo, DeArrow titles and thumbnails, and optional Return YouTube Dislike counts.
- Phone casting from the YouTube app with a TV code (experimental).
- Update offers and in-place self-update through the signed homebrew.page catalog.
- Known limitations: videos that need the account (age-restricted, private, members-only) do not play yet, and the player's own controls stay in English.
