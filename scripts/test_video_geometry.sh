#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="${PROSPEROTUBE_NATIVE_WORK:-${HOME}/.cache/prosperotube/native}"
destination="${root}/build/tests/video-geometry"
for variant in current staged; do
  # A generated tree with verified shaders already uses the activated path.
  if [[ "$variant" == staged ]] && grep -q 'g_tube_frame_geometry' "${work}/projects/evoplayer/media/src/evo_agc_runtime.c"; then continue; fi
  case_dir="$destination/$variant"
  python3 "${root}/scripts/extract_video_geometry_test.py" "${work}/projects/evoplayer" "$case_dir" "$variant"
  g++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -I"${root}/native" -I"$case_dir" "${root}/tests/test_video_geometry_render.cpp" "${root}/native/tube_video_destination.c" -o "$case_dir/test"
  ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "$case_dir/test"
done
