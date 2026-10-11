# Changelog

Notable changes to ProsperoTube, newest version first. Each `## version`
section lists one change per `- ` bullet; `scripts/generate_about.py` turns the
bullets into the in-app changelog (`native/tube_about_data.h`), so keep every
bullet under 300 bytes and run the generator after editing this file.

## 0.1.0

First release. It needs the LAN companion. Most of it was tested offline only;
[docs/STATUS.md](docs/STATUS.md) records what was tested and where.

- Native PS5 application on the EVO player: hardware and software decoding, its own storage under /data/prosperotube and a packaged elevation helper. The LAN companion is required.
- Guest search with options, suggestions and history; Home with filters, Discover sections, channels with their sections, playlists and related videos.
- Playback in AVC, VP9, HEVC and AV1 (SDR) with captions in two languages at once, caption styling, quality, codec and frame-rate limits, audio language and 0.25x to 4x speed.
- Seeking with confirmation modes, chapters, zoom, aspect, rotation and mirror, volume boost, auto volume, audio delay, a sleep timer and recovery after stream failures.
- Profiles with their own favorites, watch history, resume positions, play queue and preferences; seven end-of-video modes; Next and Previous keep the playlist position.
- SponsorBlock segment skipping with review, and DeArrow titles and thumbnails.
- YouTube account sign-in by device code, subscriptions, library, ratings, Watch later and playlist editing through the companion; every change is confirmed and sent once.
- Comments with replies and comment rating, live chat and chat replay.
- Video actions, a suggestions list, chapter notification and screen dimming in the player.
- Channel blocking by ID or name, hide-content switches, content language and country, local backups, subscriptions import and About with QR links.
- Experimental, marked (experimental) and tested offline only: phone casting, voice search, sending chat messages, posting comments, account-history pause, resume and clear, feedback actions and the profile password.
- Not included yet: translations, the remaining interface options, drawn seek previews, update check and self-update, running without the companion, auto frame rate, HDR and background playback.
