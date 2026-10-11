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
- Resize, zoom, aspect correction, rotation and mirror paths; compiled transform shaders.

Implemented does not mean accepted on hardware or equivalent in every SmartTube edge case.

## Tested

- Earlier PS5 packages played public YouTube with captions/history; the user confirmed smooth, synchronized 1080p60 AVC/AAC Big Buck Bunny playback.
- Scoped PS5 cases cover local unavailable-video queue advancement and paused dual-caption selection/rendering/clear with speed-reopen retention. Moving timing after that reopen remains unverified.
- At `09873cd`, the complete Python suite passed 518 tests and five subtests, with one existing dependency deprecation warning.
- Latest worker baseline: 123 tests and bundle build passed. Recent native suites and PS5 SDK provider compilation passed within their documented scope.
- The full native regression was interrupted for the requested pause. No complete full-native pass is claimed.
- Recent browsing-group/filter/progress changes have no console acceptance. No console was contacted during this offline phase.

## Still missing

- Standalone/on-console service and SABR/proof-token integration; companion dependence remains.
- Remaining SmartTube settings/localization, Home/suggestion layouts, group/recommendation semantics and alternate selection around blocked recommendations.
- Secondary-account/brand-channel context and broad real-account/history/localized-layout validation.
- Real phone, microphone and physical-controller acceptance.
- Broader codec/HDR/live/DVR, recovery/timing, GPU transform presentation and long-session acceptance.
- Complete native/integrated regression, a rebuilt and frozen final package, then bounded PS5 acceptance. The last fully frozen package is `c9721611`; subsequent scoped checks do not refresh that package.
- UI polish after YouTube functionality is accepted.

Complete all remaining offline implementation and final-candidate packaging before any console connection. Both consoles require the shared lock and running-title protocol when that gate is met.

See [validation](VALIDATION.md), [hardware acceptance](HARDWARE_ACCEPTANCE.md), [goal](../GOAL.md) and [plan](../IMPLEMENTATION_PLAN.md).
