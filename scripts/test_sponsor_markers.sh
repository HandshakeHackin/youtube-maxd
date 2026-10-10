#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
evo="${work}/projects/evoplayer"
destination="${root}/build/tests/sponsor-markers"
python3 "${root}/scripts/extract_sponsor_markers_test.py" "${evo}" "${destination}"
g++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer -I"${destination}" -I"${root}/native" -I"${evo}/addons/include" \
    "${root}/tests/test_sponsor_markers.cpp" -o "${destination}/test"
ASAN_OPTIONS=detect_leaks=1 "${destination}/test"
