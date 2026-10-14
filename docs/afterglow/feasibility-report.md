# ProsperoTube "Afterglow": feasibility report

What it would take to ship the proposed interface in ProsperoTube. Evidence is
given as file and line. Paths: `PT` = `~/ProsperoTube` (branch
`codex/first-version`), `EVO` = `PT/third_party/evo/projects/evoplayer`,
`KIT` = `~/ps5-homebrew-ui` (main, `4b3fbb7`). Everything was read on the PC;
nothing was built for a console, run on one, or changed in any repository.

"Read" means I opened the file; "research pass" means a read-only helper
gathered it and I did not re-open every line; "inferred" and "estimate" mean
what they say.

## 1. Where the app stands

### 1.1 The kit inside EVO

| Fact | Evidence |
| --- | --- |
| ProsperoTube pins EVO v0.11.0; EVO pins the kit as a submodule at `4bd9425` (`01.000.000-1-g4bd9425`) | `git submodule status` in `PT` and in `PT/third_party/evo` (read) |
| The pin is 7 commits behind kit main `4b3fbb7`, and only one of them touches `src/`: `f775c31`, "Let a font fall back to other faces and break a word wider than its line" (`gfx/font.hpp`, `font.cpp`, `draw_list.cpp`) | research pass; `git log -- src/gfx/font.hpp` shows `f775c31` directly on the first commit (read) |
| So the pinned kit already has every component the proposal uses: `src/ui/components/` holds the same 148 files at the pin and at main, with `ui/theme.*`, `ui/widgets.*` (Painter) and `ui/motion.hpp` | research pass |
| EVO compiles the kit's draw list, font, all of `ui/*.cpp`, all of `ui/components/*.cpp`, `core/*.cpp` and `audio/cues|mixer|wav.cpp` | `EVO/Makefile:238-245` (read) |
| EVO does **not** compile the kit's OpenGL backend, `renderer`, `canvas` or `backdrop` | `EVO/Makefile:232-237`, the comment says so (read) |
| EVO's own kit screens use only shapes, text, glyphs, springs: no `Painter`, no `ui::Theme`, no component | research pass (grep of `EVO/ui_kit`) |

**The components are compiled into the app today and nobody calls them.**
Moving to them is not a kit upgrade; it is starting to use what is linked.

### 1.2 What draws the kit's lists on the console

EVO's own backend on sceAgc, not the kit's OpenGL one:
`EVO/ui_kit/src/hui_agc_batch.cpp` with the shader
`EVO/shaders/agc/ui_sdf.pipe` (and `ui_sdf_pq_out.pipe` for HDR output).

| Kit feature | On EVO's backend | Evidence |
| --- | --- | --- |
| The nine shape modes (rounded rectangle, glyph, shadow, image, capsule, triangle, star, arc, chamfer), rotation, gradients | Yes | research pass: `ui_sdf.pipe:179-283` |
| Clip rectangles, polygons | Yes (scissor; polygons without anti-aliasing) | research pass: `hui_agc_batch.cpp:327-383` |
| RGBA textures for `image()` | Yes: `create_texture` / `release_texture` | `hui_agc_batch.cpp:204,223` (read) |
| Font atlases on their own bindings | Yes, six slots as in the kit; `update_font_texture` exists and nothing calls it | `hui_agc_batch.cpp:176,188` (read) |
| **`glass()`: the blurred copy of the screen** | **No.** `DrawList::glass` only draws a texture that the kit's GL `Renderer` produces; nothing in EVO produces one | research pass. A blur pipe exists for the RmlUi backend: `EVO/shaders/agc/ui_backdrop_blur.pipe` (read: the file is there) |
| Procedural backdrops (`BackdropSpec`) | No; not needed: the proposal's page is a two-colour gradient and a stretched picture | research pass |
| Instancing | No: each shape is six vertices (576 bytes) in a transient ring; about 10,900 shapes per draw | research pass: `hui_agc_batch.cpp:35` |

So the kit's current components **can run on this backend as they are**, with
one visible difference: without a glass texture the Acrylic theme's panels are
tinted, not frosted (`KIT/src/ui/widgets.cpp:346-353`: `if (glass_ != 0)`).
Frosted glass needs one new piece (section 3, step 1).

### 1.3 The browser is not the kit

