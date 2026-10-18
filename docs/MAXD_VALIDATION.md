# YouTube Max’d integration validation

Target: PS5 firmware 11.00, ShadowMount+, on-console runtime. App: 1.2.0;
content version: 01.002.000; title: PPSA99991. Pins: `UPSTREAM.json`.

## Local checks

The following checks passed locally on 2026-10-17:

- 210 YouTube library/worker tests.
- Embedded service: 21 engine self-checks, 186 API checks, 232 account checks,
  51 subscriptions-import checks and 302 remote/casting checks, repeated in
  normal and ASan/UBSan builds (792 checks per build).
- Native provider, quality/audio switching, per-profile persistence, codecs,
  SponsorBlock, recovery, account settings/actions and device-code UI checks.
- SponsorBlock player tick/pad integration; release identity/version/ZIP rules.
- Repeated preparation preserves Max’d branding, storage and controller hooks.
- Console observation/session tools: 132 offline checks, including the fork's
  title ID, storage paths and build identifier handling.
- Build orchestration: four checks, including the default pinned shader path,
  optional compiler path, dependency caching and ZIP verification.
- Native FFmpeg 7.1.1/dav1d builds, atempo and checked HLS DVR symbols.
- 81 default video shader headers match the pinned EVO source; transform
  controls are hidden and imported rotation/mirroring bits are masked.
- Native application: all 310 player objects compiled, linked and converted
  into the PS5 executable; its signed plaintext SELF container reports valid
  integrity. The exact-title Lapy helper and 0777 app ZIP checks passed.
- Afterglow host visuals: 638 boards in each of four looks, 2,552 pictures,
  28,112 checks and zero failures. A representative fixture image was inspected.
- Source distribution: file hashes/CRCs, pinned dependency restoration and
  executable permissions checked after extraction with Python's ZIP client.

The native
controller bridge passes ASan/UBSan checks for transitions, steady output,
write throttling, settings, foreground signals, disconnect, LED failure/reconnect
and shutdown. Identity/storage/updater isolation checks pass.

Service and provider regressions use synthetic fixtures and mocked account
responses. They do not sign in a real Google account.

The build uses the public PS5 Payload SDK v0.42, PacBrew v0.39 and Clang
19.1.7. The installed SDL identifies itself as SDL-2.30.12-g0baf4ac49, and
the source archive supplies that matching source and dependency recipes.

A read-only live extraction probe could not reach YouTube: this runner's
HTTPS proxy rejected its CONNECT request with HTTP 403. No live extraction,
Google consent or media playback result can be claimed from that probe.

## Voice-search checks (2026-10-18)

- Real CPU-model recognition of a six-second speech sample; silence rejection,
  wrong-model SHA rejection, cancellation/retry and complete-result copying,
  with the app's C++ integration under ASan/UBSan.
- Local driver: physical-controller ownership, profile retirement, invalid
  queries, capture/recognition failures, timeout and zero network requests.
- Existing companion voice lifecycle and native microphone capture/wipe/restart
  regressions pass under ASan/UBSan.
- Afterglow: seven voice boards in four looks, 28 pictures, 408 checks, no
  failures. Listening and recognition indicators, search results/back, protected
  account/settings routes and indicator cleanup are covered. A host image was inspected.
- Generated input/shortcut regressions pass. Repeated embedded preparation and
  switching to the companion build correctly retire local model/source inputs.
- The generated app input loop is tested between button edges, so L3 reaches
  its hold threshold. Native keyboard, Classic panel and sign-in/start guards
  keep their input ownership. PS5 archives reject desktop process/discovery imports.

The host model test uses public sample audio. It does not capture a real
DualSense microphone or measure PS5 recognition speed. Dependency archives are
optimized CPU builds; the app integration and lifecycle tests use sanitizers.

## Smart playlists, Auto Max and artwork checks (2026-10-18)

- ASan/UBSan policy tests cover AND filtering, duration boundaries, watched/live
  exclusions, text validation, rule limits, JSON persistence, profile ownership,
  and Auto Max warm-up, measurement windows, cooldown and format constraints.
- Shipping-provider tests cover rule editing/clearing, restart persistence,
  account removal, shared-preference isolation, deduplication, fresh multi-page
  feeds and callbacks cancelled by a profile change. Playback windows contain
  only filtered video IDs; an active snapshot survives a refreshed result list.
- Auto Max provider checks cover retained position/captions after reopening,
  settings persistence, cooldown, the startup format picker, manual quality
  pins, live streams and profile retirement. Existing quality/audio switching,
  profile settings, playlist-window, section/pin ordering, native provider and
  account-action regressions pass.
- Seven new Afterglow boards in four looks pass 400 checks. The wider Library
  run identified an expectation for the old six-tab layout; the Favorites
  reduced-motion board now uses five tab advances. The seventh-tab edge is
  covered by a new Smart playlist board. Representative rule and Library
  images were inspected.
- The red-to-white controller bridge passes the native feedback checks.
  The approved icon is retained as a master and a 512-pixel launcher asset;
  generated native code matches 624 freshly prepared source inputs. Existing
  held-L3 dispatch and voice input ownership checks still pass.

See [feature usage and limits](MAXD_FEATURES.md). All feed and playback
adaptation checks use fixtures; they do not measure real PS5 playback.

## Required console acceptance

No PS5 is attached to this workspace. The following remain unverified for this
exact integrated build:

- Firmware 11.00 / ShadowMount+ boot, Lapy startup, persistence and clean exit.
- Live search/extraction; 720p/1080p/1440p/2160p AVC/VP9/HEVC playback; seeking,
  quality switching, replay and audio synchronization.
- AV1 throughput, HDR, frame-rate switching and recovery. Rotation/mirroring is omitted from
  default builds; its optional shader path requires separate acceptance.
- TV consent, refresh after restart, account feeds and removal/revocation.
- Physical DualSense colors, reconnect, system-overlay focus, switch and exit.
- Local voice capture, mute/reconnect, L3 hold/release, six-second auto-stop,
  Circle cancellation, recognition speed/memory, profile switching and shutdown
  while recognizing. See [voice acceptance](VOICE_SEARCH.md).
- Live DVR, captions, casting and background audio on the console's network.
- Smart playlist refresh/next/previous in both interfaces, real account and
  local watched markers, slow scans and profile changes during a refresh.
- Auto Max decisions under real network/decoder pressure, quality/FPS/codec
  limits, manual overrides and position/audio/caption continuity after a switch.

The bridge borrows EVO's pad and opens no second handle. Upstream records in
`docs/HARDWARE_ACCEPTANCE.md` apply to upstream builds, not to this build or
firmware 11.00. Fixture screenshots are not live YouTube or console captures.
