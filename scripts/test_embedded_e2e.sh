#!/usr/bin/env bash
# The provider in built-in mode against the real service in the same process,
# through EVO's own HTTP client, with YouTube played by canned answers.
# Offline: every upstream request is answered from standalone/fixtures, the
# phone connection by a local stand-in.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
evo="${work}/projects/evoplayer"
bash "${root}/standalone/build_host.sh" > /dev/null
engine="$("${root}/standalone/fetch_quickjs.sh" 2>/dev/null)"
out="${root}/build/tests"
mkdir -p "${out}"
read -r -a curl_flags <<< "$(pkg-config --cflags --libs libcurl)"
cc -std=c11 -D_GNU_SOURCE -DTUBE_EMBEDDED_SERVICE=1 -O1 -g -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"${root}/native" -I"${root}/standalone/include" -I"${root}/standalone/src" -I"${root}/standalone/ps5" -I"${engine}" \
  -I"${evo}/include" -I"${evo}/addons/include" \
  "${root}/tests/test_embedded_e2e.c" "${root}/native/tube_provider.c" "${root}/native/tube_remote.c" \
  "${root}/native/tube_player_extra.c" "${root}/native/tube_backup.c" "${root}/native/tube_phone.c" \
  "${evo}/addons/src/cJSON.c" "${evo}/addons/src/evo_provider_mgr.c" "${evo}/src/evo_data_path.c" \
  "${evo}/addons/src/evo_net.c" "${evo}/src/evo_error.c" \
  "${root}/standalone/src/tube_engine.c" "${root}/standalone/src/tube_listener.c" \
  "${root}/standalone/ps5/tube_js_http.c" "${root}/standalone/ps5/tube_js_platform.c" "${root}/standalone/build/quickjs.o" \
  -Wl,--wrap=tube_service_configure "${curl_flags[@]}" -lssl -lcrypto -lm -lpthread \
  -o "${out}/test_embedded_e2e"
# YouTube's phone connection service is played by the stand-in of standalone/test/remote_test.py on 127.0.0.1.
coproc LOUNGE { python3 "${root}/standalone/test/remote_test.py" --serve; }
trap 'kill "${LOUNGE_PID}" 2>/dev/null || true' EXIT
read -r lounge <&"${LOUNGE[0]}"
TUBE_JS_FIXTURES="${root}/standalone/fixtures" TUBE_E2E_BUNDLE="${root}/standalone/build/tube_js.bundle.js" TUBE_E2E_LOUNGE="${lounge}" \
  ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "${out}/test_embedded_e2e"
