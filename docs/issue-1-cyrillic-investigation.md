# Issue #1: Russian playback titles

Investigated main `d812a207ff4b2ab916d6d743cc0a6954ef03d149`, with EVO
submodule `9715f6ee9bf9b53461d6fdf9a2e11b4c48357783`.

## Result and limits

The baked display-font renderer reproduces the reported question marks on the
host. It decodes Cyrillic correctly, but `gfx::Font::resolve()` substitutes
`?` for any code point absent from its atlas and fallbacks. Both the pinned
kit and EVO display atlases lack Cyrillic. Sanitized host fixtures preserve Cyrillic through the standalone
resolver's `createPlayback`, JSON serialization, vendored cJSON parsing, and
the production provider's bounded UTF-8 copy. These are component tests, not
an end-to-end replay of the reporter's upstream response or a PS5 test.

The correction rebakes **only the display atlas** from the pinned Montserrat
TTF, using the existing baker's `european` set (including А–я and Ё/ё), and
stages it into `projects/evoplayer/assets/hui/fonts/` before asset bundling.
It preserves the original 56px size and 8px SDF spread. JSON parsing, title
contents, the Unicode engine and voice search are unchanged. This fixes the
demonstrated no-shaper path. The reporter's actual mode and PS5 result remain
unverified; a separate Unicode-engine failure is not ruled out.

## Trace

Paths below are relative to this repository unless prefixed with EVO, which
means `third_party/evo/projects/evoplayer/`.

1. Upstream: the standalone service obtains parsed YouTube.js `info.basic_info`.
   `standalone/js/resolve.js:createPlayback()` assigns `text(details.title)` to
   the playback response. `standalone/js/errors.js:text()` removes NULs and
   bounds Unicode code points; it does not transliterate Cyrillic. The test
   supplies invented parsed metadata; it does **not** test YouTube.js's raw
   upstream parser. The separate companion uses
   `companion/prosperotube/models.py:text()` with equivalent NUL removal and
   character bounds.
2. Native: `native/tube_provider.c:str()` reads cJSON `valuestring`;
   `copy()` preserves UTF-8 and backs up over continuation bytes when its
   destination is too small. `parse_item()` reads catalog titles, and the
   resolution response refreshes the cached item's title from `root.title`.
   cJSON accepts both literal UTF-8 and JSON `\uXXXX` escapes in the host test.
3. Selection: `native/afterglow/app/mount.cpp:tube_afterglow_take_play()`
   forwards the selected title. `scripts/afterglow_overlay.py:OFFER` copies
   it to the provider selection. EVO
   `core/src/screens/ProviderHostScreen.cpp` copies selection title to
   `g_pending.title`, then assigns it to `PlaybackSource::title`.
   `PlaybackController::startPlaybackSource()` copies the source; `getDisplayTitle()` returns
   that stored title. These forwarding steps were inspected, not compiled by
   the new component test. Fixed-size `snprintf` forwarding can split a very
   long UTF-8 title at its end; that is distinct from replacing an entire
   ordinary Russian title with question marks.
4. Playback: EVO `core/src/screens/PlayerScreen.cpp` takes
   `getDisplayTitle()` and sets `p.title` without an ASCII conversion.
   With Afterglow enabled, `native/afterglow/player/app/mount.cpp` copies
   `params.title` into `pb.title`, then `view.title`.
   `native/afterglow/player/player_draw.cpp` fits and draws that title using
   `g.f.display`. EVO's original kit OSD similarly uses `fonts.display`.
5. Baked rendering: `gfx/font.cpp:next_codepoint()` decodes UTF-8.
   `Font::resolve()` tries its own atlas and registered fallbacks, then sets
   the code point to `?` and selects that glyph. There is no registered
   Cyrillic fallback in the inspected EVO kit startup. The new regression
   compiles the actual patched kit font code with no shaper and proves every
   letter in `Русское` selects the question-mark UV rectangle before the
   correction, and a distinct Cyrillic UV rectangle afterwards.
6. Font dispatch: `scripts/kit_text_patch.py` redirects `Font::fit`,
   `measure`, `layout`, `wrap`, and `has_glyph` to the installed shaper for
   known fonts. `native/afterglow/app/mount.cpp` creates `TextEngine` when
   the interface first mounts. It loads bundled Inter, Montserrat and DejaVu
   TTFs plus console font folders. `TextEngine::bind()` registers all four
   font roles and sets `gfx::Font::shaper`.
   This integration exists today; the Unicode engine is not merely unused
   source. Its documented lack of console verification still applies.
7. If engine creation fails, the log says the baked fonts draw. A session
   using Classic without first mounting Afterglow also has no initialization
   from this mount. An engine installed earlier is global, so a mode switch
   is not equivalent to a clean Classic launch. All these cases require
   separate tests.
8. `native/afterglow/text_kit.cpp:drawable()` does remove unsupported glyphs,
   but the inspected player title path does not directly call it.
   `TextEngine::bind()` clears the reading-face filter. An isolated host test
   of production `drawable()` with an ASCII-only face produces an empty
   string for an all-Russian title, not question marks. It cannot by itself
   explain the reported output.

