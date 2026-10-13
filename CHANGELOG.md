# Changelog

Notable changes to ProsperoTube, newest version first. Each `## version`
section lists one change per `- ` bullet; `scripts/generate_about.py` turns the
bullets into the in-app changelog (`native/tube_about_data.h`), so keep every
bullet under 300 bytes and run the generator after editing this file.

## 0.1.0

First release. It runs on the console by itself, as Guest or with YouTube
accounts signed in on the console. Most of it was tested offline only, no real
account has signed in, and the self-sufficient build has not run on a console;
[docs/STATUS.md](docs/STATUS.md) records what was tested and where.

- Native PS5 application on the EVO player: hardware and software decoding, its own storage under /data/prosperotube and a packaged elevation helper. No computer is needed: the YouTube service runs inside the app.
- Built-in service: a JavaScript engine running YouTube.js behind a loopback address, verified HTTPS to YouTube, video played directly from YouTube's servers, a new resolve when a link is refused.
- Guest search with options, suggestions and history; Home with filters, Discover sections, channels with their sections, playlists and related videos.
- Playback in AVC, VP9, HEVC and AV1 (SDR) with captions in two languages at once, caption styling, quality, codec and frame-rate limits, audio language and 0.25x to 4x speed.
- Seeking with confirmation modes, chapters, zoom, aspect, rotation and mirror, volume boost, auto volume, audio delay, a sleep timer and recovery after stream failures.
- Profiles with their own favorites, watch history, resume positions, play queue and preferences; seven end-of-video modes; Next and Previous keep the playlist position.
- SponsorBlock segment skipping with review, and DeArrow titles and thumbnails.
- YouTube accounts on the console: sign-in with a device code, account profiles, subscriptions, history, Watch later, playlists, notifications, ratings and playlist editing. Tested offline only; no real account has signed in yet.
- A signed-in profile plays videos as Guest does and reports watch history to the account. Sign-ins are kept as plain text in /data/prosperotube/accounts: the PS5 has no encrypted storage.
- Comments with replies and comment rating, live chat and chat replay.
- Video actions, a suggestions list, chapter notification and screen dimming in the player.
- Channel blocking by ID or name, hide-content switches, content language and country, local backups and About with QR links.
- Guest Home explains itself and leads to Search and Discover; sections YouTube withholds from a Guest say so; a preferred codec a video lacks falls back to what it has.
- Experimental, marked (experimental) and tested offline only: chat and comment posting, account-history controls, feedback actions and the profile password.
- Not included yet: videos that need the account itself (age-restricted, private, members-only), phone casting, voice search, subscriptions import, translations, the remaining interface options, drawn seek previews, update check and self-update, auto frame rate, HDR and background playback.
