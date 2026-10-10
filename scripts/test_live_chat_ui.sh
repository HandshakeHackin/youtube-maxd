#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="${PROSPEROTUBE_NATIVE_WORK:-${HOME}/.cache/prosperotube/native}"
evo="${work}/projects/evoplayer"
cache="${HOME}/.cache/prosperotube/rmlui"
include="${cache}/RmlUi-6.2/Include"
[[ -d "${include}" ]] || include="${cache}/RmlUi-2230d1a6e8e0848ed87a5761e2a5160b2a175ba4/Include"
mkdir -p "${root}/build/tests/chat-ui"
for source in "${root}/native/tube_provider.c" "${root}/native/tube_remote.c" "${evo}/addons/src/cJSON.c" "${evo}/addons/src/evo_provider_mgr.c" "${evo}/src/evo_data_path.c"; do
  cc -std=c11 -D_DEFAULT_SOURCE -Dclock_gettime=tube_test_clock_gettime -O1 -g -ffunction-sections -fdata-sections \
    -I"${root}/native" -I"${evo}/include" -I"${evo}/addons/include" -c "${source}" \
    -o "${root}/build/tests/chat-ui/$(basename "${source}" .c).o"
done
g++ -O1 -g -std=c++17 -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -I"${include}" -I"${evo}/ui_rml/include" -I"${evo}/ui_rml/src" -I"${evo}/addons/include" -I"${evo}/include" \
  "${root}/tests/test_live_chat_ui.cpp" "${evo}/ui_rml/src/evo_rmlui_render.cpp" "${evo}/ui_rml/src/evo_rmlui_system.cpp" \
  "${root}/build/tests/chat-ui/"*.o "${cache}/build/librmlui.a" $(pkg-config --libs freetype2) -pthread \
  -o "${root}/build/tests/test_live_chat_ui"
"${root}/build/tests/test_live_chat_ui" "${root}/native" "${evo}/assets/fonts"
