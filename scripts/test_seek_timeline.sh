#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
destination="${root}/build/tests/seek-timeline"
mkdir -p "${destination}"
cc -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer -I"${root}/native" \
    "${root}/tests/test_seek_timeline.c" \
    $(pkg-config --cflags --libs libavformat libavcodec libavutil) -lm \
    -o "${destination}/test"
ASAN_OPTIONS=detect_leaks=1 "${destination}/test" "${root}/fixtures/generated/fixture.m3u8"
