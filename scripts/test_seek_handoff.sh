#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
evo="${work}/projects/evoplayer"
destination="${root}/build/tests/seek-handoff"
mkdir -p "${destination}"
python3 "${root}/scripts/extract_seek_test.py" \
    "${evo}/core/src/services/PlaybackController.cpp" \
    "${evo}/media/src/evo_demux.c" "${destination}"
g++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer -I"${destination}" \
    "${root}/tests/test_seek_handoff.cpp" -pthread -o "${destination}/test"
ASAN_OPTIONS=detect_leaks=1 "${destination}/test"
g++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer -I"${destination}" -I"${root}/native" \
    "${root}/tests/test_resume_timeline.cpp" \
    $(pkg-config --cflags --libs libavformat libavcodec libavutil) -lm \
    -o "${destination}/resume-test"
ASAN_OPTIONS=detect_leaks=1 "${destination}/resume-test" "${root}/fixtures/generated/fixture.m3u8"
