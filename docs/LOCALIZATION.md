# Interface languages

ProsperoTube shows its browser, its preferences and its messages in 21
languages besides English. Classic player controls and toasts stay English for now. This page says how that works, how to add a
language, what to do after the interface changes, and what is not translated
yet.

## Languages

| Tag | Language | Tag | Language | Tag | Language |
|---|---|---|---|---|---|
| `cs` | Čeština | `hu` | Magyar | `pt-PT` | Português (Portugal) |
| `da` | Dansk | `id` | Bahasa Indonesia | `ro` | Română |
| `de` | Deutsch | `it` | Italiano | `ru` | Русский |
| `el` | Ελληνικά | `nb` | Norsk bokmål | `sv` | Svenska |
| `es` | Español | `nl` | Nederlands | `tr` | Türkçe |
| `fi` | Suomi | `pl` | Polski | `uk` | Українська |
| `fr` | Français | `pt-BR` | Português (Brasil) | `vi` | Tiếng Việt |

The classic interface is covered by every catalog. Afterglow is complete in
`da`, `de`, `es`, `fi`, `fr`, `hu`, `id`, `it`, `nb`, `nl`, `pt-BR`, `pt-PT`, `sv`.
The remaining 8 languages fall back to English for missing Afterglow texts.
`bash scripts/test_i18n.sh` reports both totals and requires 99% coverage
of the classic texts in every language. Completed Afterglow catalogs also
require every Afterglow text and 99% overall coverage.

Afterglow uses `tube_i18n_translate` through its platform text hook and draws
with its dynamic font engine. It can use these translations independently of
the old player's baked-atlas restriction below. Content such as video titles,
channel names, comments and captions is kept as YouTube supplied it.

Russian, Ukrainian, Greek and Vietnamese write the four face buttons as their
symbols (✕ ○ □ △), as the key line of the browser does in every language: no
official PlayStation wording for them could be checked offline. The other
catalogs use the names PlayStation uses in that language.

The language is chosen in this order:

1. **YouTube preferences > Interface language**: System default, English, or
   one of the languages above. The choice is one line in
   `/data/prosperotube/interface-language` and applies to all profiles.
2. The language of the PS5 system settings
   (`sceSystemServiceParamGetInt(1)`). A regional variant uses the catalog of
   its language: Spanish (Latin America) and Spanish show `es`, French (Canada)
   shows `fr`; Portuguese has both of its catalogs.
3. English, for every other system language (Japanese, Korean, Chinese, Arabic,
   Thai) and whenever the console does not answer.

With English in use nothing is looked up: every text is what it was before.

## How a text is translated

Texts are looked up by their English source, so the code keeps its English
literals. `native/tube_i18n.c` holds the lookup; the catalogs are compiled into
it (`native/tube_i18n_data.h`), and nothing is read from disk while the app
runs except the one-line preference.

- **Whole texts**: `Subscriptions`, `Circle returns to the search`.
- **Patterns** for lines made at run time: `Up to {0}p`, `Add to {0}`,
  `PAGE {0} OF {1}`. A number passes through as it is; a text in a slot is
  translated when the catalog knows it and kept when not (a playlist name).
- **Counted patterns**: `{0} seconds` has a form per plural category of the
  language (one/other for most, one/few/many/other for Russian, Ukrainian and
  Polish, one/few/other for Czech and Romanian, one form for Turkish,
  Hungarian, Indonesian and Vietnamese). The count picks the form, not the
  English wording.
- **Label and value**: `Show dislikes: on` is translated as its label and its
  value, joined the way the language writes it (`{0}: {1}`, French
  `{0} : {1}`). The label must be a text of the catalog, so `Some channel: Live`
  is left alone. The same goes for a trailing `(experimental)`.
- **A miss returns the source.** A text that is not in the catalog, or whose
  translation does not fit the place it is written to, stays in English,
  whole. Nothing is cut.

## Where texts are translated

Four places, each a single passage:

