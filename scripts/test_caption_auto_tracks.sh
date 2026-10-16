#!/usr/bin/env bash
# Automatic and translated captions: the provider keeps their target languages, finds the
# translation into the interface language and starts an automatic track only when allowed;
# the generated picker lists them (scripts/test_caption_auto_tracks.py).
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
evo="${work}/projects/evoplayer"
mkdir -p "${root}/build/tests"
cc -std=c11 -D_DEFAULT_SOURCE -Dclock_gettime=tube_test_clock_gettime -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"${root}/native" -I"${evo}/include" -I"${evo}/addons/include" \
  "${root}/tests/test_caption_auto_tracks.c" "${root}/native/tube_provider.c" "${root}/native/tube_remote.c" "${root}/native/tube_player_extra.c" "${root}/native/tube_backup.c" \
  "${evo}/addons/src/cJSON.c" "${evo}/addons/src/evo_provider_mgr.c" "${evo}/src/evo_data_path.c" -lm \
  -o "${root}/build/tests/test_caption_auto_tracks"
ASAN_OPTIONS=detect_leaks=1 "${root}/build/tests/test_caption_auto_tracks"
python3 "${root}/scripts/test_caption_auto_tracks.py"
