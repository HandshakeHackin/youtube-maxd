# Current implementation status

Updated 2026-10-11. Full SmartTube equivalence on PS5 remains incomplete.
Implementation has resumed offline after the recap. The LAN companion is required.
This document summarizes current evidence; older candidate entries are historical.

## Current checkpoint

Latest implementation commit: [`3420686`](https://github.com/blackbearreloaded/ProsperoTube/commit/3420686fc4fe84af27fd002d08509a40493b99cf),
on `codex/first-version` in [draft PR #1](https://github.com/blackbearreloaded/ProsperoTube/pull/1).
The validation below covers that implementation; it is not a new frozen release.

The phone fallback adapter from `891ddf8` now has provider lookup and application
callbacks. Remote-only next metadata can return a verified suggestion group; native
traversal skips blocked identities and names without changing saved queue records.
Host integration covers receiver commands through EOF, service jobs and a new launch
ticket. Full occurrence-context parity still needs work: the callback currently
returns a video ID and does not carry a returned playlist occurrence or permit a
same-ID successor at a distinct playlist position. No new release package is ready.

## Implemented

- Native PS5 EVO playback/UI/decoder integration, Lapy storage under `/data/prosperotube`, packaging and clean-lifecycle tooling.
- Search, topics, channel/playlist browsing, profiles, favorites, history/resume and persistent queues.
- Account/library actions, subscriptions, ratings, playlist edits and watched-state management.
- Seeking, speed/quality/language preferences, recovery, dual captions, seven completion modes and occurrence-preserving Next/Previous navigation.
- SponsorBlock, DeArrow, comments, live chat/replay, phone-control and voice-search integration.
- Profile-owned channel ID/name blocking, retained browsing groups, Home watched filtering and catalog progress/resume propagation.
- Related-video browsing from the native actions menu, with watched filtering, profile-owned paging and occurrence-preserving group navigation.
- Related-video chip filters and supported nested shelves, plus Home rows and primary/secondary filters, with expiring profile-owned folders and independent filtered/section navigation.
- Resize, zoom, aspect correction, rotation and mirror paths; compiled transform shaders.

Implemented does not mean accepted on hardware or equivalent in every SmartTube edge case.

## Tested

- Earlier PS5 packages played public YouTube with captions/history; the user confirmed smooth, synchronized 1080p60 AVC/AAC Big Buck Bunny playback.
- Scoped PS5 cases cover local unavailable-video queue advancement and paused dual-caption selection/rendering/clear with speed-reopen retention. Moving timing after that reopen remains unverified.
- At `09873cd`, the complete Python suite passed 518 tests and five subtests, with one existing dependency deprecation warning.
- Latest worker baseline: 145 tests and bundle build passed, including remote-only fallback discovery and verified channel-name replacement. The targeted Python request/library/occurrence suite passes 55 tests. Recent native suites and PS5 SDK provider compilation passed within their documented scope.
- After resuming, all 31 default native ASan/UBSan cases passed, including groups, progress, queues and channel-name blocks. The following related-video/filter batches passed four scoped native suites, PS5 provider compilation and expanded actual RmlUi folder/video selection; the full baseline predates that batch. Transient empty-row and empty-width warnings remain in the UI log.
- Recent browsing-group/filter/progress changes have no console acceptance. No console was contacted during this offline phase.
- The latest phone adapter/real-driver suite passes with ASan, UBSan and leak detection. Four scoped provider suites also pass, including integrated phone EOF through verified fallback to a launch ticket with unchanged saved records. PS5 SDK compilation passes for the provider, adapter, generated Application, ProviderHostScreen and PlayerScreen objects, with the existing 280 provider math-header warnings. Hardware/live-service acceptance remains open.

## Still missing

- Standalone/on-console service and SABR/proof-token integration; companion dependence remains.
- Remaining SmartTube settings/localization, other Home/suggestion layouts and controls, group/recommendation semantics and remote-playback fallback selection. The pinned baseline only substitutes a suggestion when a remote video's next metadata is absent; generic replacement of blocked autoplay is not established by that source.
- Secondary-account/brand-channel context and broad real-account/history/localized-layout validation.
- Preserve returned recommendation playlist contexts and distinct same-ID successor occurrences through the phone handoff; real phone, microphone and physical-controller acceptance.
- Broader codec/HDR/live/DVR, recovery/timing, GPU transform presentation and long-session acceptance.
- Remaining integrated/candidate checks, a rebuilt and frozen final package, then bounded PS5 acceptance. The last fully frozen package is `c9721611`; subsequent scoped checks do not refresh that package.
- UI polish after YouTube functionality is accepted.

Complete all remaining offline implementation and final-candidate packaging before any console connection. Both consoles require the shared lock and running-title protocol when that gate is met.

See [validation](VALIDATION.md), [hardware acceptance](HARDWARE_ACCEPTANCE.md), [goal](../GOAL.md) and [plan](../IMPLEMENTATION_PLAN.md).
