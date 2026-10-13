# Stage S: interface sound and vibration

What `native/afterglow/sound` is, what it asks of the shared files, and what
only a console can show. Branch `wt/ag-s`.

## What is there

| File | What |
| --- | --- |
| `sound/cues.hpp/.cpp` | The vocabulary: every cue of the interface with the recordings that may serve it, in order of preference; eight moments past the kit's enum (`sound::kPlayStart`, `kPause`, `kSeekCommit`, `kSkip`, `kSignedIn`, `kGoodbye`, `kCountdown`, `kCountdownDone`). |
| `sound/bank.hpp/.cpp` | The recordings, read by name through a reader and decoded once. |
| `sound/sound.hpp/.cpp` | `Sound`: `step(frame.feedback(), step)` after every `Frame::update()`. The rules of `sound-design.md` and of section 9 of the design notes. Posts voices to the kit's `audio::Mixer`, pulses to an `Output`. |
| `sound/worker.hpp/.cpp` | The turn of the thread that owns the devices: port opened by the first voice, written only while something sounds, pulses started and ended, everything let go on park and exit; every failure ends in silence. |
| `sound/recorder.hpp` | `Recorder` and `Tape`: the Output of the tests and of the harness. |
| `sound/app/*` | Console only: the port (`sceAudioOut*`), the pad (`scePadSetVibration*`), the thread, `app_begin()`, `app_step()`, and `tube_afterglow_sound.h` for EVO's application. |

Which recording plays which cue today (the kit's sets, pinned; `scripts/afterglow_overlay.py` ships
them as `assets/hui/afterglow/sfx/<set>/` and writes `assets/prosperotube-notices/afterglow-sounds.txt`):

- `glass`: focus, select, back, tab, open, modal_open, modal_close, toggle, slider, error, notify, saved, welcome, resume.
- borrowed from `paper` until glass has them: tick, type, erase, favorite_on, favorite_off.
- stand-ins: launch and play_start = `glass/launch`; pause = `glass/toggle` at 0.92; seek_commit = `glass/select` at 1.1;
  skip = `glass/notify` at 1.1, gain 0.7; signed_in = `glass/saved`; goodbye = `glass/back`; countdown = `glass/slider`;
  countdown_done = `glass/notify`.

**The ten recordings** need no source change: a file `<cue>_NN.wav` (48 kHz, 16 or 24 bit) for `type`, `erase`,
`tick`, `favorite_on`, `favorite_off`, `play_start`, `pause`, `seek_commit`, `skip`, `signed_in` (and `goodbye`)
is played as soon as the package carries it as `glass/<cue>_NN.wav`: either in the kit's glass set after a pin
change, or in `native/afterglow/sound/recordings/glass/` of this repository (shipped over the kit's file of the
same name). `scripts/test_afterglow_sound.sh` proves it with made-up files.

## Rules implemented

- One sound per action: of a step's cues only the highest of confirm < specific < chime is heard; ticks sound
  beside it, each once.
- At most 12 of the same tick a second. A held direction (told by `Step::held`, or seen as ticks less than
  250 ms apart: the app repeats after 220 ms and then every 65 to 28 ms): from the sixth repeat every second
  surviving tick is left out and the rest play 4 dB down. A refusal on a held direction is silent and has no pulse.
- Rare moments: welcome once per start, the start of a video once in 2 s, signed_in once in 5 s; a toast's
  `notify` within 300 ms of `favorite_on`, `favorite_off`, `saved` or `signed_in` is not played.
- Beside a playing video (`Situation::playing`): with "Notices only" (the default) only skip, countdown and the
  start of the next video, 6 dB down, and a dialog in full; with "All" everything 6 dB down; toasts never; "Off"
  nothing. `Sound::video_gain()` says when the video itself should be 3 dB down (a dialog over a video that keeps
  playing); nothing in this stage applies it.
