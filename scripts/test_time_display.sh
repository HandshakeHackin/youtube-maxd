#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="${PROSPEROTUBE_NATIVE_WORK:-${HOME}/.cache/prosperotube/native}"
destination="${root}/build/tests/time-display"
python3 "${root}/scripts/extract_time_display_test.py" "${work}/projects/evoplayer" "$destination"
g++ -std=c++17 -D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -I"${root}/native" -I"$destination" "${root}/tests/test_time_display.cpp" -o "$destination/test"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "$destination/test"
