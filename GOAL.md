# ProsperoTube goal

The long-term product goal is a 1:1 equivalent SmartTube port for
**PlayStation 5**, originally confirmed on 2026-10-09. The active task is the
first release under the owner's later priorities below. Full parity remains
tracked separately and must not be claimed from an alpha release.

## Current first-release priorities (owner decisions through 2026-10-15)

The active release task is to finish essential engineering for the initial
Guest alpha. Full SmartTube parity remains the long-term target; the older
full-parity acceptance inventory below must not be treated as a requirement
to implement every future feature before this first release.

The owner's explicit playback priorities are live DVR rewind and position
restore, automatic frame-rate switching, and HDR selection/output. Sustained
4K reliability and the historical GPU faults require investigation, not just
a request for human sign-off. Read-only comments are sufficient; writing
comments is excluded. The missing-glyph and physical PS5 keyboard follow-ups
are also part of the current candidate. Use Afterglow for fixes and tests;
do not substitute Classic to avoid a modern-interface bug.

Use only the currently authorized test console under the shared-console
protocol, with a running-title check first. The owner has authorized bounded
candidate tests; those later instructions supersede the older blanket ban on
intermediate console candidates below. Never interrupt an unrelated title or
replace a build during the owner's replay. Package ZIP only, without FFPFSC,
and push current source/documentation to GitHub. Public release/tag/catalog
publication is a separate owner decision.

Current proof and remaining gaps live in [Status](docs/STATUS.md) and
[Handoff](docs/HANDOFF.md). Preserve the distinction between an implementation,
a bounded automated result, and physical acceptance. In particular, the
36-second VP9 identity fix result is not proof of sustained playback, all
stream formats, or the cause of every historical GPU fault.

## First release scope (owner decisions, 2026-10-11)

Full SmartTube parity stays the long-term target, tracked in
[docs/PARITY.md](docs/PARITY.md).

**Self-sufficient on the console is a hard requirement** (owner, 2026-10-11,
replacing the earlier scope of the same day that required a LAN companion). The
released application must work with nothing but the PS5 and an internet
connection. The PC companion remains in the repository as a development tool
only; no release may depend on it.

- **First MVP: Guest, standalone.** Search with filters, Home (as far as
  YouTube serves a Guest), Discover sections, channels, playlists, comments
  read-only, playback with captions, quality and speed, local history and
  resume, favorites, queue, followed channels, SponsorBlock and DeArrow, the
  settings already implemented, English interface.
- **Next: accounts on the console.** Sign-in by device code, subscriptions,
  library, ratings and playlists, served by the built-in service.
- **Then the rest:** phone casting, voice search, subscriptions import, live
  chat and comment posting, account-history controls, feedback actions;
  localization, remaining interface options, drawn seek previews, update check
  and self-update (which must also be able to replace the service bundle),
  auto frame rate, HDR, background playback.
- Code for features outside the MVP stays in the tree and inactive; the
  interface does not offer what cannot work.

Sequence: build and freeze a standalone candidate; run one bounded console
session on the core path (start of the service, search, play, pause, seek,
options, end, history, clean close); fix and release. Use the console as little
as possible: other agents share it. Always check that nothing is running
first; either console may be used, whichever is free.

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
