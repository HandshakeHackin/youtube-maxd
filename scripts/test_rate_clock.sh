#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
mkdir -p "${root}/build/tests"
cc -std=c11 -D_DEFAULT_SOURCE -Dgettimeofday=tube_test_gettimeofday -Dusleep=tube_test_usleep \
    -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"${work}/projects/evoplayer/pp/include" "${root}/tests/test_rate_clock.c" \
    "${work}/projects/evoplayer/pp/src/pp_clock.c" -lm -o "${root}/build/tests/test_rate_clock"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "${root}/build/tests/test_rate_clock"
