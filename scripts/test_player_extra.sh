#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "${root}/build/tests"
flags=(-std=c11 -D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer
       -I"${root}/native" -I"${root}/native/vendor/cJSON")
cc "${flags[@]}" "${root}/tests/test_player_extra.c" "${root}/native/tube_player_extra.c" \
  "${root}/native/vendor/cJSON/cJSON.c" -lm -o "${root}/build/tests/test_player_extra"
ASAN_OPTIONS=detect_leaks=1 "${root}/build/tests/test_player_extra"
cc "${flags[@]}" "${root}/tests/test_storyboard.c" -lm -o "${root}/build/tests/test_storyboard"
ASAN_OPTIONS=detect_leaks=1 "${root}/build/tests/test_storyboard"
cc "${flags[@]}" "${root}/tests/test_audio_extra.c" -lm -o "${root}/build/tests/test_audio_extra"
ASAN_OPTIONS=detect_leaks=1 "${root}/build/tests/test_audio_extra"
