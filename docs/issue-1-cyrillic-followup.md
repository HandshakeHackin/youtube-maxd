# Issue #1: remaining Cyrillic text after 1.4.1

The reporter confirmed the playback video title is corrected, but reported
question marks in an "Opening…" message and above the player timeline. They
suspect the latter is chapter text; no screenshot identifies it conclusively.

The corrected 1.4.1 display font is Montserrat. The Classic chapter label
inserted by `scripts/player_extra_overlay.py` uses `fonts.regular` (Inter),
as do toast messages in EVO's `evo_hui_misc.cpp`. Diagnostics use `fonts.mono`.
The three other baked atlases—Inter Regular, Inter SemiBold and DejaVu Sans
Mono—contain no Russian alphabet glyphs. The real pinned font implementation
reproduces question-mark quads for Cyrillic in each original atlas. This is a
confirmed source defect consistent with the remaining symptoms, rather than
proof that every reported question mark comes from the same screen.

`scripts/playback_title_font.py` now bakes the existing licensed sources for
all four faces with the European/Cyrillic set. The overlay stages each atlas
in the application assets; no vendor submodule edits or rendering-engine
replacement are needed. Original pixel size, SDF spread and ASCII advances
are retained.

The host regression loads the production patched Font implementation and
checks each original and replacement atlas: question-mark substitution before,
distinct glyphs after, А–я and Ё/ё coverage, mixed-language fitting and equal
ASCII advances. Chapter fixtures exercise both literal and escaped Cyrillic
through cJSON, chapter lookup and time-prefixed labels. Opening-message and
production log-redaction fixtures preserve the UTF-8 bytes. These checks do
not establish the contents of an unseen console log; an actual exported log
is needed if literal question-mark bytes remain after the font update.

No new PS5 build or hardware run has been verified for this follow-up. The
current public 1.4.1 ZIP has not been replaced. Console acceptance must check
the Opening toast, on-screen diagnostics and Russian chapter labels, and
compare an exported UTF-8 log separately from its on-screen presentation.

Host validation: font layout, UTF-8 title/log preservation, chapter/player
data, voice driver, local voice driver and model probe checks passed (six
runner steps). The real recognizer translation unit also passes host syntax
checking. After preparing the native tree, all four staged font files contain
the Russian alphabet plus Ё/ё (407/472/472/475 glyphs respectively).
The additionally selected `test_player_extra_overlay.sh` fails before its
behavior harness: it searches generated source for `// ProsperoTube ...`,
while the existing branding overlay emits `// YouTube Max’d ...`. That
existing fixture mismatch is not suppressed or counted as a passing test.
