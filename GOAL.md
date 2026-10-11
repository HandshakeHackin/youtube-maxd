# ProsperoTube goal

Implement a 1:1 equivalent SmartTube port for **PlayStation 5**. Stop only when
the full objective is implemented and verified. This is the current user-approved
goal, confirmed on 2026-10-09; the 0.1.0 prototype is an intermediate milestone.

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

Current instruction (2026-10-11): continue implementation and use the newly
reserved console for bounded hardware validation. The user explicitly renewed
console testing authorization and reserved it exclusively for this work. Other
consoles remain outside this session's authorization.

Use `ps5-homebrew-dev-protocol` for bounded hardware cases. Preserve exact
candidate hashes, use the shared console lock, avoid unrelated running titles,
and stop on a service/health anomaly. Keep private environment details and raw
logs ignored. Continue independent offline work between hardware cases.

Before every future console session, check the shared lock locally and make a
read-only running-title check the first console operation. If any title is
running, including ProsperoTube, stop console work and continue offline. Do not
probe other services, deploy, launch, close a title or send input before the
idle state is established. Recheck immediately before launching after an install.

Current progress and limitations: [README](README.md),
[library service](docs/LIBRARY_SERVICE.md), [build](docs/BUILD.md),
[hardware acceptance](docs/HARDWARE_ACCEPTANCE.md), and
[reference candidates](docs/REFERENCE_CANDIDATES.md).
