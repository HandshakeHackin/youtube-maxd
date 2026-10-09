#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cache="${HOME}/.cache/prosperotube/rmlui"
work="$(python3 "${root}/scripts/prepare_native.py")"
evo="${work}/projects/evoplayer"
if [[ ! -f "${cache}/build/librmlui.a" ]]; then
    mkdir -p "${cache}"
    curl -fsSL https://github.com/mikke89/RmlUi/archive/2230d1a6e8e0848ed87a5761e2a5160b2a175ba4.tar.gz -o "${cache}/source.tar.gz"
    tar -xzf "${cache}/source.tar.gz" -C "${cache}"
    cmake -S "${cache}/RmlUi-2230d1a6e8e0848ed87a5761e2a5160b2a175ba4" -B "${cache}/build" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DRMLUI_SAMPLES=OFF -DRMLUI_TESTS=OFF
    cmake --build "${cache}/build" -j 8
fi
include="${cache}/RmlUi-6.2/Include"
[[ -d "${include}" ]] || include="${cache}/RmlUi-2230d1a6e8e0848ed87a5761e2a5160b2a175ba4/Include"
mkdir -p "${root}/build/preview" "${root}/output/preview"
g++ -O2 -std=c++17 -Wall -Wextra -I"${include}" -I"${evo}/ui_rml/include" \
  -I"${evo}/addons/include" -I"${evo}/include" \
  "${root}/tests/preview_native.cpp" "${evo}/ui_rml/src/evo_rmlui_render.cpp" \
  "${evo}/ui_rml/src/evo_rmlui_system.cpp" "${cache}/build/librmlui.a" \
  $(pkg-config --libs freetype2) -pthread -o "${root}/build/preview/native_ui"
"${root}/build/preview/native_ui" "${root}/native" "${evo}/assets/fonts" "${root}/output/preview"
for file in "${root}/output/preview/"*.ppm; do
    ffmpeg -v error -y -i "${file}" -frames:v 1 "${file%.ppm}.png"
done
