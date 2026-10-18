#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
destination="${root}/build/tests/player-controls"
python3 "${root}/scripts/extract_player_controls_test.py" "${work}/projects/evoplayer" "${destination}"
g++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer -I"${destination}" "${root}/tests/test_player_controls.cpp" -o "${destination}/test"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "${destination}/test"
