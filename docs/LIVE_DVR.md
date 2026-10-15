# Live DVR acceptance and implementation status

The stock SDK still opens live streams at the live edge and refuses live
scrubbing. The checked controller integration admits DVR only when the opt-in
FFmpeg extension establishes a fresh, calibrated window for the current launch
owner. A playlist containing several segments alone never enables seeking.
The companion's bounded HLS byte buffer is useful for previously downloaded
finite media; it does not establish a console live timeline or position restore.

## Current status (2026-10-14)

The packaged opt-in extension now supports one continuous HLS playlist or a
calibrated video/audio pair; other layouts and uncalibrated streams remain
non-seekable. On PS5 .30, a synthetic split-HLS AVC/AAC feed rewound from
5.50 to 3.31 seconds, restored a remembered UTC point within 0.98 seconds
after the window advanced, and rejected an evicted point. The signed build,
result and teardown are recorded in [hardware acceptance](HARDWARE_ACCEPTANCE.md).
A public live feed and high-resolution live DVR have not been tested.

The sections below retain the implementation stages and earlier stock-library
diagnostics; their stage-specific pending statements are historical.

## Reproduce without YouTube or a console

Build a separate host copy of the pinned FFmpeg 7.1.1 with HLS, MPEG-TS and H.264
support. Use an isolated build directory, never the PS5 SDK or a shared build in
use by another task. The diagnostic links that copy's actual static archives;
its library version is checked before probing. The fixture generator requires a
host `ffmpeg` with `libx264`.

```sh
PROSPEROTUBE_DVR_FFMPEG_SOURCE=/absolute/path/to/isolated/ffmpeg-7.1.1 \
  bash scripts/test_live_dvr.sh --diagnose-current-limitations
```

The runner generates twenty seconds of small H.264/AAC media, exposes four segments
through a loopback-only, random session path, and limits the origin to 200
requests. It retains no media after the run. It exercises the real demuxer's
finite control, an unfinished rolling playlist, an EVENT playlist, a moved
window and an expired playback session. `diagnose-clear-gate` in the C probe is
an explicit diagnostic confined to this test process, never shipping behavior.
The result is saved at `build/tests/live-dvr/result.json`.

Against the pinned library, the finite control starts at packet PTS 1.400 s and
seeks successfully. The live playlists start at 5.400 s despite retaining the
earlier segments. A target of 1.650 s is rejected by the rolling playlist,
by the same playlist after clearing `AVFMTCTX_UNSEEKABLE` in the diagnostic, and
by an EVENT playlist which is not marked unseekable. Moving the window makes the
old position unavailable while the new window's position is seekable in the
finite control. Expiring the session rejects access. The baseline run makes 21
bounded loopback requests.

Run without `--diagnose-current-limitations` to require the earlier live-window
seek. This is an acceptance failure on the current shipping library, not a
successful DVR test. Passing that mode alone would still not establish native
controls, remembered positions, A/V synchronization or PS5 playback.

## Why lifting the native gate is insufficient

In `libavformat/hls.c`, `parse_playlist` sets `AVFMTCTX_UNSEEKABLE` unless the
first variant is finished or EVENT; `hls_read_seek` rejects that flag. Separately,
`hls_read_packet` initializes private `first_timestamp` from the first decoded
live-edge packet. `find_timestamp_in_playlist` interprets it as the beginning of
the listed window. Starting partway into a longer window therefore makes earlier
listed segments fail the timestamp-range check. Public `AVFormatContext` exposes
neither these private window bounds nor a calibrated media-sequence origin.
FFmpeg's parser also does not expose a program-date-time/discontinuity identity
that could safely identify the same remembered position across reopened sessions.

## Required implementation contract

A complete implementation needs a demux-owned, calibrated window expressed in
absolute packet time, with the audio/video intersection and explicit invalidation
on gaps, sequence rollback, discontinuities, unsupported playlists and expiry.
Publish a consistent snapshot to the controller instead of reading mutable HLS
private data from the UI thread. Revalidate every committed target on the demux
thread before pausing/flushing, because the window can move during scrubbing.
Preserve the existing failed-seek clock and pause-state rules.

Keep the session's UI origin fixed while the window moves, expose both its lower
and upper seek bounds, and initially anchor the clock to the decoded live-edge
packet. The existing VOD assumption that every target lies between zero and a
fixed duration is insufficient. Native scrubbing, clamping, direct/phone seeks,
seek discard gates, live-edge recovery and timeline drawing must share this
contract; truly non-seekable live sources must retain their current behavior.

A remembered position must identify an actual point in the same broadcast using
a validated timeline identity and time mapping. Reopen only while that point is
still inside the current window; otherwise start at the live edge. A stale
absolute UI offset or simply retaining a delay behind the edge does not remember
the point the viewer left. Do not infer identity solely from video ID or transport
PTS, which can reset or wrap. Scope records to the selected profile and clear them
on account removal. No unbounded segment recording cache is necessary.

