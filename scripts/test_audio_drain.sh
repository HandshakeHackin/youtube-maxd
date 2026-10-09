#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
evo="${work}/projects/evoplayer"
mkdir -p "${root}/build/tests"
cc -std=c11 -D_DEFAULT_SOURCE -O1 -g -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"${root}/native" -I"${evo}/include" -I"${evo}/media/include" \
    -I"${evo}/media/src" -I"${evo}/pp/include" \
    "${root}/tests/test_audio_drain.c" "${evo}/media/src/evo_audio_resample.c" \
    "${evo}/media/src/evo_playback.c" \
    $(pkg-config --cflags --libs libavformat libavcodec libavutil libswresample) \
    -Wl,--gc-sections -lm -pthread -o "${root}/build/tests/test_audio_drain"
ASAN_OPTIONS=detect_leaks=1 "${root}/build/tests/test_audio_drain" "${root}/fixtures/generated/audio.m4a"
