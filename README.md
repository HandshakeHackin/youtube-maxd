# ProsperoTube 0.1.0

A native PS5 homebrew YouTube client inspired by
[SmartTube](https://github.com/yuliskov/SmartTube), with its own TV interface
and service provider built on [EVO Player](https://github.com/sainsaji/EVO-PLAYER-PS5).
It is a native implementation of the SmartTube experience, not a port of the
Android application: no SmartTube or MediaServiceCore source is incorporated.

ProsperoTube **runs on the console by itself**: no computer, no pairing. The
application carries its own YouTube service (a small JavaScript engine running
[YouTube.js](https://github.com/LuanRT/YouTube.js) inside the app) and plays
video straight from YouTube's servers. It browses and plays as **Guest**, and
YouTube accounts can be signed in on the console itself with a device code.
The account features were tested offline only: **no real account has signed in
with this build yet** (see [Accounts](#accounts-on-the-console)). The YouTube
app on a phone can cast to it with a TV code (experimental, tested offline
only: **no real phone has been linked yet**), and subscriptions can be imported
from an export file. The one thing that still needs a computer is voice search,
which this release does not have.

Release 0.1.0 is being prepared. [docs/STATUS.md](docs/STATUS.md) states what is
implemented, what was tested and where, and what is still open before release.
Throughout this page, **host-tested** means covered by the offline test suite on
a PC, and **checked on a console** means it ran on a PS5; most of 0.1.0 is
host-tested only (see [What was checked on a console](#what-was-checked-on-a-console)),
and **the self-sufficient build has had one bounded console session, on its
Guest core path** (2026-10-13).

## What you need

- A PS5 that runs homebrew, with [Lapy](native/elevation/README.md) available:
  ProsperoTube keeps its data under `/data/prosperotube` and stops at startup
  when it cannot get access to that folder.
- An internet connection on the PS5. Nothing else: no computer is involved.
- The package from the release: `PPSA99142.zip`, the `PPSA99142` folder title
  (a local build can also make a `.ffpfsc` image). Nothing in this repository
  deploys to a console.

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
- **Preview while seeking** (off by default; single frame or a carousel of five):
  the thumbnail of the seek target above the seek bar with its time, from
  YouTube's storyboard sheets. A sheet is fetched once the target rests on it,
  and at most four are kept in memory. Host-tested only.
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

**YouTube accounts** (signed in on the console by the built-in service; see
[Accounts](#accounts-on-the-console) for what is unverified)

- Sign-in with a device code, removal of a signed-in account, up to eight
  account profiles next to Guest.
- Subscriptions, subscribed channels, notifications, YouTube history, Watch
  later, playlists and own videos.
- Like, dislike, subscribe and unsubscribe, Watch later, playlist add, remove,
  create, rename, delete, order and save. Every change is reviewed on a
  confirmation screen and sent once.

**Phone casting** (experimental; the receiver runs inside the app, see
[docs/REMOTE_SERVICE.md](docs/REMOTE_SERVICE.md))

- **Phone pairing** in the player preferences: **Link a phone** shows a TV
  code; enter it in the YouTube app under *Watch on TV > Enter TV code*. Off
  until a phone is linked; **Reset phone pairing** forgets this TV's identity.
- From the phone: start a video or a queue, play, pause, seek, next, previous,
  volume, captions and stop; the phone shows what plays and where. A choice of
  what happens when the phone disconnects (keep playing, stop, close the app),
  and whether phone playback enters the history.
- Host-tested against a stand-in for YouTube's phone service. Whether a real
  phone links, and how the connection behaves over an evening, is unknown.

**Settings** (YouTube preferences)

- Player and caption choices above, per profile or shared (**Separate settings
  per account**), **Start in** section, **Choose account at startup**, menu
  sections and pins, content language and country, sixteen hide-content
  switches, thumbnail source, channel blocks by ID or name.
- **Interface**, per profile: interface scale and video grid scale with
  SmartTube's values (the page holds more or fewer cards accordingly), one to
  four title lines, the selected card's long title scrolling at a chosen speed,
  two-line subtitles, rounded or square cards, nine colour schemes (three of
  them true black), which rows the video actions menu shows and in what order
  (in the browser and in the player), and what Circle does on the main menu:
  nothing, close after a second press, or close at once. **Search options**
  can ask for two presses of Circle to leave typed results.
- A colour scheme colours the browser and lends its accent to the player
  controls. **Player settings > Theme** (EVO's own, shared by every profile)
  colours the keyboard, dialogs and player settings, and the player controls
  while the scheme is **ProsperoTube**.
- **Backup and restore** in `/data/prosperotube/backup` with an optional
  automatic backup, **Import subscriptions** from a NewPipe, PocketTube or
  GrayJay export placed in `/data/prosperotube/import`, and **About** with the
  version, project links as QR codes, credits and changes.

**Languages**

- The browser, the preferences, messages and toasts in 21 languages besides
  English: Čeština, Dansk, Deutsch, Ελληνικά, Español, Suomi, Français, Magyar,
  Bahasa Indonesia, Italiano, Norsk bokmål, Nederlands, Polski, Português
  (Brasil), Português (Portugal), Română, Русский, Svenska, Türkçe, Українська
  and Tiếng Việt. The app follows the language of the PS5 system settings;
  **YouTube preferences > Interface language** chooses another one or English.
- The player (controls, in-player menus, pickers, the keyboard) stays English:
  its fonts hold no accented letters yet. Languages that need other fonts,
  shaping or right-to-left layout (Chinese, Japanese, Korean, Thai, Hindi,
  Arabic, Hebrew) are not included.
  [docs/LOCALIZATION.md](docs/LOCALIZATION.md) has the details and the plan.

### Accounts on the console

**Discover > Accounts & library > Sign in to YouTube** shows a code and the
address to enter it at (also as a QR code). The app checks by itself until the
sign-in is authorized, denied, cancelled or the code expires. The new account
is a profile; **Remove selected account** signs it out of this console.

- **Unverified until a real sign-in.** Everything about accounts was tested on
  a PC against canned answers: the sign-in exchange, the stored sign-in and its
  renewal, every account list and every change. Whether Google accepts the
  sign-in from the console, and whether each list and change works against the
  live service, is not known yet. A list or change that fails does not affect
  Guest browsing or playback.
- **Playback.** A signed-in profile plays videos the way Guest does: the
  account is not part of the player request. Watch history is reported to the
  account separately (unless **History** is off or the video is played
  incognito). Videos that need the account itself (age-restricted, private,
  members-only, purchased) are refused as they are for Guest; the path that
  would use the account for them is in the code, switched off.
- **Where the sign-in is kept.** In
  `/data/prosperotube/accounts/oauth-profiles.json`, as plain text: the PS5
  offers no encrypted storage. Whoever can read the app's data directory can
  use those accounts on YouTube. The file is not part of the app's backups.
  Removing the account in the app deletes its sign-in from the console; to
  revoke it at Google as well, remove "YouTube on TV" under
  <https://myaccount.google.com/permissions>.

### What was checked on a console

The self-sufficient build ran on a PS5 in one bounded session on 2026-10-13,
as Guest with no computer involved
([hardware acceptance](docs/HARDWARE_ACCEPTANCE.md) has the records):

- The built-in service started, and Discover and one of its topics listed
  their videos through it.
- A public video resolved on the console and played from YouTube directly, in
  hardware-decoded AVC at 1080p with AAC sound: a ten-second seek, pause, the
  caption picker and the playback options, Stop, the saved Guest history and a
  clean close. Nobody judged sound or motion, and no caption was on screen.

Earlier development builds ran on a PS5 for these bounded cases only:

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
Everything else on this page is host-tested only and not yet verified on a
console: VP9/HEVC/AV1 and 4K playback, recovery when a media link expires
during playback, accounts, phone casting, updates, the interface languages,
real live chat, the in-player actions, backups, the seek preview and every
settings screen added since those runs, the interface preferences among them.
**The built-in service, its HTTPS client and the direct playback from YouTube
have run on a console in that one session only**; on the host they pass the
offline suite and a live Guest check (search, sections, channels, playlists,
comments, resolve, captions, playback and seeking with the console's FFmpeg
patch; see [docs/VALIDATION.md](docs/VALIDATION.md)).

## Experimental features

These are in 0.1.0's code and were tested offline only. All but the profile
password need a signed-in account. Their rows end in
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

- Translations of the player interface, and languages outside the Latin,
  Greek and Cyrillic scripts ([docs/LOCALIZATION.md](docs/LOCALIZATION.md)).
- The remaining SmartTube interface options (buttons at the top of the main
  window, card previews, playlists as rows); [docs/PARITY.md](docs/PARITY.md)
  tracks each.
- Playing videos that need the account itself (age-restricted, private,
  members-only, purchased): a signed-in profile resolves videos as Guest does.
- Voice search: there is no speech recogniser on the console, so the L3
  shortcut is hidden ([docs/VOICE_SEARCH.md](docs/VOICE_SEARCH.md) weighs the
  ways to add one). With a phone linked, the YouTube app's own voice search
  works from the phone. The voice code remains in the tree for the
  development companion.
- Trending and Shorts as sections: YouTube does not offer them to a Guest; the
  app shows one explanatory row.
- Automatic frame-rate switching, HDR and background playback.

## Updates

Host-tested only; the check has not met GitHub yet (the repository is private)
and nothing here has run on a console. [docs/UPDATES.md](docs/UPDATES.md) has
the whole mechanism.

- Once a day, some seconds after start, the app asks this project's GitHub
  releases whether a newer version exists and says so in a notice. **Settings >
  About** has **Check for updates** (installed and latest version), **Notify
  about updates** (on by default), the release notes, and **Skip version**.
- **Update to ... now** downloads the release ZIP in pieces (it can be paused
  and continues where it stopped), requires the size and SHA-256 the release
  lists, unpacks it beside the app's data and checks every file. **Restart to
  finish updating** then replaces the app's files and closes ProsperoTube.
- The previous version is kept until the new one has run for ten seconds. A
  power loss during the replacement is undone at the next start; a new version
  that fails to start twice is replaced by the previous one.
- Only a copy installed in `/data/homebrew/PPSA99142` updates itself. A copy
  on a USB drive, an image install and a development build list the newer
  version and say why they stay as they are.
- The request carries nothing about the console, the user or the installed
  version: a generic `User-Agent` and nothing else.

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
- The console talks to YouTube directly and without cookies: YouTube sees the
  console's address, as it would for any player. HTTPS to YouTube's API is
  verified against a root-certificate list built into the app. Video, sound
  and thumbnails are fetched by the player without certificate verification
  (as EVO Player does); they carry no credentials.
- A signed-in account's tokens go to Google's and YouTube's API over verified
  HTTPS and nowhere else; the app's interface never receives them. They are
  stored as plain text under `/data/prosperotube/accounts`
  ([Accounts](#accounts-on-the-console)).
- SponsorBlock and DeArrow, when turned on, are asked directly by the console.
- The update check asks `api.github.com` at most once a day over verified
  HTTPS, with a generic `User-Agent` and no identifier; a download comes from
  GitHub's release hosts. **Notify about updates** off silences the notice,
  not the check.
- Local history is independent of Google. With an account profile, playback
  reports go to that account unless **History** is disabled or the video is
  played incognito.
- **Show dislikes** is not offered by the built-in service.
- The PS5 diagnostics redact media session addresses. See the
  [protocol and security notes](docs/PROTOCOL.md).

## Build

[docs/BUILD.md](docs/BUILD.md) covers dependencies, the PS5 package build, the
offline checks (`bash scripts/test_all.sh`) and the release procedure.
[docs/VALIDATION.md](docs/VALIDATION.md) is the dated validation record,
[docs/DECISIONS.md](docs/DECISIONS.md) and the [goal](GOAL.md) explain the
direction.

GPL-3.0-or-later. See [LICENSE](LICENSE) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
