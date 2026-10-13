#!/usr/bin/env bash
# The Afterglow data layer on the shipping provider: ProviderSource and Data
# against native/tube_provider.c, with the network replaced by canned answers
# of the built-in service (tests/afterglow/provider/provider_test.cpp).
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
kit="$(python3 "${root}/scripts/ui_kit.py")"
evo="${work}/projects/evoplayer"
destination="${root}/build/tests/afterglow-data"
mkdir -p "${destination}"
sanitize=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
cflags=(-std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror "${sanitize[@]}" -I"${root}/native" -I"${evo}/include" -I"${evo}/addons/include")
cxxflags=(-std=c++20 -Wall -Wextra -Werror "${sanitize[@]}" -I"${root}/native" -I"${evo}/include" -I"${evo}/addons/include" -I"${kit}/src" -I"${kit}")
objects=()
for source in "${root}/native/tube_provider.c" "${root}/native/tube_remote.c" "${root}/native/tube_player_extra.c" "${root}/native/tube_backup.c" \
    "${root}/native/tube_phone.c" "${evo}/addons/src/cJSON.c" "${evo}/addons/src/evo_provider_mgr.c" "${evo}/src/evo_data_path.c"; do
    object="${destination}/$(basename "${source}" .c).o"
    cc "${cflags[@]}" -c "${source}" -o "${object}"
    objects+=("${object}")
done
for source in "${root}/native/afterglow/data.cpp" "${root}/native/afterglow/data_rows.cpp" "${root}/native/afterglow/platform.cpp" \
    "${root}/native/afterglow/text_kit.cpp" "${root}/native/afterglow/app/data_provider.cpp" \
    "${kit}/src/gfx/font.cpp" "${kit}/src/gfx/draw_list.cpp" "${kit}/src/gfx/triangulate.cpp" \
    "${root}/tests/afterglow/provider/provider_test.cpp"; do
    object="${destination}/$(basename "${source}" .cpp).o"
    g++ "${cxxflags[@]}" -c "${source}" -o "${object}"
    objects+=("${object}")
done
g++ "${sanitize[@]}" "${objects[@]}" -lm -o "${destination}/provider_test"
ASAN_OPTIONS=detect_leaks=1 "${destination}/provider_test"