Required acceptance extends this probe with live window rollover during an active
session, A/V timeline calibration, discontinuity/PTS-wrap refusal, concurrent
window snapshots/seek requests, expired-target rejection before clock mutation,
profile/session ownership, meaningful reopen/restore and console decode/seek/audio
proof. No PS5 FFmpeg rebuild or SDK modification was performed for this diagnostic.

## Isolated host candidate

`scripts/patch_ffmpeg_hls_dvr.py` now supplies a checked, opt-in host candidate.
The isolated candidate can be applied to a pristine,
isolated pinned `libavformat/hls.c`, then rebuild that copy's `libavformat.a`:

```sh
python3 scripts/patch_ffmpeg_hls_dvr.py /isolated/ffmpeg/libavformat/hls.c
make -C /isolated/ffmpeg -j4 libavformat/libavformat.a
PROSPEROTUBE_DVR_CANDIDATE=1 \
  PROSPEROTUBE_DVR_FFMPEG_SOURCE=/isolated/ffmpeg \
  bash scripts/test_live_dvr.sh --candidate-window
```

The patch is default-off (`prosperotube_dvr=1` enables it). Its digest makes
reapplication idempotent; changed overlays require a fresh pinned source file.
At this stage, it supported only a single unencrypted continuous playlist with
explicit UTC program-date-time, positive bounded durations, a 12-hour maximum
window and an initial key packet with matching PTS/DTS. Missing/invalid identity, multiple
playlists/renditions, gaps, discontinuities, transport reset/reordering, sequence
rollback, changed overlapping timestamps/durations, no-overlap window jumps and
PTS-wrap boundaries leave DVR unsupported or retire its calibration. Split
YouTube audio/video playlists remained unsupported at this stage; current
video/audio-pair support is summarized above.

The demux thread calibrates the packet/program timeline and refreshes the
upstream playlist before accepting each live seek. Snapshot copies use a mutex;
readers must finish before their owner closes the format context. Snapshots age
out after three target durations (at least five seconds). No media cache or
additional background fetch loop is added. The API in `native/tube_hls_dvr.h`
returns exclusive PTS/UTC bounds and checked conversions between a point in those
timelines. It deliberately carries no profile/video authority: native code must
bind every snapshot and saved point to its active source/session/profile/video.

The candidate acceptance includes real H.264/AAC packets, the earlier live-window
seek, active rollover with concurrent snapshot reads, stale-deadline refusal,
upstream expiry, unsupported playlists, transport reset and reopened-window
mapping. Reopening restores the same UTC point while its lag increases by four
seconds; a different program timeline and a point evicted from the window are
refused. The run makes 138 loopback requests. The modified `hls.c` itself and test
probe pass ASan/UBSan instrumentation. Its PS5 object compiles in an isolated
copied configuration/source tree using read-only SDK headers; only existing
upstream pointer-sign warnings remain. No PS5 archive was rebuilt or installed.

The subsequent stages below add native ownership, persistence and controller
integration. This host stage does not establish console DVR or remote split-stream
calibration, and must not be advertised as completed DVR support.

## Native ownership and persistence prerequisite

The provider now supplies `tube_live_capture`, `tube_live_owner_current`,
`tube_live_restore` and `tube_live_remember`. Capture runs on the UI launch path
before the worker starts and binds an immutable owner to the selected profile,
video, exact playback URL, service origin and format. Background readers use an
atomic token and a mutex-protected copy of that binding, without reading mutable
provider account globals. Source/profile/selection/shutdown, history-policy
changes and history clearing revoke old tokens. Re-pairing clears saved live
points rather than assuming the newly paired service is the same source.

Only validated UTC points inside a fresh bounded window enter the existing
profile/video Record. Session URLs/tokens are never persisted. Saving follows the
existing ten-second progress/stop flush policy. Incognito, disabled history and
opted-out phone history suppress saving/restoration. Reopen requires matching
source/format plus the current window containing the actual saved UTC point;
other profiles, selections, formats, expired windows and forged owner fields
cannot consume it. The loader rejects malformed and out-of-range microseconds.

`bash scripts/test_native.sh live_owner history_modes playback_recovery remote_history`
passes under ASan/UBSan, including concurrent restore versus selection revocation,
source/profile isolation, persistence across shutdown/reopen, moved windows,
history clearing and privacy-policy changes. The touched provider object compiles
with read-only PS5 SDK headers. These APIs retain unsupported streams at the live edge; the controller wiring
below calls them only for a captured YouTube live source.

The demux-owner-only `av_hls_dvr_prepare_seek` entry point now refreshes and
validates an absolute PTS target before the native caller pauses or flushes its
queues. Invalid numeric targets issue no playlist request. A rolling-window
advance rejects the expired target at this preparation phase and accepts a fresh
one; `hls_read_seek` still revalidates if the window moves again meanwhile. The
instrumented H.264/AAC acceptance now makes 147 loopback requests. The updated HLS
object also compiles with the isolated PS5 configuration and read-only headers.

