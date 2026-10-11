#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
evo="${work}/projects/evoplayer"
source "${root}/scripts/rmlui_host.sh"
mkdir -p "${root}/build/preview" "${root}/output/preview"
g++ -O2 -std=c++17 -Wall -Wextra -I"${rmlui_include}" -I"${evo}/ui_rml/include" \
  -I"${evo}/addons/include" -I"${evo}/include" \
  "${root}/tests/preview_native.cpp" "${evo}/ui_rml/src/evo_rmlui_render.cpp" \
  "${evo}/ui_rml/src/evo_rmlui_system.cpp" "${rmlui_library}" \
  $(pkg-config --libs freetype2) -pthread -o "${root}/build/preview/native_ui"
"${root}/build/preview/native_ui" "${root}/native" "${evo}/assets/fonts" "${root}/output/preview"
for file in "${root}/output/preview/"*.ppm; do
    ffmpeg -v error -y -i "${file}" -frames:v 1 "${file%.ppm}.png"
done
