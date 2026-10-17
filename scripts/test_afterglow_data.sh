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
python3 "${root}/tests/afterglow/provider/extract_link_phone.py" "${destination}/link_phone_probe.hpp"
sanitize=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
cflags=(-std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror "${sanitize[@]}" -I"${root}/native" -I"${evo}/include" -I"${evo}/addons/include")
cxxflags=(-I"${destination}" -std=c++20 -Wall -Wextra -Werror "${sanitize[@]}" -I"${root}/native" -I"${evo}/include" -I"${evo}/addons/include" -I"${kit}/src" -I"${kit}")
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

# The pictures those rows name, as YouTube sends them: the picture cache with the app's decoder for
# what its own does not read (tests/afterglow/provider/picture_test.cpp), on the host's FFmpeg.
pictures="${destination}/pictures"
mkdir -p "${pictures}"
for name in webp:libwebp jpg:mjpeg; do
    ffmpeg -v error -y -f lavfi -i "color=c=red:s=320x180" -frames:v 1 -c:v "${name#*:}" "${pictures}/red.${name%%:*}"
done
read -r -a media <<< "$(pkg-config --cflags --libs libavcodec libswscale libavutil)"
picture_objects=()
for source in "${root}/native/afterglow/images.cpp" "${root}/native/afterglow/app/picture_decode.cpp" \
    "${root}/tests/afterglow/provider/picture_test.cpp"; do
    object="${destination}/$(basename "${source}" .cpp).o"
    g++ "${cxxflags[@]}" -I"${evo}" "${media[@]}" -c "${source}" -o "${object}"
    picture_objects+=("${object}")
done
g++ "${sanitize[@]}" "${picture_objects[@]}" "${destination}/platform.o" "${destination}/draw_list.o" "${destination}/triangulate.o" \
    "${destination}/font.o" "${media[@]}" -lm -o "${destination}/picture_test"
# A portrait picture of a width that is no multiple of 8, as Subscriptions tiles carry. Drawn as
# RGB: FFmpeg 6.1's colour source in 4:2:0 drops an odd width's last column (151 became 150).
ffmpeg -v error -y -f lavfi -i "color=c=red:s=151x270,format=rgb24" -frames:v 1 -c:v libwebp "${pictures}/portrait.webp"
ASAN_OPTIONS=detect_leaks=1 "${destination}/picture_test" "${pictures}/red.webp" "${pictures}/red.jpg" "${pictures}/portrait.webp"
