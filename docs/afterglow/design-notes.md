# ProsperoTube interface proposal: "Afterglow"

Design notes for the pictures in `img/` and the reel in `reel/`. Everything
shown was drawn by the kit's own PC renderer from a scratch design
(`prosperotube-design/`); nothing has been built into the app.

**The direction in two sentences.** The picture you are about to watch lights
the room: the page takes its colour from the video in focus, and that same
picture is carried, unbroken, from its card to its page to the whole screen.
The furniture is ProsperoEden's Canopy family (frosted Acrylic panels, a
120 pixel rail, staggered arrivals) turned toward video: neutral glass, white
focus, and one ember accent kept for progress, live and "on".

## 1. What is real in the pictures

| | |
| --- | --- |
| Drawn by kit code | Every pixel: `gfx::DrawList` shapes and text through `gfx::Renderer` (backdrop, scene, glass blur, overlay), the kit's fonts, glyph hints, springs and easing |
| Kit components, restyled by one theme | `ui::SearchField`, `ui::Keyboard` (twice), `ui::TabBar` (four), `ui::ListView` (three), `ui::Form`, `ui::Dialog`, `ui::ToastStack`, `ui::Skeleton`, `ui::EmptyState`, `ui::ProfilePicker`, `ui::Countdown` (twice), `ui::Spinner` |
| Bespoke drawing in the scratch file | The rail, the hero, shelves and cards, the video page, the player's controls, timeline, seek preview and notices, the sheets' frames, tiles, comment thread, sign-in and phone screens, all icons (59, drawn from strokes), the app mark |
| Sample data | Every channel, title, number, comment and chat line is invented. Thumbnails, banners and "video frames" are painted by arithmetic in `prosperotube_art.cpp` (14 scenes); no photograph, logo or real thumbnail is used. QR codes are real (ProsperoTube's own `tube_qr.h`) and encode example addresses |
| Not proven | Anything on a console: speed on EVO's renderer, glass over real video, sound on the hardware, text beyond the characters baked for these boards |

## 2. Grid and safe area

The canvas is the kit's 1920 x 1080 virtual pixels (scaled to the surface; the
4K pictures are the same layout at twice the scale). Spacing is on an 8 pixel
rhythm.

| Token | Value | Use |
| --- | --- | --- |
| Rail | 0 to 120 (392 open) | Navigation; frosted, full height |
| Content left edge | 176 | Every title, shelf, form and hint that starts at the left starts here |
| Content right edge | 1824 | 96 from the screen edge: clock, hints, right panels end here |
| Top safe line | 60 | Nothing readable above it; the clock's baseline is 104 |
| Hint row | centre 1026 | One row, right-aligned at 1824 |
| Shelf card | 352 x 198, pitch 376 | 4.4 cards per shelf: the fifth is cut by the edge to say "there is more" |
| Grid cell | 394 x 222, pitch 418 | Four columns fill 176 to 1824 exactly |
| Shelf pitch | 404 | Title, picture, two title lines, channel, numbers |
| Player margins | 96 left and right | Title, timeline, controls and hints share both edges |

Shared edges are literal: on the video page the overline, title, numbers,
channel, description, fact chips, actions and the related shelf all start at
176; the picture, the clock and the hints all end at 1824.

A shelf that only peeks over the foot of the screen shows pictures and, if a
line fits, one title line, never half a text block; a scrim closes the last
76 pixels so the hint row always sits on a clean ground.

## 3. Type

Three of the kit's faces. Sizes are virtual pixels.

| Role | Face | Size | Notes |
| --- | --- | --- | --- |
| Hero title | Montserrat Medium | 52 (line 60) | Two lines, anchored at its foot so a longer title grows upward |
| Page title | Montserrat Medium | 44 | Also the video page title (line 52, three lines) |
| Player title, panel titles | Montserrat Medium | 38 / 34 | One line, fitted |
| Shelf title | Inter SemiBold | 26 | |
| Body, buttons, list rows | Inter Regular / SemiBold | 24 to 27 | The kit's floor for body text is 24 |
| Card title | Inter SemiBold | 23 (line 30) | Two lines, ellipsis |
| Secondary lines | Inter Regular | 20 to 22 | Channel, numbers, help |
| Overline | Inter SemiBold | 17, tracked 3.2, capitals | Short labels only ("CONTINUE WATCHING") |
| Clocks and codes | DejaVu Sans Mono / Montserrat | 23 / 60 to 66 | Mono where digits change, so they do not jitter |
| Wordmark | Montserrat Medium | 23 tracked 6.5 (50 tracked 15 in the opening) | "PROSPERO" at 72 %, "TUBE" at 100 % |