- Attract mode: silent, no pulses; the button that ends it plays `resume`.
- Quiet hours: 9 dB down, welcome / play_start / signed_in become `select`, no vibration. Reduced motion with a
  volume of 3 or lower: the same replacements and no vibration, at the chosen volume (see "For the owner").
- Off means off: nothing is posted and no port is ever opened. Volume 0 is off.
- Pan within 0.55, pitch within 0.7 to 1.4, the kit's 3 % detune and rotation of variations, the mixer's limiter
  (never above -1 dBFS).
- Vibration: one pulse per step at most, 0.6 and 200 ms at most, 120 ms apart at least; off with its option, in
  quiet hours, in the calm set and in attract mode.

## Blocks added to shared files

All marked `stage S`:

- `native/afterglow/app/mount.cpp`: the include; `sound::app_begin()` after the frame is made;
  `sound::app_step()` after `Frame::update()`.
- `scripts/afterglow_overlay.py`: `SOUND_BORROWED`, `SOUND_NOTICE`, `sound_files()`, `SOUND_PARK`, `SOUND_EXIT`,
  `apply_sound()` and its call at the end of `apply()`: recordings and their notice, `tube_afterglow_sound.h` into
  `addons/include`, two wildcards in EVO's Makefile, and four lines in `core/src/Application.cpp` (the include,
  `tube_afterglow_sound_pad()` after `evo_feedback_init`, `tube_afterglow_sound_park()` in the soft close,
  `tube_afterglow_sound_close()` in `shutdown()`).
- `tests/afterglow/host_main.cpp`: `--sound <file.wav>`; the recordings and a `Tape`; per board a `Recorder`
  and a `Sound` stepped after every `Frame::update()`; a board's `"sound"` object (`sounds`, `vibration`,
  `volume`, `quiet_from`, `quiet_until`, `minute`); `expect` keys `"cues"` (the names heard, in order, as one
  text) and `"pulses"` (a count).

New files outside the module: `tests/afterglow/sound/sound_test.cpp`, `scripts/test_afterglow_sound.sh` (picked up
by `scripts/test_all.sh` by its name), `tests/afterglow/boards/sound.json` (eleven boards on the frame, Home and
the reader as they are at `f803f80`).

## Requests

1. **Bridge (package G): the preferences.** Rows under Settings > Interface > Sounds, stored with the other
   interface preferences, and after loading and after every change one call of
   `tube_afterglow_sound_options(&values)` (`native/afterglow/sound/app/tube_afterglow_sound.h`):

   | Row | Kind | Values | Default | Field |
   | --- | --- | --- | --- | --- |
   | `settings:ui:sounds` | switch | Off, On | On | `sounds` |
   | `settings:ui:sound-volume` | list (a slider in Afterglow) | 0 to 10 | 7 | `volume` |
   | `settings:ui:sounds-playback` | list | All, Notices only, Off | Notices only | `playback` (0, 1, 2) |
   | `settings:ui:quiet-hours` | switch | Off, On | Off | `quiet_from` and `quiet_until` are -1 when off |
   | `settings:ui:quiet-from`, `settings:ui:quiet-until` | list | times of day in steps of 30 minutes | 22:00, 07:00 | minutes after midnight |
   | `settings:ui:vibration` | switch | Off, On | On | `vibration` |
   | `settings:ui:reduced-motion` | switch | Off, On | Off | `reduced_motion` (and `Frame::set_reduced_motion`) |

   EVO's "Navigation sounds" switch belongs to the classic interface only. Until the rows exist the defaults
   apply, and a console session can switch with the files `afterglow-sound.off` and `afterglow-vibration.off`
   in `/data/prosperotube` (looked at every two seconds).
2. **Mount: `InputFrame::nav_repeat`.** `tube_afterglow_input()` hands every auto-repeat of EVO's pad over as a
   fresh press, so `!input.nav_repeat` in the screens is always true and a held direction at an edge asks for
   `error` and a rumble 15 to 35 times a second. The sound module silences that by cadence; the nudge of the
   focus ring still fires every time. A direction that is in `pressed` while it already was in `held` at the
   previous call is a repeat.