| Surface | Passage | What goes through it |
|---|---|---|
| Browser rows | `EvoRmlProviderHost::ApplyItems` (`evo_rmlui_provider.cpp`) | Title, line below and note of every row the provider marks `tube_interface`. `folder()` in `native/tube_provider.c` sets the mark: every menu, preference and notice row is built by it. Rows parsed from the service (videos, channels, playlists, comments) never carry it |
| Status line, page counter | `SetStatus`, `SetLoading`, `PublishRowWindow` (same file) | Every error of `tube_set_error`, every status the browser screen sets |
| Toasts | `toast()` (`src/evo_toast.c`) | Title and message, after the English classification of the toast. A toast is drawn by the player interface, so it is asked for as one of its lines: English until those fonts hold the letters (see below) |
| Browser document | `youtube.rml` | Each static text becomes `{{ 'text' \| tr(i18n) }}`; `tr` asks the catalog, `i18n` is the language serial, so a language change redraws them |

`scripts/i18n_overlay.py` installs the four passages when
`scripts/prepare_native.py` builds the tree, after every other overlay, so the
document it marks already holds what those added. The provider's own output
does not depend on the language: its tests, the in-player menus and the logs
read English.

The overlay also adds one style rule: a key line that does not fit on one line
(a long language, a large **Interface scale**) takes a second line above the
first instead of leaving the page. One line keeps the size it always had. The
rule is part of the sheet the interface preferences restyle, so it holds at
every scale, theme and colour scheme; no language needs a third line at any
scale (`scripts/test_i18n_ui.sh` and `scripts/test_ui_prefs.sh` check it).

The player interface (controls, in-player menus, pickers, dialogs, the
keyboard) is drawn by EVO's kit, and every line of it passes through
`hui::ui::text()`. The overlay puts a fifth passage there, but **no language
uses it today**: see below.

## Files

| File | Role |
|---|---|
| `native/tube_i18n.h`, `native/tube_i18n.c` | Lookup, patterns, plural rules, language choice, the preference rows |
| `native/tube_i18n_data.h` | Generated tables (do not edit) |
| `native/i18n/template.json` | Generated list of every translatable text: source, where it is shown, pattern slots (do not edit) |
| `native/i18n/patterns.json` | The few texts and patterns the scan cannot derive |
| `native/i18n/<tag>.json` | One catalog per language |
| `scripts/i18n_extract.py` | Scans the provider, its headers, the overlays and the document; writes the template; `--check` fails when it is stale |
| `scripts/i18n_build.py` | Checks the catalogs and writes the tables; `--check` fails when they are stale |
| `scripts/i18n_overlay.py` | The passages above |

## After changing the interface

```sh
python3 scripts/i18n_extract.py     # the template follows the sources
python3 scripts/i18n_build.py       # lists what each catalog now misses
```

Add the new texts to every `native/i18n/<tag>.json`, run `i18n_build.py` again
and `bash scripts/test_i18n.sh`. A text nobody translated yet is shown in
English; `test_i18n.sh` fails when a catalog falls below its coverage requirement or
keeps a text the template no longer has, so `scripts/test_all.sh` catches a
forgotten step.

What the scan picks up: literals passed to `CATALOG_FOLDER`, `LINK_ROW`,
`folder`, `tube_set_error`, `toast`, the status calls and `ui::text`; formats of
`snprintf` and of the updater's `format` into a variable named like a text
(`label`, `detail`, `hint`, `title`, `message`, `error`, ...), turned into
patterns; label tables; sentences returned as reasons; the static texts of the
document. A new text reached another way is added to
`native/i18n/patterns.json` (today: the generic patterns, the Interface
language rows, values a label function writes into a plain buffer such as
`On` and `Off`, and labels that are names or hold an address).
`tests/test_i18n_provider.c` walks the main menu, every page of YouTube
preferences, Discover, Accounts & library, the Interface screens, the video
actions menu setup, About and more in six languages and fails on the first row
that stays English.

## Adding a language

1. Copy a catalog to `native/i18n/<tag>.json`; set `language` (the tag),
   `name` (the language in its own words) and `english`. Translate every value
   of `strings`. Keys are the English sources and are never changed. A counted
   pattern takes an object with the plural categories of the language, for
   example `{"one": "{0} segundo", "other": "{0} segundos"}`.
