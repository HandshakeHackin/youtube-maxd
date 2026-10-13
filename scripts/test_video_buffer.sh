#!/usr/bin/env bash
# The Video buffer preference against the shipping read-ahead sizing: the
# function is taken from the generated PlaybackController.cpp, as compiled
# into the app, and run with each choice, with and without the packet ring.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
evo="${work}/projects/evoplayer"
out="${root}/build/tests"
mkdir -p "${out}"
python3 "${root}/scripts/extract_read_ahead_test.py" "${evo}/core/src/services/PlaybackController.cpp" "${out}/tube_read_ahead.inc"
flags=(-O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -I"${root}/native" -I"${evo}/addons/include")
cc -std=c11 -D_DEFAULT_SOURCE "${flags[@]}" -c "${root}/native/tube_player_extra.c" -o "${out}/video_buffer_extra.o"
cc -std=c11 -D_DEFAULT_SOURCE "${flags[@]}" -c "${evo}/addons/src/cJSON.c" -o "${out}/video_buffer_cjson.o"
c++ -std=c++17 "${flags[@]}" -I"${out}" "${root}/tests/test_video_buffer.cpp" "${out}/video_buffer_extra.o" "${out}/video_buffer_cjson.o" \
  -lm -o "${out}/test_video_buffer"
ASAN_OPTIONS=detect_leaks=1 "${out}/test_video_buffer"