ProsperoTube's browser is an RmlUi document (`PT/native/youtube.rml`, 87 lines;
`youtube.rcss`, 47 lines) drawn by EVO's RmlUi provider host. EVO would draw
an embedded provider with its kit grid
(`EVO/ui_rml/src/evo_rmlui_provider.cpp:1680`, `RenderKit` at `:1691`);
ProsperoTube patches that test to leave `youtube` out
(`PT/scripts/prepare_native.py:169`, and `:158` for the navigation overlay).

What feeds it: `evo_provider_item_t`
(`EVO/addons/include/evo_provider.h:171-192`: id, title, subtitle, overview,
`art_url`, `backdrop_url`, `duration_sec`, `resume_pos_sec`, kind, live,
played). There is **no field for view count, age, channel avatar, badges or
shelf**; a page is 8 rows (research pass: `evo_rmlui_provider.h:172`). The
catalogue behind it (`PT/native/tube_provider.c`, 7,860 lines) knows more than
the row carries.

### 1.4 How ProsperoTube is built from EVO

`PT/scripts/prepare_native.py` exports EVO at the pin and the nested kit at
its checked-out HEAD (`:45`, `:50`), then applies exact-match text
replacements that fail when the text is not found (`:17`), from 45 overlay
scripts (`ls PT/scripts/*overlay*.py`). About fifteen of them patch the
player's kit-drawn interface (SponsorBlock, chat, captions, seek, speed,
controls, sleep timer, quality, playlists; research pass). **A new interface
is integrated through this mechanism or by replacing it**, and that, more
than drawing, is the cost and the risk.

### 1.5 Motion and sound today

- **Animation state** already lives where the proposal needs it: every kit
  screen in EVO has an `update(dt)` that `EVO/ui_kit/src/evo_hui_app.cpp`
  calls each frame (18 call sites; read). The kit's springs
  (`core/tween.hpp`, `ui/motion.hpp`) are in the pin.
- **The pinned kit lacks nothing for motion or sound** compared with main: the
  one source change since the pin is about fonts.
- **Sound** does not go through the kit. EVO has its own
  `SoundEffectEngine` that loads five samples (`hui/sfx/focus_01`, `focus_02`,
  `select_01`, `back_01`, `toggle_01`; `EVO/core/src/services/SoundEffectEngine.cpp:37-41`)
  and opens **its own AudioOut port** (`:44`, stereo), separate from the
  player's (`EVO/core/src/services/PlaybackController.cpp:1247`). Screens ask
  for a sound by number (`evo_sfx_play`, `EVO/core/src/Bridge.cpp:383`); one
  switch, "Navigation sounds", turns it off
  (`EVO/core/src/screens/SettingsScreen.cpp:468`). The seven WAVs EVO ships
  are files of the kit's `glass` set.
- The kit's `audio::SoundBank` and `audio::Mixer` are compiled
  (`EVO/Makefile:244`) and unused: no EVO source names them (read: grep).

So interface sound **already plays beside the video on a second port**; the
system mixes the two. What is missing is the vocabulary (42 cues, pitch, pan,
gain, variations) and the recordings.

## 2. Routes

### Route A (recommended): the browser moves onto the kit, inside EVO's kit integration; the player's interface is restyled with it

One renderer, one theme, one focus model for the whole app. Everything the
pictures show is drawn this way already (on the PC).

What must be written:

1. **A frame for the app** in the kit integration: page gradient, the ambient
   picture, a glass capture for AGC, and the screen stack with its hand-overs
   (the state machine of `prosperotube-design/src/concepts/prosperotube.cpp`
   is the model: about 1,000 lines with its prepared boards).
2. **The screens**, as kit code fed by the provider: the scratch design is
   about 5,100 lines of screens, shared pieces and component set-up that draw
   sample data (8,100 with the state machine, the sample data and the
   procedural pictures); the app's versions bind real data and real actions.
3. **A richer row.** Shelves instead of 8-row pages; views, age, channel
   avatar, badges, progress, chapters and segments in the item. The provider
   has the data; the struct and the host do not carry it.
4. **Pictures as kit textures**: the art cache's decoded pixels go to
   `AgcBatch::create_texture` instead of RmlUi's `SetMemoryTexture`, plus the
   64 x 36 ambient copy and two colours per picture.
5. **Text** (section 4).
6. **The player's interface** rebuilt from the same pieces, with the overlays
   that patch today's drawing rewritten against it.
7. **Sound** through the kit's bank and mixer on EVO's second port.

### Route B: keep the RmlUi browser, restyle it, draw kit chrome around it

Cheaper at first: new RCSS, the rail and the hints from the kit. It cannot
reach the proposal:

