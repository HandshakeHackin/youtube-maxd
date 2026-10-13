# Console session driver (`tools/console`): requests to the frame owner

The session's Afterglow stage (`tools/console/afterglow_stage.py`, run with
`session.py --afterglow`) reads `Frame::snapshot_json()` as the instrumented
build writes it into `dev_ui.json`, and the lines the mount writes into the
app's log. These are the things it needs and does not find there. None of them
blocks the session: each request says what the stage does until the field
exists, and the stage reads the field as soon as a build reports it (the
names below are the ones `tools/console/observe.py` looks for).

## 1. The text engine's numbers in `stats`

**What.** In the snapshot's `stats`: `text_engine` (1 when the shaping engine
of `native/text` draws, 0 when the kit's baked fonts do), `glyph_pages`,
`glyphs` (glyphs in the atlas), `fonts` (font files opened, the four faces and
the system fonts together) and `missing_glyphs` (characters no font had).

**Why.** The session is asked whether "the text engine and fonts work". The
snapshot carries every title as a string whether or not a single glyph was
drawn, so the texts being present proves the data, not the drawing. Section
12a lists the glyph pages and the system fonts of `/preinst/common/font` among
what the console has not shown: these numbers are how a session can record
them. The frame does not own the engine (the mount does), so the mount would
hand them over, or add them in `tube_afterglow_snapshot()`.

**Proposed.**

```cpp
// frame.hpp, FrameStats, filled by whoever runs the frame:
bool text_engine = false;
int glyph_pages = 0, glyphs = 0, fonts = 0, missing_glyphs = 0;
// frame_draw.cpp, snapshot_json(), inside "stats":
json_number(out, "text_engine", stats_.text_engine ? 1 : 0);
json_number(out, "glyph_pages", stats_.glyph_pages);   // and glyphs, fonts, missing_glyphs
```

**Until then.** The step "(c) glyphs and fonts of the text engine" is a
`NOTE`, so the stage's best result is `partial`. "(c) texts are present" still
fails when the log has the mount's line `the text engine did not start`. With
the fields: `FAIL` when `text_engine` is 0 or `glyphs` is 0, `PASS` with the
numbers otherwise.

## 2. The output size in `stats`

**What.** `output_w` and `output_h`: the `width` and `height`
`mount_render()` was given.

**Why.** The session records the resolution the interface draws at (frame time
and `fill` at 4K are on section 12a's list). Today the only source is the line
the mount logs once, when it builds the frame (`afterglow: interface up, look
..., WxH`): it is not there when the frame was built before the session's part
of the log, or after the log rotated.

**Proposed.** `json_number(out, "output_w", ...)`, `json_number(out,
"output_h", ...)` in `stats` (two members of `FrameStats` set by the caller of
`update()`).

**Until then.** The stage reads the log line; without it "(b) output
resolution" is a `NOTE` and the capture's own size stands in.

## 3. A snapshot that is JSON whatever the texts and numbers are

**What.** Two ways `snapshot_json()` can write a file no JSON reader takes:
`json_number()` prints with `%.6g`, which writes `nan` or `inf` for a number
that is none; `json_string()` passes every byte from 0x80 up as it is, so a
title that ends inside a UTF-8 character (the provider's rows are fixed-size
buffers) puts an invalid sequence into the file.

**Why.** One such value makes the whole `dev_ui.json` unreadable, and with it
every step of the session that looks at the interface: the classic reader of
the session sees no snapshot at all.

**Proposed.** `json_number()`: write `0` when `!std::isfinite(value)`.
`json_string()`: replace a byte sequence that is not complete UTF-8 with
U+FFFD (or leave it out). A board that feeds the frame such a title and a
non-finite stat would keep it so: the boards already check that the snapshot
is valid JSON.

**Until then.** The stage reads such a file after repairing exactly these two
things and says so (`afterglow/repaired-snapshot.json` holds the bytes). A file
it cannot repair turns the steps that need it into `NOTE`s; the capture and the
switch-off do not need it.

## 4. What Cross does on the focused card: `focus.kind`, `focus.live`

**What.** In `focus`: `kind` (`video`, `topic`, `channel`, `playlist`,
`action`, ...) and `live` (1 for a live stream).

**Why.** The stage presses Cross only where it has read back that a video is
in focus, and then holds the result to the engine's gates (decoder active,
position advancing). The snapshot does not say what a card is. The stage knows
Home's tile shelf by its id (`topics`, from `data.cpp`) and takes every other
shelf for videos; a shelf of tiles under another id would be taken for videos,
and a live stream, whose position does not run like a video's, cannot be
avoided.

**Proposed.** `FocusInfo` gets `std::string kind` and `bool live`;
`json_focus()` writes them; Home sets them from the row it already has.

**Until then.** The shelf-id rule. When Cross opens a screen instead of the
player, the stage returns with Circle and records a `NOTE`; a card whose stream
does not resolve is passed over once. A build that reports `kind` is believed:
anything but `video` is not played.

## 5. A frame counter: `stats.frames`, `stats.slow_frames`

**What.** `frames`: `update()` calls since the frame was built.
`slow_frames`: how many of them came later than one and a half display
intervals after the one before.

**Why.** The session reads the snapshot about twice a second. `frame_ms` is one
frame and `worst_frame_ms` the last 120, so a hitch more than two seconds
before a sample is never seen, and the average rate over the session's window
can only be guessed from samples. With `frames` it is the difference of two
counts over the difference of two `t_ms`; with `slow_frames` every hitch is
counted, wherever it fell.

**Proposed.** Two counters in `FrameStats`, incremented in `Frame::update()`.

**Until then.** Minimum, median and maximum of the samples, at rest and right
after each key.

## 6. Pictures that failed: `stats.pictures_failed`

**What.** How many pictures the cache gave up on (download or decode).

**Why.** `pictures_pending` that stays above zero does not say whether
pictures are still on their way or will never come.

**Until then.** "(c) pictures become ready" passes as soon as one picture is
ready and records what was still pending after 30 s.

## 7. The rail's destinations in `focusable` while it is open

**What.** While `rail_open` is 1, `focusable` is empty: the snapshot names the
focused destination (`rail/<screen>`) and their number, not the others.

**Why.** The session can only record the one destination the rail opens on;
it cannot say that Search, Library or Settings are there without walking the
rail, and Cross on a destination replaces the whole stack.

**Proposed.** List the destinations (`rail/<screen>`, label) in `focusable`
while the rail is open.

**Until then.** The stage records the rail's focus id, its text and `total`,
and closes the rail with Right.
