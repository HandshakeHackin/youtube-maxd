# Handoff notes

For the next person or agent who picks up ProsperoTube. Read this first, then
[Status](STATUS.md), the [README](../README.md) roadmap and
[Afterglow's architecture](afterglow/ARCHITECTURE.md). Written 2026-10-14.

## Latest 4K hitch investigation (2026-10-15)

The owner requested offline analysis before another console test. Keep .30 as
the only test console and build ZIPs only. Audio, TV HDR indication and stable
playback-menu transitions have owner confirmation on `b8d57c0`; smooth 4K is
still unaccepted.

A verified audio-clock interpolation candidate (`90ed17f247d5bbd8` eboot prefix)
still visibly hitched on native 3840x2160 VP9/24, despite 807 published frames
and zero late drops in 34.8 seconds. Evidence:
`/home/denis/pt-console/clock-smooth-production-verified-30/`.
An earlier replay was inconclusive because installed bytes differed and its
log was unchanged; the verified replay supersedes it.

Offline review then found a specific delayed-output association bug:
`decode_one` used the current input's VP9 show flag for a picture from an
older input. Local decoder research establishes that hidden coded frames also
produce outputs. The new `vp9_output_order_overlay.py` queues each coded
input's PTS and display flag and consumes them together on decode and drain.
`scripts/test_vp9_output_order.py` compiles the generated functions with a
bounded delayed-output oracle. The old implementation fails picture identity
(expected picture 9 / PTS 9000, got picture 10 / PTS 9000); the fix passes,
including hidden/show-existing, drain, seek, errors and queue bounds. Existing
native frame-lease tests also pass. This is an offline fix, not yet evidence
that the TV hitch is gone. Validate the same clip after installing the newly
built candidate; do not infer smoothness from counters alone.

The proposed flip-timing probe was archived outside product source at
`/home/denis/pt-console/offline-hitch-diagnostics/`. It measures CPU-observed
retirement, not authenticated scanout time, and is not part of the release.
The generated renderer already uses a locked frame/PTS/serial snapshot; the
unpatched upstream shared-PTS race does not exist in the built application.

## Playback continuation on 2026-10-14

A later 2026-10-15 public live DVR candidate (`964bb5c`, eboot SHA-256
`4eba00b13cacdf11ceb9e4f17889780dfdc99d0fd7bf6032ca4048574ffab927`)
passed on .30 as Guest with Bloomberg Business News Live (`QB5BNdBFujE`).
Native 1280x720/29.97 AVC playback rewound 3591.58 to 3472.61 seconds
after a -120-second command and resumed frames. After stop and an 18.4-second
playlist-window advance, reopen restored the saved UTC point within 2.61
seconds and resumed frames. The candidate includes HLS fixes for timezone
offsets, discontinuity metadata, AAC DTS with absent PTS, and wrap reference
reset on seek (`e1d0806`, `2fdf356`, `964bb5c`). One launch, saved-state
restoration, unchanged error records, idle/healthy services and ShadowMount
release passed. Private evidence:
`/home/denis/pt-console/public-dvr-bloomberg-964bb5c-30-run/result.json`.
Long or high-resolution public live DVR and physically audible sound remain
unverified.

The detailed 4K fault chronology below is historical. The current source adds
DVR read interruption and UTC restore, automatic frame-rate switching from
actual VideoOut state, PQ/HLG/SDR selection, pending-start cancellation, strict
GPU-mapped UI allocations and separate image/control HTTP lanes. Bounded console
evidence now includes synthetic live DVR rewind/restore on .30; PQ/HLG/SDR
scanout and first-playback 60-to-120-to-60 Hz switching on .40; direct
30-to-25 fps active switching from 120 to 60 Hz on .40; a 304-second direct-LAN
4K VP9/Opus run with seeks on both consoles; and a held-artwork control
request completing in 51 ms on .30. See [Status](STATUS.md) and
[Hardware acceptance](HARDWARE_ACCEPTANCE.md) for the exact build and limits.

Three later GPU unmapped texture reads at 0x2a5248000, 0x2a50fb000 and
0x2a5256000 occurred before the strict GPU-pool guard. They lie outside
logged scanout and decoder direct mappings; the diagnostic generic direct
pool occupied 0x272c00000 through 0x276c00000. The old allocator could
fall back to ordinary malloc for HUI/RmlUi GPU descriptors when its 64 MB slab had no
fitting chunk; its usage counters did not include fallback allocations.
That is an address-compatible hypothesis, not an identified owner.
Decoder-internal mapping ownership remains possible. If a fault recurs, log
each bound AGC texture/vertex address and allocator origin at submission to
correlate it with the reported fault address. Current source also retains HUI
textures until GPU fences retire, including at shutdown. Generated host checks
and a signed PS5 build pass. The revised build completed 304-second 4K
playback with rewind, forward seek and natural EOF, followed by 4K Afterglow
texture/navigation churn and 12 seconds of idle, on .30 without a new fault.

One later public BBC 4K/25 attempt on .30 got HTTP 403 at DASH segment 85
before any frame; exact state restoration passed. Host probes reproduced
intermittent signed links that returned 206 for the first 64 KiB but 403 for
the midpoint and segment 85, alongside links that returned 206 for all three.
Current source probes the selected video's midpoint in the existing bounded
same-client retry loop. It does not strip signed query parameters. A .30
retest of this link-selection fix reached native 3840x2160 VP9/25 playback
without an HTTP 403. The first 150-second interval advanced 150.14 seconds;
a -60-second seek landed within 0.5 seconds of target. The second 150-second
interval advanced only 24.07 seconds, with output falling to about 3.5
frames/s. Video packets stayed queued while audio packets ran dry. FFmpeg's
premature-file-end notices also occurred during the healthy interval, and
there was no GPU fault. Audio starvation is plausible but unproven. A focused
.30 diagnostic retest advanced 149.93 seconds in the first 150-second interval,
landed a -60-second seek, then advanced 53.38 seconds in the next 60 seconds.
Token-safe counters showed about 6–7 seconds of seek preroll before audio
packets and PCM output resumed; thereafter audio and video clocks tracked
closely and PCM underruns stayed flat. The earlier sustained 3–4 frames/s
collapse did not reproduce in this one run. There was no HTTP 403, GPU fault
or new error. Stage 6 restored all nine saved-state files, verified idle and
services, and released the console lock. Evidence is at
`/home/denis/pt-console/seek-audio-diag-30-run`. Temporary diagnostic
instrumentation was removed from product source.

The selected 4K DASH/WebM track advertised a bogus 2 kbit/s rate and its
video read-ahead was capped at 8 MB. Commit `cd45171` keeps 4K read-ahead at
48 MB for implausibly low rates without changing the unrelated core item's
8 MB cap. The signed `prosperotube-cd45171b_1014-2355` build (eboot SHA-256
`9697e4ecb8d60a267848138b34d2f13efd3db818bdefc719195d499662dad01f`)
passed two independent .30 public BBC 3840x2160 VP9/25 sessions, each with
150 seconds of playback, one -60-second seek and another 150 seconds. First
intervals advanced 149.97 and 149.84 seconds; post-seek intervals advanced
138.88 and 144.85 seconds. Both captured different BBC footage in initial and
post-seek 4K scanouts. Neither logged HTTP 403, `DEMUX_OVERSHOOT`, a GPU fault
or a new error record. Both used one launch, restored all nine saved-state
files, left the title idle and services healthy, and released the .30 lock.
Evidence: `/home/denis/pt-console/read-ahead-cd45171-30-run` and
`/home/denis/pt-console/read-ahead-cd45171-30-repeat`. The earlier bounded
test reproduced a severe slowdown before the cap fix; it did not recur in
these two passes. Continue treating other public CDN/timing conditions as unproven;
the historical GPU fault address owner is still unknown.

The current ZIP-only production package was built from `964bb5c` without
the USB remote: `/home/denis/pt-candidates/playback-20261015-final-964bb5c/`.
Its signed `PPSA99142/eboot.bin` has SHA-256
`dd9cf60618b3a1a9c0125e098644891620f57201879896e1334bb53228ec02b2`;
`PPSA99142.zip` has SHA-256
`ca5777c67b1f33e8b5f378e2109c0f7842f27ddf03f86b2d2bf0a75672100822`.
Signed-container and packaged Lapy helper checks passed. The ZIP passed
full CRC verification and its embedded eboot hash matched the folder.
The 185-file production folder was installed on .30 solely to restore the
title after the public DVR test: 185/185 files matched after 130 seconds of
settling, then the title was idle, error records unchanged and the lock
released. It was not launched or playback-tested as a production build; the
separately signed USB-remote candidate above supplied the DVR evidence.

The same instrumented `964bb5c` candidate later passed a 600-second direct-LAN
4K VP9/Opus watch on .30 with two -170-second rewinds, three distinct nonblank
4K captures, no stall or GPU fault signature, and unchanged error records.
Teardown restored all ten saved-state files, including the owner's
`afterglow.on`, then released the title and lock. Evidence:
`/home/denis/pt-console/lan4k-964bb5c-30-run2/result.json`. A separate
application acquired the .30 lock immediately afterwards, so the instrumented
candidate remains installed there; the production ZIP and folder above remain
verified locally but have not been launch-tested. Do not replace the installed
title while that other application owns .30.

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
  pass on the host. Bounded offline 4K/30 and live YouTube 4K/25 VP9 console
  runs now pass; two five-minute public-network 4K VOD sessions with rewind
  also pass. Longer public-network endurance, audible Opus and physical HDR
  indication remain pending.
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

1. **Complete VP9 and Opus validation**: ranged manifests for WebM tracks
   (Cues give the byte ranges), so 4K VP9 and HDR come back. Implementation now enables WebM
   indexing and fixes FFmpeg's zero-byte range-boundary read, which caused
   the seek hang. Generated MP4/WebM seeks pass on the host and VP9 1080p
   passed on PS5 (ec4660ac; HARDWARE_ACCEPTANCE.md). The offline 4K/30
   VP9/Opus path now passes on .40; a bounded live 4K/25 path passes on .30.
   Long public live playback, audible Opus and physical HDR indication remain
   to validate; synthetic PQ/HLG/SDR selection is recorded in hardware acceptance.
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
   after a new console error record. The owner subsequently authorized
   clearing it for another bounded test. A strict GPU-pool guard and a fix
   for an AGC UI frame left open on no-swap loops now pass host checks. On .40,
   the frame fix eliminated
   command-buffer and ring exhaustion, but live-video testing stopped at
   DNS failure for YouTube and homebrew.page. An offline generated 8-second
   3840x2160 VP9 fixture on .40 then opened the native decoder, presented
   full-width frames and published all 240 with zero late drops or GPU faults.
   At 7.99/8.0 s, natural EOF, audio drain, video decode and display retirement
   gates were true and the player was paused. The source pauses after observing
   completion when no next item exists, but one-second status polling never
   sampled `complete=1`; the strict completion observer therefore timed out.
   This is not evidence of stalled decoding. Clean teardown restored state and
   released the .40 lock. A subsequent 64-second offline 3840x2160 VP9/Opus
   case on .40 passed 15/30/45/55-second checkpoints and reached 63.99/64 s
   with natural EOF, audio drain, video decode and display retirement gates
   true. The native decoder published all 1,920 frames with no late/seek drops,
   decode fatal or GPU fault. Stage 6 parked and closed the app, restored state,
   left services healthy and released the .40 lock. This accepts one bounded
   offline 4K/30 path, but does not isolate the strict GPU-pool guard or prove
   HDR, physical audio, 4K seek/repeat, very long sessions or a directly
   sampled completion transition. The same frozen 57efd5b2 candidate then
   ran a bounded live YouTube 4K/25 case on .30: Classic native VP9 1080p and
   a +10 s seek passed, followed by the public BBC Earth item at native
   3840x2160. Position advanced by 11 s; 315 frames were published with four
   late drops (1.3%), no decode fatal or GPU fault. It stopped through the
   confirmation and returned to Afterglow. Stage 6 parked and closed the app,
   restored state byte for byte, found services healthy and error records
   unchanged, and released the .30 lock. The 4K file discarded 253 frames
   as seek preroll at the saved 600 s position, separate from the four late
   presentation drops. FFmpeg logged premature-end and earliest-resync
   warnings in the WebM media stream; playback recovered, but transport and demux
   robustness remain to validate. No playback scanout image was captured, so
   visual output rests on presenter counters, not a picture review. This is
   one bounded live 4K result, not long playback, audible output, HDR or 4K
   seeking. The earlier network first-frame faults remain unexplained. See
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
board coverage. All still need console acceptance.

Live DVR includes the production SDK extension and passed bounded .30
synthetic split-HLS and public Bloomberg 720p AVC/AAC rewind/UTC restore.
Longer-window and high-resolution public console validation remain pending.

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
  `PROSPEROTUBE_NATIVE_WORK=~/.cache/prosperotube/native-ps5 PROSPEROTUBE_PS5_DIST=<dist> bash scripts/build_ps5.sh`
  writes `<dist>/PPSA99142/` and `PPSA99142.zip`. An
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
