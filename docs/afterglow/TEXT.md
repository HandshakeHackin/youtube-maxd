# Afterglow: the text engine

`native/text/` turns UTF-8 text in any script into positioned glyph quads and
the atlas pixels they sample. It reads font files (the PS5 system software's
own, plus any the app carries), orders text with the Unicode bidirectional
algorithm, picks a font for every cluster, shapes with HarfBuzz, breaks and
cuts lines, and keeps the glyphs it draws as single-channel distance fields in
the format the interface kit's glyph shader already reads.

It depends on FreeType, HarfBuzz and GNU FriBidi and on nothing else: not on
the interface kit, not on EVO, not on `native/afterglow`. An adapter of a few
dozen lines connects it to the kit (section 9).

**State.** Built and tested on the PC only, under ASan and UBSan. Compiled with
the PS5 SDK and linked into a probe module and into the app's own link
(section 7). Nothing here has run on a console; section 8 lists what that
leaves open, in the order it should be settled.

| Command | What it does |
|---|---|
| `bash scripts/test_text.sh` | The tests (384,361 checks), the contact sheet and the link probe on the host, all under ASan and UBSan. Picked up by `scripts/test_all.sh` |
| `bash scripts/text_sheet.sh <folder>` | Contact sheets as PNG files: invented titles in every script at the 1080p and the 4K surface, drawn from the atlas |
| `bash scripts/text_bench.sh` | What the engine costs on this PC (section 6) |
| `bash native/text/ps5/build_ps5.sh` | PS5 objects, probe module, relink of the app, sizes and imports in `build/text-ps5/report.txt` |
| `bash scripts/text_deps.sh [plain\|asan]` | Fetches and builds the three libraries for the host; prints their folder |

## 1. Files

```
native/text/
  tube_text.hpp            the interface: Config, Style, Constraints, Layout, Context
  tube_text.cpp            Context: the cache of shaped texts, the frame budget, quads
  tube_text_unicode.*      UTF-8, clusters (UAX #29), where a line may break
  tube_text_fonts.*        font files (mapped or read on demand), faces, the fallback chain
  tube_text_layout.*       direction, scripts, runs, shaping, lines, the cut mark, alignment
  tube_text_atlas.*        glyph to distance field; atlas pages, eviction, dirty rectangles
  licenses/                HarfBuzz, FreeType and FriBidi licence texts
  ps5/                     text_probe.cpp, build_ps5.sh (the link proof)
tests/
  test_text.cpp            the tests            text_host.hpp   the fonts of this PC they read
  text_sheet.cpp           the contact sheet    text_bench.cpp  the measurements
scripts/
  text_deps.sh text_common.sh test_text.sh text_sheet.sh text_bench.sh
```

About 4,600 lines of C++17 in the engine; it compiles as C++20, and without
exceptions and RTTI.

## 2. The interface

Everything is in `tube_text.hpp`, plain data in and out. One thread uses a
`Context`; there is no global state.

```cpp
tube::text::Config config;
config.font_dirs = tube::text::system_font_folders();   // /preinst/common/font
config.faces = {...};                                    // faces the app carries (optional)
config.language = "ja-JP";                               // the interface language
config.pixel_scale = 2.0f;                               // output pixels per layout unit (4K)
auto text = tube::text::Context::create(config, &error);

// every frame
text->advance_frame();                                   // first
tube::text::Metrics m = text->measure(title, style, constraints);
text->layout(title, style, constraints, &layout);        // lines of quads
for (const tube::text::AtlasUpdate &u : text->atlas_updates())
    upload(u);                                           // before drawing
```

