#!/usr/bin/env bash
# Offline tests of the text engine (native/text), under ASan and UBSan with
# HarfBuzz, FreeType and FriBidi built from their pinned sources (also with
# ASan): characters and clusters, line breaking, font files and the fallback
# chain, shaping per script, visual order, ellipsis and wrapping, the glyph
# atlas with its bound, eviction and frame budget, hostile and huge input; then
# the contact sheet drawn once under the same sanitizers.
# The fonts are this PC's (tests/text_host.hpp); a test whose fonts are absent
# is reported as skipped. TUBE_TEXT_REQUIRE_FONTS=1 makes a skip a failure.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
source "$root/scripts/text_common.sh"
out=$root/build/tests/text/asan
text_engine asan "$out"
scratch=$root/build/tests/text/scratch
rm -rf "$scratch"
mkdir -p "$scratch/preinst" "$scratch/sheet"
export ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} UBSAN_OPTIONS=print_stacktrace=1
"${CXX:-c++}" "${TEXT_FLAGS[@]}" "$root/tests/test_text.cpp" "${TEXT_OBJECTS[@]}" "${TEXT_LIBS[@]}" -o "$out/test_text"
"$out/test_text" "$scratch"
"${CXX:-c++}" "${TEXT_FLAGS[@]}" "$root/tests/text_sheet.cpp" "${TEXT_OBJECTS[@]}" "${TEXT_LIBS[@]}" -lz -o "$out/text_sheet"
"$out/text_sheet" "$scratch/sheet" 1
"$out/text_sheet" "$scratch/sheet" 2 stream
# The link probe of native/text/ps5, on the host: the folder the tests laid out
# like the console's stands in for /preinst/common/font.
"${CXX:-c++}" "${TEXT_FLAGS[@]}" "$root/native/text/ps5/text_probe.cpp" "${TEXT_OBJECTS[@]}" "${TEXT_LIBS[@]}" -o "$out/text_probe"
TUBE_TEXT_FONTS="$scratch/preinst" "$out/text_probe"
