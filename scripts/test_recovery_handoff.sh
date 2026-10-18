#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
evo="${work}/projects/evoplayer"
destination="${root}/build/tests/recovery-handoff"
mkdir -p "${destination}"
python3 "${root}/scripts/extract_recovery_handoff_test.py" "${evo}/core/src/screens/ProviderHostScreen.cpp" "${destination}"
g++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"${destination}" -I"${root}/native" -I"${evo}/addons/include" \
    "${root}/tests/test_playback_recovery_handoff.cpp" "${root}/native/tube_background_play.c" -pthread -o "${destination}/test"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "${destination}/test"
