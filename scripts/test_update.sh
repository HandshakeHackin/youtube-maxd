#!/usr/bin/env bash
# The update engine with the app's HTTPS client against a loopback fixture
# server (plain HTTP, and HTTPS with a certificate made for this run when
# openssl is installed), then the About rows through the provider.
# Nothing here reaches any host but 127.0.0.1.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
evo="${work}/projects/evoplayer"
out="${root}/build/tests"
mkdir -p "${out}"
fixtures="$(mktemp -d /tmp/tu-XXXXXX)"
pids=()
cleanup() {
    for pid in "${pids[@]}"; do kill "${pid}" 2>/dev/null || true; done
    rm -rf "${fixtures}"
}
trap cleanup EXIT

read -r -a curl_cflags <<< "$(pkg-config --cflags libcurl)"
read -r -a curl_libs <<< "$(pkg-config --libs libcurl)"
flags=(-std=c11 -D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer
       -I"${root}/native" -I"${root}/native/vendor/cJSON" -I"${root}/standalone/ps5")
engine=("${root}/native/tube_update.c" "${root}/native/tube_update_zip.c")
# The app's own HTTPS client, built once as a desktop program has it.
client=("${out}/update_js_http.o" "${out}/update_js_platform.o")
cc "${flags[@]}" "${curl_cflags[@]}" -c "${root}/standalone/ps5/tube_js_http.c" -o "${client[0]}"
cc "${flags[@]}" -c "${root}/standalone/ps5/tube_js_platform.c" -o "${client[1]}"
cc "${flags[@]}" "${curl_cflags[@]}" "${root}/tests/test_update.c" "${engine[@]}" "${root}/native/tube_update_http.c" \
  "${client[@]}" "${root}/native/vendor/cJSON/cJSON.c" "${curl_libs[@]}" -lz -lm -lpthread -o "${out}/test_update"

python3 "${root}/tests/update_fixture.py" make "${fixtures}"
port=""
serve() {   # starts a server on the fixtures and leaves its port in `port`; arguments are its certificate and key
    local log="${fixtures}/server-$#.out"
    python3 "${root}/tests/update_fixture.py" serve "${fixtures}" "$@" > "${log}" 2>&1 &
    pids+=($!)
    port=""
    for _ in $(seq 100); do
        port="$(sed -n 's/^PORT //p' "${log}")"
        [[ -n "${port}" ]] && break
        sleep 0.1
    done
    [[ -n "${port}" ]] || { cat "${log}" >&2; echo "The fixture server did not start." >&2; exit 1; }
}
serve
export TUBE_UPDATE_TEST_DIR="${fixtures}" TUBE_UPDATE_TEST_URL="http://127.0.0.1:${port}"
if command -v openssl > /dev/null; then
    openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj "/CN=127.0.0.1" -addext "subjectAltName=IP:127.0.0.1" \
      -keyout "${fixtures}/key.pem" -out "${fixtures}/cert.pem" > /dev/null 2>&1
    serve "${fixtures}/cert.pem" "${fixtures}/key.pem"
    export TUBE_UPDATE_TEST_TLS_URL="https://127.0.0.1:${port}" TUBE_UPDATE_TEST_CA="${fixtures}/cert.pem"
fi
ASAN_OPTIONS=detect_leaks=1 "${out}/test_update"

# The console build reaches folders through EVO's wrappers: the same cases through that branch.
cc "${flags[@]}" -DEVO_TARGET_PS5=1 -I"${evo}/include" "${curl_cflags[@]}" "${root}/tests/test_update.c" "${engine[@]}" \
  "${root}/native/tube_update_http.c" "${client[@]}" "${evo}/src/evo_readdir.c" "${evo}/src/evo_data_path.c" \
  "${root}/native/vendor/cJSON/cJSON.c" "${curl_libs[@]}" -lz -lm -lpthread -o "${out}/test_update_evo"
ASAN_OPTIONS=detect_leaks=1 "${out}/test_update_evo"

# About's update rows, the setting and the notes page, through the shipping provider.
cc "${flags[@]}" -I"${evo}/include" -I"${evo}/addons/include" "${root}/tests/test_update_provider.c" "${root}/native/tube_provider.c" \
  "${root}/native/tube_remote.c" "${root}/native/tube_player_extra.c" "${root}/native/tube_backup.c" "${root}/native/tube_update_app.c" "${engine[@]}" \
  "${evo}/addons/src/cJSON.c" "${evo}/addons/src/evo_provider_mgr.c" "${evo}/src/evo_data_path.c" -lz -lm -lpthread -o "${out}/test_update_provider"
ASAN_OPTIONS=detect_leaks=1 "${out}/test_update_provider" "${fixtures}/www/good.zip"