- Two renderers do not share a blur, so there is no frosted rail over the
  hero and no ambient page under RmlUi content without duplicating it.
- RmlUi has no springs: the gliding ring, the carried picture and the
  staggered arrivals would be CSS transitions at best, and none crosses from
  the document to the kit-drawn player.
- The browser's text engine stays as it is: FreeType without shaping, CJK as
  boxes, Arabic unjoined, emoji stripped (section 4).

Route B is a reskin, which is what the owner asked not to get.

### Route C: A, in slices, with RmlUi as the fallback until each screen is replaced

Not a third design, a way to do A: the kit frame and Home first behind a
switch (`settings:interface:afterglow`), every other destination still opening
the RmlUi document until its kit screen exists. The cost is a visible seam
(no carried picture into an RmlUi screen) for the weeks in between. **This is
the order recommended in section 7.**

## 3. Frosted glass and the page on AGC

The kit makes glass by replaying the layers under the overlay into a
480 x 270 target and blurring it four times (`KIT/docs/PERFORMANCE.md:50`).
On EVO this is new code, small in kind:

1. An off-screen 480 x 270 colour target; draw the page list into it with the
   existing `ui_sdf` pipe; run a separable blur (EVO has
   `ui_backdrop_blur.pipe` for its RmlUi layers); hand the result to the draw
   list as the glass texture. Estimate: 2 to 4 days including the HDR output
   path, unproven until it runs on the console.
2. The proposal is arranged so that this is cheap: only **the page** (wash,
   backdrop picture, the video) is blurred, panels are drawn after it. A
   dialog blurs the whole screen; that frame draws the screen into the
   captured layer first.
3. If glass proves too slow or is postponed, the look degrades to tinted
   panels (the kit already does this when there is no glass texture) and
   still reads: the high-contrast look is exactly that.

## 4. Text: the critical part

A video app shows titles it did not write, in every script, with emoji; and
the interface itself now ships in 21 languages
(`wt/companion:docs/LOCALIZATION.md`).

### 4.1 Today

| | Browser (RmlUi) | Player interface (kit) |
| --- | --- | --- |
| Engine | FreeType through RmlUi, no HarfBuzz, no bidirectional layout | Baked signed-distance atlases |
| Faces | LatoLatin, Roboto, then NotoSans and DejaVuSans (`EVO/ui_rml/src/evo_rmlui_app.cpp:334-349`) | Inter, Montserrat, DejaVu Sans Mono with the baker's `basic` set (`EVO/ui_kit/src/evo_hui_app.cpp:220-223`) |
| Coverage | Latin with marks, Greek, Cyrillic, Vietnamese; Hebrew and Arabic unjoined and left to right; CJK, Thai, Indic as boxes (`LOCALIZATION.md:191-203`) | 113 characters: "not one accented letter" (`LOCALIZATION.md:163-164`) |
| Emoji | Removed from every row before drawing: `display_text()` drops U+1F000 and above and U+2190 to U+2BFF (`evo_rmlui_provider.cpp:766-783`) | A missing glyph is a blank |

`img/text-engine/stock-atlases-german.png` and
`stock-atlases-mixed-scripts.png` show the proposal's own boards drawn with
the kit's stock atlases: every umlaut and every non-Latin character is a "?".

### 4.2 What the pictures did, exactly

- **German and all accented text**: the three text faces were re-baked with
  the kit baker's own `european` option into 2048-wide atlases
  (`KIT/tools/font-baker/bake_font.cpp:41-66`, `KIT/tools/bake-fonts.sh:7-9`):
  472, 472 and 407 glyphs, files of 1.4 to 1.7 MB each. No code change.
- **Mixed scripts** (picture 44): three fallback faces attached with
  `Font::add_fallback` (`KIT/src/gfx/font.hpp:81`), baked with **exactly the
  characters of that board** from the Noto Sans JP / KR subsets and Noto Emoji
  that are on this PC (SIL OFL). The Japanese and Korean titles were written
  to use characters those subsets hold. A different title would show "?".
  The six font slots (`KIT/src/gfx/draw_list.hpp:73`) are taken by the kit's
  six faces, so fallback atlases are ordinary textures: text that uses them
  starts extra draw calls (29 on that board against 17 on the same board in
  Latin).
- **Not drawn at all, on purpose**: Arabic, Hebrew, Thai, Indic scripts,
  colour emoji, emoji sequences. The kit has no shaping and no right-to-left
  order (`KIT/docs/KIT.md:261-264`); a picture of them would have been faked.

