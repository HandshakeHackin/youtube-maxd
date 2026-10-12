# ProsperoTube 0.1.0

A native PS5 homebrew YouTube client inspired by
[SmartTube](https://github.com/yuliskov/SmartTube), with its own TV interface
and service provider built on [EVO Player](https://github.com/sainsaji/EVO-PLAYER-PS5).
It is a native implementation of the SmartTube experience, not a port of the
Android application: no SmartTube or MediaServiceCore source is incorporated.

ProsperoTube **runs on the console by itself**: no computer, no pairing. The
application carries its own YouTube service (a small JavaScript engine running
[YouTube.js](https://github.com/LuanRT/YouTube.js) inside the app) and plays
video straight from YouTube's servers. Version 0.1.0 browses and plays as
**Guest**; signing in to a YouTube account comes in a later version.

Release 0.1.0 is being prepared. [docs/STATUS.md](docs/STATUS.md) states what is
implemented, what was tested and where, and what is still open before release.
Throughout this page, **host-tested** means covered by the offline test suite on
a PC, and **checked on a console** means it ran on a PS5; most of 0.1.0 is
host-tested only (see [What was checked on a console](#what-was-checked-on-a-console)),
and **the self-sufficient build has not run on a console yet**.

## What you need

- A PS5 that runs homebrew, with [Lapy](native/elevation/README.md) available:
  ProsperoTube keeps its data under `/data/prosperotube` and stops at startup
  when it cannot get access to that folder.
- An internet connection on the PS5. Nothing else: no computer is involved.
- The package from the release: a `PPSA99142` folder title, a ZIP of it and a
  `.ffpfsc` image. Nothing in this repository deploys to a console.

## How it works

At start the app elevates through Lapy for its data folder, then starts its
service: a QuickJS engine thread that runs the bundled service script, and a
listener on `127.0.0.1`. The browser part of the app talks to that address
exactly as it used to talk to a companion on a PC. A video is played from a
small DASH manifest the service writes; the manifest names YouTube's media
servers, and the player (FFmpeg) reads them directly. Closing ProsperoTube from
its menu stops the service before the app parks.

If the service cannot start, the app still opens: the first row of the main
menu says why, and selecting it (or pressing **Options**) tries again.

### For developers: an external companion

The PC companion (`companion/`, Python with yt-dlp and a Node worker) is now a
development tool. A file named `external-companion` in `/data/prosperotube`
makes the same package use a companion again (address and pairing code on
**Options**, as before); [docs/BUILD.md](docs/BUILD.md) and
[docs/LIBRARY_SERVICE.md](docs/LIBRARY_SERVICE.md) describe running one.

## What works

The first release is limited to the following. All of it is host-tested; the
next section lists the part that also ran on a PS5.

**Browsing as Guest**

- Search with suggestions, per-profile search history and options (upload date,
  duration, type, features, order; channels and playlists in results). Search
  also accepts YouTube video/playlist URLs, `video:VIDEO_ID`,
  `channel:CHANNEL_ID` and `playlist:PLAYLIST_ID`. YouTube's "last hour" date
  and its date and rating orders are not available.
- Home recommendations with their filters, and the Discover sections: topics,
  Gaming, News, Music, Sports, Live, Movies, Trending, Shorts and Kids.
- Channels (Home, Videos, Shorts, Live, Playlists, sort orders, search in
  channel), playlists and related videos.
- Comments with replies, live chat and chat replay, read-only as Guest.

**Playback**

- AVC, VP9, HEVC and AV1 in SDR with AAC or Opus sound, adaptive and
  progressive streams and HLS; AV1 is decoded in software. This is not a claim
  that every current YouTube video or resolution plays on a PS5.
- Captions with a second language at the same time, a caption picker during
  playback, caption style, size and position.
- Maximum resolution (144p to 2160p), frame-rate limit, codec and audio
  language per profile; speed from 0.25x to 4x remembered for all videos, a
  channel, a video or one playback.
- Seeking with three confirmation modes and ten intervals, chapters, video
  sizing, zoom, aspect, rotation and mirror, three time displays, a sleep timer,
  volume up to 300%, auto volume and audio delay.
- Local history with resume positions, favorites, followed channels and an
  ordered play queue, all per profile; seven end-of-video modes; **Next** and
  **Previous** keep the playlist position; unavailable videos in a queue are
  skipped after a cancellable five seconds.
- Recovery after stream failures: up to three fresh resolutions per selected
  video, keeping profile, format, position and captions.
- Video actions, a suggestions list, chapter notification, screen dimming and
  screen off from the player.

**Integrations**

- SponsorBlock: actions per category, minimum segment length, channel
  exclusions, timeline colours and review of a skip.
- DeArrow titles and thumbnails, each with its own switch; originals are kept.

**YouTube accounts** (not active in 0.1.0: the built-in service has no sign-in
yet, and the app shows one line saying so instead of these rows; the code
remains and works with the development companion)

- Sign-in with a device code, removal of a signed-in account, several profiles.
- Subscriptions, subscribed channels, notifications, YouTube history, Watch
  later, playlists and own videos.
- Like, dislike, subscribe and unsubscribe, Watch later, playlist add, remove,
  create, rename, delete, order and save. Every change is reviewed on a
  confirmation screen and sent once.

**Settings** (YouTube preferences; English interface)

- Player and caption choices above, per profile or shared (**Separate settings
  per account**), **Start in** section, **Choose account at startup**, menu
  sections and pins, content language and country, sixteen hide-content
  switches, thumbnail source, channel blocks by ID or name.
- **Backup and restore** in `/data/prosperotube/backup` with an optional
  automatic backup, **Import subscriptions** from a NewPipe, PocketTube or
  GrayJay export placed in `/data/prosperotube/import`, and **About** with the
  version, project links as QR codes, credits and changes.

### What was checked on a console

Earlier development builds ran on a PS5 for these bounded cases only
([hardware acceptance](docs/HARDWARE_ACCEPTANCE.md) has the records):

- Startup with storage access, companion pairing and search.
- A public YouTube video through the companion in hardware-decoded AVC with
  English captions and saved Guest history; pause, resume and a ten-second seek.
- More than two minutes of public 1080p60 AVC/AAC with smooth picture and
  synchronized sound, and a clean close.
- With generated test media and a fixture service instead of YouTube: resume
  and seeking, speed changes, the SponsorBlock skip review, advancing past an
  unavailable video in the queue, two caption languages at once while paused,
  the chat reader, thumbnail decoding, and phone commands sent by a test
  receiver (not a real phone).

Those runs used the companion on a PC, before the service moved into the app.
Everything else on this page is host-tested only: VP9/HEVC/AV1 playback, real
live chat, the in-player actions, backups and every settings screen added since
those runs. **The built-in service, its HTTPS client and the direct playback
from YouTube have never run on a console**; on the host they pass the offline
suite and a live Guest check (search, sections, channels, playlists, comments,
resolve, captions, playback and seeking with the console's FFmpeg patch; see
[docs/VALIDATION.md](docs/VALIDATION.md)).

## Experimental features

These are in 0.1.0's code and were tested offline only. In this Guest-only version just the profile password can be reached; the others need an account and are not offered until sign-in arrives. Their rows end in
**(experimental)**, each acts only when you select it or turn it on, and
**YouTube preferences > Experimental features** repeats this explanation.

| Feature | Where | Default |
| --- | --- | --- |
| Sending a live-chat message | **L3** in an open live chat | Typed, reviewed, sent once |
| Writing a comment or a reply | Comments of a video | Typed, reviewed, posted once |
| Pause, resume and clear the account watch history | Touchpad on **YouTube history** | Confirmed, sent once |
| Not interested, Don't recommend channel, Hide from this list | Video menu on cards that carry them | Confirmed, sent once |
| Profile password | YouTube preferences > Account settings | No password; typed on the visible PS5 keyboard, stored as a salted hash |

## Not included yet

- Translations: the interface is English only.
- The remaining SmartTube interface options (card styles, scaling, colour
  schemes, context-menu editing); [docs/PARITY.md](docs/PARITY.md) tracks each.
- Drawn thumbnails while seeking.
- Update check and self-update.
- Signing in to YouTube and everything that needs an account: subscriptions,
  the account's history, Watch later, playlists, ratings, notifications. Home
  as Guest shows what YouTube gives a signed-out visitor, which is a notice;
  the app says so and leads to Search and Discover.
- Phone casting and voice search (they need a service the app does not carry
  yet; their rows and the L3 shortcut are hidden).
- Importing subscriptions from another application.
- The code of all of these remains in the tree, inactive in this build.
- Trending and Shorts as sections: YouTube does not offer them to a Guest; the
  app shows one explanatory row.
- Automatic frame-rate switching, HDR and background playback.

## Controls

| Browser | Action |
| --- | --- |
| D-pad / Cross | Focus / open or watch |
| Square | Search |
| Circle | Back; cancel a pending request |
| Triangle | Toggle favorite |
| R2 / L2 | Toggle queue / follow the video's channel |
| L1 / R1 | Previous / next page |
| R3 | Comments, or live chat on a live video |
| Touchpad | Video actions; section and pin actions on a folder; playlist edits; ratings for a displayed comment |
| Options | Retry the built-in service if it did not start |

In comments, Cross opens replies, L2/R2 scroll the full text, L1/R1 page, R3
switches top/newest and Triangle opens the chat replay. In chat, R3 switches
top/live, Triangle loads more replay messages and L1/R1 move the replay by one
minute.

| Player | Action |
| --- | --- |
| Cross | Pause or resume (four behaviours selectable while controls are hidden) |
| Left / Right | Seek |
| L1 / R1 | Previous / next chapter |
| Up / Down | Toggle captions / caption picker |
| R2 | Playback options: quality, audio, speed and the video actions |
| L2 / Touchpad halves | Caption timing |
| Triangle | Video sizing |
| R3 | Jump to the next SponsorBlock highlight |
| Square / Options | Media information / statistics |
| Circle | Stop (confirmed) |

Left/Right and Up/Down can be reassigned under **Player controls, keys and
Shorts**. Live streams have no seekable timeline. Closing ProsperoTube from its
menu releases its resources and parks the application; close it from the PS5
switcher afterward.

## Storage and privacy

- Settings, profiles, history, favorites and queues are in one file under
  `/data/prosperotube`, replaced atomically; local lists keep at most 384
  videos. PS5 storage encryption is not provided. Backups stay in
  `/data/prosperotube/backup`.
- The built-in service listens on `127.0.0.1` only, on a port chosen at each
  start, behind a random token that exists in memory only. It keeps a small
  cache under `/data/prosperotube/cache` and writes no log.
- The console talks to YouTube directly, as Guest and without cookies: YouTube
  sees the console's address, as it would for any player. HTTPS to YouTube's
  API is verified against a root-certificate list built into the app. Video,
  sound and thumbnails are fetched by the player without certificate
  verification (as EVO Player does); they carry no credentials.
- SponsorBlock and DeArrow, when turned on, are asked directly by the console.
- Local history is independent of Google. With an account profile, playback
  reports go to that account unless **History** is disabled or the video is
  played incognito.
- **Show dislikes** is not offered by the built-in service.
- The PS5 diagnostics redact media session addresses. See the
  [protocol and security notes](docs/PROTOCOL.md).

## Build

[docs/BUILD.md](docs/BUILD.md) covers dependencies, the PS5 package build and
the offline checks (`bash scripts/test_all.sh`).
[docs/VALIDATION.md](docs/VALIDATION.md) is the dated validation record,
[docs/DECISIONS.md](docs/DECISIONS.md) and the [goal](GOAL.md) explain the
direction.

GPL-3.0-or-later. See [LICENSE](LICENSE) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
