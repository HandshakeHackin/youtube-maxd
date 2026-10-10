#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
evo="${work}/projects/evoplayer"
mkdir -p "${root}/build/tests"
if [[ -n "${PROSPEROTUBE_TEMPO_FFMPEG:-}" ]]; then
    ff="${PROSPEROTUBE_TEMPO_FFMPEG}"
    flags=("-I${ff}" -Wl,--start-group "${ff}/libavformat/libavformat.a"
           "${ff}/libavcodec/libavcodec.a" "${ff}/libavfilter/libavfilter.a"
           "${ff}/libswscale/libswscale.a" "${ff}/libswresample/libswresample.a"
           "${ff}/libavutil/libavutil.a" -Wl,--end-group -lxml2)
else
    read -ra flags <<< "$(pkg-config --cflags --libs libavformat libavcodec libavfilter libavutil libswresample)"
fi
cc -std=c11 -D_DEFAULT_SOURCE -O1 -g -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"${root}/native" -I"${evo}/include" -I"${evo}/media/include" \
    -I"${evo}/media/src" -I"${evo}/pp/include" \
    "${root}/tests/test_audio_drain.c" "${evo}/media/src/evo_audio_resample.c" \
    "${evo}/media/src/evo_playback.c" "${root}/native/tube_audio_tempo.c" \
    "${flags[@]}" \
    -Wl,--gc-sections -lm -pthread -o "${root}/build/tests/test_audio_drain"
ASAN_OPTIONS=detect_leaks=1 "${root}/build/tests/test_audio_drain" "${root}/fixtures/generated/audio.m4a"
