#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
evo="${PROSPEROTUBE_NATIVE_WORK:-${HOME}/.cache/prosperotube/native}/projects/evoplayer"
mkdir -p "${root}/build/tests"
cc -std=c11 -D_DEFAULT_SOURCE -DEVO_TARGET_PS5=1 -Dclock_gettime=tube_test_clock_gettime -O1 -g -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"${root}/native" -I"${evo}/include" -I"${evo}/addons/include" \
  "${root}/tests/test_phone_provider.c" "${root}/native/tube_provider.c" "${root}/native/tube_remote.c" "${root}/native/tube_player_extra.c" "${root}/native/tube_backup.c" \
  "${evo}/addons/src/cJSON.c" "${evo}/addons/src/evo_provider_mgr.c" "${evo}/src/evo_data_path.c" "${evo}/src/evo_readdir.c" -lm \
  -o "${root}/build/tests/test_phone_provider"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "${root}/build/tests/test_phone_provider"
