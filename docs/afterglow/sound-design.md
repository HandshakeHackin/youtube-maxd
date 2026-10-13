# ProsperoTube: sound design

Part of the "Afterglow" interface proposal. This page says what every
interaction sounds like, which of those sounds exist today, which have to be
made, how interface sound lives beside video sound, and what the player can
change. No audio was downloaded or generated for this proposal: the reel plays
the kit's existing recordings.

## 1. What the kit gives

Read in the working copy of `ps5-homebrew-ui` at `4b3fbb7`.

- **A vocabulary, not files.** A screen asks for a *cue*
  (`feedback.play(audio::Cue::focus, pitch, pan, gain)`,
  `src/ui/feedback.hpp`); the shell plays it in the active *sound set*
  (`src/audio/cues.hpp`: `enum class Cue`, 42 cues; `enum class SoundSet
  { paper, glass }`). A cue its set does not record falls back to the other
  set, then to a synthesized tone (`SoundBank`, same header).
- **Components speak for themselves.** Every kit component plays its own cues
  through `ui::play_cue` with a `ui::Sounds` map (`move`, `activate`,
  `cancel`, `refuse`, `change`, `step`, `page`, `open`, `close`, `notify`;
  `src/ui/components/component.hpp`). A theme names its set
  (`ui::Theme::sounds`); the proposal's theme names `glass`.
- **A mixer.** `audio::Mixer` (`src/audio/mixer.hpp`): 32 voices, three buses
  (`ui`, `sfx`, `music`) with a gain each, 48 kHz stereo S16, a soft limiter
  that never exceeds -1 dBFS, commands posted without blocking.
- **Variation.** The bank rotates through a cue's recordings and detunes each
  play by up to 3 % (`docs/SOUND.md`).
- **Two sets**, both the owner's: `assets/audio/sfx/paper` (47 files, from
  ProsperoPuzzles: warm, wooden, tactile) and `assets/audio/sfx/glass` (20
  files, from ProsperoEden: soft, airy, tuned chimes in one key).
  `THIRD_PARTY_NOTICES.md` (lines 53 to 61) says they were generated for those
  projects with ElevenLabs Sound Effects v2, prepared with
  `tools/process-sfx.py`, are Copyright (C) 2026 BlackBearReloaded and are
  distributed under the project licence.
- **Levels** the recordings are held to (`docs/SOUND.md`): ticks about
  -35 dBFS RMS (peak under -19), interface sounds -28 to -31 (peak under -12),
  chimes and fanfares -23 to -27 (peak under -1).

The `glass` set records: `focus` (2), `select`, `back`, `tab`, `open`,
`modal_open`, `modal_close`, `toggle` (3), `slider` (3), `error`, `notify`,
`launch`, `saved`, `welcome`, `resume`. It does **not** record `tick`, `type`,
`erase`, `favorite_on` or `favorite_off`; those fall back to `paper`, whose
character (wood, felt) does not match.

## 2. The voice

Afterglow is the family voice of ProsperoEden's Canopy (the `glass` set), used
more sparingly, because this app spends most of its life playing somebody
else's sound.

- **Glass, low and short.** Navigation is a soft tick you stop noticing.
  Confirmations are one clear note. Nothing in the browser lasts longer than
  half a second except the opening and the start of playback.
- **Space.** Every cue is panned by where it happened on screen (the kit's
  `ui::pan_for_x`, at most 0.55 either way): the rail sounds from the left,
  the fifth card from the right.
- **Pitch means position.** Lower shelves sound lower (1.10 down to 0.89
  across four rows), rail entries step down, a slider rises as it fills, a
  switch goes up when it turns on and down when it turns off.
- **Silence is a sound too.** Over a playing video the interface says almost
  nothing (section 5).

## 3. Cue sheet

"Kit cue" is what the scratch design asks for today, so it is what the reel
plays. "State" is: **ok** (the `glass` recording serves), **borrowed** (it
falls back to the `paper` set and should be replaced), **stand-in** (a
`glass` cue used for a moment that deserves its own sound).

