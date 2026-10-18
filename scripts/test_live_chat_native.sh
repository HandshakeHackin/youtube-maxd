#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
evo="${work}/projects/evoplayer"
mkdir -p "${root}/build/tests"
cc -std=c11 -D_DEFAULT_SOURCE -Dclock_gettime=tube_test_clock_gettime -O1 -g -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"${root}/native" -I"${evo}/include" -I"${evo}/addons/include" \
  "${root}/tests/test_live_chat_native.c" "${root}/native/tube_provider.c" "${root}/native/tube_remote.c" "${root}/native/tube_player_extra.c" "${root}/native/tube_backup.c" \
  "${evo}/addons/src/cJSON.c" "${evo}/addons/src/evo_provider_mgr.c" "${evo}/src/evo_data_path.c" -lm \
  -o "${root}/build/tests/test_live_chat_native"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "${root}/build/tests/test_live_chat_native"
cc -std=c11 -D_DEFAULT_SOURCE -Dclock_gettime=tube_test_clock_gettime -O1 -g -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"${root}/native" -I"${evo}/include" -I"${evo}/addons/include" \
  "${root}/tests/test_chat_compose_native.c" "${root}/native/tube_provider.c" "${root}/native/tube_remote.c" "${root}/native/tube_player_extra.c" "${root}/native/tube_backup.c" \
  "${evo}/addons/src/cJSON.c" "${evo}/addons/src/evo_provider_mgr.c" "${evo}/src/evo_data_path.c" -lm \
  -o "${root}/build/tests/test_chat_compose_native"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "${root}/build/tests/test_chat_compose_native"
