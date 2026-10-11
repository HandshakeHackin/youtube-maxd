#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="${PROSPEROTUBE_NATIVE_WORK:-${HOME}/.cache/prosperotube/native}"
evo="${work}/projects/evoplayer"
mkdir -p "${root}/build/tests"
flags=(-std=c11 -D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer
       -I"${root}/native" -I"${root}/native/vendor/cJSON")
cc "${flags[@]}" "${root}/tests/test_backup.c" "${root}/native/tube_backup.c" \
  "${root}/native/vendor/cJSON/cJSON.c" -lm -o "${root}/build/tests/test_backup"
ASAN_OPTIONS=detect_leaks=1 "${root}/build/tests/test_backup"
# The console build reaches directories through EVO's wrappers. Run the same
# cases through that branch with the generated tree's host implementations.
cc "${flags[@]}" -DEVO_TARGET_PS5=1 -I"${evo}/include" "${root}/tests/test_backup.c" "${root}/native/tube_backup.c" \
  "${evo}/src/evo_readdir.c" "${evo}/src/evo_data_path.c" \
  "${root}/native/vendor/cJSON/cJSON.c" -lm -o "${root}/build/tests/test_backup_evo"
ASAN_OPTIONS=detect_leaks=1 "${root}/build/tests/test_backup_evo"
