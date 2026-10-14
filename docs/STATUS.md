# Current implementation status

Updated 2026-10-13. Features are frozen for release 0.1.0. Full SmartTube
equivalence stays the long-term target ([goal](../GOAL.md)); this page states
what the first release contains, how far each part was checked, and what is
left before it ships. **The application is self-sufficient**: its YouTube
service runs inside the app ([decision](DECISIONS.md)), as Guest and, since
2026-10-13, with YouTube accounts signed in on the console (host-tested only;
no real account has signed in). The self-sufficient build passed one bounded
console session on its Guest core path on 2026-10-13
([hardware acceptance](HARDWARE_ACCEPTANCE.md)); what that session did not
cover is listed under [Checked on a console](#checked-on-a-console).

Three levels of evidence are kept apart on this page:

- **host-tested**: covered by the offline suite on a PC (`bash scripts/test_all.sh`);
- **checked on a console**: ran on a PS5, in the bounded cases recorded in
  [hardware acceptance](HARDWARE_ACCEPTANCE.md);
- **inferred**: built from what the pinned libraries and the reference sources
  suggest about the live service, with synthetic responses in the tests.

## First-release scope

Owner decisions of 2026-10-11, from the [goal](../GOAL.md): self-sufficient on
the console is a hard requirement; the first MVP is Guest, standalone.

- **In the release:** Guest search with filters, Home (a notice for a Guest,
  with rows leading on), Discover sections, channels, playlists, comments and
  chat read-only, playback with captions, quality and speed, local history and
  resume, favorites, queue, followed channels, SponsorBlock, DeArrow, the
  settings already implemented.
- **Added 2026-10-13, host-tested only:** accounts in the built-in service:
  sign-in by device code, account profiles, subscriptions, subscribed
  channels, history, Watch later, playlists, notifications, own videos,
  ratings, subscriptions, playlist editing, and the experimental chat and
  comment posting, account-history controls and feedback actions. Offered
  whenever the service's feature list names `device_auth`.
- **Added 2026-10-13, host-tested only:** in the built-in service, the
  subscriptions import (`subscriptions_import`) and the phone receiver
  (`remote`: TV-code pairing, the phone's commands, player-state feedback).
  Their rows are offered whenever the service's feature list names them. The
  receiver has met a local stand-in for YouTube's phone service only: **no
  real phone has been linked to this build.**
- **In the tree, inactive in the release:** voice search (no speech recogniser
  on the console; [assessment](VOICE_SEARCH.md)) and signed-in playback of
  videos that need the account (`account_playback`, off). Voice search still
  works against the development companion.
- **Added 2026-10-13, host-tested only:** the update check and the in-place
  self-update ([updates](UPDATES.md)). The check cannot answer while the
  repository is private.
- **Added 2026-10-13, host-tested only:** the interface in 21 languages
  besides English ([localization](LOCALIZATION.md)): browser, preferences
  and messages follow the PS5 system language or the **Interface language**
  preference. The player (controls, in-player menus, pickers, keyboard) and
  toasts stay English: their fonts hold no accented letters.
- **Added 2026-10-13, host-tested only:** interface preferences per profile
  (interface and grid scale, card text, themes and classic colour schemes,
  the video actions menu, Circle rules) and the drawn seek preview.
- **Afterglow, approved 2026-10-13, in progress:** the interface of the
  [proposal](afterglow/design-notes.md) is being built on the UI kit
  ([architecture](afterglow/ARCHITECTURE.md)), and the first release waits for
  all of its screens ([decision](DECISIONS.md)). Built so far, host-tested
  only: the frame (rail, hand-overs, toasts, dialog, attract mode), Home and
  the opening, the reader, the data layer on the shipping provider, the text
  engine for every script behind the kit's fonts, and the four looks, with 39
  boards drawn and checked in each look (`scripts/test_afterglow.sh`). It is
  linked into the PS5 package behind **Interface: Afterglow (preview)** under
  YouTube preferences > Interface; **Classic** stays the default, and pages
  Afterglow lacks say so. It has never run on a console. The browser's own
  **Theme** preference (Afterglow by default, Onyx, Daybreak, High contrast)
  and the new mark are in the classic interface.
- **Deferred:** the remaining interface options (buttons at the top of the
  main window, card previews, playlists as rows), a translated player,
  languages that need other fonts, shaping or right-to-left layout, auto
  frame rate, HDR and background playback.

## The built-in service

- `standalone/`: QuickJS-NG 0.17.0 running YouTube.js 18.1.0 and the
  companion's library modules; a loopback listener with the companion's API;
  libcurl + OpenSSL with an embedded root list. Host-tested (20 engine and 131
  HTTP checks, plain and under sanitizers) and checked live as Guest.
- Subscriptions import: `POST /v1/subscriptions/import` takes the export file
  as its body, up to 1 MiB, on this one route (the others keep 8 KiB); the
  parser is the companion's, ported, with its test cases. Host-tested: 51 HTTP
  checks, plain and under sanitizers, and the provider end to end.
- Phone receiver: `POST /v1/remote` with the companion's request and answer
  shapes ([details](REMOTE_SERVICE.md)). The back channel is a long poll read
  as it arrives, which the HTTPS client gained for it (a streaming request
  with a silence limit; 114 more checks of the bindings on plain HTTP, the
  console's way of waiting and TLS). Host-tested: 272 HTTP checks against a
  stand-in for YouTube's phone service with a streamed back channel, plain and
  under sanitizers, and the provider with its driver end to end (TV code,
  commands in order, state report, off). Voice commands from the phone stay
  unsupported: the receiver never advertises a microphone.
- The provider starts it after storage is ready, uses its address and token in
  memory only, never pairs, retries from a visible row when it did not start,
  and stops it on the first soft-close frame. Host-tested against a stand-in
  and, end to end, against the real service in one process.
- Accounts ([decision of 2026-10-13](DECISIONS.md)): device-code sign-in with
  the companion's bounds, credentials in
  `/data/prosperotube/accounts/oauth-profiles.json` (plain text, atomic
  replace), token refresh, removal, every account list and change of the
  library modules behind one-attempt guards, history reports. Host-tested: 201
  HTTP checks against canned Google and YouTube answers, plain and under
  sanitizers, and the provider end to end (sign-in, profile, subscriptions, one
  change, removal). Every profile resolves videos as Guest; health says
  `account_playback: false`.
- The PS5 package compiled, linked and packed with the Guest-only service. An
  instrumented build of `6b3b1c41`, a tree that has the account code, **ran on
  a console once, as Guest** (2026-10-13,
  [hardware acceptance](HARDWARE_ACCEPTANCE.md)): the service's start,
  Discover, one resolve and direct playback. Accounts, the subscriptions
  import and the phone receiver have not run on a console.

## Updates

- `native/tube_update*.c`: the check against this repository's GitHub releases
  (once a day and on request, answer kept in `/data/prosperotube/update`), the
  ranged download with resume and cancel, size and SHA-256 against the release,
  validated unpacking with every file read back, a journaled replacement of the
  installed top-level entries, the previous version kept until the new one has
  run, rollback after two failed starts. About lists **Check for updates**,
  **Notify about updates**, the release notes, update, restart and skip.
- It uses the app's HTTPS client in a client of its own on a worker thread and
  does not need the JavaScript service. The console side exists only in the
  package that carries the built-in service.
- Host-tested (`scripts/test_update.sh`, `scripts/test_update_host.py`,
  `scripts/test_release.py`): a loopback server over HTTP and HTTPS, a failure
  and a power loss injected at every step, the About rows through the provider.
  The PS5 objects compile and add no import the app did not already have.
  **Never run on a console, and never against GitHub**; [updates](UPDATES.md)
  lists what only a console can show.
- `.github/workflows/build.yml` builds version tags and manual runs (pull
  request builds are off), and publishes a tag as a pre-release with
  `PPSA99142.zip` and `SHA256SUMS`. It has not run: the first run shows what a
  hosted runner lacks.

## Implemented and tested

Everything below is in the tree and host-tested. "Implemented" does not mean
accepted on hardware or equivalent to SmartTube in every edge case.

- **Application:** native PS5 EVO playback, interface and decoder integration,
  Lapy storage under `/data/prosperotube`, packaging with the elevation helper.
- **Browsing:** search with options, suggestions and history; Home rows and
  filters; Discover sections including Trending, Shorts and Kids; channel
  sections, sort orders and search in channel; playlists; related videos;
  description, video details and share links with QR codes; menu sections and
  pins; channel blocks; hide-content switches; content language and country.
- **Playback:** AVC, VP9, HEVC and AV1 in SDR; adaptive, progressive and HLS;
  captions in two languages, caption picker, style, translated tracks and
  per-channel memory; quality, frame-rate and codec limits and presets; speed
  memories; seeking modes and intervals; chapters and chapter notification;
  sizing, zoom, aspect, rotation and mirror; volume, auto volume and audio
  delay; sleep timer; screen dimming and screen off; in-player video actions,
  button switches and a suggestions list; seven end-of-video modes; Next and
  Previous with playlist position; recovery and the error fixer; a video
  buffer choice (how far ahead a stream is read).
- **Local state:** profiles with favorites, history, resume, queue, follows and
  preferences (shared or per account); backups, automatic backup and
  subscriptions import; About with links, credits and changes.
- **Integrations:** SponsorBlock actions, review, exclusions and timeline
  colours; DeArrow titles and thumbnails; optional Return YouTube Dislike relay
  (off by default).
- **Accounts:** device-code sign-in and account removal; subscriptions,
  subscribed channels with new-content marks, notifications, history, Watch
  later, playlists and own videos; ratings, subscribe with notification level,
  playlist edits and order, stream reminders; comments with replies and rating;
  live chat and chat replay.
- **Experimental:** phone casting, voice search (development companion only),
  chat and comment posting, account-history pause/resume/clear, feedback
  actions and the profile password. All are host-tested, labelled **(experimental)** in the interface
  and inactive until the user turns them on or selects them.

On 2026-10-11 `bash scripts/test_all.sh` passed all 58 of its steps on the
frozen tree: fixture generation; the library worker build with its 189 Node
tests; 680 Python tests of the companion and tools; the 54 native provider
suites of `scripts/test_native.sh` under AddressSanitizer and
UndefinedBehaviorSanitizer with mocked HTTP; 44 further native, player-overlay
and interface scripts; and 10 Python source checks. The host interface preview
(`scripts/preview_native.sh`, not one of those steps) also passes.

### Parity tracker

[PARITY.md](PARITY.md) (generated from `native/parity/status/*.json`) compares
every audited SmartTube surface. **Done** there means implemented with offline
evidence, not console acceptance.

| Area | Done | Partial | Missing | Not applicable | Total |
| --- | ---: | ---: | ---: | ---: | ---: |
| Player and playback | 41 | 24 | 8 | 5 | 78 |
| Browsing, search and menus | 63 | 15 | 1 | 1 | 80 |
| Accounts, remote control and integrations | 20 | 7 | 1 | 1 | 29 |
| General, interface and maintenance | 26 | 16 | 10 | 6 | 58 |
| **All** | **150** | **62** | **20** | **13** | **245** |

Voice search moved from done to partial on 2026-10-13: the release has no
speech recogniser, so it exists only with the development companion. The
SponsorBlock alternative server had moved to partial for the same reason and
is done again: with the built-in service its address is typed on the console
and sent with the segments request. The built-in service also has the Return
YouTube Dislike relay now, asked only for a profile that turned **Show
dislikes** on; the dislike counts stay partial for where they are shown. The
web proxy stays partial: the built-in service has none, and nothing in its
menus offers one.

The tracker's files were brought up to date on 2026-10-13 for the built-in
service, accounts on the console, the update rows, the interface languages,
the interface preferences and the seek preview. Of the 20 missing items, ten
need hardware work (auto frame rate, background playback, a remembered live
position, card previews), six need a Google sign-in with Drive access (cloud
backup) and four are not built: the player's interface switches, the network
settings group (it would be empty), buttons at the top of the main window and
playlists as rows (the browser has no shelf layout). The video buffer is a
setting now and partial: it sets how far ahead a stream is read, without
SmartTube's back buffer. The 62 partial items are mostly the English player and
the languages that need other fonts, shaping or right-to-left layout, states
and entry points the in-player list of actions lacks, interface preferences
that reach the browser but not the player, and what only the development
companion offers.

## Verified only on the host

The built-in service ran on a console for the Guest core path only (next
section); everything else that depends on it (which is every request the app
makes) is in this section. None of the following has run on a
console. Where an earlier check against the
live service exists it is named; everything else has seen fixtures only.

- Every account feature, now served by the built-in service. Sign-in runs
  against canned answers of Google's device-code endpoints; the account lists
  against hand-made answers in the shapes the pinned parsers expect, which may
  not be what the TV client really returns. No real account has been used with
  this tree.
- VP9, HEVC and AV1 playback: relay, decode and seek with host FFmpeg. PS5
  performance and presentation are open.
- Live chat and chat posting. A Guest chat-replay check against the live
  service passed during development; posting has only met fixtures.
- The in-player actions, suggestions list, chapter notification, screen
  dimming, presets and the error fixer.
- Backups, restore, subscriptions import, About and the QR pages.
- Kids, stream reminders, playlist order, notification levels, video details,
  likes and dislikes, comment posting and new-content marks.
- Phone casting with a real phone. The companion's receiver registered against
  the live service once, with a temporary receiver; the built-in receiver
  speaks the same protocol but has met only a local stand-in, and the cadence
  of YouTube's keep-alive frames against its 65-second silence limit is
  unobserved from this engine.
- Voice search with a real microphone (recognition was tested with real
  whisper.cpp inference on the host, through the development companion; the
  release has no recogniser).
- GPU presentation of zoom, rotation and mirror, long sessions and controller
  feel.
- Recovery when a media link expires during playback, the update check and
  self-update, the interface languages, the interface preferences and the
  drawn seek preview.

The PS5 package of the frozen tree compiles, links and packs (see the
2026-10-11 entry in [validation](VALIDATION.md)); that package has not been
installed or started. The build that ran on 2026-10-13 is a later,
instrumented one.

## Checked on a console

The self-sufficient build, in one bounded Guest session on 2026-10-13 with no
companion running: the built-in service's start, Discover and one topic
through it, a public video resolved on the console and played from YouTube
directly (H.264 1080p on the hardware decoder, AAC), a +10 s seek, pause, the
caption picker and playback options, Stop, the saved history record and a
clean park and close. Not assessed in it: sound and motion by a person,
captions on screen, VP9, AV1, HEVC and 4K, expiry recovery, accounts, phone
casting, updates, the interface languages, the interface preferences and the
seek preview.

Earlier development builds, with the companion, in bounded cases: startup with
storage access, pairing and search; a public video in hardware AVC with
captions and Guest history; pause, resume and seek; public 1080p60 AVC/AAC
confirmed smooth and in sync by the owner; and, with generated media and a
fixture service, resume, speed controls, the SponsorBlock review, the
unavailable-video queue advance, paused dual captions, the chat reader, artwork
decoding and phone commands from a test receiver.

## Inferred about the live service

These contracts in [the library service](LIBRARY_SERVICE.md) rest on layouts
that could not be checked offline. Each sends nothing and returns its stated
error when the layout is not recognised; each needs the live-service check.

- **Trending, Shorts, My videos:** the TV browse identities `FEtrending`,
  `FEshorts` and `FEmy_videos` come from the SmartTube service; their live
  layouts are unverified.
- **Channel sort orders:** recognised by position in a chip bar of two or three
  chips (newest, popular, oldest); any other layout reports no orders.
- **Account history pause, resume and clear:** the control is recognised by its
  menu icon (`PAUSE`, `PLAY`, `DELETE`); the icon names come from the web
  client's layout and were not verified against an account.
- **Hide a Subscriptions entry:** the token is recognised by the menu icon `HIDE`.
- **Stream reminder:** read from the raw `/player` response (one toggle button
  with add/remove upcoming-event reminder endpoints), which the pinned parser
  does not keep.
- **Playlist order:** read from the raw browse response; `playlistVideoOrder`
  1 to 5 is mapped to the five order names.
- **Comment replies:** confirmation expects `actionResult.status:
  "STATUS_SUCCEEDED"`.
- **New-content mark of subscribed channels:** read from the raw `/guide`
  response (`presentationStyle`).
- **Subscriptions import:** the NewPipe, PocketTube and GrayJay formats are
  taken from those applications' export files.
- **Dislikes:** Return YouTube Dislike as SmartTube's source is recalled, not
  read from the reference tree.

More generally, the third batch of contracts (Kids, reminders, playlist order,
details, comment posting, first search result, new-content marks, the proxy
option) has only run against the pinned parser and synthetic responses. Real
authorization, private-account feeds, live cookie validity, account playback
with TV signature challenges, and secondary-account or brand-channel mapping
are unverified.

## Remaining gates, in order

1. **Live-service check of the built-in service on the host: done for Guest**
   (2026-10-11, [validation](VALIDATION.md)). Account contracts wait for the
   owner's sign-in on the console: sign-in itself, then each list, then one
   change of each kind, then history reports.
2. **Frozen candidate: built** from the standalone tree (release and
   instrumented packages, hashes in the validation record).
3. **One bounded console session on the core path: passed** (2026-10-13,
   [hardware acceptance](HARDWARE_ACCEPTANCE.md)), under the shared-console
   protocol (`tools/console/session.py`): startup, the service's start,
   Discover, playback with seek, pause and options, Stop, history, clean park
   and close. It was the first time the engine, the HTTPS client and direct
   playback ran on a console. Its record names Discover and Stop where this
   gate asked for search and the end of a video, and leaves open: sound and
   motion by a person, VP9, AV1 and 4K, long sessions, sign-in.
4. Fix what the session finds, then release.
5. **Phone casting with a real phone**, after the core path holds: link with a
   TV code, cast one video and one queue, control it, leave it idle for some
   minutes, turn it off ([what to look at](REMOTE_SERVICE.md#built-in-receiver)).

Before the first public release the repository has to be public, or the
update check gets no answer ([updates](UPDATES.md)).

Known limits that stay after the release: accounts unverified against the
live service and no playback of videos that need the account, phone casting
unverified with a real phone, no voice search, one YouTube client identity
(`VISIONOS`) with no fallback but HLS, an English player and no language
outside the Latin, Greek and Cyrillic scripts, no auto frame rate, HDR or
background playback.

See [validation](VALIDATION.md), [hardware acceptance](HARDWARE_ACCEPTANCE.md),
[parity](PARITY.md) and [goal](../GOAL.md).
