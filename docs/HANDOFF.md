# Handoff notes

For the next person or agent who picks up ProsperoTube. Read this first, then
[Status](STATUS.md), the [README](../README.md) roadmap and
[Afterglow's architecture](afterglow/ARCHITECTURE.md). Written 2026-10-14.

## Where things stand

- `main` holds everything. The repository is **private**; it goes public only
  when the owner says so after reviewing the README. The history is not to be
  squashed (owner's decision).
- No version has been released. 0.1.0 (`01.000.000`) waits until every
  Afterglow screen is built and validated on a console
  ([decision](DECISIONS.md)). No tag, version bump or release without the
  owner's word.
- The app is self-sufficient on the console: a built-in YouTube service
  (QuickJS-NG running YouTube.js, `standalone/`) answers the provider on
  `127.0.0.1`; playback is EVO Player patched at build time by
  `scripts/prepare_native.py` and the `scripts/*_overlay.py` files. The PC
  companion in `companion/` is a development tool only.
- Two interfaces: Classic (RmlUi, the default) and Afterglow (the owner's UI
  kit, `native/afterglow/`), chosen in YouTube preferences > Interface, or
  forced with an empty file `afterglow.on` / `afterglow.off` in
  `/data/prosperotube`.
- `GOAL.md` describes the scope of 2026-10-11 (companion era) and is out of
  date; the README roadmap and this file supersede it.

### What the owner checked by hand on a PS5 (2026-10-14)

Afterglow drew; Guest Home with real video shelves, opening a topic (with
thumbnails), search with suggestions while typing, About, the player screen
with a spinner while a video starts, several videos played with seeking, the
new icon, backgrounds and selection music. No EVO Player notice at start.

### Fixed in the last session, worth knowing

- **YouTube refuses open-ended ranges since 2026-10-14.** A request for a
  whole media file (`Range: bytes=0-`) returns 403. The provider now asks the
  service for ranged manifests: each MP4 track's fragments as small byte ranges
  read from its index (`native/tube_provider.c`, `standalone/js/resolve.js`).
  The continuation now also indexes WebM and fixes FFmpeg's range-boundary
  EOF handling. VP9 1080p with AAC passed on PS5; VP9/Opus generated fixtures
  pass on the host. 4K and HDR acceptance remain pending.
- **Lapy elevation follows ProsperoEden's current method** (helper built with
  upstream's `make owned-helper`, manifest shipped unchanged, packaged copy
  checked, every outcome logged). ProsperoTube deliberately differs in two
  ways: it needs `/data` to be listable, and it has no sandbox fallback (it
  stops at startup without access). See `native/elevation/README.md`.
- **EVO's packager copies a fixed list of `sce_sys` pictures**; the overlay in
  `prepare_native.py` adds the DDS backgrounds to it. The owner's art and music
  are in `native/branding/` (`icon0.png`, `pic0.dds`, `pic1.dds`, `snd0.at9`).
- Code patched into EVO's `ui_rml/src/evo_rmlui_provider.cpp` is also compiled
  by PC tests where `EVO_AGC_DEVICE` is not defined: guard calls into the
  Afterglow header with it.

## What to do next

In the owner's order of interest (he asked what is missing; the list is the
README's **Roadmap**). The two recommended first:

1. **VP9 and Opus again**: ranged manifests for WebM tracks (Cues give the
   byte ranges), so 4K VP9 and HDR come back. Implementation now enables WebM
   indexing and fixes FFmpeg's zero-byte range-boundary read, which caused
   the seek hang. Generated MP4/WebM seeks pass on the host and VP9 1080p
   passed on PS5 (ec4660ac; HARDWARE_ACCEPTANCE.md). Opus on the console,
   4K performance and HDR selection/output remain to validate or implement.
   The combined candidate 93b2e0c7 passed native VP9 1080p again, then faulted
   on the GPU after the first valid 4K frame of a second video. Local hardware
   decoding research exposed a frame-pool reuse race; explicit leases now hold
   native surfaces through GPU retirement, with generated ownership and package
   checks passing. The 8c8d7b15, 0184b32d and 88c154a6 retests passed 1080p,
   then faulted on retired 1080p decoder allocations after a first 2160p
   surface; bounded close draining alone was insufficient. The ca173808
   candidate instead kept one VP9 decoder at a 4K-capable maximum: 1080p and
   full 3840x2160 shared its frame pool with no resize. The full-width frame
   reached the presenter, then a different GPU unmapped read outside the
   decoder pools stopped playback. A later diagnostic build with address logs
   but unchanged playback logic opened full 3840x2160 and published 52 frames
   with zero late drops and no GPU fault. That short observation does not
   explain the earlier first-display fault or accept sustained 4K; sound was
   unjudged and Afterglow stop/return checks failed. Every case restored state
   and left the console idle. The AGC display path is the current investigation.
   Later .30 cases reproduced an unmapped GPU read near `0x2a5` even with
   AGC fence retirement and with VP9 close/open Reset omitted. The latest
   fault occurred before a valid 4K output frame and outside every logged
   decoder pool. The driver restored saved state but retained the .30 lock
   after a new console error record; do not reuse .30 until the owner clears
   that lock. A strict GPU-pool guard and a fix for an AGC UI frame left open
   on no-swap loops now pass host checks. On .40, the frame fix eliminated
   command-buffer and ring exhaustion, but live-video testing stopped at
   DNS failure for YouTube and homebrew.page. An offline generated 8-second
   3840x2160 VP9 fixture on .40 then opened the native decoder, presented
   full-width frames and published all 240 with zero late drops or GPU faults.
   At 7.99/8.0 s, natural EOF, audio drain, video decode and display retirement
   gates were true and the player was paused. The source pauses after observing
   completion when no next item exists, but one-second status polling never
   sampled `complete=1`; the strict completion observer therefore timed out.
   This is not evidence of stalled decoding. Clean teardown restored state and
   released the .40 lock. The short case exercises the integrated display path
   but does not isolate the GPU-pool guard or accept long/network 4K, HDR,
   physical audio, seeking or direct completion-transition proof. See
   HARDWARE_ACCEPTANCE.md for the exact cases.

2. **Afterglow player as on its boards**: next-video card, Undo after a
   skipped segment, grouped options, tabbed captions, chat beside the video.
   What the existing player cannot tell yet is listed in
   `docs/afterglow/requests/H.md`.
   The continuation wires the next-video card's five-second Play now/Cancel
   offer, automatic-skip Undo and category colours. Host provider/controller
   checks pass; console acceptance remains pending. Grouped options and
   caption tabs now use the real picker actions and pass generated controller
   and board checks. Chat beside the picture is implemented using the existing
   video quad and Player layout, with generated geometry and lifecycle checks;
   console calibration remains pending.

Then: the Afterglow translations (all 21 catalogs and integer-count forms are
complete; native-speaker and console review remain;
`python3 scripts/i18n_extract.py`, catalogs in `native/i18n/`), live DVR,
keep-playing-while-browsing (implemented and host-tested, console pending), the interface's
own sound assets (`docs/afterglow/sound-design.md`), and the smaller items
of the roadmap. The Live now shelf is implemented for Guest and account Home,
loaded when reached and filtered to live videos. Provider checks and 62 Home
boards in four looks pass; console acceptance remains pending.
The bell now uses the validated account-wide unseen count, refreshed every
minute and after loading notifications; unknown counts hide the badge.
Notification rows preserve upstream read flags. Unsupported remote mark-read
actions are unavailable. Host service/provider/board checks pass; real-account
console acceptance remains pending.

The interface now packages 34 original procedural WAVs covering all 28 cues,
with reproducible source and provenance. Format, level, mixer and packaging
checks pass. Human listening and console acceptance remain pending.
The Afterglow controller map, verified Keep watching stop default, and Library
queue Move up/down actions have generated input, native persistence and four-look
board coverage. All still need console acceptance. Live DVR has calibrated
window/owner/segment identity and controller work under host validation; its
production SDK extension and console acceptance are still pending.

### Open decisions for the owner (do not decide them yourself)

- Update decision resolved on 2026-10-14: use the homebrew.page catalog and
  helper, like the owner's other apps. Integration and host checks are done;
  a complete PS5 package also builds. Console acceptance and a published signed
  catalog entry and release are required for an end-to-end update.
- The README footer entry for ProsperoTube in the protocol repository's
  `scripts/readme-footer/repos.toml` (prepared, not committed).
- Making the repository public, and the first release.
- Storage notice decision resolved on 2026-10-14: show an explanation before
  closing when Lapy access is missing. The startup overlay now sends a system
  notification; console acceptance remains pending.

### Loose ends

- The saved sound-last console-driver edits are reviewed and integrated:
  earlier Afterglow checks silence sound/vibration, the last step checks
  audio-port initialization and frame progress, and teardown restores all
  overrides. Offline driver tests pass; the original stash is retained as
  a backup. The revised sound step was reached after the 4K stall and could
  establish no sound result. A regression now requires frame progress before
  enabling sound; all 132 driver tests pass.
- The six Afterglow screen packages now reuse the existing shared pieces
  (ARCHITECTURE.md section 12d). The migration passes 611 boards in four looks
  and targeted checks of the final cleanup. Typed Settings/comment/reply
  submissions now reach the provider. Console acceptance remains pending;
  remaining `// REQUEST:` comments name backend or design limitations.
- Pull request 1 (`codex/first-version`) is already merged; verified on GitHub
  during the 2026-10-14 continuation. No closure is needed.

## How to work on it

- **Build and test from Linux or WSL.** Write commands into script files and
  run those; long jobs as background tasks. Never kill processes by script
  name (`pkill -f`): other worktrees run the same scripts.
- **Checks.** The full offline suite `bash scripts/test_all.sh` takes 15 to 25
  minutes; the owner asked (2026-10-14) not to run it before each console
  install for now. Run what the change touches, for example:
  `bash scripts/test_package_prepare.sh`, `bash scripts/test_native.sh`,
  `bash scripts/test_embedded_service.sh`, `bash scripts/test_embedded_e2e.sh`,
  `bash scripts/test_afterglow.sh` (Afterglow boards in four looks, about three
  minutes), `bash scripts/test_i18n.sh`, `python3 -m pytest -q`. Ask before a
  release whether the full suite should run.
- **PS5 package.**
  `PROSPEROTUBE_NATIVE_WORK=~/.cache/prosperotube/native-ps5 PROSPEROTUBE_PS5_DIST=<dist> bash scripts/build_ps5.sh --ffpfsc`
  writes `<dist>/PPSA99142/`, `PPSA99142.ffpfsc` and `PPSA99142.zip`. An
  instrumented build for console sessions adds `--usb-remote --virtual-keyboard`.
  Freeze a candidate with the protocol repository's
  `scripts/freeze-candidate.sh <dist>/PPSA99142 <frozen>` and check that
  `sce_sys` holds the four branding files.
- **Install on a console.** Files only, with the protocol repository's
  `scripts/ps5-console.py install PPSA99142 <frozen>/PPSA99142` (`PS5_HOST`
  set). It refuses while ProsperoTube runs; an install needs no console lock.
  It does not delete files a build no longer has: remove stale ones by FTP.
  App logs are in `/data/prosperotube/console.log`.
- **Console sessions** (launching the app) follow the shared-console rules:
  take the console's lock file first, check that nothing is running, never
  close another session's title, one launch per session, no blind retries,
  ask the owner before each session. The driver is `tools/console/session.py`
  ([README](../tools/console/README.md)); `--afterglow` adds the Afterglow
  stage.
- **YouTube from the PC**: anonymous, read-only, a handful of requests at a
  time. Never sign in with a real account; the owner tests accounts himself.
- **Do not edit** `third_party/evo` (change an overlay instead) or the UI kit
  (pinned by `scripts/ui_kit.py`, fetched at build time).
- **Commits**: one-line imperative subject, no body, no trailers; author
  BlackBearReloaded. Push to `main` once the owner has confirmed a change on
  the console, or when he asks.
- **Public-facing text** (README, docs, notices) names no private repository
  and no personal paths or LAN addresses.