Hierarchy is by size and opacity, not colour: primary text is the ink colour,
secondary 72 %, tertiary 50 %.

## 4. Colour roles

| Role | Afterglow (recommended) | Onyx | Daybreak | High contrast |
| --- | --- | --- | --- | --- |
| Page | `#06070C` to `#0A0C14` | `#000000` | `#F3EDE4` to `#E9E0D3` | `#000000` |
| Wash (how much of the focused picture colours the page) | 62 % | 14 % | 42 % | none |
| Ink | `#FFFFFF` | `#F5F1E8` | `#1C1714` | `#FFFFFF`, secondary at 100 % |
| Panels | blur + `#10121C` at 55 % | blur + `#0A0A0A` at 60 % | blur + white at 58 % | solid black, 75 % white edge |
| Accent | ember `#FF5A47` | gold `#F2C14E` | terracotta `#E2533D` | yellow `#FFD60A` |
| Focus | white ring, light in the picture's own colour | warm white ring | ink ring | yellow ring, 5.5 px |
| Primary plate | white, dark text | warm white | ink, light text | white |

- **One accent, three jobs**: progress, live, and "on" (switches, selected
  marks, the active rail entry). It is never the focus.
- **The ambient colour is the content's.** Each picture gets a 64 x 36 blurred
  copy when it is decoded, with its highlights compressed so that any picture,
  however bright, leaves a page that still carries white text. That copy is
  stretched over the page (and drifts slowly); the focus ring's light and the
  picture's glow use the picture's most vivid colour.
- SponsorBlock segments keep their own category colours (cyan intro, green
  sponsor, yellow self-promotion, blue endcards) on a lane under the timeline,
  so the progress bar itself stays one colour.
- Status colours (danger `#FF5D6C`, success `#4FD8A0`, warning `#F5C451`) are
  the theme's and appear only in dialogs, toasts and the offline banner.

Corner radii, one per kind of thing: pictures 16, panels and tiles 28, sheets
36 to 44, tags 8, buttons and chips full pills. Elevation: resting pictures
have none; the focused one lifts 5 to 6 px on a 40 px shadow; panels float on
a 44 px shadow; sheets cast one toward the content.

## 5. The mark

A ring that does not quite close (the mouth of the tube), a second arc turned
the other way behind it, and one ember dot: the afterglow. It is drawn from
three kit arcs, so it animates by drawing itself, scales to any size and
takes any palette. It is not a play button and uses nothing of YouTube's
marks; "YouTube" appears only as plain text where the service must be named.
The opening draws it, sets the wordmark from the middle outward, shows the
one true thing happening meanwhile (the service starting) as a hairline, and
then carries the mark to its place at the head of the rail.

## 6. Screens, and what they are built from