So the pictures prove layout with long words and with several scripts in one
line, and prove `add_fallback` works. They do **not** prove arbitrary titles.

### 4.3 What the app needs

Three steps, each useful alone:

1. **Bake more (days).** Bump the kit pin to main, bake `european` for the
   four atlases EVO loads. That alone makes the player interface translatable
   for 18 of the 21 languages. The `european` set is U+00A1 to U+017F, Greek,
   U+0410 to U+044F with Ё: it lacks Romanian ș and ț (U+0218 to U+021B),
   Ukrainian і ї є ґ (outside U+0410 to U+044F) and Vietnamese (U+01A0 to
   U+01B0, U+1EA0 to U+1EF9). Three ranges added to the baker cover all 21;
   Montserrat has no Greek, so headings in Greek fall back to Inter (the
   fallback mechanism does that). Memory: about 1.4 MB per atlas instead of
   0.3.
2. **A real text engine for titles (1 to 2 weeks, the owner has done it three
   times).** Glyphs rasterised on demand into a growing atlas, from the
   console's own fonts, shaped by HarfBuzz, ordered by a bidi pass:
   - ProsperoRadio: `~/ProsperoRadio/ui-kit/overrides/gfx/{font,harfbuzz,bidi,system_fonts}.cpp`;
     the font table names `SST-*`, `SSTJpPro-Regular`, `SSTThai-*`,
     `SSTArabic-*` under `/preinst/common/font` (`system_fonts.cpp:58-63,176`; read).
   - ProsperoTV: `opengl-ui/src/tv/font_shape.cpp` (research pass).
   - The kit's own account of it: `KIT/docs/ADOPTING.md:114-125` (first glyph
     of a new script costs a frame of about 110 ms: warm the scripts up).
   - EVO already links `libharfbuzz.a` and `libfreetype.a`
     (`EVO/Makefile:52-53`) and calls neither; `AgcBatch::update_font_texture`
     (`hui_agc_batch.cpp:188`) is the unused hook for uploading a changed
     atlas.
   - Unknowns: whether ProsperoRadio's overrides drop onto EVO's AGC batch as
     they did onto the kit's GL batch (the atlas upload path differs), and
     system font access from ProsperoTube's sandbox.
3. **Emoji (a decision).** Colour emoji need an RGBA atlas and a colour font;
   the atlases are one channel. Options: keep stripping them (today), draw
   outline emoji from a symbol face (what picture 44 shows), or add a colour
   atlas later.

Until step 2 exists, a kit browser would be **worse** than today's RmlUi one
for Greek, Cyrillic and Vietnamese titles unless step 1 is done first, and
equal to it for CJK (boxes either way). This orders the plan: step 1 before
any kit screen ships, step 2 before the kit browser replaces RmlUi.

## 5. Thumbnails

Today (`EVO/ui_rml/src/evo_rmlui_provider_art.cpp`; read for the constants,
research pass for the flow): fetched with `evo_net`, 8 at a time (`:122`),
cached on disk, decoded by FFmpeg two per frame, scaled to fit 480 pixels
(`:88`), uploaded as RGBA8 with a CPU copy kept, least-recently-used budget
24 MiB (about 48 pictures at 480 x 270).

For Afterglow:

| Need | Size | Notes |
| --- | --- | --- |
| Shelf and grid cards (352 to 394 wide; 704 to 788 real pixels at 4K) | 480 x 270: 0.5 MB each | The current 480 cap is enough: at 4K a card is the picture enlarged 1.5 times, like YouTube's own TV app with `hqdefault` |
| The focused picture (Home backdrop, video page, the frame the player opens on) | 1280 x 720: 3.7 MB, two alive at once | One request per focus change, cancelled when the focus moves on; until it lands the 480 picture is shown enlarged |
| Ambient copy and two colours per picture | 64 x 36: 9 KB | Computed once at decode time (box filter, blur, highlight compression: 100 lines in `prosperotube_art.cpp`) |
| Channel avatars | 88 x 88: 31 KB | |
| Seek previews | YouTube storyboard sheets, drawn with `image(texture, rect, uv)` | Not drawn today: "the HUD has no network texture path" (`PT/docs/PARITY.md:24`). With kit textures that path exists |

- **Budget**: 60 cards (31 MB) + 2 large (7.5 MB) + avatars and ambient
  copies (1 MB) is about 40 MiB of textures with no CPU copies, against
  today's 24 MiB. On Home, 12 to 14 pictures are on screen.
