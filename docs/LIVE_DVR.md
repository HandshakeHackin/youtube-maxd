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

The runner generates twelve seconds of small H.264 media, exposes four segments
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
