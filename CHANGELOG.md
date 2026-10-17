# Changelog

Notable changes to ProsperoTube, newest version first. Each `## version`
section lists one change per `- ` bullet; `scripts/generate_about.py` turns the
bullets into the in-app changelog (`native/tube_about_data.h`), so keep every
bullet under 300 bytes and run the generator after editing this file.

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
