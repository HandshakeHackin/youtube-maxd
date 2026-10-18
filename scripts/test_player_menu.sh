#!/usr/bin/env bash
# In-player video actions on the shipping provider: the browser's catalog pages
# listed for the playing video, launches, quality presets and the error fixer.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
evo="${work}/projects/evoplayer"
destination="${root}/build/tests/player-menu"
mkdir -p "$destination"
flags=(-std=c11 -D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer
       -I"${root}/native" -I"${evo}/include" -I"${evo}/addons/include")
objects=()
for source in "${root}/native/tube_provider.c" "${root}/native/tube_remote.c" "${root}/native/tube_player_extra.c" "${root}/native/tube_backup.c" \
              "${root}/native/tube_phone.c" "${evo}/addons/src/cJSON.c" "${evo}/addons/src/evo_provider_mgr.c" "${evo}/src/evo_data_path.c"; do
  object="${destination}/$(basename "$source" .c).o"
  cc "${flags[@]}" -c "$source" -o "$object"
  objects+=("$object")
done
cc "${flags[@]}" "${root}/tests/test_player_menu.c" "${objects[@]}" -lm -o "${destination}/test"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "${destination}/test"
