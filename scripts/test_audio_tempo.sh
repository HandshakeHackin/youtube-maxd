#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "${root}/build/tests"
if [[ -n "${PROSPEROTUBE_TEMPO_FFMPEG:-}" ]]; then
    ff="${PROSPEROTUBE_TEMPO_FFMPEG}"
    flags=("-I${ff}" "${ff}/libavfilter/libavfilter.a" "${ff}/libswscale/libswscale.a"
           "${ff}/libswresample/libswresample.a" "${ff}/libavutil/libavutil.a" -lm -pthread)
else
    read -ra flags <<< "$(pkg-config --cflags --libs libavfilter libavutil)"
fi
cc -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"${root}/native" "${root}/native/tube_audio_tempo.c" "${root}/tests/test_audio_tempo.c" \
    "${flags[@]}" -o "${root}/build/tests/test_audio_tempo"
ASAN_OPTIONS=detect_leaks=1 "${root}/build/tests/test_audio_tempo"
