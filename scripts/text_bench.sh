#!/usr/bin/env bash
# What the text engine costs on this PC: font opening, shaping per script, the
# cached path of a frame, rasterising per size, memory. Optimised build, the
# pinned libraries; fonts mapped, then read on demand.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
source "$root/scripts/text_common.sh"
objects=$root/build/tests/text/plain
text_engine plain "$objects"
"${CXX:-c++}" "${TEXT_FLAGS[@]}" "$root/tests/text_bench.cpp" "${TEXT_OBJECTS[@]}" "${TEXT_LIBS[@]}" -o "$objects/text_bench"
"$objects/text_bench"
echo
"$objects/text_bench" stream