- **`Style`**: `size` (the em, in layout units), `weight` (regular, bold),
  `tracking`, `line_height` (0: the face's own), `tabular_digits`,
  `direction` (automatic, ltr, rtl), `family` (section 4).
- **`Constraints`**: `max_width` (0: none), `max_lines` (1: one line; n: wrap
  to at most n; 0: wrap without limit), `ellipsis` (none, end, start), `align`
  (start, end, left, right, center).
- **`Layout`**: `quads`, `lines`, the block's `width` and `height`, `ascent`,
  `descent`, `line_height`, `pending`, `missing`, `truncated`, `rtl`.
  A `Quad` is a rectangle in layout units from the block's top-left corner, its
  rectangle in an atlas page (0..1), the page number and `range`: the distance
  field's spread in layout units at the size drawn. A `Line` has its quads, its
  `x`, `baseline` and `width`, the bytes of the text it shows
  (`text_begin`, `text_end`), and whether it carries the cut mark.
- **`measure()`** returns the same block without touching the atlas.
- **`fit()`** returns a cut line as a string, for callers that keep strings.
- **`prewarm()`** lays a text out without the frame budget (loading screens).
- **`can_draw()`**: every cluster has a glyph.
- **`atlas_updates()`**: page, rectangle, pixels, stride, and `created` for a
  new page. **`page_pixels()`**, **`page_size()`**, **`page_count()`**.
- **`stats()`**, **`messages()`**, **`describe()`**: what was found, read,
  refused, drawn, evicted and how long it took, for the log.

Rules a caller keeps:

1. `advance_frame()` once, before the frame's first `layout()`.
2. Lay every text out every frame. Quads are good until the next
   `advance_frame()`: no glyph moves in the atlas or leaves it while a frame
   is laid out; between frames both can happen.
3. Upload `atlas_updates()` after the frame's last `layout()` and before
   drawing.
4. `Layout::pending` counts glyphs not rasterised yet (the frame budget was
   used up, or the atlas was full). Positions are final from the first frame,
   since they come from shaping and not from the atlas, so nothing jumps: a
   caller either draws what is there, or holds the text back (or fades it in)
   until `pending` is 0. A glyph waiting at one raster size is drawn from a
   copy at another size when the atlas has one.

## 3. From a text to quads

1. **UTF-8** is decoded strictly. An ill-formed sequence is one U+FFFD (the
   longest prefix that could have started a character), the rest of the text
   is read. Controls that draw nothing are dropped, a tab is a space, every
   line end is a line feed. Reading stops at `max_codepoints` (16,384) and the
   result says `truncated`.
2. **Paragraphs** are split at line feeds (with `max_lines == 1` line feeds
   are spaces).
3. **Clusters**: extended grapheme clusters of UAX #29 (accents, Hangul
   letters, emoji sequences with joiners, selectors, skin tones and flags, the
   conjuncts of nine Indic scripts). Nothing is ever broken, cut or given two
   fonts inside one.