| # | Picture | Kit components | Bespoke |
| --- | --- | --- | --- |
| 01-02 | Opening | (shapes and text only) | Mark, wordmark, start-up line |
| 03-05 | Home, signed in | ToastStack, glyph hints | Hero with the focused picture dissolving into the page, play line, shelves, cards, focus ring |
| 06 | Home, rail open | glyph hints | Rail: labels, gliding plate, pinned channels, profile |
| 07 | Home, attract | glyph hints | Full-screen picture, lockup, time-to-next line |
| 08 | Home, Guest | glyph hints | Topic tiles, sign-in invitation, trending shelf; as built, shelves of videos stand above the topic tiles (ARCHITECTURE.md, 5.4) |
| 09 | Search | SearchField (field, suggestions, recent searches, match underline), Keyboard | "Best matches" strip that fills as you type |
| 10 | Results | Skeleton (bones while loading) | Filter chips, video, channel and playlist cards |
| 11-12 | Channel | TabBar (underline) | Banner, avatar, Subscribe and notification level, featured video, latest uploads, grid |
| 13 | Video page | glyph hints | Picture, details, fact chips, actions, related shelf |
| 14 | More actions | ListView with an icon slot | Sheet frame |
| 15 | Share | glyph hints | Panel, QR (the app's encoder), link chips |
| 16 | Player | glyph hints | Title block, quality pill, chaptered timeline, SponsorBlock lane, controls |
| 17 | Seek | glyph hints | Preview tile, time, swelling chapter, stem |
| 18 | Up next | glyph hints | Rising shelf, autoplay chip |
| 19 | Captions | TabBar (pill), ListView | Sheet frame, check mark, two-language caption preview |
| 20 | Options | ListView with an icon slot | Sheet frame, value column |
| 21 | Live chat | glyph hints | Theatre layout, chat column, live bar |
| 22 | Skip notice | glyph hints | Notice with draining line |
| 23 | Next video | Countdown (ring) | Card, buttons |
| 24 | Stop | Dialog | |
| 25 | Library | TabBar (pill, with counts) | Grid |
| 26 | Subscriptions | TabBar (pill) | Channel strip, grid |
| 27 | Notifications | glyph hints | Rows, gliding plate |
| 28 | Channels | glyph hints | Tiles, order chips |
| 29-30 | Comments, reply | Keyboard | Thread, context column, reply sheet and field |
| 31 | Settings | glyph hints | Twelve tiles |
| 32 | Settings: Captions | Form (switch, choice, slider rows, section headers, help text) | Live caption preview |
| 33 | Accounts | ProfilePicker | Action buttons |
| 34 | Sign in | Spinner, Countdown (text) | Steps, code boxes, QR panel |
| 35 | Link a phone | glyph hints | Code groups, device list, phone drawing |
| 36 | Loading | Skeleton (lines, block, cards), Spinner | |
| 37 | Empty | EmptyState, TabBar | |
| 38 | Offline | EmptyState, Spinner | Banner, "kept on this PS5" tiles |
| 39 | Toasts | ToastStack (frosted) | |
| 40 | Update | Dialog | |
| 41 | Remove account | Dialog (destructive), ProfilePicker | |
| 42-43 | German | as 03 and 31 | |
| 44 | Mixed scripts | as 10 | |

In the app more of the bespoke column would become kit components that
already exist and were not needed for pictures: `ui::Sheet` and `ui::Menu`
(sheet frames), `ui::Chip` / `ui::TagSelect` (filters), `ui::Banner`
(offline), `ui::TextView` (description, release notes), `ui::PinEntry`
(profile password), `ui::FocusGroup` (moving between zones), `ui::Avatar`.

## 7. Focus

- **One focus, never by colour alone.** The focused thing has a ring
  (3.5 px, 5 px out), grows (6 % for shelf cards, 5 % in grids, 3 to 4 % for
  tiles), lifts on a shadow, and the ring spills light in the picture's own
  colour. In lists the focus is a frosted plate that glides under the row.
- **The ring is its own object.** One `ui::SpringRect` per screen glides
  between targets; cards read how much of it covers them, so growth follows
  the ring instead of popping.
- **Focus order is spatial**: left of the first card is the rail; up from a
  grid is its filters or tabs; down from the actions is the related shelf.
- **Edges answer.** Past the last card: a soft `error` at 60 %, a short
  rumble, the ring nudges 14 px and settles. Held directions stay silent.
- **Selected is not focused.** A selected chip is a filled plate; the chosen
  caption language keeps a check mark whatever is in focus; the active rail
  entry has an ember bar.

## 8. Motion

Nothing teleports and nothing blocks: every position is a spring or a short
timeline, so a press in the middle of a move bends it. Durations are as
written in the scratch design; springs are critically damped, "w" is the
spring's speed in rad/s (20 settles in about 150 ms, 13 in about 300 ms).

| Element or transition | Duration, easing | What moves | With "Reduce motion" |
| --- | --- | --- | --- |
| Opening | 3.3 s in all | Mark draws itself (arcs, 1.15 s cubic-out; the dot pops with overshoot); letters set from the middle outward, 50 ms apart, rising 16 px; a hairline fills while the service starts | Letters and mark fade, no travel |
| Opening to Home | 0.52 s, cubic in-out | The mark travels to the head of the rail and shrinks 208 to 46 px while Home assembles under it | 0.2 s cross-fade |
| A screen arrives | groups 50 to 70 ms apart, each 0.45 to 0.6 s cubic-out | Rail from the left (48 px), header from above (14 px), content rising 20 to 30 px, cards one after another | Opacity only |
| Focus glide | spring w 20 | Ring position and size; card grows 6 % and lifts 6 px; shadow and light fade in; the light breathes on a 2.8 s period | Snaps (w 60), no breathing |
| Hero follows the focus | 0.5 s | Old title leaves in the first 40 % (30 px left, cubic-in), the new lands from 28 % (38 px, quint-out); the backdrop picture cross-fades | 0.12 s cross-fade |
| Ambient wash | cross-fade 0.7 s, smoothstep; drift on 57 s and 70 s periods | The blurred copy of the focused picture; it wanders 44 x 30 px so a still screen is not dead | 0.2 s; no drift |
| Shelf scroll, shelf to shelf | springs w 12 and w 11 | Shelves slide; the one above fades out, the one below waits at 60 % | Snaps |
| Edge of a list | decays in about 0.3 s | Ring shakes 14 px at 8 Hz | No shake; the cue and rumble remain |
| Rail opens / closes | spring w 15 | Width 120 to 392, labels fade in from 40 % of the way, content dims under a scrim, the plate glides at w 22 | Snaps |
| Section change (rail, prepared boards) | 0.34 s | Old screen sinks to 97.5 % and is gone by 40 %; the new assembles | 0.2 s fade |
| Card to video page | 0.52 s, quint-out | The card's picture grows into the page's picture (radius 16 to 24); the page's text and buttons start 160 ms later; the old screen's furniture is gone in 150 ms | 0.2 s fade |
| Video page back to card | 0.44 s, cubic in-out | The picture returns to its card on top of the arriving screen | 0.2 s fade |
| Picture to player | 0.62 s, quint-out | The same picture becomes the whole screen (corners to 0); controls fade in over the second half; `launch` cue and a rumble | 0.2 s fade |
| Player back | 0.5 s, cubic in-out | Controls are gone in the first quarter; the picture returns to where it came from (page or card) | 0.2 s fade |
| Deeper / back out (tile to form, search to results) | 0.4 s | Old slides 90 px and fades (cubic-in); new comes 110 px from the other side (quint-out) | Fades |
| Channel tabs | kit TabBar (w 18, slight overshoot) | The underline glides; each tab's content slides 220 px with it and cross-fades | Fades |
| Sheets over the video (captions, options) | spring w 13 | Slide 140 to 160 px from the right; a shadow falls on the video; the video is never resized | Fades |
| Up next | spring w 13 | Rises 150 px over the foot; the timeline gives way | Fades |
| Live chat | spring w 13 | The video steps aside into a theatre frame while the chat slides in 120 px; messages rise one by one | Jumps to the layout; messages fade |
| Seek | thumb and bar spring at w 20 | Bar 6 to 11 px, thumb 9 to 13, the chapter under the thumb swells 3 px; preview tile rises 22 px; hold accelerates from 2 % to 7 % of the video per second over 1.5 s | No rise; values change at once |
| Skip notice | in 0.4 s cubic-out, 4.3 s on screen, out 0.4 s | Slides 60 px from the right; a line drains to show how long "Undo" stands | Fades |
| Next-video card | spring w 13; ring 5 s | Card rises 70 px; the kit's ring drains | Fades; ring still counts |
| Pause mark | decays in about 0.5 s | Pops 16 % | No pop |
| Like, Watch later | decays in about 0.4 s | Button swells 10 % with a burst of accent light; the count rolls | Colour change only |
| Subscribe | spring w 13 | Label and width change, a check replaces the plus; a toast confirms | Instant |
| Switches, sliders, choices | kit Form (w 18) | Thumb travels with a slight overshoot; slider fill; values cross-fade; the caption preview follows live | Kit's reduced mode |
| Keyboard | rows 40 ms apart, 0.34 s; rise 70 px for the panel | Keys arrive by row; a pressed key dips 6 %; the highlight glides, wraps at the edges | Fades |
| Dialog | kit Dialog | Panel grows from 90 % and rises 28 px with a small bounce; scrim to 50 %; leaves 1.8 times faster | Fades |
| Toasts | kit ToastStack | Slide in, wait with a time line, slide out; the stack closes the gap | Fades |
| Loading | shimmer every 1.7 s; 0.3 s cross-fade | Bones in the shape of the content, then the content in the same places | Bones without shimmer |
| Live badge | 1.6 s period | A halo breathes around the dot | Still |
| Sign-in code, TV code | 0.4 s each, 50 to 70 ms apart, overshoot | Boxes pop in one by one | Fade |
| Attract | after 45 s idle (a setting; Off possible) | Furniture fades over about 1 s; the focused picture takes the whole screen and drifts 5 % in 6 s; every 6 s the next video cross-fades in; any button returns and is swallowed | Off |

Rules behind the table: arrivals ease out, departures ease in and are
shorter; focus moves settle in 120 to 200 ms, panels in 250 to 350, screen
changes in 400 to 620; input is taken on the frame it arrives, during any
transition.

## 9. Sound and haptics

Sound has its own page: `sound-design.md` (cue sheet, mixing beside the
video, settings).

**Haptics (DualSense), a plan. None of it has been felt on hardware; the
scratch design only requests it through the kit's `feedback.rumble`.**

| Moment | Request | Why |
| --- | --- | --- |
| The focus reaches an edge (end of a shelf, top of a list) | 0.25 for 50 ms | A soft stop; nothing on ordinary moves |
| Playback starts | 0.6 for 160 ms | The one "launch" of the app |
| Like, Watch later, Subscribe confirmed | 0.3 for 60 ms | A tick of agreement |
| Scrubbing passes a chapter mark | 0.18 for 30 ms | A detent, like a dial |
| Refused action (disabled, signed out) | 0.25 for 50 ms, with the `error` cue | The same answer everywhere |
| Countdown to the next video: the last second | 0.2 for 40 ms | Only if sounds are off |

- Never on focus moves, typing or sheets.
- **Settings > Interface > Vibration**: On / Off (the kit's
  `Settings::haptics`), off inside quiet hours.
- To verify on a console: that 30 to 60 ms pulses are felt at these
  strengths, that the detent does not blur into a buzz while scrubbing fast
  (drop to one per 120 ms if it does), and whether the adaptive triggers
  should carry the scrub instead.

## 10. Controller map

`Cross` confirms and `Circle` goes back everywhere (or the reverse when the
player swapped them: the design uses the kit's `Action::confirm` and
`Action::back`). Hints show the PlayStation glyphs; the middle button is the
**Touchpad**.

| Screen | Cross | Triangle | Square | Other |
| --- | --- | --- | --- | --- |
| Home, a video | Play | Details (video page) | Watch later | Touchpad: Actions. Left of the first card: rail |
| Home, a topic (Guest) | Open topic | | Search | Options: Sign in |
| Rail | Open | | | Circle or right: close |
| Search | Type the key | Space | Delete | L2 Shift, R2 Symbols, Options: Search |
| Results | Play | Details | Search again | Up: filters. Touchpad: Actions |
| Channel | Play | | Search in channel | Section: previous / next. Touchpad: Actions |
| Video page | Choose the action | | | R2: Comments. Touchpad: Actions |
| Player | Pause / play | Up next | Captions | D-pad left / right: 10 s, hold to scrub. Options: playback options (quality, audio, speed, video size). Circle: back |
| Player, seeking | Play from here | | | Chapter: previous / next. Circle: cancel |
| Player, a sheet | Choose | | | Circle: close |
| Skip notice / next video | Undo / Play now | | | Circle: cancel the next video |
| Library, Subscriptions | Play | Channels (Subscriptions) | Remove (Library) | Section or filter: previous / next. Touchpad: Actions |
| Notifications | Play | Open channel | Mark all as read | |
| Channels | Open | Mark as watched | Order | Touchpad: Actions |
| Comments | Reply | Like | | Top or newest. Writing: Options posts, Square deletes, Triangle is space |
| Settings | Open | | | Options: reset a section |
| A settings form | Switch | Defaults | | D-pad left / right: change |
| Accounts | Watch as | Set a password | | Sign in: Square gets a new code |

Two honest notes. In the pictures "section previous / next" is drawn as
**L2 / R2**, because the kit's demo shell keeps L1 / R1 and the touchpad for
itself; in the app L1 / R1 would turn sections and the shoulder triggers stay
free for chapters, keyboard shift and symbols. And the map changes some of
today's buttons (today Square searches and Triangle saves a favourite
everywhere); that is a decision for the owner, not a consequence of the look.

