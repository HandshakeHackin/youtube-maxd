#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="${PROSPEROTUBE_NATIVE_WORK:-${root}/.deps/native-work}"
cjson="${work}/projects/evoplayer/addons/src/cJSON.c"
if [ ! -f "$cjson" ]; then work="$(python3 "${root}/scripts/prepare_native.py")"; fi
mkdir -p "${root}/build/tests"
cc -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"${root}/native" -I"${root}/native/maxd" -I"${root}/native/update/kit" \
  -I"${work}/projects/evoplayer/addons/include" -I"${work}/projects/evoplayer/include" \
  "${root}/tests/test_maxd_release_notice.c" "${work}/projects/evoplayer/addons/src/cJSON.c" \
  -pthread -o "${root}/build/tests/test_maxd_release_notice"
"${root}/build/tests/test_maxd_release_notice"
