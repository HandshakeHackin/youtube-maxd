#!/usr/bin/env bash
# The provider built with TUBE_EMBEDDED_SERVICE against a stand-in service.
# Every other host test builds without the flag and keeps the external companion.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
evo="${work}/projects/evoplayer"
interface="${root}/standalone/include"
mkdir -p "${root}/build/tests"
cc -std=c11 -D_DEFAULT_SOURCE -DTUBE_EMBEDDED_SERVICE=1 -O1 -g -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"${root}/native" -I"${interface}" -I"${evo}/include" -I"${evo}/addons/include" \
  "${root}/tests/test_embedded_service.c" "${root}/native/tube_provider.c" "${root}/native/tube_remote.c" \
  "${root}/native/tube_player_extra.c" "${root}/native/tube_backup.c" "${root}/native/tube_phone.c" \
  "${evo}/addons/src/cJSON.c" "${evo}/addons/src/evo_provider_mgr.c" "${evo}/src/evo_data_path.c" -lm \
  -o "${root}/build/tests/test_embedded_service"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "${root}/build/tests/test_embedded_service"
