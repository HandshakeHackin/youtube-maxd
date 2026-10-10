#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
evo="${work}/projects/evoplayer"
mkdir -p "${root}/build/tests"
cc -std=c11 -D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"${root}/native" -I"${evo}/include" -I"${evo}/addons/include" \
  "${root}/tests/test_native.c" "${root}/native/tube_provider.c" \
  "${evo}/addons/src/cJSON.c" "${evo}/addons/src/evo_provider_mgr.c" \
  "${evo}/src/evo_data_path.c" -lm -o "${root}/build/tests/test_native"
ASAN_OPTIONS=detect_leaks=1 "${root}/build/tests/test_native"
cc -std=c11 -D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"${root}/native" -I"${evo}/include" -I"${evo}/addons/include" \
  "${root}/tests/test_playback_recovery.c" "${root}/native/tube_provider.c" \
  "${evo}/addons/src/cJSON.c" "${evo}/addons/src/evo_provider_mgr.c" \
  "${evo}/src/evo_data_path.c" -lm -o "${root}/build/tests/test_playback_recovery"
ASAN_OPTIONS=detect_leaks=1 "${root}/build/tests/test_playback_recovery"
cc -std=c11 -D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"${root}/native" -I"${evo}/include" -I"${evo}/addons/include" \
  "${root}/tests/test_sponsor.c" "${root}/native/tube_provider.c" \
  "${evo}/addons/src/cJSON.c" "${evo}/addons/src/evo_provider_mgr.c" \
  "${evo}/src/evo_data_path.c" -lm -o "${root}/build/tests/test_sponsor"
ASAN_OPTIONS=detect_leaks=1 "${root}/build/tests/test_sponsor"
