#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "${root}/build/tests"
cc -std=c11 -D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"${root}/native" -I"${root}/native/vendor/cJSON" \
  -I"${root}/third_party/evo/projects/evoplayer/addons/include" \
  "${root}/tests/test_phone_player.c" "${root}/native/tube_remote.c" "${root}/native/tube_phone.c" \
  "${root}/native/vendor/cJSON/cJSON.c" -lm -o "${root}/build/tests/test_phone_player"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "${root}/build/tests/test_phone_player"