2. A language whose plural rule is not one/other gets a line in `PLURALS` of
   `scripts/i18n_build.py` and, when the rule is new, a case in `plural_form()`
   of `native/tube_i18n.c`.
3. A PS5 system language is matched by its tag (`tube_i18n_system_tag()`): a
   catalog `ja` would be used by a console set to Japanese without more work.
4. `python3 scripts/i18n_build.py`. It refuses a translation that loses or
   invents a slot, holds a control character, is not in composed (NFC) form,
   is several times longer than its source, or uses a letter none of the
   browser fonts holds. Add the tag to the lists in `tests/test_i18n.c` and to
   the table above, then `bash scripts/test_i18n.sh` and
   `bash scripts/test_i18n_ui.sh`.

Wording rules the shipped catalogs follow: the terms of the YouTube TV app in
that language (Subscriptions, History, Watch later, Playlists, Captions,
Quality, Queue); the PlayStation names of the buttons; ProsperoTube, YouTube,
SponsorBlock, DeArrow, Shorts and other names unchanged; row titles about as
short as the English, because a card shows one line and cuts the rest.

## What is not translated

- **The Classic player interface**: controls, in-player menus and lists, pickers,
  dialogs, the on-screen keyboard and toasts (a toast is drawn by the same
  kit, also over the browser). Its fonts are baked atlases with 113
  characters, ASCII and a few signs: not one accented letter, and a letter
  without a glyph is drawn as a question mark. A catalog is
  drawn there only when it sets `"player_interface": true` and
  `i18n_build.py` finds every letter of every text in all four atlases; none
  does. The texts ProsperoTube adds there are in the template and translated,
  ready for the day the fonts are.
- **EVO's own texts**: its player settings screen, its toasts
  (`SCREENSHOT`, `PAD OPEN OK`, ...) and its dialogs. They pass through the
  same passages, so adding their sources to `native/i18n/patterns.json` and
  the catalogs is all it takes; they were left out of this first set.
- **Content**: titles, channel names, descriptions, comments, chat, captions,
  section names YouTube sends. The content language is its own preference.
- **Messages the service words itself**: an error text YouTube.js or YouTube
  returns is shown as it came. An update message keeps a reason the network
  library worded (`The update server could not be reached (...)` is translated
  around it).
- **Release notes and descriptions**: the text of the release-notes page and of
  a video description is the author's. The two lines shown in their place
  when there is none (`This release has no notes.`) take the same path and
  stay English.
- **Credits and the changelog** in About.
- **The breadcrumb** keeps the names of the pages as they were when opened.
- **Lists of languages and countries**: translated, but sorted as in English.
- A row built by `folder()` from content (a pinned playlist, a blocked
  channel, a search suggestion) is looked up like any other. A playlist named
  exactly `Subscriptions` would be shown translated in such a row; its video
  and playlist cards are never touched.

## Scripts the app cannot draw yet

What the packaged fonts and the text engines can do today (read from the font
files by `scripts/i18n_build.py`, and from the libraries):

| | Browser (RmlUi) | Player interface (kit) |
|---|---|---|
| Fonts | LatoLatin and Roboto (Latin only), then NotoSans and DejaVuSans as fallbacks | Inter, Montserrat, DejaVu Sans Mono, baked with the `basic` character set |
| Latin with marks, Vietnamese, Greek, Cyrillic | Yes (NotoSans) | No |
| Armenian, Georgian | Letters present (DejaVuSans); no catalog | No |
| Hebrew, Arabic | Letters present, but drawn left to right and Arabic unjoined | No |
| Thai, Devanagari, Telugu, Chinese, Japanese, Korean | No glyphs: empty boxes | No |
| Shaping, right to left | None: the pinned RmlUi 6.2, on the host and in the PS5 SDK, uses its FreeType engine without HarfBuzz and has no bidirectional layout | None |

So the first set is the Latin, Greek and Cyrillic languages. No catalog is
shipped for the others: it would be boxes. The SmartTube languages still open
are Arabic, Azerbaijani, Bulgarian, Catalan, Estonian, Persian, Galician,
Hebrew, Hindi, Croatian, Armenian, Japanese, Korean, Lithuanian, Latvian,
Moldovan, Slovak, Slovenian, Albanian, Serbian, Telugu, Thai and both Chinese.
The ones in Latin and Cyrillic letters only need a catalog.

