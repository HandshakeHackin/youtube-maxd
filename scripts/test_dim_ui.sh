#!/usr/bin/env bash
# Screen dimming on the real RmlUi host build: the generated browser screen and
# layout with the pinned RmlUi and the CPU rasteriser.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
evo="${work}/projects/evoplayer"
source "${root}/scripts/rmlui_host.sh"
destination="${root}/build/tests/dim-ui"
mkdir -p "${destination}/objects"
for source in "${root}/native/tube_provider.c" "${root}/native/tube_remote.c" "${root}/native/tube_player_extra.c" "${root}/native/tube_backup.c" "${evo}/addons/src/cJSON.c" "${evo}/addons/src/evo_provider_mgr.c" "${evo}/src/evo_data_path.c"; do
  cc -std=c11 -D_DEFAULT_SOURCE -Dclock_gettime=tube_test_clock_gettime -O1 -g -ffunction-sections -fdata-sections \
    -I"${root}/native" -I"${evo}/include" -I"${evo}/addons/include" -c "${source}" \
    -o "${destination}/objects/$(basename "${source}" .c).o"
done
g++ -O1 -g -std=c++17 -ffunction-sections -fdata-sections -Wl,--gc-sections -I"${root}/native" \
  -I"${rmlui_include}" -I"${evo}/ui_rml/include" -I"${evo}/ui_rml/src" -I"${evo}/addons/include" -I"${evo}/include" \
  "${root}/tests/test_dim_ui.cpp" "${evo}/ui_rml/src/evo_rmlui_render.cpp" "${evo}/ui_rml/src/evo_rmlui_system.cpp" \
  "${destination}/objects/"*.o "${rmlui_library}" $(pkg-config --libs freetype2) -pthread \
  -o "${destination}/test"
"${destination}/test" "${evo}/assets/rml" "${evo}/assets/fonts"
