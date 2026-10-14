#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
evo="${work}/projects/evoplayer"
python3 "${root}/tests/test_completion_status.py" "${evo}/src/evo_usb_remote.c"
mkdir -p "${root}/build/tests"
python3 "${root}/scripts/extract_native_drain_test.py" "${evo}/media/src/evo_vdec_native.c" "${root}/build/tests"
python3 "${root}/scripts/extract_demux_input_test.py" "${evo}/media/src/evo_demux.c" "${root}/build/tests/tube_demux_input.inc"
cc -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"${root}/build/tests" -I"${evo}/media/include" \
    "${root}/tests/test_native_video_drain.c" "${root}/native/tube_frame_lease.c" -I"${root}/native" -pthread -o "${root}/build/tests/test_native_video_drain"
ASAN_OPTIONS=detect_leaks=1 "${root}/build/tests/test_native_video_drain"
cc -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"${root}/build/tests" -I"${evo}/media/include" \
    "${root}/tests/test_native_surface.c" -o "${root}/build/tests/test_native_surface"
ASAN_OPTIONS=detect_leaks=1 "${root}/build/tests/test_native_surface"
cc -std=c11 -D_DEFAULT_SOURCE -O1 -g -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"${root}/build/tests" -I"${evo}/addons/include" -I"${evo}/include" -I"${evo}/media/include" -I"${evo}/media/src" -I"${evo}/pp/include" \
    "${root}/tests/test_video_drain.c" "${root}/tests/host_audio_rate.c" "${evo}/media/src/evo_vdec_ffmpeg.c" \
    "${evo}/media/src/evo_vdec_native.c" "${evo}/pp/src/pp_playback.c" "${evo}/pp/src/pp_clock.c" "${root}/native/tube_frame_lease.c" \
    $(pkg-config --cflags --libs libavformat libavcodec libavutil libswscale) \
    -Wl,--gc-sections -lm -pthread -o "${root}/build/tests/test_video_drain"
ASAN_OPTIONS=detect_leaks=1 "${root}/build/tests/test_video_drain" \
    "${root}/fixtures/generated/video.mp4" "${root}/fixtures/generated/video.webm"
