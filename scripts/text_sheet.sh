#!/usr/bin/env bash
# Draw the text engine's contact sheets (invented video titles in every script,
# at the 1080p and the 4K surface) from the atlas the engine fills, as PNG files.
# usage: text_sheet.sh [output folder]      (default build/text-sheet)
# Reads the fonts tests/text_host.hpp names; draws nothing when they are absent.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
source "$root/scripts/text_common.sh"
out=${1:-$root/build/text-sheet}
objects=$root/build/tests/text/plain
mkdir -p "$out"
text_engine plain "$objects"
"${CXX:-c++}" "${TEXT_FLAGS[@]}" "$root/tests/text_sheet.cpp" "${TEXT_OBJECTS[@]}" "${TEXT_LIBS[@]}" -lz -o "$objects/text_sheet"
"$objects/text_sheet" "$out" 1
"$objects/text_sheet" "$out" 2
"$objects/text_sheet" "$out/stream" 1 stream 2>/dev/null || { mkdir -p "$out/stream"; "$objects/text_sheet" "$out/stream" 1 stream; }
# Read on demand or mapped, the pictures are the same.
for board in "$out"/titles-1080p-*.png; do
  cmp -s "$board" "$out/stream/$(basename "$board")" || { echo "text_sheet: $(basename "$board") differs when fonts are read on demand" >&2; exit 1; }
done
rm -rf "$out/stream"
echo "sheets in $out"
