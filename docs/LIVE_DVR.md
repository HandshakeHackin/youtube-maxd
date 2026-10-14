# Live DVR acceptance and current blocker

The shipping app still opens live streams at the live edge and refuses live
scrubbing. Do not remove those gates based on a playlist containing several
segments: the pinned FFmpeg 7.1.1 demuxer cannot safely seek that rolling window.
The companion's bounded HLS byte buffer is useful for previously downloaded
finite media; it does not establish a console live timeline or position restore.

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
It is not connected to the PS5 build or native controls. Apply it to a pristine,
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
It supports only a single unencrypted continuous playlist with explicit UTC
program-date-time, positive bounded durations, a 12-hour maximum window and an
initial key packet with matching PTS/DTS. Missing/invalid identity, multiple
playlists/renditions, gaps, discontinuities, transport reset/reordering, sequence
rollback, changed overlapping timestamps/durations, no-overlap window jumps and
PTS-wrap boundaries leave DVR unsupported or retire its calibration. This is a
conservative first stage; split YouTube audio/video playlists remain unsupported.

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

Native seek gates, moving lower/upper timeline bounds, initial clock anchoring,
account-scoped saved-point persistence and source/session ownership tests remain
pending. This host stage does not establish console DVR or remote split-stream
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
with read-only PS5 SDK headers. These APIs are prerequisites only: the player does
not call them yet, and production live seek gates are still unchanged.

The demux-owner-only `av_hls_dvr_prepare_seek` entry point now refreshes and
validates an absolute PTS target before the native caller pauses or flushes its
queues. Invalid numeric targets issue no playlist request. A rolling-window
advance rejects the expired target at this preparation phase and accepts a fresh
one; `hls_read_seek` still revalidates if the window moves again meanwhile. The
instrumented H.264/AAC acceptance now makes 147 loopback requests. The updated HLS
object also compiles with the isolated PS5 configuration and read-only headers.