## 11. Long words, other scripts, right to left

- **Titles** wrap to two lines and end in an ellipsis (`Font::wrap`,
  `ui::paragraph`, `Font::fit`); a word wider than its line breaks between
  characters (the kit's commit `f775c31`). The hero is anchored at its foot,
  so one line or two never moves the play line or the shelf.
- **Buttons, chips and tags are measured**, not sized by hand; the rail opens
  to 392 px, enough for "Einstellungen" and "Benachrichtigungen".
- **German** (pictures 42 and 43): settings tiles take a two-line name and
  then give their summary one line instead of two ("Sicherung und
  Wiederherstellung"); summaries wrap ("Wiedergabegeschwindigkeit, Auflösung
  und Bildrate"); nothing is cut mid-word.
- **Hint rows** are right-aligned and measured. When a language makes a row
  longer than the width, the last hints drop first (they are written in order
  of importance) and remain reachable through Touchpad: Actions.
- **Other scripts** (picture 44) are drawn by the kit's fallback faces: a
  title may mix Latin, Japanese, Korean, Cyrillic, Greek and symbols in one
  line, each character in the first face that has it, with that face's own
  metrics. Line height is the Latin face's, so shelves do not move.
- **What picture 44 does not show, because the kit cannot draw it yet**:
  Arabic and Hebrew (no shaping, no right-to-left order), Indic scripts and
  Thai (no shaping), colour emoji (atlases are one channel; the two emoji
  shown are outline drawings from Noto Emoji), emoji sequences and skin tones,
  and bold or semibold in the fallback faces (the Japanese and Korean titles
  are lighter than the Latin ones beside them). Any character outside the
  baked sets draws as "?". `img/text-engine/` keeps the same two boards as
  the kit's stock atlases draw them. The feasibility report says what the
  text engine needs.
- **Right-to-left interface** (not drawn): the rail moves to the right edge,
  shelves start at the right and scroll the other way, the hero text is
  right-aligned and the picture dissolves toward the left; timelines, clocks
  and the SponsorBlock lane stay left to right.

## 12. Accessibility

| Need | Answer | State |
| --- | --- | --- |
| High contrast | A fourth look: no wash, solid black panels with a 75 % white edge, secondary text at full strength, yellow focus 5.5 px thick, yellow accent | Rendered: `img/alt-contrast-home.png`, `img/alt-contrast-settings-captions.png` |
| Less motion | The table in section 8; the kit's `reduced_motion` setting is honoured in every bespoke animation and passed to every component | Implemented in the scratch design; not rendered as pictures (a still looks the same) |
| Larger text | A type scale of 100 / 115 / 130 %: at 115 % a shelf shows 3.8 cards (card 404 wide) and grids stay at four columns with one title line less; at 130 % shelves show 3.3 cards, grids three columns | Specified, not rendered |
| Colour is never the only signal | Focus is ring, size and lift; selected is a filled plate or a check; live is a dot and the word; unread is a dot beside the row; the offline banner has an icon and words | In the pictures |
| Captions | Size 60 to 200 %, four styles, background strength, position, a second language; the preview shows the real size | Picture 32 |
| Hint size | The kit's hint-size setting (small, medium, large) applies to every hint row | Kit feature |
| Screen reader | Not available: neither the kit nor EVO has speech output | Open |
| Photosensitivity | No flashes; the fastest repeating motion is the 1.6 s live halo | By design |

## 13. Polish checklist, and every picture against it

What each screen was held to:

1. **Grid**: readable things inside 176 to 1824 and 60 from top and bottom;
   shared left and right edges; rows that fill the grid.
2. **Spacing**: an 8 px rhythm; related things closer than groups.
3. **Text**: never clipped or overlapping; long text wraps or ends in an
   ellipsis.
4. **Focus**: exactly one, visible at a glance, not by colour alone.
5. **Radii and elevation** from the short list in section 4.
6. **Loading is never blank**: bones in the shape of the content.
7. **Arrival**: the screen assembles in a stagger.
8. **Sound**: every move, confirmation, refusal and overlay has a cue.
9. **Reduced motion** honoured.
10. **Budget**: shapes and draw calls inside the kit's comfortable range
    (feasibility report, section 6).

Marks: "ok", or a note. Columns 7 to 9 are properties of the code and hold
for every screen except where noted. All 44 pictures were looked at on review
sheets at half size; the hero screens (03, 08, 13, 16) and the mixed-script
board at full size and in enlarged crops.

| Picture | 1 Grid | 2 Space | 3 Text | 4 Focus | 5 Radii | 6 Loading | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 01-02 opening | ok | ok | ok | none (no input) | ok | is the loading state | Centred composition, the only one |
| 03 home | ok | ok | ok | ok | ok | 36 | The next shelf peeks as pictures only |
| 04 shelf focus | ok | ok | ok | ok | ok | 36 | |
| 05 second shelf | ok | ok | ok | ok | ok | 36 | Hero picture crosses under the title's right end; the page is evened out there |
| 06 rail open | ok | ok | ok | ok | ok | 36 | The open rail covers the hero's first words by design (scrim, 392 px) |
| 07 attract | ok | ok | ok | none (any button) | ok | n/a | Picture is the 1080p sample frame: softer at 4K |
| 08 guest | ok | ok | ok | ok | ok | 36 | Trending shows one title line; a sample card carries a progress bar a Guest would only have from local history |
| 09 search | ok | ok | ok | ok (kit) | ok | spinner in the field | |
| 10 results | ok | ok | ok | ok | ok | bones, then content | The channel card is taller than a video cell on purpose |
| 11-12 channel | ok | ok | ok | ok | ok | not drawn | Second grid row peeks as pictures |
| 13 video page | ok | ok | ok | ok | ok | not drawn | Fact chips end 26 px left of the picture: tight in longer languages |
| 14 more actions | ok | ok | one row ends in an ellipsis | ok (kit) | ok | n/a | "Exclude channel from SponsorBlock" is longer than the sheet; the row would scroll or the sheet widen |
| 15 share | ok | ok | ok | ok | ok | n/a | |
| 16 player | ok | ok | ok | ok | ok | n/a | |
| 17 seek | ok | ok | ok | ok | ok | preview tile needs storyboards | The preview picture is a stand-in view of the scene |
| 18 up next | ok | ok | ok | ok | ok | n/a | |
| 19 captions | ok | ok | ok | ok (kit) | ok | n/a | |
| 20 options | ok | ok | ok | ok (kit) | ok | n/a | The frosted sheet shows the sun through it; tint raised to 1.5 for legibility |
| 21 live chat | ok | ok | ok | none drawn | ok | n/a | Video picture enlarged from a 960 x 540 sample: soft |
| 22 skip notice | ok | ok | ok | ok | ok | n/a | |
| 23 next video | ok | ok | ok | ok | ok | n/a | The kit ring's figure is small at this ring size |
| 24 stop | ok | ok | ok | ok (kit) | ok | n/a | |
| 25 library | ok | ok | ok | ok | ok | as 10 | |
| 26 subscriptions | ok | ok | names end in an ellipsis under the faces | ok | ok | as 10 | Second row peeks as pictures |
| 27 notifications | ok | ok | ok | ok | ok | not drawn | |
| 28 channels | ok | ok | ok | ok | ok | not drawn | Two rows of six; the foot of the page is empty with 12 channels |
| 29-30 comments | ok | ok | ok | ok | ok | not drawn | A comment is shown whole or not at all |
| 31 settings | ok | ok | ok | ok | ok | n/a | |
| 32 captions form | ok | ok | ok | ok (kit) | ok | n/a | |
| 33 accounts | ok | ok | ok | ok (kit) | ok | n/a | The kit picker colours avatars from the name, not from the palette |
| 34 sign in | ok | ok | ok | none needed | ok | spinner, time left | |
| 35 link a phone | ok | ok | ok | none drawn | ok | n/a | |
| 36 loading | ok | ok | ok | n/a | ok | is the state | |
| 37 empty | ok | ok | ok | hint only | ok | n/a | |
| 38 offline | ok | ok | ok | ok | ok | spinner in the banner | |
| 39 toasts | ok | ok | ok | ok | ok | n/a | |
| 40 update | ok | ok | ok | ok (kit) | ok | n/a | |
| 41 remove account | ok | ok | ok | ok (kit) | ok | n/a | |
| 42-43 German | ok | ok | ok | ok | ok | n/a | |
| 44 mixed scripts | ok | ok | ok | ok | ok | n/a | Fallback faces are one weight; emoji are outlines; no right-to-left script |

Not held to the checklist: the two alternative looks beyond their Home
picture (the other `alt-*` pictures are the same boards unreviewed in detail),
and anything on a console.

## 14. What changed after looking

The pictures were rendered, looked at and corrected in passes. The changes
that came from looking:

**Pass 1 (first full render of Home).**
- A hard-edged dark rectangle behind the hero title: replaced by a soft
  shadow shape, so the page under the title is evened out with no visible
  edge.
- The next shelf's title sat 19 px under the previous shelf's numbers: shelf
  pitch 384 to 404.
- The peeking shelf showed through the hint row: shelves that only peek draw
  pictures (and one title line where it fits), and a scrim closes the last
  76 px.
- Bright pictures (a white plate, a day sky) made a page too light for white
  text: the ambient copy's highlights are compressed when the picture is
  decoded.
- Tags sat on the focus ring's corner: moved 2 px in.

**Pass 2 (all boards on sheets, hero screens at full size).**
- Guest: the trending shelf's text ran under the hints; the whole Guest
  layout moved up and the shelf got its one-line titles.
- Filter chips had their chevrons outside the pill: a chip that opens a list
  now carries its chevron inside.
- The seek board showed no preview (the scrub committed at once without a
  held button): the prepared board pins it.
- Options sheet: values cut the labels ("Audio tra..."): values shortened;
  both player sheets got a stronger tint, because the frosted sun reduced
  contrast.
- Stop dialog: "Stop" was the white primary button while "Keep watching" had
  the focus; the safe choice is now the primary one.
- Subscriptions: the filter collided with the clock; moved left of it.
- Accounts: profile cards were small and left-aligned in a wide page: larger
  and centred, with the actions centred under them.
- Channels: tiles enlarged to fill the page better.
- Comments: a sixth comment was cut by the foot of the list; a comment is now
  shown whole or not at all. The reply keyboard was missing (its animation
  was never advanced): fixed.
- Video page: the fact chips ran 26 px past the text column toward the
  picture: labels shortened.
- Text: accented letters, curly quotes and anything beyond ASCII drew as "?"
  with the kit's stock atlases. The three text faces were re-baked with the
  baker's `european` set, and fallback faces were added for the mixed-script
  board; its Japanese and Korean titles were rewritten to use characters the
  open fonts on this PC hold.
- Daybreak: the scrim behind the hero title showed as a pale block and the
  clock was dark on the picture: scrim lowered for light pages, the clock got
  a glass plate.
