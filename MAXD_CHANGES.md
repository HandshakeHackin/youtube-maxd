# YouTube Max’d integration

## Unreleased

The next release is intended to be the first official build. PS5 acceptance
testing is still outstanding; the current download remains an experimental preview.

- Optional Ko-fi support under Settings → About, with a readable link and phone QR code in Classic and Afterglow. Uses the same `https://ko-fi.com/handshakehackin` destination as Nuvio Max’d.
- Add the matching Ko-fi homepage badge and GitHub Sponsor link.

## 1.3.0-preview.1 — 2026-10-18

- Search available video captions and jump to a line; save named, profile-owned timestamp bookmarks.
- Optional bottom-right in-app PiP while Afterglow browses. Off by default; explicitly select it per session.
- Extend local hold-to-talk recognition with deterministic playback commands.
- Build unwatched viewing sessions from a bounded feed scan and a chosen runtime budget.
- Add four independent signed-out family slots with local names, parent PIN controls, approved-channel playback and daily viewing limits.
- Host an opt-in LAN party page on the PS5, with expiring invites, guest votes and host-approved queue additions.
- Raise the installable content version to `01.003.000` while retaining title ID `PPSA99991` and existing app data.
- Keep linked Google supervised-profile discovery as research. This is an experimental preview; PS5 hardware acceptance remains outstanding.

Usage and limits: [MAXD_NEXT.md](docs/MAXD_NEXT.md).

## 1.2.0-preview.1 — original standalone preview

This fork is based on ProsperoTube commit
`3446f364172c299a17cecf29a3efcf23c8463f68`, copyright (C) 2026 BlackBearReloaded,
and its pinned EVO Player and dependencies. It remains GPL-3.0-or-later.
Upstream credits, licenses, source pins and corresponding build overlays are
retained. Additional native components keep the licenses documented in
`THIRD_PARTY_NOTICES.md`.

Local changes made on 2026-10-17:

- YouTube Max’d launcher identity, title PPSA99991, content version 01.002.000,
  preserved development content ID, icon and coral/pink/violet palette.
- Independent state under `/data/youtube-maxd`, and title-bound helper identity.
- A native bridge for the original Max’d menu/loading/play/pause/sign-in/error
  light-bar effects, using EVO's existing pad handle and feedback setting.
- Persisted per-profile maximum quality choices from 144p through 2160p, alongside
  the existing per-video quality/audio-track picker and FPS/codec preferences.
- Upstream automatic updates disabled for this fork until an independent
  release source is configured. Upstream update code is retained for source
  completeness and testing; it is not activated by the application.
- Upstream shell splash pictures and shell selection music omitted; existing
  Max’d icon used in launcher and classic header artwork.
- Source preparation restores every pinned input before reapplying overlays;
  repeated builds preserve the fork's identity without stale branding anchors.
- Default builds retain EVO's pinned video shaders and hide rotation/mirroring
  controls. Stored transform bits are masked until the optional matching shader
  path is enabled. Normal scaling, aspect, zoom and HDR remain included.
- Optional host shader tooling uses LLPC's standalone build, avoiding unrelated desktop
  driver dependencies. Host compiler optimization is disabled for bootstrapping;
  shader optimization and gfx1013 metadata validation remain enabled.
- Accelerated account/casting fixture clocks leave enough time for checks on
  loaded build machines. Production consent/polling/retry behavior is unchanged.

The on-console service, TV OAuth, catalog/account integrations, media player,
hardware decoding, dav1d, captions, chapters, quality/audio controls, local
profiles/history/queues, SponsorBlock, DeArrow and optional casting derive from
the credited upstream implementation. The account flow no longer needs a
developer Google Cloud configuration. Existing prototype Data API tokens are
not imported; sign in again through the TV activation code.

No private Sony SDK, console firmware module, Google password or account token
is supplied. Console playback, sign-in and actual DualSense effects still need
testing on the intended PS5 firmware 11.00 / ShadowMount+ setup.

Local voice-search changes made on 2026-10-18:

- Bundled multilingual Whisper tiny recognition with pinned whisper.cpp 1.9.5;
  CPU-only inference runs on a worker using four threads. No companion or
  speech-service credentials are required. Model integrity is checked before use.
- Hold L3 for 350 ms, speak after Listening appears, release to search; Circle
  cancels. Both Classic and Afterglow show persistent status. Short L3 actions
  and comment refresh remain available. Playback pauses before recording.
- Six-second capture limit, silence/error handling, UTF-8 query validation,
  profile retirement, timeout and shutdown cleanup. Local microphone activation
  rejects remote owners. Recognized text uses the normal search/history path.
- Coral/pink/violet feedback covers listening and recognition. Engine/model
  licenses, corresponding source and model manifest ship with the build.
- Real-model and lifecycle host tests are included. Firmware 11.00 microphone
  routing, inference speed and physical controller behavior remain unverified.

Smart playlists, Auto Max and approved artwork changes on 2026-10-18:

- Local, profile-owned Smart playlist rules: Home, Subscriptions, Favorites or
  Play queue sources; title/creator phrases, duration, unwatched and live rules.
  Bounded fresh scans deduplicate matches and retain a filtered playback snapshot.
  Rule prompts, profile changes, cancellation, persistence and local deletion
  follow the native provider's existing flows in Classic and Afterglow.
- Optional Auto Max uses sustained buffering and atomic frame counters to
  select a suitable returned format. It respects saved quality/FPS/codec limits,
  manual selection, live streams, playback ownership and recovery scope, and
  reopens at the current position with retained audio/caption state.
  Warm-up, cooldown, stable upgrades and per-video limits bound its changes.
- The user-approved red-to-white icon replaces the earlier red/purple artwork.
  The launcher, Afterglow mark and DualSense effects now share red, soft red
  and white. Console chassis LEDs are not controlled by this build.
- Host policy/provider and UI tests cover the new paths. Console performance,
  real feed refreshes and physical lighting still need hardware acceptance.
  See `docs/MAXD_FEATURES.md` for usage, limits and acceptance steps.
