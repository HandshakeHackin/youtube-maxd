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
2. **Afterglow player as on its boards**: next-video card, Undo after a
   skipped segment, grouped options, tabbed captions, chat beside the video.
   What the existing player cannot tell yet is listed in
   `docs/afterglow/requests/H.md`.
   The continuation wires the next-video card's five-second Play now/Cancel
   offer, automatic-skip Undo and category colours. Host provider/controller
   checks pass; console acceptance remains pending. Grouped options and
   caption tabs now use the real picker actions and pass generated controller
   and board checks. Chat beside the picture remains in progress.

Then: the Afterglow translations (many texts still fall back to English;
`python3 scripts/i18n_extract.py`, catalogs in `native/i18n/`), the bell count
and accurate read-state updates, live DVR, keep-playing-while-browsing, the interface's
own sound recordings (`docs/afterglow/sound-design.md`), and the smaller items
of the roadmap. The Live now shelf is implemented for Guest and account Home,
loaded when reached and filtered to live videos. Provider checks and 62 Home
boards in four looks pass; console acceptance remains pending.

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
  a backup. The revised sound step has not yet run on a console.
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