## Reproducible host checks

Run:

```sh
python3 scripts/test_playback_title_unicode.py
python3 scripts/test_playback_title_font.py
```

The existing `scripts/test_all.sh` discovers `test_*.py`, so this probe is
included automatically. The preservation probe requires Node, a C and C++
compiler, and sanitizers; it does not require a network connection, PS5 SDK,
or real YouTube data. The font regression additionally requires the verified
pinned kit (recovered as described below); `ui_kit.checkout()` can attempt a
network fetch when that dependency is not already present.

Fixtures include `Русское видео — Ёжик`, mixed Latin/Cyrillic, and a Russian
pangram. The test calls the actual `createPlayback()` with a synthetic HLS
choice, exercises cJSON with literal and escaped Cyrillic, and compiles the
actual provider copy/accessor and adapter filter functions extracted from
their source. The extraction intentionally avoids claiming the whole native
application was compiled. Font inventory reads actual EVO `.huifont` records
using the format already used in `scripts/i18n_build.py`. Missing submodule
assets are explicitly reported as skipped; inventory is diagnostic, not an
assertion that future fonts must lack Cyrillic.

Verified here with:

```sh
ASAN_OPTIONS=detect_leaks=0 bash scripts/test_all.sh test_playback_title
```

The preservation/filter checks and real font-layout regression passed under address/undefined-behavior
instrumentation. LeakSanitizer could not inspect `/proc` in this environment,
so leak detection was disabled for this run only. The layout regression checks
all Russian alphabet glyphs, Ё/ё, mixed titles, fit width and unchanged ASCII
advance. No full app build, complete existing text suite, or console test was completed.

## Recovered dependency and focused correction

`python3 scripts/ui_kit.py` could not obtain pinned UI-kit revision
`bbe19054ee2e7ec069254775273aeb8846c4bd3e` from
`blackbearreloaded/ps5-homebrew-ui`. A GitHub API request for that tree returned
404. The dependency was subsequently recovered from the public **v1.4.0
YouTube-Maxd-source.zip** release asset, under
`YouTube-Maxd/third_party/maxd-ui-kit-source/`. The archive SHA256 matched the
release's SHA256SUMS:
`089eefcc5e2349af672bca156fbf36a1529b1007f3117cb5459ff7817e985f94`.
Each of 886 extracted kit files matched SOURCE_MANIFEST.json; running
`scripts/ui_kit.py` then verified its used files against ui_kit.lock.json.
No substitute revision was used and upstream dependency files were not edited.

For another host lacking private-repository access, retrieve that release
source archive, extract the kit subtree into the directory returned by
`scripts/ui_kit.py:directory()`, and run `python3 scripts/ui_kit.py` to verify
the lock before use. A raw archive source tree does not need synthetic Git
history for this verification.

`scripts/playback_title_font.py` builds the existing host baker and caches
its output by input source/font hashes, compiler command and bake parameters.
`afterglow_overlay.apply()` copies the generated display atlas to the **EVO
player asset directory that tools/bundle_rml_assets.py embeds**. Replacing
only the kit source directory's assets would not update this bundle.

The display font grows from 111 to 407 glyphs. Atlas dimensions grow from
1024×287 to 2048×594; on-disk font size grows from 328,152 to 1,659,736 bytes
(including kerning records). Raw single-channel atlas memory increases by
922,624 bytes; actual GPU allocation/padding must be checked on console.
The larger existing baker set also includes European Latin letters, though
Greek letters absent from Montserrat are skipped. Other roles' baked atlases
remain unchanged to keep this playback-title correction focused. This is not
a universal-script or subtitle-font fix.

The font regression loads both old and generated assets with the actual
production-patched `gfx::Font`, verifies the failing-before/passing-after UV
selection, and checks fitting and advances. Package asset embedding and the
complete PS5 binary were not built in this session. Confirm those plus AGC
texture upload and visible glyphs before publishing a console fix.

## PS5 acceptance checks

Use the same invented titles or public test videos in a clean Classic launch,
a clean Afterglow launch, and a mode switch after engine initialization.
Record interface mode and startup text-engine success/failure logs. Verify
the title beside codec/quality, not just catalog cards; compare literal bytes
at the provider and overlay boundaries without recording tokens or media
URLs. Check Cyrillic including Ё/ё, mixed text, long-title ellipsis, and ASCII
titles at 1080p/4K. Confirm the source font and atlas paths exist in the
installed package and that console system-font discovery works. A successful
host layout does not verify AGC uploads, PS5 font availability or console pixels.

## Separate L3 observation

The issue's follow-up says "Voice model is unavailable".
`native/tube_voice.c` has the local-mode message;
`native/maxd/local_asr.cpp` separately verifies and loads the voice model.
Model packaging, integrity, paths and loading are a separate investigation.
There is no established causal connection to Cyrillic rendering.
