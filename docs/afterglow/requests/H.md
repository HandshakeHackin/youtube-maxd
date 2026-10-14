# Package H (the player's controls): requests to the frame owner

The player is drawn from one `PlayerView` (`native/afterglow/player/view.hpp`).
On the host every member is filled by fixtures and the approved boards 16 to 24
are reproduced. In the app `native/afterglow/player/app/mount.cpp` fills what
EVO's player and the provider's accessors can say today. Each request below
names a member the app cannot fill, or a shared file this package may not
edit. Until a request is applied the stand-in named with it is what draws; in
the code they are marked `// REQUEST:`.

## What the player has to say or do (PlayerScreen, the provider, overlays)

1. **The proposal's controller map in the player.** Implemented for Afterglow
   through `scripts/afterglow_controls_overlay.py`: Triangle opens the existing
   Up next / related-video list, Square opens Captions, and Options opens
   grouped playback options. Down and R2 remain aliases. Classic retains its
   bindings. Scrubbing, phone ownership, SponsorBlock asks/Undo and the next
   video offer keep ownership ahead of these shortcuts. Media Details and
   Playback details (codec statistics) remain reachable from playback options.
   Mounted hints match the actual map; host fixtures carry the same proposal.
   `scripts/test_afterglow_controls.py` executes the generated map and detail
   actions, ownership guards and aliases. The 85 player boards pass in four
   looks; console acceptance remains pending.
2. **Facts of the playing video**: implemented from optional resolve metadata.
   `tube_playing_fact()` owns bounded channel name/id, views and publish date
   for the active video; stop and source changes clear them. The mount fills
   `PlayerView::channel`, `channel_id`, and `meta`; playback status still takes
   the second line while buffering. An avatar appears only for an allowed
   channel image address. The current services provide none, so the player's
   existing initial stands in. Host-tested; console acceptance pending.
3. **How far the stream is read ahead**, in seconds, for the lighter stretch of
   the timeline (`PlayerView::buffered`). Stand-in: not drawn.
4. **The next video as an offer** (board 23): implemented through
   `tube_next_offer()`, `tube_next_offer_hold()` and `tube_next_offer_input()`.
   The card previews the validated next item without consuming it. It waits
   five seconds; Cross plays now or activates the focused Cancel button,
   Circle cancels and directions select the button. Classic, phone ownership
   and explicit Previous/Next keep their existing paths. Host-tested; console
   acceptance pending.
5. **Undo of an automatic segment skip** (board 22): implemented through
   `tube_sponsor_last_skip()` and `tube_sponsor_undo_started()`. The notice
   starts after the seek lands, lasts five seconds and preserves pause when
   undone. A final-segment skip delays completion while Undo stands. Silent
   skips stay silent. Host-tested; console acceptance pending.
6. **The category colour of a segment that asks**: implemented by
   `tube_sponsor_prompt_rgb()` and used in the mounted notice.
7. **The video's rectangle** (board 21, the theatre layout): implemented with
   a per-frame destination prepared before EVO blits video. Direct YUV and
   final upscaling/interpolation passes use `Player::video_rect()`; scratch
   passes stay local. Existing aspect, zoom, rotation and mirror compose inside
   that rectangle. The existing rounded mat follows `Player::video_radius()`;
   no shader or driver change is needed. Mounted chat enables theatre on the
   profile's chosen side, including a mirrored left-side layout. Closing chat,
   Classic, stopping, another screen, scrubbing, stats, comparison, music and
   phone ownership restore the full rectangle before the next video draw.
   Generated application/render mapping checks and the real Player's spring
   lifecycle pass; six theatre boards pass in four looks. Console acceptance
   of the video/mat alignment and moving-frame rendering remains pending.
8. **Chat facts**: a line's badge (member, moderator, the channel), the
   author's picture, the filter in use ("Top chat"), how many are watching and
   since when (`PlayerView::live_status`), and the prompt of the field to write
   in. Stand-in: names take a colour from their text, no badges, no field.
9. **The player's state while a picker or the stop confirmation is on top.**
   Boards 19, 20 and 24 show the controls beside the sheet and under the
   dialog. In EVO a picker is a screen of its own and PlayerScreen does not
   render under it. Proposed: PlayerScreen keeps calling
   `evo_rmlui_update_playback_params()` while one of its pickers or
   ExitConfirm is the top screen. Stand-in: the mount shows the sheet or the
   dialog over the bare picture.
10. **Lists as the boards have them.** Implemented for Afterglow through
    `scripts/afterglow_picker_overlay.py`: repeated playback choices become
    groups with their current values; the focused group's real choices appear
    beside it. Cross opens that choice list and Circle restores the group
    focus. Existing preference, preset and chapter groups expose their actual
    choices too. Long previews show their window and full count; all choices
    remain selectable in the opened list. Captions use primary/second-language
    tabs selected with L1/R1, Cross selects in the active language, Square
    changes the shared caption size and Left/Right adjusts the active
    language's sync. Off in the second tab clears only the second language;
    Auto-sync stays in the primary tab. The list reports its actual length.
    Classic retains its original picker behavior. Generated-input sanitizer
    checks and four-look player boards pass; console acceptance remains pending.

