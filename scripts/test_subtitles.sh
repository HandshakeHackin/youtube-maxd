#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
evo="${root}/third_party/evo/projects/evoplayer"
out="${root}/build/tests/subtitles"
mkdir -p "${out}"
cc -O1 -g -Wall -Wextra -Wno-unused-parameter \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"${root}/native" -I"${evo}/media/include" -I"${evo}/include" -I"${evo}/addons/include" \
  $(pkg-config --cflags libavformat libavcodec libswresample libavutil) \
  "${root}/tests/test_tube_subtitles.c" \
  "${evo}/media/src/evo_subtitle.c" "${evo}/media/src/evo_subsync.c" \
  $(pkg-config --libs libavformat libavcodec libswresample libavutil) \
  -lpthread -lm -o "${out}/test_tube_subtitles"
ASAN_OPTIONS=detect_leaks=1 "${out}/test_tube_subtitles" "${out}"
