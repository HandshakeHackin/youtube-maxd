#!/usr/bin/env bash
# Compile against the exact candidate FFmpeg; never install or mutate an SDK.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
source="${PROSPEROTUBE_DVR_FFMPEG_SOURCE:?Set PROSPEROTUBE_DVR_FFMPEG_SOURCE to an isolated configured FFmpeg 7.1.1 build}"
destination="${root}/build/tests/live-dvr"
mkdir -p "${destination}"
for archive in libavformat/libavformat.a libavcodec/libavcodec.a libswresample/libswresample.a libavutil/libavutil.a; do
    [[ -f "${source}/${archive}" ]] || { echo "Missing candidate archive: ${source}/${archive}" >&2; exit 2; }
done
candidate=()
[[ "${PROSPEROTUBE_DVR_CANDIDATE:-0}" == 1 ]] && candidate=(-DTUBE_DVR_CANDIDATE=1)
runtime=()
if [[ "${PROSPEROTUBE_DVR_RUNTIME:-0}" == 1 ]]; then
    evo="${PROSPEROTUBE_NATIVE_WORK:?Set an isolated native work for runtime headers}/projects/evoplayer"
    candidate+=(-DTUBE_DVR_RUNTIME=1 -I"${evo}/include" -I"${evo}/addons/include")
    runtime=("${root}/native/tube_live_runtime.c")
fi
cc "${candidate[@]}" -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"${source}" -I"${root}/native" "${root}/tests/test_live_dvr.c" "${runtime[@]}" \
    "${source}/libavformat/libavformat.a" "${source}/libavcodec/libavcodec.a" \
    "${source}/libswresample/libswresample.a" "${source}/libavutil/libavutil.a" \
    -lm -pthread -lxml2 -lssl -lcrypto -latomic -lz -lbz2 -llzma -o "${destination}/probe"
if [[ "${PROSPEROTUBE_DVR_CANDIDATE:-0}" == 1 ]]; then python3 "${root}/tests/test_hls_dvr_anchor.py"; fi
ASAN_OPTIONS=detect_leaks=1 python3 "${root}/scripts/probe_live_dvr.py" \
    --binary "${destination}/probe" --result "${destination}/result.json" "$@"

if [[ "${PROSPEROTUBE_DVR_RUNTIME:-0}" == 1 && -n "${PROSPEROTUBE_DVR_STOCK_SOURCE:-}" ]]; then
    stock="${PROSPEROTUBE_DVR_STOCK_SOURCE}"
    cc -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
        -I"${stock}" -I"${root}/native" -I"${evo}/include" -I"${evo}/addons/include" \
        "${root}/tests/test_live_runtime_missing.c" "${root}/native/tube_live_runtime.c" \
        "${stock}/libavformat/libavformat.a" "${stock}/libavcodec/libavcodec.a" \
        "${stock}/libswresample/libswresample.a" "${stock}/libavutil/libavutil.a" \
        -lm -pthread -lxml2 -lssl -lcrypto -latomic -lz -lbz2 -llzma -o "${destination}/missing-extension"
    ASAN_OPTIONS=detect_leaks=1 "${destination}/missing-extension"
fi