11. **The stop confirmation opens on Keep watching** (design notes, section
    14: "the safe choice is the primary one"): implemented in the generated
    `ModalDialogScreen::onEnter()` through `scripts/afterglow_overlay.py`.
    Afterglow ExitConfirm opens with action 0 focused, so an initial Cross
    keeps watching; directions can select Stop, and Circle keeps watching.
    Classic retains its Stop default and ResumePrompt retains Resume.
    `scripts/test_stop_confirmation.py` executes the generated entry and input
    paths with and without the console build flag. The Afterglow console
    driver reads the mounted dialog focus, moves from Keep watching to Stop
    and verifies that selection before Cross; unknown or unresponsive focus
    falls back to the explicit development stop command. The simulator and
    console driver regressions cover both focus positions and refusal cases.
    Host-tested; console acceptance remains pending.
12. **Related videos with their facts**: implemented through
    `tube_player_menu_row_t`. The provider keeps each suggestion's selected
    picture address (including DeArrow), duration, live state and badges for
    the mounted card. The picture passes the provider artwork allowlist;
    a valid video id supplies the YouTube thumbnail when none is usable.
    Provider and mounted-card host checks pass; console acceptance pending.
13. **Reduced motion and the sounds of the player.** `PlayerView::reduced_motion`
    has no preference to read; `Player::feedback()` collects the cues of
    sound-design.md's "Playing" column (6 dB down) and nobody plays them, as
    with the frame's.

## Shared files of the interface

14. **theme.hpp**: the sizes of design-notes section 3 that it does not name:
    38 (title over the video), 34 (panel titles), 27 (chapter name), 25 (rows
    of a sheet), 23 (times, mono), 22, 21, 19. Stand-in: `player/parts.hpp`,
    `kTypePlayerTitle` to `kTypeSmall`.
15. **theme.hpp / Look**: roles for what is drawn on the video itself, which is
    dark under every look: `on_video` (white), the shade (black), `on_lit`
    (ink on the lit mark). Stand-in: `kOnVideo`, `kBlack`, `kOnLit` in
    `player/parts.hpp`.
16. **text.hpp**: `fit()` with tracking, for overlines (capitals tracked 3.2
    are wider than `fit()` measures). Stand-in: `fit_tracked()` in
    `player/player.cpp`.
17. **design.hpp**: the live tag as a piece (`live_tag()` in
    `player/player_draw.cpp` draws the same tag `video_card()` draws inside
    itself); a sheet frame and a row with value, mark and chevron, if package
    B's actions sheet wants the same rows (`Player`'s list content).
18. **app/mount.cpp**: (a) who owns the text engine's frame while the player
    is up. Stand-in: the player's mount reaches the engine through
    `gfx::Font::shaper` and calls `begin_frame()` once per presented frame and
    `end_frame()` after each part, unless the Afterglow frame drew that frame.
    Proposed: `afterglow::mount_text_begin()` / `mount_text_end()`. (b) The
    app's Platform for the player's pictures when no frame was ever created
    (playback started from the Classic browser, then the preference changed).
    Stand-in: `afterglow::platform()`, pictures stay placeholders while it is
    null. (c) EVO's toasts over the Afterglow browser are drawn by the
    player's own `ToastStack`, beside the frame's: route them into
    `Nav::toast()` so there is one stack.
19. **The hand-over picture** from the video page to the player (section 12b,
    open): `Player` arrives with its controls only.
20. **Catalogs**: 51 interface texts of `native/afterglow/player` are written
    as `tr("...")` and are in no catalog yet (`scripts/i18n_extract.py`).
21. **The console session driver**: while the player's controls or a sheet
    drew, `evo_hui_dev_focus_json()` answers with the document
    `afterglow_player` and `Player::snapshot_json()` (focus ids `osd-playing`,
    `osd-paused`, `osd-scrub`, `sponsor-review`, `sheet/<n>`, `upnext/<n>`,
    `next/play`, `dialog/<n>`).
22. **ARCHITECTURE.md**: section 1 ("The player's controls: stay the existing
    ones in this stage") and the table of section 12 (package H: "later
    stage") are out of date; section 12b can name `player/view.hpp`,
    `player/app/mount.cpp` and the boards `player*.json`.

## Blocks this package added to shared files (all marked "package H")

- `tests/afterglow/host_main.cpp`: one include; the lambda `player_board`
  before the loop over board files (brings a `"player"` board up, steps it,
  checks it, writes its picture); four lines at the top of the loop over looks
  that hand such a board to it. At the merge with the frame owner's newer
  harness these two blocks move as they are; they use `renderer`,
  `framebuffer`, `pixels`, `row`, `engine`, `platform`, `fonts`, `reel`, `kDt`,
  `results`, `options`, `most_missing`, `text_of`, `number_of`, `fail` and
  `read_file`.
- `scripts/afterglow_overlay.py`: the call `player_mount(work, replace, player)`
  at the end of `apply()`, and below it `PLAYER_FOCUS`, `PLAYER_LIST`,
  `PLAYER_PAGE` and `player_mount()`: the Makefile line for
  `ui_kit/src/afterglow/player/*.cpp` and `player/app/*.cpp`, and the hooks in
  `ui_kit/src/evo_hui_app.cpp` (six `evo_hui_update_*`, five `evo_hui_render_*`,
  the report).
- New, not shared: `tests/afterglow/tools/player_boards.py` writes the five
  board files.