3. **Frame: two cues against the sheet.** The opening hands over to Home through `root()`, which plays `tab`:
   the sheet has no sound there. The rail opening plays `focus`; the sheet says `open` at pan -0.5 (cue 3).
4. **Frame: no rumble from the keyboard and the dialog.** The kit's keyboard and dialog ask for a 0.3 pulse on
   every activation; the design says never on typing or sheets. `wear()` should set `style.sounds.rumble = 0`
   for them (the sound module cannot tell where a pulse comes from).
5. **Screens: the moments by their own names.** Where a screen means pause, the scrub being let go, a skipped
   segment, a sign-in, a second of the countdown or its end, it should play `sound::kPause`, `kSeekCommit`,
   `kSkip`, `kSignedIn`, `kCountdown`, `kCountdownDone` (`afterglow/sound/cues.hpp`) instead of the stand-in
   (`toggle` at 0.92, `select` at 1.1, `notify`, `saved`, `slider`): the stand-in still plays today, the
   recording will when it exists, and the rules can tell a notice from a toast. `Cue::launch` already is the
   start of a video. Attract mode ending is played by the sound module (`resume`); Home need not.
6. **Player package (H).** Step the app's `Sound` (`sound::app_sound()`) with `Situation{playing, layer}` while
   the interface draws the player; multiply the player's own volume by `Sound::video_gain()`; give the
   countdown's last second its pulse only when `!Sound::audible()`; start the video's sound with its 150 ms
   fade after `play_start`.
7. **Documents.** A paragraph on the sound module in `ARCHITECTURE.md` (sections 2 and 12a: "sounds" is no
   longer "collected and not played"), and a line for the kit's sound sets in `THIRD_PARTY_NOTICES.md` (the
   package already carries `afterglow-sounds.txt`).

## For the owner

- **The ten recordings** (and `goodbye`), made as the glass set was made: see "What is there".
- **"The same reduced set"** (sound-design.md, section 6) is read as: the big moments become `select` and
  vibration is off, without the further 9 dB (somebody at volume 3 would otherwise hear volume 3 jump by more
  than 9 dB at volume 4). One constant if the other reading is meant.
- **The parting sound** plays only when the app is closed from the interface, never from the player or a timer.

## What only a console session with a person listening can confirm

Nothing of this has been heard or felt: the PC has no port and no pad, and the tests record. In this order:

1. The log line `afterglow: sound: N recordings, 28 cues served, 0 silent` (the assets are in the bundle).
2. The log line `afterglow: sound: port open, handle ...` at the first cue (a third MAIN port beside the
   player's and EVO's effects port is accepted), or the failure line and a silent interface that otherwise works.
3. The welcome under the opening, at a sensible level against the system's own sounds, with no click at its
   start or end; that the first cue is not late by the port opening.
4. Moving on Home: ticks, their stereo position, shelves stepping down in pitch; a held direction thinning to a
   soft purr instead of a rattle; the end of a shelf: one soft error, none while held.
5. Starting a video: `launch` plays through the hand-over and ends before or under the video's first sound;
   the video's sound is never interrupted, late or distorted by the interface's port (start, seek, long play).
6. Back from the player: the next cue is there at once (the port stayed open and idle), and after an hour too.
7. Vibration: whether a pulse is felt at all (EVO's notes say no vibration call had any effect on firmware
   12.70; the log line `afterglow: vibration: mode ..., first pulse ...` gives the return codes), then whether
   50 ms at 0.25 and 160 ms at 0.6 are felt as meant and whether a pulse ever sticks.
8. `afterglow-sound.off`: silence, and no `port open` line when it is there from the start;
   `afterglow-vibration.off`: no pulses.
9. Closing the app from Home: the parting sound, `afterglow: sound: parked`, no hang and no noise at the park;
   closing from the player: no parting sound.
10. Half an hour of ordinary use: `afterglow: sound: closed, N grains written, 0 refused` at exit.
