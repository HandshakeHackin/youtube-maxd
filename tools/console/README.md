# Bounded console session

`session.py` runs **one** hardware acceptance session of ProsperoTube
(`PPSA99142`) on a PS5: one verified install, one launch, the core path, one
clean close. It stops at the first anomaly, never repeats a launch or a close,
and always runs the teardown and lock-release path. It follows
[ps5-homebrew-dev-protocol](https://github.com/blackbearreloaded/ps5-homebrew-dev-protocol)
and the rules in [GOAL.md](../../GOAL.md) and
[docs/HARDWARE_ACCEPTANCE.md](../../docs/HARDWARE_ACCEPTANCE.md).

A console session needs the owner's word. The script refuses to start without
`--i-have-owner-approval`.

## Prerequisites

- WSL or Linux, Python 3.11+, `nc`, the PS5 Payload SDK (`/opt/ps5-payload-sdk`
  or `PS5_PAYLOAD_SDK`) and a clone of the protocol repository
  (`--protocol`, default `~/ps5-homebrew-dev-protocol`).
- A **frozen, instrumented** candidate: built with
  `scripts/build_ps5.sh --usb-remote --virtual-keyboard`, then frozen with the
  protocol's `freeze-candidate.sh <dist>/PPSA99142 <frozen-dir>`. The session
  checks every file against `<frozen-dir>/SHA256SUMS` and refuses a build
  without the development interface. A release build cannot be driven.
- An internet connection on the console. The application is self-sufficient:
  there is no companion, no address and no pairing code.
- On the console: FTP (2121), kernel log (3232), payload receiver (9021) and
  ShadowMount Plus scanning `/data/homebrew`, as the protocol expects.
- The console's shared lock file must not exist. The session creates it
  (create-new, content `ProsperoTube`) and deletes it only while it still says
  exactly that.

## The one command

```bash
PS5_HOST=<console address> \
PS5_LOCK=/mnt/c/Users/<you>/Documents/PS5-Claude/lock-<NN>.txt \
python3 tools/console/session.py --i-have-owner-approval \
  --candidate <frozen-dir>
```

Add `--dry-run` first: it does everything that does not touch the console
(candidate check, controller build, lock code on a scratch file) and walks the whole session against an in-memory console, printing every
console operation a real run performs, in order. Nothing is sent.

Useful options: `--out DIR` (evidence, default `~/pt-console/<UTC time>/`),
`--keep-state` (leave the session's history on the console),
`--end eof|stop` (default: play to the end when under two minutes remain,
otherwise Stop with confirmation), `--root-row` and `--topic-row` (the two
rows the route opens, regular expressions on their text; default Discover and
Science), `--with-search` with `--search-query`, `--settle SECONDS`,
`--afterglow` (the optional stage for the Afterglow interface, described
[below](#the-afterglow-stage---afterglow)).

Exit status: 0 pass, 1 an anomaly with a clean teardown, 3 no run (busy
console, held lock, failed precondition), 4 unresolved (see below). The
status follows the hard gates only; what the Afterglow stage found beyond them
is in its own result (`Afterglow PASS | PARTIAL | FAIL` on the last line).

## What each stage proves

| Stage | Proves | Stops when |
| --- | --- | --- |
| 0 local | The candidate is the frozen one; both controllers build; the lock is ours | anything is missing; the lock exists |
| 1 idle | **First console operation**, read-only: no title has a sandbox under `/mnt/sandbox` | any title runs, ours included: lock released, nothing else sent |
| 2 snapshot | Copies of every state file in `/data/prosperotube` (hashes recorded), the installed folder's listing, the error-record baseline | a copy is short |
| 3 install | Every file uploaded under a temporary name, read back, renamed (`ps5-console.py install`); watched for `--settle` seconds and compared again (`settle`) | any difference |
| 4 launch | Idle again; services answer; ShadowMount mounts the installed folder; one launch payload; then the ladder: sandbox exists → status file written by **this build** → Lapy storage → frame loop advancing and browser drawn | another title appeared (lock released, nothing sent); any rung not observed in time |
| 5 core path | See the table below: hard gates on what the engine reports, the rest observed and recorded | a hard gate is not observed within its limit |
| 5a Afterglow (`--afterglow` only) | The Afterglow interface draws, what its frame costs, Home, the picture of it, playback from it; the interface switched off again | never before its own switch-off; it fails the session when one of its hard gates does |
| 6 teardown | `quit` → `parked=1` (media released, GPU idle) → one exact-title close → sandbox gone; development files removed; state restored and re-read; services answer; no new error record; idle; lock released | never skipped |

Every wait is bounded and ends on an observed fact from `dev_status`,
`dev_ui.json`, `console.log`, `youtube.json` or the sandbox listing. A fixed
sleep is never taken as proof.

### The core path: route, hard gates, soft observations

The first console session (2026-10-13) showed how this build draws itself.
The browser is the provider's own RmlUi document: `docs` is `["youtube"]`, the
rows are cards **without ids** (`class` `tube-card`, plus `folder` for a row
that opens a list), the cursor is known by `index`/`total`, a row's text is
its artwork initial, title and detail (`D Discover Search public videos by
topic`). The kit draws the keyboard, the dialogs, the pickers and the player's
controls; over the RmlUi browser its keyboard appears in the snapshot only as
RmlUi's hidden twin (`modal.kind` `keyboard`), and a dialog may not appear at
all. So a screen is identified by the number in `dev_status` (`scr=`: 23
browser, 2 player, 18 caption picker, 31 playback options, 20 stop
confirmation, 32 closed), which the engine writes whatever draws the screen,
and the provider's own log lines prove what a key press did.

Route to a video, no keyboard: **Discover → Science → first playable row**.
Keys: `cross` (the cursor starts on Discover), `right` `right` (Science is the
third card; the cursor's position is read back after every press), `cross`,
`cross`. Cross is pressed only on a row whose text was read back as the wanted
one, so a moved menu cannot end on "Close ProsperoTube".

| Step | Kind | Observed fact |
| --- | --- | --- |
| (b) service started | hard | `embedded service: listening on 127.0.0.1:<port>` in `console.log` |
| (c) Discover opened | hard | new `provider: 'youtube' catalog parent='discover' -> N rows` line, N > 0 |
| (b,c) the service answered | hard | new catalog line for the topic (`parent='search:science'`), N > 0: that list exists only if the built-in service fetched it. The service itself logs nothing per request |
| (d) a video was activated | hard | `provider: 'youtube' activate '<id>' (playable) title='...'` |
| (d) playing | hard | `scr=2 active=1 fatal=0`, position advancing; `provider: resolve failed` fails at once |
| (e) +10 s seek | hard | development `seek` command, position lands near the target |
| (e) pause | soft | position held, and the engine's paused flag when the snapshot has it; Cross is pressed a second time only if the first did nothing visible |
| (e) caption picker, (f) playback options | soft | `scr=18` / `scr=31`, rows recorded, Circle back to `scr=2`. If the player does not come back after two Circle presses, no further key is sent |
| (h) capture | soft | a new `evo_shot_NNN.bmp`, fetched |
| (e) resume | hard, only after an observed pause | position advancing again |
| (g) stop | soft by keys, hard by fact | Circle → `scr=20` → Cross; whatever the keys did, the player must retire (`active=0`, not `scr=2`): the development `stop` command settles it when they did not |
| (d,g) statistics | hard | `stats frames_in=... published=... late_drop=...`, published > 0 |
| (g) history | hard | a record in `youtube.json` for the activated id (or its exact title) |
| (c) search by keyboard (`--with-search`) | soft, last | keyboard seen, typed text in it, `provider: Search query='...'` and a new catalog line; an unseen keyboard gets no text and is closed with `kbclose` |

Soft misses are listed in `result.json` (`observations.soft_misses`) and as
`SOFT-MISS` steps; they never change the verdict.

### Close

The app is asked to quit through its own soft close and must report
`parked=1` before the exact-title close payload is sent, once. If the title is
already gone, no payload is sent. If it does **not** park, nothing is forced:
the title is left running, the lock is kept and the result is `unresolved`.
`--allow-unparked-close` is the only forced path and is off by default.

### State

Without `--keep-state` the console ends with the provider state it had:
changed files are written back from the copies (temporary name, readback,
rename) and re-read; a `youtube.json` the console did not have is removed.
`dev_command`, `dev_status`, `dev_ui.json` and the session's captures are
removed, and so is the Afterglow stage's `afterglow.on` should the stage not
have removed it itself. `console.log` and caches the app created are left and
listed; the four font files the Afterglow interface writes for its text
engine (`afterglow-*.ttf`) are such a cache.
The instrumented build stays installed: put the release candidate back before
anyone uses the app (`ps5-console.py install`, files only).

## The Afterglow stage (`--afterglow`)

Afterglow is the app's new interface, mounted behind a switch
([docs/afterglow/ARCHITECTURE.md](../../docs/afterglow/ARCHITECTURE.md),
section 12a). With `--afterglow` the same launch that ran the core path goes
on to it: the classic session is untouched and comes first, so a fault of the
new interface cannot cost the core path its result. The stage
(`afterglow_stage.py`) switches the interface on with the override file
`afterglow.on` in the app's data folder, which the app looks at once a
second, and removes that file again at its end. It never writes
`afterglow.off` and never touches the Interface preference.

Before enabling Afterglow, the stage writes `afterglow-sound.off` and
`afterglow-vibration.off`. The interface's audio port stays closed during
the earlier checks. The last functional step removes the sound override,
moves focus on Home and records the port's log and frame-counter progress.
This verifies port initialization and liveness, not audible quality. Sound
results are PASS or NOTE. All three overrides and temporary upload names
are removed by the stage or teardown; pre-existing files are restored.

```bash
PS5_HOST=<console address> \
PS5_LOCK=/mnt/c/Users/<you>/Documents/PS5-Claude/lock-<NN>.txt \
python3 tools/console/session.py --i-have-owner-approval \
  --candidate <frozen-dir> --afterglow
```

Run it with `--dry-run` first, as any session. Options of the stage:
`--afterglow-other square|triangle|none` (the key that opens one other
screen: Square is Search, the default; Triangle a video's page. Square is
pressed for a Guest only: for an account it keeps the video in focus for
later, so the stage opens the video's page instead),
`--afterglow-after-core-fail` (try the stage even when the core path failed,
provided the app is back on its browser with nothing playing; the session's
verdict stays the core path's failure).

The stage refuses, before any console operation, a candidate whose
`eboot.bin` lacks the mount (`afterglow.on`) or the capture command
(`DEV_CAPTURE`), and stops before the install when `afterglow.off` is in the
data folder. An `afterglow.on` left by an earlier run is removed for the
launch and put back with the saved state at the end.

### What the build reports

While the interface is shown, `dev_ui.json` lists the document `afterglow`
and carries the frame's own snapshot (`Frame::snapshot_json()`) where the
classic layouts report the focused element: `interface`, `look`, `screen`,
`stack`, `moving`, `rail_open`, `attract`, `dialog`, `focus` (`id`, `text`,
`index`, `total`, `rect`), `focusable`, `toasts`, `state` (the screen's own
values as text, and `lists`) and `stats`. On Home the focus id is
`<shelf>/<index>` and `state.lists["shelf:<id>"]` holds the titles. The frame
writes its stats every frame, so the file changes about ten times a second.
`dev_status` goes on as before: screen number, decoder, position.

### Steps, in the order they run

Each step records `PASS`, `FAIL` or `NOTE` with its numbers (`steps` in
`result.json`, with `"stage": "afterglow"`, `"gate"` and `"numbers"`).

| Step | Kind | What it proves, and from what |
| --- | --- | --- |
| (pre) the app is on its browser | precondition | `scr=23 active=0 fatal=0 parked=0` and a moving clock, no keyboard. Otherwise the stage is not run (`NOTE`, result `not-run`) |
| (a) switched on: the frame draws | hard | `afterglow.on` written (temporary name, readback, rename); within 40 s the snapshot says `interface: afterglow`. The frame was built, the batch drew its two lists, the report hook answers. Recorded with it: how long that took, the longest of the frame's first frames, and the look and output size from the mount's log line |
| (c) Home lists its shelves | soft | `screen: home` after the opening moment, `state.lists` with titles, `load` no longer `loading`: the data layer read the provider's catalogs through the built-in service |
| (c) pictures become ready | soft | `stats.pictures_ready > 0` within 30 s, with pending and bytes: thumbnails came through the network and the decoder |
| (b) frame statistics at rest | soft | 6 s of samples on Home: `frame_ms`, `worst_frame_ms`, `update_ms`, `draw_ms`, `instances`, `runs`, `fill`, `pictures_ready`, `pictures_pending`, `picture_bytes`, `data_pending` as minimum, median and maximum. `FAIL` only when the frame reports no frame time or nothing drawn: what is fast enough is for the reader of the numbers |
| (b) output resolution | soft | the size the interface draws at: from the snapshot once it carries it, today from the mount's line `afterglow: interface up, look ..., WxH` |
| (c) focus ids are `<shelf>/<index>` | soft | the focus names a card of a listed shelf and carries that card's title; every focusable id has the form |
| (c) texts are present | soft | no card without a title, the focus and the hero have text, and the log has no `the text engine did not start` line |
| (c) glyphs and fonts of the text engine | soft | which fonts drew and how many glyphs. **Today always a `NOTE`**: the snapshot does not carry these numbers yet ([requests](../../docs/afterglow/requests/drv.md)) |
| (e) scanout capture of Home | soft | the development command `shot`: `afterglow-home.bmp` and `.png`, taken as soon as Home is real and **before any key is sent**, of a page at rest. Checked as pixels: a readable bitmap, not one flat colour, not dark; its size is the output's |
| (d) focus moves along a shelf | soft | Right (Left at the last card): the focus id's index follows |
| (d) focus moves between shelves | soft | Down to the next shelf and Up again: `state.row` and the focus id follow, the column is kept |
| (d) the rail opens and closes | soft | Left at the first card: `rail_open: 1`, focus `rail/<destination>`; Right: shut, focus back on the card |
| (d) another screen opens and Circle returns | soft | Square (Search, a Guest's Home) or Triangle (a video's page): `stack` is `home > search` or `home > video`; Circle: `home` again. The other screen's snapshot is saved |
| (b) frame statistics while moving | soft | the same numbers from the snapshots read right after each key |
| (e) scanout capture of Home, a video in focus | soft | a second picture, `afterglow-home-video.bmp` and `.png`: the hero and the page's colour are the focused video's |
| (f) Cross on a video starts playback | hard | on the first shelf of videos, with the card's id and title read back first: `scr=2 active=1 fatal=0`. The interface handed the video to the existing player. `provider: resolve failed` for the card: the next card is tried once |
| (f) position advancing | hard | the core path's own gate on `dev_status` |
| (f) stopped through the confirmation | soft by keys, hard by fact | Circle, `scr=20`, Cross; when the keys did not do it, the development `stop` command must retire the player (`(f) player retired after the stop command`, hard) |
| (f) engine statistics for the playback | soft | the engine's `stats frames_in=...` lines for this playback |
| (f) the interface shown again is Afterglow | soft | after the player, the snapshot is the frame's again |
| (g) `afterglow.on` removed, nothing else written | hard | the file is deleted and gone from the listing |
| (g) the classic browser draws again | hard | the browser screen is reported without the document `afterglow`: the console is as the stage found it |

Rules the stage keeps:

- **A step that cannot be judged is a `NOTE`**, and the bytes of the snapshot
  it was looking at are saved as `afterglow/unjudged-NN-<step>.json`: a field
  the build does not report, a snapshot that is not JSON, a Home without
  shelves, an interface that did not come back to Home. A fault in the driver
  itself ends as a `NOTE` too. Nothing in the stage ends the session before
  the teardown except a console that stops answering.
- **Cross is pressed only on a card whose id and title were read back** as a
  video of a listed shelf, **Circle only while a screen lies above Home**
  (on Home itself Circle asks to leave the app), and no key at all goes to a
  screen the snapshot does not describe.
- **Attract mode.** Left alone for 45 s Home shows off its videos, and the
  press that ends that does nothing else. Before a key or a capture the stage
  ends it with one press of L1, which Home ignores, and restarts the 45 s the
  same way when they are nearly over.
- **A snapshot that is not valid JSON** because of bytes that are not UTF-8 or
  a number written as `nan` or `inf` is read after repair and said so
  (`afterglow/repaired-snapshot.json`). One that cannot be repaired costs the
  judgements, not the picture: the capture needs no snapshot.
- **A soft step that fails changes nothing else**: no second press, no second
  launch. The session's verdict follows the hard gates; the stage's own result
  in `result.json` (`observations.afterglow.result`) is `pass` (every step
  passed), `partial` (nothing failed, something was not judged), `fail` (a
  step failed) or `not-run`. With today's build the best result is `partial`,
  because of the text engine's numbers.

### The capture command

On the browser both sticks belong to the app's own controls, in either
interface, so the capture gesture of the player (`key l3`) takes no picture
there. The instrumented build therefore has one more development command,
`shot`: it writes what the display shows as the next free
`evo_shot_NNN.bmp` of the data folder, the 24-bit bitmap the stick click
writes, on any screen and without the pad, and logs
`bc DEV_CAPTURE wrote <file> <W>x<H>` or `bc DEV_CAPTURE failed <why>`. It is
the delimited block "ProsperoTube dev capture" of
`scripts/prepare_native.py` and lives only in `src/evo_usb_remote.c`, a file
whose whole body is compiled only with `--usb-remote`: a release build has
none of it. A build without the command answers `bc REMOTE_CMD unknown`, and
the step fails saying so.

### What only a person in front of the TV can judge

The stage proves that the interface draws, navigates, shows pictures and
plays, and records what a frame costs to build. It sees two still frames. It
does not see:

- **Motion**: the hand-overs between screens, the rail sliding in, the focus
  ring's spring, the hero's cross-fade, the page's wash drifting; tearing and
  judder. `frame_ms` is the time between two frames as the app counts it, not
  what the panel showed.
- **The opening moment**: it plays once, when the interface first draws in a
  launch, and is over before the first picture is taken.
- **Text as drawn**: sharpness at the TV's resolution, spacing, other scripts
  and right-to-left titles, a box where no font had the character, where a
  long title is cut. The captures show two pages of it.
- **Pictures as drawn**: upright, in proportion, without banding; how they
  arrive (a pop or a fade); the avatars.
- **Colour and panels**: the page taking the focused picture's colour, the
  panels (solid on this backend, as designed, not frosted), contrast on the
  real TV, the edges of the picture against the TV's overscan. The capture
  is the scanout read as 8-bit SDR: an HDR output does not look like it.
- **Sound**: the interface's cues are collected and not played yet, so there
  should be silence without clicks; the video's sound starting and stopping
  cleanly when playback begins from Home.
- **The pad in the hand**: how soon a press answers, a held direction (the
  stage sends single presses), the rumble on a refused move, attract mode
  arriving after 45 s and leaving on a press.
- **The clock** in the header: the right time, in the console's time zone.
- **The way into the player and back**: a black or wrong frame between Home
  and the player, Home returning where it was.
- **The switch a person uses**: YouTube preferences > Interface. The stage
  uses the override file only.
- **Everything beyond Home**: the other screens are opened once at most, and
  long browsing, many shelves and memory over time are not exercised.

## Evidence directory

| File | Content |
| --- | --- |
| `result.json` | verdict, stages, every step with its observed detail and wait, observations (rows, captions, options, statistics, history record, restore report) |
| `session.log` | the timestamped run, one line per fact |
| `ops.jsonl`, `console-operations.txt` | every console operation, in order; the folded list |
| `status.jsonl`, `ui.jsonl` | every status line and UI snapshot read |
| `console.log`, `klog.txt`, `shadowmount.log` | the app's log of this launch, the kernel log, the mount manager's log |
| `browser-ui.json`, `playback-scanout.bmp` | the browser as text, the player as pixels |
| `state-before/`, `state-after/`, `data-listing-*.json`, `installed-before.json` | what was on the console, what the session left |
| `install.txt`, `settle.txt` | the protocol tool's own reports |
| `afterglow-home.bmp`, `.png`; `afterglow-home-video.bmp`, `.png` | with `--afterglow`: Home as first drawn, and Home with a video in focus, from the scanout |
| `afterglow/first-snapshot.json`, `home.json`, `rail.json`, `other-screen.json`, `after-playback.json` | the frame's snapshot at each of those moments, as the build wrote it |
| `afterglow/stats-rest.json`, `stats-moving.json` | every sample of the frame's stats and their minimum, median and maximum |
| `afterglow/unjudged-NN-<step>.json`, `repaired-snapshot.json`, `unreadable-snapshot.json` | the bytes behind a step that could not be judged, or a snapshot that was not valid JSON |
| `afterglow/classic-before.json`, `stop-dialog-ui.json` | the classic browser before the stage; the stop confirmation over the video played from Home |

`state-*/youtube.json` may hold the pairing token of an earlier development companion: keep the directory private.

## What it cannot verify

Sound, motion smoothness, lip sync, caption timing and controller feel need a
person. The capture is one paused frame. The core path takes no capture of the
classic browser: on that screen the stick click belongs to the app's own
controls (the Afterglow stage has its own command for a picture of Home).
Text reaches the app through its own keyboard (`--virtual-keyboard`), not the
PS5 system keyboard. What the Afterglow stage leaves to a person is listed
[above](#what-only-a-person-in-front-of-the-tv-can-judge).

## If something goes wrong

- **The console stops answering:** do nothing. No ping, no retry, no relaunch,
  no payload. The session ends, keeps the lock and says so. Report to the
  owner with the evidence directory.
- **`unresolved`:** the title may still be running or the state is unproven.
  The lock file still says `ProsperoTube`; the owner decides when to remove it.
- **`no-run`:** nothing was launched. If it stopped after the install, the
  verified files are in place and a later session will find them unchanged.
- **An anomaly in stage 5:** the teardown already ran. Read `result.json`, fix
  offline, and ask before another session. Never run the same bytes again to
  see whether it happens twice.

## Offline checks

```bash
python3 -m py_compile tools/console/*.py
python -m pytest tests/test_console_session.py
```

The tests cover the parsers, each postcondition, the lock, the candidate
check and the session's control flow (busy console, held lock, no park, no
release, silent console, restoration) against the in-memory console. They
open no socket.

For the Afterglow stage the in-memory console (`sim.py`) models the frame as
`native/afterglow` reports it: the override files looked at once a second,
the opening moment, Home of a Guest or of an account, the rail, the pages
that are not built yet, attract mode, the frame's stats, the capture command
and playback through the player. Its failure modes each have a test showing
that the session still tears down, puts the state back and releases the lock:
an interface that never appears, a snapshot that is not JSON or holds `nan`,
Home offline, empty or loading for ever, pictures that never arrive, a text
engine that did not start, a rail that does not open or close, a screen that
does not open or return, a capture that is refused, unknown to the build,
silent or flat, playback that is ignored, refused or does not resolve, an
interface that does not come back after the player, an app that stops taking
commands and a console that stops answering. Two more tests take the capture
command's own C out of `scripts/prepare_native.py`: one compiles and runs it
on the host and reads its bitmap with the session's reader, the other checks
that it touches only the file a release build compiles to nothing.
