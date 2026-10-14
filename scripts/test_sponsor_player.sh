#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
destination="${root}/build/tests/sponsor-player"
python3 "${root}/scripts/extract_sponsor_test.py" \
    "${work}/projects/evoplayer/core/src/screens/PlayerScreen.cpp" "${destination}"
g++ -std=c++17 -DEVO_AGC_DEVICE=1 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer -I"${destination}" \
    "${root}/tests/test_sponsor_player.cpp" -o "${destination}/test"
ASAN_OPTIONS=detect_leaks=1 "${destination}/test"
