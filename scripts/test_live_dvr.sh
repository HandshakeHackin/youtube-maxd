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
cc "${candidate[@]}" -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"${source}" -I"${root}/native" "${root}/tests/test_live_dvr.c" \
    "${source}/libavformat/libavformat.a" "${source}/libavcodec/libavcodec.a" \
    "${source}/libswresample/libswresample.a" "${source}/libavutil/libavutil.a" \
    -lm -pthread -lxml2 -lssl -lcrypto -latomic -lz -lbz2 -llzma -o "${destination}/probe"
ASAN_OPTIONS=detect_leaks=1 python3 "${root}/scripts/probe_live_dvr.py" \
    --binary "${destination}/probe" --result "${destination}/result.json" "$@"
