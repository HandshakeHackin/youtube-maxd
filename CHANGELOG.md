# Changelog

Notable changes to ProsperoTube, newest version first. Each `## version`
section lists one change per `- ` bullet; `scripts/generate_about.py` turns the
bullets into the in-app changelog (`native/tube_about_data.h`), so keep every
bullet under 300 bytes and run the generator after editing this file.

## 0.1.0

First development version. It needs the LAN companion and has not been accepted
on PS5 hardware as a whole; [docs/STATUS.md](docs/STATUS.md) records what was tested.

- Native PS5 application on the EVO player: hardware and software decoding, its own storage under /data/prosperotube and a packaged elevation helper.
- Search with suggestions and history, topics, Home rows with filters, channel and playlist browsing, and related videos with chip filters.
- Profiles with their own favorites, watch history, resume positions, play queues and preferences.
- YouTube account sign-in by device code, subscriptions, ratings, playlist editing and watched-state management through the companion.
- Player controls for seeking, speed, quality, audio language, zoom, aspect, rotation and mirror, with seven end-of-video modes and Next/Previous that keep the playlist position.
- Captions with a second language at the same time, caption styling, volume boost, loudness normalisation, audio delay and a sleep timer.
- SponsorBlock segment skipping with review, and DeArrow titles and thumbnails.
- Comments with replies and rating, live chat, chat replay and sending chat messages.
- Phone remote control with queue handoff, and voice search through the companion.
- Channel blocking by ID or name per profile, hide switches for watched and unwanted content, and content language and country settings.
- Playback recovery after stream failures, and advancing past unavailable videos in a queue.