| # | Interaction | Kit cue (pitch, pan, gain) | State | If new: the sound to make |
| --- | --- | --- | --- | --- |
| 1 | Focus moves (card, tile, row, key) | `focus` (1.0; pan by x) | ok | |
| 2 | Focus moves to another shelf, or between zones of a screen (tabs, grid, actions) | `tab` (1.10 - 0.07 per row) | ok | |
| 3 | The rail opens | `open` (pan -0.5) | ok | |
| 4 | Focus moves in the rail | `focus` (1.08 - 0.03 per entry; pan -0.5) | ok | |
| 5 | The rail closes | `back` (pan -0.5) | ok | |
| 6 | Confirm | `select` | ok | |
| 7 | Back, leave a screen | `back` | ok | |
| 8 | A screen opens deeper (card to video page, tile to form, search to results) | `open` | ok | |
| 9 | A section changes (rail destination, L1 / R1 section) | `tab` | ok | |
| 10 | Overlay opens (sheet, menu, dialog, share) | `modal_open` (pan toward the sheet) | ok | |
| 11 | Overlay closes | `modal_close` | ok | |
| 12 | Switch on / off | `toggle` (1.08 on, 0.94 off) | ok | |
| 13 | Slider or stepper, one step | `slider` (rises with the value) | ok | |
| 14 | Filter chip switched | `toggle` (1.08 / 0.94) | ok | |
| 15 | Keyboard: a character | `type` | **borrowed** | `type` for glass: a glass tick, 35 to 50 ms, 3 variations, as quiet as `focus` (-35 dBFS RMS), slightly brighter so typing reads apart from moving |
| 16 | Keyboard: delete | `erase` | **borrowed** | `erase` for glass: the same tick a third lower with a 60 ms tail; repeats while held, so no attack click |
| 17 | Keyboard: submit (Done, Options) | `select` | ok | |
| 18 | Refused: the end of a list, a disabled action | `error` (gain 0.6), silent on a held direction | ok | |
| 19 | Toast, notice | `notify` (kit ToastStack: a little higher for success, lower for danger) | ok | |
| 20 | Like, Watch later, favourite: set | `favorite_on` (1.12 for Like) | **borrowed** | `favorite_on` for glass: two rising glass notes a fifth apart, 220 ms, -28 dBFS RMS |
| 21 | Like, Watch later, favourite: cleared | `favorite_off` | **borrowed** | `favorite_off` for glass: the same two notes falling, 180 ms, 3 dB quieter |
| 22 | Subscribe | `favorite_on`, then the toast's `notify` | **borrowed** | uses 20; the toast is suppressed when the button itself confirmed |
| 23 | Playback starts (card or Resume to player) | `launch` | **stand-in** | `play_start`: a soft swell into one low glass note, 350 to 450 ms, -26 dBFS RMS. `launch` is a one-second flourish made for starting a game; a video app starts something every few minutes |
| 24 | Pause | `toggle` (0.92) | **stand-in** | `pause`: one damped glass note falling a tone, 120 ms, -30 dBFS |
| 25 | Resume | `resume` | ok | |
| 26 | Seek: a step, and a detent at each chapter while scrubbing | `tick` (rises 4 % per chapter; pan follows the thumb) | **borrowed** | `tick` for glass: a 20 to 30 ms glass click, 3 variations, -36 dBFS RMS; it repeats fast, so no tail |
| 27 | Seek: commit | `select` (1.1; pan follows the thumb) | **stand-in** | `seek_commit`: `tick` with a short settling tail, 90 ms, -31 dBFS |
| 28 | A segment is skipped (SponsorBlock) | `notify` (1.1, pan 0.4, gain 0.7) | **stand-in** | `skip`: a quick airy slide forward ending in a tick, 200 ms, -30 dBFS; it sounds over the video, so it is narrow-band and soft |
| 29 | Countdown to the next video: the last seconds | `slider`, rising (kit `Countdown`) | ok | |
| 30 | Countdown reaches zero, next video starts | `notify`, then 23 | ok | |
| 31 | Tab or section switch (channel tabs, library tabs, caption language bank) | `tab` (kit TabBar, pitched by position) | ok | |
| 32 | Page turn in a long list | `tab` (0.96 down, 1.04 up) | ok | |
| 33 | Settings saved (leaving a form), reply posted | `saved` (gain 0.7) | ok | |
| 34 | Sign-in succeeded | `saved` today | **stand-in** | `signed_in`: a warm three-note glass arpeggio, 600 ms, -25 dBFS RMS; once per sign-in, the only "achievement" the app has |
| 35 | Update available | `modal_open` (the dialog), no extra sound | ok | |
| 36 | The app opens | `welcome` (the kit shell plays it once) | ok | The opening is 3.3 s long; `welcome` already fits under it |
| 37 | The app closes | `back` today | **stand-in** | `goodbye`: the last two notes of `welcome` falling, 400 ms, -27 dBFS |
| 38 | Attract mode ends (a button wakes Home) | `resume` | ok | |
| 39 | A profile is chosen | `select` (kit ProfilePicker), then 19 | ok | |
| 40 | A dialog's destructive choice is confirmed | `select` (kit Dialog) | ok | No special sound: the dialog itself is the warning |