4. **Emoji** (section 5).
5. **Direction**: GNU FriBidi gives every character its embedding level
   (UAX #9 with bracket pairs and isolates). Text with no right-to-left
   character skips the pass.
6. **Scripts**: each character's script; spaces, digits and punctuation take
   the script of the text before them.
7. **Fonts**: one face per cluster (section 4).
8. **Runs** of one level, script and face are shaped by HarfBuzz, with the
   whole paragraph as context (Arabic letters join across a change of font),
   the script's language tag, `tnum` for tabular digits, and ligatures off
   when letter-spacing is on. Default-ignorable characters leave no glyph.
9. **Lines**: break opportunities come from a subset of UAX #14 (section 5).
   A line is filled greedily; spaces at its end hang past the edge. The last
   allowed line is cut at a cluster so that it fits with the mark.
10. **Order**: each line's runs are reordered by rule L2; HarfBuzz already
    gives the glyphs of a right-to-left run from left to right, and mirrors
    brackets.
11. **The cut mark** is U+2026 in the face of the text beside the cut (three
    full stops when no face has it). It sits at the paragraph's end: at the
    right of a left-to-right paragraph, at the left of a right-to-left one
    (`Ellipsis::start`: the other way round).
12. **Alignment**: `start` is left for a left-to-right paragraph and right
    for a right-to-left one, each paragraph by its own direction.
13. **Quads**: each glyph is looked up in the atlas at the raster size that
    fits the size drawn, and rasterised when it is new and the frame's budget
    allows.

Steps 1 to 12 depend only on the text, the style and the constraints: their
result is kept (1,024 texts, 4 MiB at most, least recently used first out), so
a text drawn every frame costs one hash and one atlas look-up per glyph.

## 4. Fonts

**The console's.** `Config::font_dirs` are searched for the names the family's
other apps read on real consoles (ProsperoEden's launcher and ProsperoRadio:
`gfx/system_fonts.cpp`); of each line the first file found is
used:

| Role | Files, in order |
|---|---|
| latin (the interface face) | `SST-Medium.otf`, `SST-Roman.otf` |
| japanese | `SSTJpPro-Regular.otf`, `SIE-RDC-Pr5N-M-JPN.otf`, `SCE-RDC-R-JPN.otf` |
| chinese | `DFHEI5-SONY.ttf` |
| korean | `YoonGothicProSIE760.otf`, `SCEPS4Yoongd-Medium.otf` |
| thai | `SSTThai-Medium.otf`, `SSTThai-Roman.otf` |
| arabic | `SSTArabic-Medium.otf`, `SSTArabic-Roman.otf` |

`system_font_folders()` is `/preinst/common/font`, preceded by the folder
`TUBE_TEXT_FONTS` names (a PC has no console). With `Config::discover` (on by
default) every other `.otf`, `.ttf`, `.ttc` file of the folders is offered
after the named ones, in name order, and opened only when no named face has a
character. No name is guessed.

**The app's.** `Config::faces` are asked before the folders', in the order
given, each with a role, optionally as a bold cut, optionally as a member of a
*family* (1 to 15). A style with `family = n` asks the faces of family n first
and gets what they lack from the faces of family 0; other styles never see
them. That is how a display face or a fixed-width face sits beside the
interface face. Line metrics are those of the style's interface face.

**Which face draws a cluster.**

1. A letter asks the face of its script's role: Arabic, Thai, Hangul, kana,
   Bopomofo; Han asks Japanese, Chinese and Korean faces in an order (below);
   everything else asks the interface face.
2. A space, digit or punctuation mark inside Japanese, Chinese or Korean text
   asks the interface face first (one style of digits in a title), elsewhere
   the face of the cluster beside it (Arabic words keep the Arabic face's
   space).
3. Then every face in order: the app's, the console's by role, the discovered
   ones.
4. A face must have every character of the cluster that draws. When none has,
   one that has the first character is taken and the marks it lacks are left
   out rather than drawn as boxes.
5. No face at all: the engine's own box, one per cluster, counted in
   `Layout::missing`.

**Han forms.** A paragraph with kana is Japanese, one with Hangul Korean.
Han alone follows the interface language when that is Japanese or Korean and
is Chinese otherwise; and when the preferred face lacks one of the paragraph's
characters (a simplified character in a Japanese font), one face that has them
all is preferred over a mix.

**Bold.** A face named as a bold cut is asked for bold text only. A face
without one is widened: the distance field is shifted by `Config::bold` (1/40
em) and every advance grows by twice that. There is no italic.

**Reading.** A file is opened the first time one of its characters is needed.
`IoMode::automatic` maps it into memory; when that is refused it is read on
demand: HarfBuzz gets each table it asks for, FreeType the bytes of each glyph,
never the whole file (4.5 MB read of 57 MB of fonts for the whole contact
sheet). Both ways give identical pixels (the tests compare them). A file that
is missing, too large (`max_font_bytes`, 64 MiB), no font, cut short, or a
colour-bitmap font is skipped and named in `messages()`; the next face is
asked. With no usable face every cluster is a box and nothing fails.

## 5. What is covered, and the limits

**Checked on the contact sheets** (`prosperotube-ui-proposal/build/text/`:
`titles-1080p-1..6.png`, `titles-4k-1..6.png`, `fallbacks-*.png`,
`atlas-*.png`), each title whole, as a two-line bold card, cut at the end, cut
at the start and centred over three lines: Japanese, Korean, Chinese
(simplified and traditional), Arabic, Persian, Hebrew, Hindi, Tamil, Bengali,
Telugu, Thai, Russian, Greek, Vietnamese, German, Turkish and Polish;
Korean, Japanese, Arabic and Hebrew mixed with English and numbers; emoji and
emoji sequences; combining marks; time codes; sizes 18 to 96 (36 to 192 at
4K); synthetic bold; tabular digits; letter-spacing; paragraph direction.
Looked at: Arabic and Persian join and run right to left with numbers and
Latin left to right, brackets mirror, the cut mark is on the correct side;
Devanagari, Bengali, Tamil and Telugu reorder and form conjuncts; Thai marks
stack; no mark is clipped by its quad; nothing is drawn as a box that a face
could draw. Nothing on them is known to be wrong.

The stand-in fonts were Segoe UI, Yu Gothic, Microsoft YaHei, Malgun Gothic,
Leelawadee UI, Nirmala UI and Segoe UI Symbol (the fonts Windows installs on
this PC, read from WSL, kept out of the repository) and DejaVu Sans 2.37 for
exact glyph numbers. **The console's own fonts were not available**, so the
sheets prove the engine, not what those fonts cover.

**Emoji.** There is no colour atlas. A face that has an outline for an emoji
draws it in the text colour (a sequence its font does not join is drawn as its
parts). When no face has it, `EmojiPolicy::strip` (the default) leaves the
whole sequence out together with the space it would leave double, and
`EmojiPolicy::placeholder` draws one neutral dot per sequence. Selectors,
joiners and skin tones never draw a box. Colour-bitmap fonts are refused.

**Limits.**

- **Line breaking** is a subset of UAX #14: after spaces, hyphens and
  zero-width spaces; between ideographs and kana, with the Japanese rules for
  what may not start or end a line; not at no-break spaces, not before closing
  or after opening punctuation; a word wider than the line is broken between
  clusters. **Thai, Lao, Khmer and Burmese** have no dictionary: lines break
  between their clusters, with rules that keep leading vowels, following
  vowels and final consonants in place, so a break can still fall inside a
  word; a real space or U+200B not far before it is preferred. **Korean**
  breaks at spaces only (and inside an overlong word). No hyphenation, no
  justification.
- A line is cut out of the paragraph's shaping, not shaped again: at a forced
  break inside an Arabic word the letters keep their joined forms (as browsers
  do); kerning across a break is not removed.
- Line height is fixed by the interface face (or `Style::line_height`). Scripts
  with tall marks (Devanagari, Thai, Vietnamese capitals) need about 1.4 em
  between baselines to stay clear of the line above; quads are never clipped
  by the engine.
- Spaces, digits and punctuation take the script of the text before them;
  script extensions and paired brackets are not looked at.
- Horizontal text only. Default instance of a variable font. Face 0 of a
  discovered collection. No italic, no small capitals.
- Texts longer than `max_codepoints` are cut.
- Scripts the console has no font for (Hebrew, Devanagari, Telugu, Tamil,
  Bengali and others, per `docs/LOCALIZATION.md`) need a face the app carries.

## 6. The atlas, and numbers from this PC

**Format.** Pages of `page_size` x `page_size` single-channel pixels (2048,
two pages). A glyph is rasterised with the em at 28, 56 or 112 pixels: the
smallest that is at least 0.8 of the size drawn in output pixels (so 1080p
body text uses 28, 4K body text 56, large headings 112). The field holds 128
on the outline and falls by 128 / spread per pixel, with spread = em x 8/56:
the kit's baked fonts exactly, so one shader setting serves both. FreeType
fills the unhinted outline (at twice the size for 28 and 56), the pixels an
edge crosses place it within themselves from their coverage and its gradient,
one Euclidean distance transform gives every other pixel its nearest edge, and
the result is averaged down.

| Field error against an eight-fold fill, pixels at the em | mean | 99 of 100 | worst | ms a glyph |
|---|---|---|---|---|
| em 28, filled 2x (used) | 0.044 | 0.19 | 0.34 | 0.09 |
| em 56, filled 2x (used) | 0.035 | 0.19 | 0.38 | 0.31 |
| em 56, filled 1x | 0.100 | 0.75 | 1.19 | 0.10 |
| em 112, filled 1x (used) | 0.080 | 0.63 | 1.25 | 0.32 |

**Budget.** At most `glyphs_per_frame` (32) glyphs are rasterised between two
`advance_frame()` calls, and none is started once rasterising has taken
`raster_ms_per_frame` (4 ms) of the frame; the rest are queued and drawn first
in the next frame. Glyphs that draw nothing cost no budget.

**Eviction.** A full atlas makes room between frames, never inside one: the
page the last frame used least is repacked. What that frame drew stays; of the
rest the more recently used half stays (and no more than leaves the page half
full); the others are dropped. A repacked page is uploaded whole. Glyphs that
found no room are drawn one frame later; `Stats::overflows` counts frames
where even that left no room (the atlas is too small for one frame's text).

**Memory bound.** Pixels: `max_pages * page_size^2` bytes, plus at most one
page more while a page is repacked: 8 MiB, 12 MiB for an instant, with the
defaults. By area, at 85% packing, a 2048 page holds roughly 2,300 CJK or
4,500 Latin glyphs at 28, 600 or 1,300 at 56, 160 or 340 at 112. Shaped texts:
`layout_cache_bytes` (4 MiB). Fonts: mapped files, or what was read from them.

**Measured** (Ryzen 9 3900X, one thread, `-O2`, the pinned libraries;
`scripts/text_bench.sh` with `TUBE_TEXT_WINDOWS_FONTS` naming a copy of the
fonts on the Linux disk: read through WSL's bridge to the Windows disk,
opening a font and reading it on demand are several times slower):

| | |
|---|---|
| Creating a context | 0.07 ms (no file is opened) |
| First text of a script (font opened) | 0.1 to 0.3 ms mapped, 0.1 to 0.6 ms read on demand |
| Shaping a title of about 45 characters, new to the cache | Latin 11 us, Cyrillic 9, Japanese 12, Chinese 7, Korean 12, Arabic 19, Thai 25, Hebrew 32, Devanagari 34 |
| The same wrapped to two lines with a cut mark | the same within 1 us |
| A text drawn before (cache, one look-up a glyph) | 0.4 to 0.8 us |
| A screen of 24 cards, 72 texts, 1,832 quads, every frame | 0.031 ms |
| Rasterising a glyph, em 28 | Latin 0.06 ms, Arabic 0.07, Devanagari 0.08, Korean 0.10, Japanese 0.10, Chinese 0.13 |
| ... em 56 | Latin 0.18 ms, Arabic 0.20, Devanagari 0.26, Japanese 0.33, Korean 0.34, Chinese 0.43 |
| ... em 112 | Latin 0.18 ms, Arabic 0.21, Devanagari 0.28, Japanese 0.34, Korean 0.36, Chinese 0.53 |
| Heap after all of the above | 10.2 MB (8 MB of it two atlas pages, 1.4 MB 1,024 shaped texts); 13.2 MB when the fonts are read on demand (3.4 MB read) |
| The contact sheet: 176 texts, 1,244 glyphs | 39 frames of 32 glyphs; 0.21 s at 1080p sizes, 0.40 s at 4K sizes |

So a screen of text that is already in the atlas costs nothing that matters;
what costs is the first appearance of glyphs: about 30 new CJK glyphs fit a
4 ms slice at 1080p sizes and about 10 at 4K sizes on this PC. A loading
screen or the moment a catalog arrives is the place for `prewarm()`.

## 7. PS5: the link proof

`native/text/ps5/build_ps5.sh` follows `standalone/ps5/build_ps5.sh`. The
engine is compiled by `prospero-clang++` with the flags EVO compiles the kit's
C++ with (`-g -O2 -fno-plt -fno-stack-protector -ffunction-sections
-fdata-sections -DEVO_APP_MODULE=1 -DEVO_TARGET_PS5=1 -std=c++20`), plus
`-Wall -Wextra -Werror`, and checked again as C++17 with `-fno-exceptions
-fno-rtti`. **No source change and no extra define was needed.**

The libraries are the SDK's own (Pacbrew v0.39): **FreeType 2.13.2, HarfBuzz
9.0.0, FriBidi 1.0.15**, the versions the host tests pin, and the app's final
link already names all three archives (EVO's `scripts/package-app.sh`).

| | |
|---|---|
| The probe alone (`text_probe.cpp`: every part of the interface once), linked as package-app.sh links EVO | links; converted by EVO's `ps5-native-tool` to a module of 4,371,156 bytes |
| The app's own objects relinked with the engine | links; `eboot.elf` 47,442,732 to 50,251,420 bytes: **+2,808,688** |
| Imports the app gains | **none** (`mmap`, `opendir`, `fseek` and the rest are imported today) |
| The engine's five objects | 151,976 bytes of text, 360 of data; 124,893 bytes loaded in the app |
| HarfBuzz | 2,196,862 bytes loaded. On the link line today, but nothing uses it: all of it is new |
| FriBidi | 57,078 bytes loaded, all new |
| FreeType, libpng, zlib, bzip2 | in the app today (RmlUi); nothing added |

Expected at run time, beyond the 2.8 MB of code: 4 MiB per atlas page in use
(8 MiB bound, 12 for an instant), up to 4 MiB of shaped texts (1.4 MB for
1,024 titles), the per-face structures of HarfBuzz and FreeType, and the font
files: mapped (file-backed), or 3 to 5 MB read on demand for a screenful in
six scripts.

## 8. What only a console can settle, in order

1. **Can the app read `/preinst/common/font`** from where it runs, and list
   it? The family's other apps read these files. If not, every cluster is a
   box until the app carries faces. `messages()` and `describe()` say what was
   found: log them once at start.
2. **Which files this system software has and what they cover.** The six
   names above were read on consoles by other apps; what else the folder holds
   (more weights of SST, a symbol or emoji face, Hebrew, Indic) is unknown.
   With `discover` on the engine uses whatever is there: log `describe()`
   after drawing one line per script, and `can_draw()` for a sample of each.
3. **Mapping the font files**: allowed, and counted against the app's memory
   or not? When refused the engine reads on demand without being told
   (`describe()` says "mapped" or "read on demand"); `Config::io` forces
   either, and `-DTUBE_TEXT_NO_MMAP` builds without the mapping code.
4. **Rasterising cost per glyph** on the console's CPU, which sets how fast a
   screen of new titles fills in. `Stats::raster_seconds / rasterised`. The
   time limit of a frame adapts by itself; `glyphs_per_frame` and
   `raster_ms_per_frame` are the knobs.
5. **Uploading** a dirty rectangle, and a whole 2048 x 2048 page after a
   repack, through the app's batch.
6. **The first text of a script**: the kit's notes measured about 110 ms a
   file when a font is read whole; here it is opened and read in parts, which
   should be far less. `Stats::font_open_seconds`.
7. **Draw calls**: pages drawn as plain textures start a new run wherever
   text follows shapes (section 9).
8. **Memory** in use with the interface up: atlas, shaped texts, fonts.

## 9. The adapter to the kit

**The seam.** `native/afterglow/text.hpp` is what screens call. Each function
maps onto one engine call:

| `text.hpp` | The engine |
|---|---|
| `measure(face, value, size, tracking)` | `measure(value, style).width` |
| `fit(face, value, size, width)` | `fit(value, style, width)` |
| `draw_text(list, face, value, x, baseline, size, color, align, tracking)` | `layout(value, style, {}, &out)`; quads moved by `x` (less half or all of `out.width` for centre and right) and by `baseline - out.ascent` |
| `draw_paragraph(..., x, y, size, width, line_height, color, max_lines, align)` | `layout(value, style, {width, max_lines, Ellipsis::end, align}, &out)` with `style.line_height`; quads moved by `x` and `y - out.ascent`; returns `y + lines * line_height` |
| `measure_block(...)` | `measure(...)`: `lines`, `width`, `height`, `truncated` |
| `covers(face, value)` | `can_draw(value)` |
| `drawable()`, `clean()` | the text itself: the engine leaves out what cannot be drawn |

A `Face` becomes a `Style` prototype: regular is `{family 0}`, semibold
`{family 0, Weight::bold}`, display `{family 1}`, mono `{family 2,
tabular_digits}`. The matching `Config::faces` are either none (the console's
SST is the interface face and bold is widened) or the TTF files of the kit's
`third_party/fonts` shipped as assets: Inter-Regular (latin), Inter-SemiBold
(latin, bold), Montserrat-Medium (latin, family 1), DejaVuSansMono (latin,
family 2). Which of the two the interface looks like is the owner's call.

`gfx::Align::left` keeps every title starting at the left. To let Arabic and
Hebrew titles hang from the right edge of their column, pass the column width
as `max_width` and `Align::start`.

**Drawing.** `gfx::DrawList::text(const Font &, ...)` is the kit's only public
way to put glyphs into a draw list, and `Font::layout` is not virtual. So the
quads reach the draw list in one of two ways:

1. *The family's way* (ProsperoRadio, ProsperoEden's launcher): the build
   takes `gfx/font.hpp` and `gfx/font.cpp` from the app instead of the kit's
   copy (the pinned kit itself stays untouched), and that `gfx::Font` is a
   style prototype on the shared `Context`. Then `text_kit.cpp` stays as it
   is, and the kit's own components (keyboard, dialogs, toasts) draw every
   script too. Its `layout()` is the heart of it (a sketch, not compiled
   here):

   ```cpp
   float Font::layout(std::string_view text, float x, float y, float size, Align align,
                      std::vector<GlyphQuad> &quads, float tracking) const
   {
       tube::text::Style style = style_;          // this face's family and weight
       style.size = size;
       style.tracking = tracking;
       engine_->layout(text, style, {}, &scratch_);
       const float left = align == Align::center ? x - scratch_.width * 0.5f
                          : align == Align::right ? x - scratch_.width : x;
       const float top = y - scratch_.ascent;      // y is the baseline
       for (const tube::text::Quad &q : scratch_.quads)
           quads.push_back({left + q.x0, top + q.y0, left + q.x1, top + q.y1,
                            q.u0, q.v0, q.u1, q.v1, page_texture_[q.page], q.range});
       return scratch_.width;
   }
   ```

   `GlyphQuad::texture` and `GlyphQuad::range` are exactly what the kit added
   for fallback faces; `DrawList::text` multiplies the range by the draw
   list's scale itself. `measure`, `fit`, `wrap`, `ascent`, `descent`,
   `line_height`, `sdf_range` (`Config::spread * size`) and `has_glyph` map
   the same way; `wrap()` returns `text.substr(line.text_begin, ...)` per
   line, with U+200F before the lines of a right-to-left paragraph so that
   each is ordered alone as it was in the paragraph.
2. A public "append these glyph quads" call in the kit. That is a change to
   the kit and needs the owner's word.

Giving the kit's baked fonts fallback faces with `add_fallback` cannot do it:
that path maps one character to one glyph, with no shaping and no direction.

**Textures.** A page is one `GL_R8` texture, `GL_LINEAR`, one level, clamped,
like `GlBatch::create_font_texture` makes. For an `AtlasUpdate` with
`created`, create it with the whole page; otherwise `glTexSubImage2D(x, y, w,
h)` with `GL_UNPACK_ROW_LENGTH = stride` (EVO's batch has
`AgcBatch::update_font_texture` for the same). A plain texture name works in
`GlyphQuad::texture` today (the shader samples `u_texture` for it) at the
price of a new run wherever text follows shapes; putting the two pages on font
slots (`kFontSlots` is 6) removes that and needs a small addition to the
batch.

**Frames.** `advance_frame()` at the start of the frame, uploads after the
frame's last text and before the draw list is drawn. Do not keep quads from
one frame to the next. Call `prewarm()` for the texts of a catalog when it
arrives, and for the interface's own texts behind the opening. Log
`messages()` and `describe()` once.

**The data layer.** `clean()` no longer needs to remove emoji or scripts: pass
YouTube's texts through. The emoji policy is `Config::emoji`.

## 10. Integration: what to add where

Nothing outside `native/text/`, `scripts/text_*.sh`, `scripts/test_text.sh`,
`tests/text_*`, `tests/test_text.cpp` and this file was touched.

**Sources** (the same five everywhere):

```
native/text/tube_text.cpp
native/text/tube_text_unicode.cpp
native/text/tube_text_fonts.cpp
native/text/tube_text_atlas.cpp
native/text/tube_text_layout.cpp
```

**The app** (`scripts/prepare_native.py`, where `native/afterglow` is given to
EVO's Makefile): the five sources among the kit's C++ sources (compiled with
`HUI_CXXFLAGS`; C++17 with `CXXFLAGS` works as well), and three include paths:

```
-I<repository>/native/text
-I$(PS5_PAYLOAD_SDK)/target/user/homebrew/include/harfbuzz
-I$(PS5_PAYLOAD_SDK)/target/user/homebrew/include/fribidi
```

(`.../include/freetype2` is in `CXXFLAGS` already.) Libraries: nothing to add.
`scripts/package-app.sh` links `libfreetype libharfbuzz libfribidi libpng16
libz libbz2` already. No pinned input is added for the console build: the
three libraries are Pacbrew v0.39's.

**The host harness** (`scripts/afterglow_host.sh`): the five sources, and

```
deps=$(bash "${root}/scripts/text_deps.sh" plain)
CXXFLAGS += -I${root}/native/text $(tr '\n' ' ' < ${deps}/include.flags)
link     += $(tr '\n' ' ' < ${deps}/lib.flags)
```

**Pinned inputs of the host build** (`scripts/text_deps.sh`, fetched into
`~/.cache/prosperotube/text-deps`, never committed):

| | Version | SHA-256 of the release archive |
|---|---|---|
| HarfBuzz | 9.0.0 | `a41b272ceeb920c57263ec851604542d9ec85ee3030506d94662067c7b6ab89e` (`harfbuzz-9.0.0.tar.xz`, github.com/harfbuzz/harfbuzz/releases) |
| FreeType | 2.13.2 | `12991c4e55c506dd7f9b765933e62fd2be2e06d421505d7950a132e4f1bb484d` (`freetype-2.13.2.tar.xz`, download.savannah.gnu.org/releases/freetype) |
| GNU FriBidi | 1.0.15 | `0bbc7ff633bfa208ae32d7e369cf5a7d20d5d2557a0b067c9aa98bcbf9967587` (`fribidi-1.0.15.tar.xz`, github.com/fribidi/fribidi/releases) |

`native/text/ps5/build_ps5.sh` says so when the SDK's versions and these pins
drift apart.

**The workflow** (`.github/workflows/build.yml`, the step that runs
`scripts/test_all.sh`): the test needs `curl`, `xz`, a C and a C++ compiler
and `zlib1g-dev` (the contact sheet's PNG writer); it builds the three
libraries once (about two minutes) unless `~/.cache/prosperotube/text-deps` is
cached. For the tests to run rather than be skipped the runner needs
`fonts-dejavu-core`; the tests that read the Windows fonts are skipped there
and say so.

**`THIRD_PARTY_NOTICES.md`**, once the engine is linked into the app:

```
* The text engine (`native/text`) links HarfBuzz 9.0.0 ("Old MIT" licence) and
  GNU FriBidi 1.0.15 (LGPL-2.1-or-later), the static archives of Pacbrew v0.39,
  beside the FreeType 2.13.2 (FreeType Project License) already linked. The
  licence texts are in `native/text/licenses` and ship under
  `assets/prosperotube-notices`. FriBidi's source is Pacbrew's package source;
  this repository's sources and build recipe are what relinking the app
  against another build of it takes. The console font names and their order, and the way glyphs become
  distance fields, follow ProsperoEden's launcher and
  ProsperoRadio (`gfx/system_fonts.cpp`, GPL-3.0-or-later, same author); the
  code is written for this project. The sub-pixel placement of an edge from
  its coverage follows Gustavson and Strand, "Anti-aliased Euclidean distance
  transform" (2011); the distance transform is Felzenszwalb and
  Huttenlocher's. Host tests build the three libraries from the release
  archives pinned in `scripts/text_deps.sh`.
```

and the three files of `native/text/licenses` copied to
`assets/prosperotube-notices` where `prepare_native.py` copies the others.
Fonts the app decides to carry bring their own licences (the kit's are SIL
OFL and Bitstream Vera/DejaVu; their texts are in the kit's
`third_party/fonts`).

**Why FriBidi and not SheenBidi.** Both licences fit a GPL-3.0-or-later app
(LGPL-2.1-or-later; Apache-2.0). FriBidi is in the SDK and on the app's link
line already, so the console build gains no new source to fetch, patch and
build, and its 57 KB are what a second implementation would cost as well. It
implements the whole of UAX #9, bracket pairs and isolates included.

## 11. The tests

`tests/test_text.cpp`, 384,361 checks, with HarfBuzz, FreeType and FriBidi
built with AddressSanitizer too and the engine under ASan and UBSan with leak
detection:

- **Characters**: strict UTF-8 (overlong forms, surrogates, cut sequences,
  stray bytes), line ends, controls, the character limit; clusters (accents,
  flags, families, skin tones, keycaps, Hangul letters, Indic conjuncts, Thai);
  break opportunities (spaces, hyphens, no-break and zero-width spaces,
  brackets, Japanese rules, Thai rules, addresses).
- **Fonts**: missing, empty, rubbish and cut-short files in both ways of
  reading; no font at all; a folder laid out like the console's (the named
  file wins, the unnamed one is opened only when needed); refused
  configurations; fonts mapped and read on demand give the same pixels.
- **Fallback chain**: the face of each script, Han by evidence and by
  interface language, digits inside Japanese text, a script no face has,
  bold cuts and widening, families.
- **Shaping**: glyph numbers and advances of DejaVu Sans 2.37 for Latin (with
  kerning and a ligature), Cyrillic, Greek, Arabic and Hebrew, checked once
  against the distribution's HarfBuzz 12.3.2; visual order of seven mixed
  strings worked out by hand; Thai, Devanagari, Hangul letters, ideographs,
  Arabic and Persian with a non-joiner.
- **Emoji**: both policies, sequences, selectors, keycaps, outline faces.
- **Layout**: the cut mark at the end and the start, in right-to-left
  paragraphs, exact fit, no room, accents, faces without U+2026; wrapping,
  hanging spaces, overlong words, line feeds, line limits with and without a
  mark; alignment per paragraph; tabular digits (with a font whose digits
  differ in width); letter-spacing; `fit()` strings, with Arabic at every
  width from the mark's own to the whole line's.
- **Atlas**: the frame budget (4, 8, 10 quads over three frames, the line
  never moves), a glyph shown from another size meanwhile, update rectangles
  against a mirror of the texture, one small page under 26 frames of new
  letters (bound held, least recently used first out, the word in use never
  redrawn and pixel-identical after every repack, nothing moves inside a
  frame), more than a page holds, two pages; the cache of shaped texts by
  count and by bytes.
- **Distance fields** against FreeType's own fill at three sizes; the slope;
  widening.
- **Hostile input**: 1,500 random byte strings and 1,500 random strings of
  the characters the engine treats specially, with random styles and
  constraints, every result checked (lines within the width, byte ranges in
  order, quads inside pages); sizes that are no numbers; 300,000 characters;
  10,000 accents on one letter; 10,000 line feeds.

The tests read fonts of this PC and say which. On a machine without them the
affected tests are reported as skipped; `TUBE_TEXT_REQUIRE_FONTS=1` makes a
skip a failure.
