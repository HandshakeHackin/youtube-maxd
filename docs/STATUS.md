# Current implementation status

Updated 2026-10-11. Features are frozen for release 0.1.0. Full SmartTube
equivalence stays the long-term target ([goal](../GOAL.md)); this page states
what the first release contains, how far each part was checked, and what is
left before it ships. The LAN companion is required.

Three levels of evidence are kept apart on this page:

- **host-tested**: covered by the offline suite on a PC (`bash scripts/test_all.sh`);
- **checked on a console**: ran on a PS5, in the bounded cases recorded in
  [hardware acceptance](HARDWARE_ACCEPTANCE.md);
- **inferred**: built from what the pinned libraries and the reference sources
  suggest about the live service, with synthetic responses in the tests.

## First-release scope

Owner decision of 2026-10-11, from the [goal](../GOAL.md):

- **Core:** Guest search with filters, Home, Discover sections, channels,
  playlists, playback with captions, quality and speed, history and resume, queue.
- **Integrations:** SponsorBlock and DeArrow.
- **Accounts:** sign-in, subscriptions, library, ratings and playlists.
- **Settings:** those already implemented. English interface.
- **Experimental (labelled, off until used):** phone casting, voice search,
  live chat and comment posting, account-history controls, feedback actions,
  account password.
- **Deferred:** localization, remaining interface options, drawn seek previews,
  update check and self-update, the standalone on-console service, auto frame
  rate, HDR and background playback.

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
  Previous with playlist position; recovery and the error fixer.
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
- **Experimental:** phone casting, voice search, chat and comment posting,
  account-history pause/resume/clear, feedback actions and the profile
  password. All are host-tested, labelled **(experimental)** in the interface
  and inactive until the user turns them on or selects them.

On 2026-10-11 `bash scripts/test_all.sh` passed all 58 of its steps on the
frozen tree: fixture generation; the library worker build with its 189 Node
tests; 680 Python tests of the companion and tools; the 54 native provider
suites of `scripts/test_native.sh` under AddressSanitizer and
UndefinedBehaviorSanitizer with mocked HTTP; 44 further native, player-overlay
and interface scripts; and 10 Python source checks. The host interface preview
(`scripts/preview_native.sh`, not one of those steps) stops at an assertion
about the review footer; its harness predates the newer footers and is open.

### Parity tracker

[PARITY.md](PARITY.md) (generated from `native/parity/status/*.json`) compares
every audited SmartTube surface. **Done** there means implemented with offline
evidence, not console acceptance.

| Area | Done | Partial | Missing | Not applicable | Total |
| --- | ---: | ---: | ---: | ---: | ---: |
| Player and playback | 41 | 23 | 9 | 5 | 78 |
| Browsing, search and menus | 62 | 15 | 2 | 1 | 80 |
| Accounts, remote control and integrations | 19 | 7 | 2 | 1 | 29 |
| General, interface and maintenance | 18 | 9 | 25 | 6 | 58 |
| **All** | **140** | **54** | **38** | **13** | **245** |

The open items are mostly interface options, localization, update checks and
hardware-dependent features, which the first release defers.

## Verified only on the host

None of the following has run on a console. Where an earlier check against the
live service exists it is named; everything else has seen fixtures only.

- Every account feature. Sign-in runs against queued responses and a
  deterministic clock; no real account has been used with this tree.
- VP9, HEVC and AV1 playback: relay, decode and seek with host FFmpeg. PS5
  performance and presentation are open.
- Live chat and chat posting. A Guest chat-replay check against the live
  service passed during development; posting has only met fixtures.
- The in-player actions, suggestions list, chapter notification, screen
  dimming, presets and the error fixer.
- Backups, restore, subscriptions import, About and the QR pages.
- Kids, stream reminders, playlist order, notification levels, video details,
  likes and dislikes, comment posting and new-content marks.
- Phone casting with a real phone (the companion's receiver registered against
  the live service once, with a temporary receiver), and voice search with a
  real microphone (recognition was tested with real whisper.cpp inference on
  the host).
- GPU presentation of zoom, rotation and mirror, long sessions and controller
  feel.

The PS5 package of the frozen tree compiles, links and packs (see the
2026-10-11 entry in [validation](VALIDATION.md)); that package has not been
installed or started.

## Checked on a console

Earlier development builds only, in bounded cases: startup with storage access,
pairing and search; a public video in hardware AVC with captions and Guest
history; pause, resume and seek; public 1080p60 AVC/AAC confirmed smooth and in
sync by the owner; and, with generated media and a fixture service, resume,
speed controls, the SponsorBlock review, the unavailable-video queue advance,
paused dual captions, the chat reader, artwork decoding and phone commands from
a test receiver. Nothing added on 2026-10-11 has run on a console.

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

1. **Live-service check from the companion host.** Run the service against
   live YouTube (Guest first, then one account): the core path, then each
   inferred contract above. No console is involved.
2. **Frozen candidate.** Rebuild after any fix from gate 1, run
   `scripts/test_all.sh`, and record the package hashes.
3. **One bounded console session on the core path**, under the shared-console
   protocol: idle check first, verified install, startup, pairing, search,
   Home, playback with captions, quality and speed, history and resume, queue,
   clean close. Experimental features are outside this session.
4. Fix what the session finds, then release.

Known limits that stay after the release: the companion is required; no
standalone service or SABR/proof-token integration on the console; English
only; no auto frame rate, HDR or background playback.

See [validation](VALIDATION.md), [hardware acceptance](HARDWARE_ACCEPTANCE.md),
[parity](PARITY.md), [goal](../GOAL.md) and [plan](../IMPLEMENTATION_PLAN.md).
