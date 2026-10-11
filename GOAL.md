# ProsperoTube goal

Implement a 1:1 equivalent SmartTube port for **PlayStation 5**. Stop only when
the full objective is implemented and verified. This is the current user-approved
goal, confirmed on 2026-10-09; the 0.1.0 prototype is an intermediate milestone.

## First release scope (owner decision, 2026-10-11)

Full SmartTube parity stays the long-term target, tracked in
[docs/PARITY.md](docs/PARITY.md). The first release is limited so that what
ships has been exercised on a console and against the live service:

- **Core:** Guest search with filters, Home, Discover sections, channels,
  playlists, playback with captions, quality and speed, history and resume, queue.
- **Integrations:** SponsorBlock and DeArrow.
- **Accounts:** sign-in, subscriptions, library, ratings and playlists.
- **Settings:** those already implemented. English interface. The LAN companion
  is required and stated as such.
- **Experimental (off by default or labelled):** phone casting, voice search,
  live chat and comment posting, account-history controls, feedback actions,
  account password.
- **Deferred to later releases:** localization, remaining interface options,
  drawn seek previews, update check and self-update, the standalone on-console
  service, and features that need hardware work (auto frame rate, HDR,
  background playback).

Sequence: finish and merge the batches in flight, then freeze features; check
the service against live YouTube from the companion host; build and freeze a
candidate; run one bounded console session on the core path; fix and release.
Use the console as little as possible: other agents share it. Always check that
nothing is running first; either console may be used, whichever is free.

Reuse the existing PS5 ecosystem: EVO Player is the integrated playback, UI,
decoder, FFmpeg and packaging foundation. ProsperoLight, ProsperoTV and the PS5
video/audio research provide reference implementations and measured contracts.
Prefer existing PS5 library recipes. Search other public projects when useful.

Store app state, cache and diagnostics under `/data/prosperotube`. Use Lapy's
verified filesystem elevation and exact-title packaged helper, following the
existing ProsperoLight/ProsperoTV tooling. Do not use transient download storage
as a persistence fallback. Finish YouTube functionality before polishing the UI.

The comparison baseline is SmartTube revision
`2c60453007dfc3775ff525c3837c2d1d4e95aab9`. Its initial inventory is in
`native/parity/smarttube-baseline.json`; this is not an exhaustive acceptance
specification. Every setting and behavior requires matching implementation and
evidence. A menu entry or passing narrow unit test does not establish parity.

Required acceptance covers browsing/search and profiles; authentication and
account state; playback, formats, timing and recovery; captions and language
selection; queue/history/subscriptions/playlists; comments/live chat and feedback;
SponsorBlock/DeArrow and related integrations; controller/remote/casting behavior;
configuration, localization and persistence; packaging, clean lifecycle and
console validation. Hardware-dependent limits must be measured and disclosed,
not silently removed from the objective.

Current instruction (2026-10-11): implement all remaining features offline,
complete offline tests and packaging, and prepare the final candidate before
returning to the console. **Do not connect to either console for intermediate
candidates**, including read-only checks. Intermediate frozen builds are internal
milestones and do not satisfy this gate. Continue the full SmartTube objective;
hardware acceptance remains separate and must not be silently assumed.

Once all remaining offline work is complete and the final candidate is ready,
use the user's shared console for bounded validation under
`ps5-homebrew-dev-protocol`. Preserve exact candidate hashes, use the shared
console lock, avoid unrelated running titles, and stop on a service/health
anomaly. Keep private environment details and raw logs ignored.

Before every future console session, check the shared lock locally and make a
read-only running-title check the first console operation. If any title is
running, including ProsperoTube, stop console work and continue offline. Do not
probe other services, deploy, launch, close a title or send input before the
idle state is established. Recheck immediately before launching after an install.

Current progress and limitations: [README](README.md),
[library service](docs/LIBRARY_SERVICE.md), [build](docs/BUILD.md),
[hardware acceptance](docs/HARDWARE_ACCEPTANCE.md), and
[reference candidates](docs/REFERENCE_CANDIDATES.md).

Console ownership update (2026-10-11): another agent also uses the fallback console.
The previous exclusive reservation no longer applies. Always respect the shared
local lock and check running titles before use; if busy, leave it untouched.
This does not lift the final-candidate-only console gate.
