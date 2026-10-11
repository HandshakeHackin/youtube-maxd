# Current implementation status

Updated 2026-10-11. Full SmartTube equivalence on PS5 remains incomplete.
Implementation has resumed offline after the recap. The LAN companion is required.
This document summarizes current evidence; older candidate entries are historical.

## Implemented

- Native PS5 EVO playback/UI/decoder integration, Lapy storage under `/data/prosperotube`, packaging and clean-lifecycle tooling.
- Search, topics, channel/playlist browsing, profiles, favorites, history/resume and persistent queues.
- Account/library actions, subscriptions, ratings, playlist edits and watched-state management.
- Seeking, speed/quality/language preferences, recovery, dual captions, seven completion modes and occurrence-preserving Next/Previous navigation.
- SponsorBlock, DeArrow, comments, live chat/replay, phone-control and voice-search integration.
- Profile-owned channel ID/name blocking, retained browsing groups, Home watched filtering and catalog progress/resume propagation.
- Related-video browsing from the native actions menu, with watched filtering, profile-owned paging and occurrence-preserving group navigation.
- Related-video chip filters, with expiring profile-owned folders and independent filtered-feed navigation.
- Resize, zoom, aspect correction, rotation and mirror paths; compiled transform shaders.

Implemented does not mean accepted on hardware or equivalent in every SmartTube edge case.

## Tested

- Earlier PS5 packages played public YouTube with captions/history; the user confirmed smooth, synchronized 1080p60 AVC/AAC Big Buck Bunny playback.
- Scoped PS5 cases cover local unavailable-video queue advancement and paused dual-caption selection/rendering/clear with speed-reopen retention. Moving timing after that reopen remains unverified.
- At `09873cd`, the complete Python suite passed 518 tests and five subtests, with one existing dependency deprecation warning.
- Latest worker baseline: 130 tests and bundle build passed, including related-video chip filters and independent navigation. Recent native suites and PS5 SDK provider compilation passed within their documented scope.
- After resuming, all 31 default native ASan/UBSan cases passed, including groups, progress, queues and channel-name blocks. The following related-video/filter batches passed four scoped native suites, PS5 provider compilation and expanded actual RmlUi folder/video selection; the full baseline predates that batch. Transient empty-row and empty-width warnings remain in the UI log.
- Recent browsing-group/filter/progress changes have no console acceptance. No console was contacted during this offline phase.

## Still missing

- Standalone/on-console service and SABR/proof-token integration; companion dependence remains.
- Remaining SmartTube settings/localization, Home/suggestion sections and layouts, group/recommendation semantics and remote-playback fallback selection. The pinned baseline only substitutes a suggestion when a remote video's next metadata is absent; generic replacement of blocked autoplay is not established by that source.
- Secondary-account/brand-channel context and broad real-account/history/localized-layout validation.
- Real phone, microphone and physical-controller acceptance.
- Broader codec/HDR/live/DVR, recovery/timing, GPU transform presentation and long-session acceptance.
- Remaining integrated/candidate checks, a rebuilt and frozen final package, then bounded PS5 acceptance. The last fully frozen package is `c9721611`; subsequent scoped checks do not refresh that package.
- UI polish after YouTube functionality is accepted.

Complete all remaining offline implementation and final-candidate packaging before any console connection. Both consoles require the shared lock and running-title protocol when that gate is met.

See [validation](VALIDATION.md), [hardware acceptance](HARDWARE_ACCEPTANCE.md), [goal](../GOAL.md) and [plan](../IMPLEMENTATION_PLAN.md).
