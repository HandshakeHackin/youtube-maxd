#!/usr/bin/env bash
# Accounts through the provider in built-in mode against the real service in
# the same process: sign-in, profile, an account list, one change, removal.
# Offline: Google and YouTube are canned answers written to a temporary
# directory; no account, token or network is involved.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
evo="${work}/projects/evoplayer"
bash "${root}/standalone/build_host.sh" > /dev/null
engine="$("${root}/standalone/fetch_quickjs.sh" 2>/dev/null)"
out="${root}/build/tests"
mkdir -p "${out}"
# The Guest fixtures plus the account ones, with a token endpoint that authorizes at the first poll.
fixtures="$(python3 - "${root}/standalone/test" <<'PY'
import sys
sys.path.insert(0, sys.argv[1])
import account_test
print(account_test.fixtures(
    {"POST www.youtube.com/o/oauth2/token": [{"body_contains": "grant_type/device", "file": "t_ok.json"}]},
    {"t_ok.json": {"access_token": "access-token-1", "refresh_token": "refresh-token-1", "expires_in": 3600}}))
PY
)"
trap 'rm -rf "${fixtures}"' EXIT
read -r -a curl_flags <<< "$(pkg-config --cflags --libs libcurl)"
cc -std=c11 -D_GNU_SOURCE -DTUBE_EMBEDDED_SERVICE=1 -O1 -g -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"${root}/native" -I"${root}/standalone/include" -I"${root}/standalone/src" -I"${root}/standalone/ps5" -I"${engine}" \
  -I"${evo}/include" -I"${evo}/addons/include" \
  "${root}/tests/test_embedded_accounts.c" "${root}/native/tube_provider.c" "${root}/native/tube_remote.c" \
  "${root}/native/tube_player_extra.c" "${root}/native/tube_backup.c" "${root}/native/tube_phone.c" \
  "${evo}/addons/src/cJSON.c" "${evo}/addons/src/evo_provider_mgr.c" "${evo}/src/evo_data_path.c" \
  "${evo}/addons/src/evo_net.c" "${evo}/src/evo_error.c" \
  "${root}/standalone/src/tube_engine.c" "${root}/standalone/src/tube_listener.c" \
  "${root}/standalone/ps5/tube_js_http.c" "${root}/standalone/ps5/tube_js_platform.c" "${root}/standalone/build/quickjs.o" \
  -Wl,--wrap=tube_service_configure "${curl_flags[@]}" -lssl -lcrypto -lm -lpthread \
  -o "${out}/test_embedded_accounts"
TUBE_JS_FIXTURES="${fixtures}" TUBE_E2E_BUNDLE="${root}/standalone/build/tube_js.bundle.js" \
  ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "${out}/test_embedded_accounts"
