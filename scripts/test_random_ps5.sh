#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "${root}/build/tests"
cc -std=c11 -D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
 -I"${root}/native" "${root}/tests/test_random_ps5.c" -o "${root}/build/tests/test_random_ps5"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "${root}/build/tests/test_random_ps5"