The plan for the rest, in order:

1. **Player interface in the shipped languages.** Bake the four kit fonts with
   the `european` set of `tools/font-baker` of ps5-homebrew-ui (Latin-1,
   Latin Extended-A, Greek, Russian Cyrillic) from the TTF files that
   repository carries, and extend the set by the Ukrainian letters
   (U+0404, U+0406, U+0407, U+0490 and their small forms), the Romanian comma
   letters (U+0218 to U+021B) and, with a larger atlas, the Vietnamese block.
   Then set `player_interface` in the catalogs, add EVO's player texts to the
   template and check the texture budget on a console.
2. **Chinese, Japanese, Korean, Thai** in the browser: load the console's own
   fonts as RmlUi fallback faces instead of packaging them. The other apps of
   this family read them from `/preinst/common/font`: `SSTJpPro-Regular.otf`
   (then `SIE-RDC-Pr5N-M-JPN.otf`, `SCE-RDC-R-JPN.otf`) for Japanese,
   `YoonGothicProSIE760.otf` (then `SCEPS4Yoongd-Medium.otf`) for Korean,
   `DFHEI5-SONY.ttf` for both Chinese, `SSTThai-Medium.otf` for Thai,
   `SSTArabic-Medium.otf` for Arabic, `SST-Medium.otf` for Latin. The order
   matters: the language's own face first, so shared Han characters get its
   forms. Chinese, Japanese and Korean need no shaping; the test needs these
   fonts on the host (an environment variable naming a folder, as those apps
   do). Thai also needs word breaks: its catalogs carry U+200B between words.
3. **Shaping** for Thai marks, Devanagari, Telugu and Arabic: a HarfBuzz font
   engine for RmlUi. The app already links HarfBuzz (libass uses it); RmlUi
   ships a sample engine (`RMLUI_HARFBUZZ_SAMPLE`) to start from. There is no
   console font for Devanagari or Telugu: those need a packaged Noto face.
4. **Right to left** for Arabic, Persian and Hebrew: bidirectional reordering
   per line (the other apps carry their own small UAX #9 implementation),
   `dir`-aware alignment in `youtube.rcss`, and a decision on mirroring the
   layout. Hebrew has no console font either.

## Tests

- `bash scripts/test_i18n.sh`: template and tables current; coverage per
  catalog; the document marking; `tests/test_i18n.c` under ASan and UBSan
  (lookup, patterns, plural rules, misses, buffer bounds, language choice, the
  preference file, the preference rows), once more with the player interface
  path forced on; `tests/test_i18n_provider.c` (rows marked, the same in every
  language, interface rows translate, content rows do not).
- `bash scripts/test_i18n_ui.sh`: the real provider screen, the generated
  document, the app's fonts and the CPU rasteriser draw the main menu and the
  preferences in all 21 languages. Every letter on screen has a glyph in a
  loaded face, every card title is rasterised inside its card and not beside
  it, one-line strips stay one line, the key line takes at most two lines and
  stays clear of the text above it, a video named like a menu entry keeps its
  name. Five languages are drawn again under other interface preferences:
  German at 1.4x, Russian with two title lines and cards at 0.8x in the red
  scheme, Greek at 0.8x with four title lines and two subtitle lines, Vietnamese
  at 0.4x, Ukrainian at 1.2x with the largest cards. The release-notes page
  shows its title and hint in the language and the notes as written.

The other host tests run in English and are unchanged.

## Only a console can tell

- That `sceSystemServiceParamGetInt(1)` returns the documented numbers on the
  installed system software, and that the app is allowed to call it before
  and after it gains its storage.
- That the preference file is read from `/data/prosperotube` at the first
  frame (it is read again when the data folder changes).
- The look of Cyrillic, Greek and Vietnamese at 4K through the GPU text path:
  the host test uses the CPU rasteriser.
- Reading distance: whether two-line key lines and cut titles are acceptable
  on a television, language by language.