**Assets to create or choose with the owner: ten.** Five to complete the
`glass` set so nothing borrows from `paper` (`type`, `erase`, `tick`,
`favorite_on`, `favorite_off`) and five new cues for a video app
(`play_start`, `pause`, `seek_commit`, `skip`, `signed_in`; `goodbye` is
optional, an eleventh). New cues are one line each in `audio::Cue` and in the
`kCues` table of `src/audio/cues.cpp`, plus the recordings, named
`<cue>_NN.wav` (`docs/SOUND.md`, "Adding or replacing sounds"). They should be
made the way the existing ones were, in the same key as the `glass` chimes,
and pass `make audio-check`.

## 4. Rules that keep it from being tiring

- **One sound per action.** Confirming something that opens a dialog plays
  `modal_open`, not `select` and `modal_open`.
- **Held navigation does not spam.** The kit already alternates recordings and
  detunes them. On top of that: at most 12 focus ticks per second; from the
  sixth repeat of a held direction every second tick is dropped and the rest
  play 4 dB down; a refusal on a held direction is silent (the kit's
  `ui::refuse` does this).
- **Nothing over -1 dBFS**: the mixer's limiter (`Mixer::kCeiling`), and the
  recordings' own targets keep the interface 10 dB or more under a normally
  mastered video.
- **Big cues are rare.** `welcome` once per start; `play_start` once per
  video; `signed_in` once per sign-in.

## 5. Beside the video

Two different situations:

**Browsing (nothing is playing).** Everything in the cue sheet plays on the
`ui` bus at the interface volume.

**Playing.** The video's own sound is the product; the interface must not
compete with it.

| What | During playback |
| --- | --- |
| Focus ticks in the controls row, D-pad seek steps | Silent by default (setting below) |
| Scrub detents, seek commit | Play 6 dB down |
| A sheet opens or closes over the video (captions, options, Up next, chat) | Play 6 dB down |
| Focus moves inside an open sheet | Play 6 dB down |
| Skip notice, countdown ticks | Play 6 dB down; these are the "explicit cues" the player may keep when everything else is off |
| Dialog over the video (Stop watching?) | Plays at full interface level; the video is paused under it |
| Toasts | Silent while playing; the toast is still shown |

- The video's audio is **never ducked** for a tick or a sheet. It is lowered
  by 3 dB only while a dialog that needs an answer is open and the video is
  not paused (for example a next-video offer on a live stream).
- Interface sound and video sound are **two outputs** on the console, so one
  never cuts the other (see the feasibility report: EVO already opens a second
  AudioOut port for its interface sounds).
- Starting playback: `play_start` ends before the first frame's audio begins
  (the video's audio starts muted and fades in over 150 ms).

## 6. What the player can change

Under **Settings > Interface > Sounds** (per console, like the look):

| Setting | Values | Default |
| --- | --- | --- |
| Interface sounds | On, Off | On |
| Interface volume | 0 to 10 (the kit's `Settings::ui_volume` and its perceptual curve) | 7 |
| Sounds during playback | All, Notices only (skip, countdown, dialogs), Off | Notices only |
| Quiet hours | Off, or a start and an end time | Off |
| Vibration | On, Off (design-notes.md, haptics) | On |

- **Separate from the video volume.** Player volume (0 to 300 % today) and
  interface volume never touch each other.
- **Quiet hours / reduced audio.** Inside the chosen hours the interface plays
  9 dB down, `welcome`, `play_start` and `signed_in` are replaced by plain
  `select`, and vibration is off. The same reduced set applies whenever the
  player has turned on "Reduce motion" together with interface sounds at 3 or
  lower: somebody who asked for a calm interface gets a calm one.
- **Off means off**: no cue is posted at all, including the opening.
- Today EVO has one switch, **Navigation sounds** (`SettingKey::NavigationSounds`,
  `core/src/screens/SettingsScreen.cpp:468`); the table above replaces it.

## 7. What is audible in the reel

`reel/prosperotube-reel-with-sound.mp4` carries real interface sound: while
the tour ran, every cue the design and the kit's components asked for was
given to the kit's own `audio::SoundBank` and `audio::Mixer` (default bus
gains: interface 7 of 10, effects 8 of 10), and what the mixer rendered for
each simulated frame was written out and muxed with the picture. Nothing was
mixed by hand. `reel/reel-cue-timeline.txt` lists every cue with its time,
pitch, pan and gain.

Honestly:

- The recordings are the kit's **existing** sets. Where the sheet says
  *borrowed*, the reel plays the `paper` recording (typing, the seek tick,
  Like and Watch later), which is why those moments sound woodier than the
  rest. Where it says *stand-in*, the reel plays the `glass` cue named there.
- The reel has **no video soundtrack** (the "video" is a still picture), so
  the playback rules of section 5 are not demonstrated: in the reel the
  player's sheets sound at full interface level.
- The rate limit on held navigation is specified here, not implemented in the
  scratch design; the tour presses directions one at a time.
- Vibration is requested by the design (`feedback.rumble`) and is not
  recorded anywhere; the host tool has no controller.
- No sound has been heard on a console in this work.
