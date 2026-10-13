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
Science), `--with-search` with `--search-query`, `--settle SECONDS`.

Exit status: 0 pass, 1 an anomaly with a clean teardown, 3 no run (busy
console, held lock, failed precondition), 4 unresolved (see below).

## What each stage proves

| Stage | Proves | Stops when |
| --- | --- | --- |
| 0 local | The candidate is the frozen one; both controllers build; the lock is ours | anything is missing; the lock exists |
| 1 idle | **First console operation**, read-only: no title has a sandbox under `/mnt/sandbox` | any title runs, ours included: lock released, nothing else sent |
| 2 snapshot | Copies of every state file in `/data/prosperotube` (hashes recorded), the installed folder's listing, the error-record baseline | a copy is short |
| 3 install | Every file uploaded under a temporary name, read back, renamed (`ps5-console.py install`); watched for `--settle` seconds and compared again (`settle`) | any difference |
| 4 launch | Idle again; services answer; ShadowMount mounts the installed folder; one launch payload; then the ladder: sandbox exists → status file written by **this build** → Lapy storage → frame loop advancing and browser drawn | another title appeared (lock released, nothing sent); any rung not observed in time |
| 5 core path | See the table below: hard gates on what the engine reports, the rest observed and recorded | a hard gate is not observed within its limit |
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
`dev_command`, `dev_status`, `dev_ui.json` and the session's capture are
removed. `console.log` and caches the app created are left and listed.
The instrumented build stays installed: put the release candidate back before
anyone uses the app (`ps5-console.py install`, files only).

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

`state-*/youtube.json` may hold the pairing token of an earlier development companion: keep the directory private.

## What it cannot verify

Sound, motion smoothness, lip sync, caption timing and controller feel need a
person. The capture is one paused frame. There is no capture of the browser:
on that screen the stick click belongs to voice search. Text reaches the app
through its own keyboard (`--virtual-keyboard`), not the PS5 system keyboard.

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
