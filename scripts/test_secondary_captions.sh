#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
evo="${PROSPEROTUBE_NATIVE_WORK:-${HOME}/.cache/prosperotube/native}/projects/evoplayer"
out="${root}/build/tests/secondary-captions"
mkdir -p "${out}"
cc -O1 -g -Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"${evo}/media/include" -I"${evo}/include" -I"${evo}/addons/include" \
  $(pkg-config --cflags libavformat libavcodec libswresample libavutil) \
  "${root}/tests/test_secondary_captions.c" "${evo}/media/src/evo_subtitle.c" "${evo}/media/src/evo_subsync.c" \
  $(pkg-config --libs libavformat libavcodec libswresample libavutil) -lpthread -lm -o "${out}/test_secondary_captions"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "${out}/test_secondary_captions" "${out}"
