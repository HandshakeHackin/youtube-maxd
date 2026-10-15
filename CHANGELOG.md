# Changelog

Notable changes to ProsperoTube, newest version first. Each `## version`
section lists one change per `- ` bullet; `scripts/generate_about.py` turns the
bullets into the in-app changelog (`native/tube_about_data.h`), so keep every
bullet under 300 bytes and run the generator after editing this file.

## 0.1.0

First release. It runs on the console by itself, as Guest or with YouTube
accounts signed in on the console. Most of it was tested offline only, no real
account has signed in, and self-sufficient builds have passed bounded Guest
console sessions including native 4K VP9 playback and 720p live DVR
([hardware acceptance](docs/HARDWARE_ACCEPTANCE.md)).
[Status](docs/STATUS.md) records what was tested and where.

- Native PS5 application on the EVO player: hardware and software decoding, its own storage under /data/prosperotube and a packaged elevation helper. No computer is needed: the YouTube service runs inside the app.
- Built-in service: a JavaScript engine running YouTube.js behind a loopback address, verified HTTPS to YouTube, video played directly from YouTube's servers, a new resolve when a link is refused.
- Guest search with options, suggestions and history; Home with filters, Discover sections, channels with their sections, playlists and related videos.
- Playback in AVC, VP9, HEVC and AV1 (SDR) with captions in two languages at once, caption styling, quality, codec and frame-rate limits, audio language and 0.25x to 4x speed.
- Seeking with confirmation modes, chapters, zoom, aspect, rotation and mirror, volume boost, auto volume, audio delay, a sleep timer, a video buffer choice and recovery after stream failures.
- Profiles with their own favorites, watch history, resume positions, play queue and preferences; seven end-of-video modes; Next and Previous keep the playlist position.
- SponsorBlock segment skipping with review and an optional alternative server, DeArrow titles and thumbnails, and dislike counts from Return YouTube Dislike for profiles that turn Show dislikes on (off by default; tested offline only).
- YouTube accounts on the console: sign-in with a device code, account profiles, subscriptions, history, Watch later, playlists, notifications, ratings and playlist editing. Tested offline only; no real account has signed in yet.
- A signed-in profile plays videos as Guest does and reports watch history to the account. Sign-ins are kept as plain text in /data/prosperotube/accounts: the PS5 has no encrypted storage.
- Comments with replies and comment rating, live chat and chat replay.
- Video actions, a suggestions list, chapter notification and screen dimming in the player.
- Channel blocking by ID or name, hide-content switches, content language and country, local backups and About with QR links.
- Updates: a signed homebrew.page catalog check, notification and skip preferences, release notes, and a loader-started helper that verifies and installs the release after the app closes. Tested offline; a complete console update from a published release remains pending.
- Import subscriptions from a NewPipe, PocketTube or GrayJay export file placed in /data/prosperotube/import; the file is read inside the app.
- Phone casting (experimental): link the YouTube app with a TV code; play, pause, seek, queue, volume and captions from the phone. The receiver runs inside the app. Tested offline only; no real phone has been linked yet.
- Guest Home explains itself and leads to Search and Discover; sections YouTube withholds from a Guest say so; a preferred codec a video lacks falls back to what it has.
- Experimental, marked (experimental) and tested offline only: phone pairing, chat and comment posting, account-history controls, feedback actions and the profile password.
- Interface in 21 languages besides English, following the PS5 system language or the Interface language preference: browser, preferences and messages. The player and toasts stay English, as do languages that need other fonts or right-to-left layout.
- Interface preferences per profile: interface and video grid scale, one to four title lines, scrolling titles, two-line subtitles, rounded or square cards, four themes (Afterglow by default) and nine classic colour schemes, the rows of the video actions menu and what Circle does on the main menu.
- Preview while seeking: the thumbnail of the seek target above the seek bar, a single frame or a carousel of five, from YouTube's storyboard sheets. Off by default.
- Automatic 60/120 Hz refresh switching and PQ/HLG HDR selection passed bounded PS5 scanout checks; the owner confirmed audible audio, TV-reported HDR and playback menus without blackouts. Afterglow background audio while browsing is host-tested only.
- Not included yet: videos that need the account itself (age-restricted, private, members-only), voice search, a translated player, and a few interface options (buttons at the top, card previews, playlists as rows).