- **Minification**: AGC textures here have one level (research pass:
  `hui_agc_batch.cpp:118-165`). A 1280 picture drawn 352 wide would shimmer;
  the cache keeps the 480 version for cards and uses the large one only large.
- **Draw calls**: the kit starts a draw call per texture. Home is 27 draw
  calls on the PC (table in section 6), under the kit's own guide of 60; an
  atlas of thumbnails is possible later and not needed first.
- **Loading is never blank**: a card is a skeleton bone until its picture
  decodes, then cross-fades (the kit's `ui::Skeleton` does both).

## 6. Speed and memory

What the kit says about itself (`KIT/docs/PERFORMANCE.md`, measured on a PS5
at 3840 x 2160 on 2026-10-02 **through the kit's OpenGL renderer**, not EVO's
AGC batch): every design held 60 fps, peak 74 draw calls and 2,438 shapes
(`:124-131`, `:155`); the guide is under 60 draw calls and 6,000 shapes per
frame, "look again" above 150 and 20,000 (`:113-114`); glass costs one
480 x 270 pass (`:50`); a full-screen layer at 4K is 8.3 million pixels
(`:54`).

The proposal's boards, as the host renderer counted them at 1920 x 1080:

| Board | Shapes | Draw calls |
| --- | --- | --- |
| Home, signed in | 889 | 27 |
| Home, rail open | 981 | 27 |
| Video page | 754 | 16 |
| Share (QR drawn from runs of modules) | 1,201 | 17 |
| Player | 267 | 9 |
| Player with captions sheet | 580 | 11 |
| Comments with reply keyboard | 1,287 | 12 |
| Update dialog over Home (the whole screen is blurred) | 1,130 | 41 |
| Mixed scripts (three fallback atlases) | 696 | 29 |
| All 44 boards | 21 to 1,287 | 9 to 41 |

Inside the kit's comfortable range by a factor of four or more. What these
numbers do **not** say:

- **Fill rate on AGC at 4K.** Home stacks several screen-sized layers: the
  page gradient, the ambient picture, the backdrop picture (82 image strips,
  because the kit tints images top to bottom only and the picture must also
  dissolve to the left), two scrims, the glass capture. This is the first
  thing to measure on the console. If it is too much: draw the backdrop
  picture in one piece with a shader gradient, or drop the wash under it.
- **No instancing**: 1,300 shapes are 0.75 MB of vertices per frame in EVO's
  transient ring. Small, but a different cost from the kit's GL numbers.
- **The player** is the lightest screen and the one that matters most: 267
  shapes over a decoding 4K video. A sheet adds one glass pass.
- Memory, estimated: three european atlases 4.2 MB (or one 2048 x 2048
  dynamic atlas, 4 MB), thumbnails about 40 MiB, glass targets under 1 MB,
  the `glass` sound set 1.3 MB of WAV on disk.

## 7. A staged plan

Sizes are estimates for focused work, PC-verified first, each stage ending in
a build that can be tried on a console. The order gets something testable in
days and never leaves the app without a working browser.

| Stage | What | Size | Testable result |
| --- | --- | --- | --- |
| 0 | Bump the kit pin to main; bake `european` plus the three missing ranges for EVO's four atlases | 2 to 3 days | The player interface can be translated today; no visual change |
| 1 | The Afterglow theme struct; EVO's kit-drawn dialogs, toasts, keyboard and pickers switched to the kit components wearing it; glass capture on AGC | 1 to 1.5 weeks | The player's overlays look like the pictures; first console measurement of glass |
| 2 | The app frame (page, ambient, screen stack, rail) and **Home** on the kit, behind a switch; richer provider rows (shelves, numbers, badges); thumbnails as kit textures with ambient copies | 2 weeks | Home as in pictures 03 to 08; everything else still opens the RmlUi browser |
| 3 | Video page, More actions, Share; the carried picture into the player | 1 week | Pictures 13 to 15 and the two transitions of the reel |
| 4 | Player: timeline with chapters and SponsorBlock lane, seek preview from storyboards, captions / options / Up next sheets, skip notice, next-video card; the player overlays rewritten against it | 2 weeks | Pictures 16 to 24 |
| 5 | Search, results, channel, library, subscriptions, notifications, channels, comments, chat | 2 weeks | Pictures 09 to 12 and 25 to 30; RmlUi browser no longer needed |
| 6 | Settings as twelve tiles and forms (today about 35 rows in one list), accounts, sign-in, phone | 1 to 1.5 weeks | Pictures 31 to 35 |
| T | Text engine: system fonts, HarfBuzz, bidi, dynamic atlas (in parallel from stage 1; needed before stage 5 ends) | 1 to 2 weeks | Any title in any script; right-to-left interface possible |
| S | Sound: the kit's bank and mixer on EVO's second port, the cue vocabulary, ten new recordings; haptics; the playback rules | 3 to 4 days plus the recordings | `sound-design.md` |
| P | Attract mode, larger-text scale, high-contrast look, polish against the checklist, removal of the RmlUi document and its overlays | 1 week | |

About 11 to 13 weeks in all; the first build worth showing (stage 2) after
about four.

Sound in the app, concretely (stage S): keep EVO's second AudioOut port;
replace `SoundEffectEngine`'s five fixed samples with `audio::SoundBank`
(loads `<cue>_NN.wav` per set) and `audio::Mixer::render` filling that port
from a small audio thread, as the kit's own `platform/ps5/audio_out` does in
the kit app (EVO does not compile the kit's platform layer, so this is about
100 lines written against EVO's port). `evo_sfx_play(kind)` becomes one of
the callers of `feedback.play(cue)`. Ducking the video is a gain on the
player's own port, which already has a volume control.

## 8. Risks

1. **The overlay model.** 45 scripts of exact-match patches against EVO's
   sources; fifteen touch what stage 4 replaces. Every EVO update can break
   them. Route A moves the browser into ProsperoTube's own files (fewer
   patches) but stage 4 is a rewrite of patched code. Mitigation: do stage 4
   as new files that replace EVO's player overlay wholesale, not as more
   patches.
2. **Glass and fill rate on AGC at 4K** are unmeasured. Mitigation: stage 1
   measures them before any screen depends on them; the tinted fallback
   exists.
3. **Text.** Without stage T a kit browser cannot show what YouTube sends.
   Mitigation: the order in section 7; RmlUi stays until T is done.
4. **Two interfaces for some weeks** (kit Home, RmlUi elsewhere): seams in
   navigation and look. Mitigation: the switch stays off by default until
   stage 5.
5. **Memory**: 40 MiB of thumbnails plus a 4K video decoder. Not measured.
6. **EVO is someone else's player.** The kit integration (`ui_kit/`) is EVO's
   code; glass on AGC and component use belong upstream or in ProsperoTube's
   overlays. A decision, below.
7. **The sample pictures are kinder than real thumbnails**: real ones carry
   text, faces and hard colours; the ambient wash and the dissolving backdrop
   were tuned on calm pictures. To check with real data on the PC first
   (the art cache can feed the host renderer).
8. **Nothing here has run on a console.** Speed, sound and vibration are
   plans until it has.

## 9. Decisions for the owner

1. **Direction**: Afterglow as shown, or changes to it.
2. **Look**: Afterglow (recommended), Onyx or Daybreak as the default; whether
   the others ship as choices in Settings > Interface (they are one struct
   each).
3. **Route**: A in slices (recommended), or B.
4. **Where the kit work lives**: in ProsperoTube's overlays, or proposed to
   EVO (glass on AGC, component adoption, the cue vocabulary).
5. **Fonts for the 21 languages**: bake the wider sets now (stage 0), or wait
   for the text engine and use the console's fonts for everything.
6. **Text engine**: reuse ProsperoRadio's overrides, or contribute shaping to
   the kit so every app gets it.
7. **Emoji**: strip (today), outline, or colour.
8. **Controller map**: the proposal changes Square and Triangle on Home
   (`design-notes.md`, section 10); L1 / R1 for sections.
9. **Sounds**: commission the ten recordings; the playback rule (notices
   only, by default).
10. **The mark and the wordmark**: adopt the new mark (it would replace the
    "PT" tile in `native/branding/`), or keep today's.
11. **Attract mode**: on by default after 45 seconds, or off.

## 10. What was not done

- No console, no LAN host, no network beyond what the kit's build needed
  (it needed none: the host build is self-contained).
- No repository was modified. The kit copy is `~/hui-work/prosperotube`;
  ProsperoTube and its worktrees were only read.
- No unit tests were run for the scratch design; it uses OpenGL directly for
  its pictures and is a host-only file.
- The reduced-motion paths are written in the scratch design and were not
  rendered or timed.
- The "before" pictures are the existing host previews from
  `~/pt-wt/native/output/preview/` (copied, not regenerated) and the console
  capture of the player.