## Context binding and exact remembered segment identity

The tested `tube_live_runtime` prerequisite binds the active format context to
its immutable launch owner, enables the HLS extension before stream probing,
fixes the session UI origin once, and exposes moving lower/upper bounds. It
refreshes/revalidates targets on the demux owner and checks authority again after
the bounded network request. Snapshot readers hold a short lifetime mutex;
network preparation holds no UI mutex. Releasing the context waits for readers
before the caller closes FFmpeg. A stock-library weak-import test keeps live
streams non-seekable when the extension is absent.

UTC containment by itself does not prove a remembered point belongs to the same
playlist timeline. The extension now publishes a bounded segment identity table
(maximum 4096 entries / 96 KiB, no media bytes) with its window under the same
mutex. `av_hls_dvr_point_identity` returns the point's exact media sequence,
program UTC start and duration. Records persist this tuple alongside the checked
UTC point; reopen requires an exact tuple match, as well as the existing
profile/video/source/format authority. Legacy receipts without identity do not
restore. An otherwise contained point with a one-second program-time shift is
refused, while an unchanged point remains restorable after window movement.

Initial calibration also requires the first packet to remain in the explicitly
selected initial segment. An inner probe that has advanced past it cannot supply
a trustworthy origin and leaves DVR unsupported. The exact helper has a host
regression for advanced/expired initial sequence, reordered/invalid timestamps
and PTS wrap; an empty selected segment has a bounded probe deadline.

The full instrumented H.264/AAC runtime acceptance makes 168 loopback requests,
including concurrent identity/window readers, preflight expiry, fixed UI origin,
owner revocation, changed-contained-timeline refusal and stock-library fallback.
The provider owner/history/recovery tests pass with the stronger identity schema.
HLS, runtime and provider PS5 objects compile using isolated/read-only inputs.
The controller/UI stage below consumes these checked APIs. At this stage, the
shared SDK was unchanged and console DVR remained untested.

## Controller and moving-window display

The launch UI captures the owner before handing a live source to the opening
worker. The worker enables/calibrates the extension, keeps a fixed session
origin, and restores only a still-contained exact segment receipt. Initial audio
and video anchor the live-edge clock instead of reporting an artificial zero.
All close paths release the snapshot binding before closing the format context,
after its workers have joined; decoder/GPU teardown ownership is unchanged.

Input checks fresh seekability, clamps scrub/chapter jumps to both moving bounds,
and leaves unsupported live controls unchanged. Demux admission and refreshed
preflight precede playback pause, presentation hold, queue clearing and decoder
flush. A preflight refusal restores the requested prior pause state without
changing the valid media/clock. Direct seeks pass through the same admission.
While paused or scrubbing, the demux owner refreshes only playlist metadata at
most once per two seconds, including when packet queues are full. Network work
holds no UI snapshot lock. No extra media recording/cache is introduced.

Afterglow retains the LIVE badge and renders the current retained window, with a
clock showing distance behind its edge. Moving the window changes the bar's
lower bound rather than changing the session origin. Invalid/stale/unsupported
windows retain the existing live-edge display. UTC receipts are separate from
VOD resume; live progress does not mark the retained window as a completed video
or report its session-relative clock as a fabricated remote broadcast offset.

Validation: actual-media seek/resume/audio/clock handoff regressions and checked
runtime/expiry/revocation tests pass with ASan/UBSan. Ninety-one player boards,
including eight new DVR states, pass in all four looks (3508 checks). Controller,
PlayerScreen, ProviderHostScreen, demux, runtime and mount PS5 objects compile in
the dedicated generated tree with read-only SDK inputs. The stock weak-import
fallback still passes. At this stage, host integration was established, while
PS5 FFmpeg archive rebuilding, packaging and console A/V seek/restore remained
pending.

## Reproducible PS5 recipe

`prepare_native.py` copies the HLS patcher and helper into the generated build
scripts and applies it beside the existing DASH patch before FFmpeg configure.
The patcher accepts only the exact pristine FFmpeg 7.1.1 HLS source, preserves a
SHA-256-checked original, and regenerates future overlay revisions from that
original. A damaged backup, unknown source or edited current overlay stops the
build. The regression accepts a pristine HLS source path:

```sh
python3 tests/test_hls_dvr_patch.py /isolated/ffmpeg-7.1.1/libavformat/hls.c
```

The SDK identity now hashes both patchers and the HLS helper; the application
build checks all five public HLS DVR symbols in the resulting avformat archive.
At this stage, the recipe change required a coordinated FFmpeg SDK rebuild;
no build or install was invoked against the shared SDK. Generated script syntax
and pinned-source regeneration tests passed, as did the isolated host HLS
archive's live-window, identity/runtime and stock-fallback acceptance. The
later packaged build and synthetic .30 result are summarized above. Public
live DVR validation remains pending; unsupported playlist layouts and
uncalibrated streams remain non-seekable.
