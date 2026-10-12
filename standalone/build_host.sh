#!/usr/bin/env bash
# Build the host executable standalone/build/tube_js and its service bundle.
# Needs clang, libcurl development files, and Node with
# companion/innertube/node_modules installed (npm ci --ignore-scripts).
#   TUBE_JS_SANITIZE=1  also builds build/asan/tube_js (AddressSanitizer + UBSan)
#   TUBE_JS_MINIFY=1    minifies the bundle
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(dirname "$here")
out=$here/build
modules=$root/companion/innertube/node_modules
CC=${CC:-clang}
mkdir -p "$out"

quickjs=$("$here/fetch_quickjs.sh")
# The native layer (HTTPS client, clock, timers, random, hashes) is the one the
# console build uses: standalone/ps5.
sources=("$here/src/main.c" "$here/src/tube_engine.c" "$here/src/tube_listener.c"
         "$here/ps5/tube_js_http.c" "$here/ps5/tube_js_platform.c")
warnings=(-std=c11 -Wall -Wextra -D_GNU_SOURCE -I"$quickjs" -I"$here/include" -I"$here/ps5")
read -r -a curl_cflags <<< "$(pkg-config --cflags libcurl)"
read -r -a curl_libs <<< "$(pkg-config --libs libcurl)"

# The service: youtubei.js (platform-neutral entry), the companion's library
# modules and the shims, as one classic script.
version=$(sed -n 's/^__version__ = "\(.*\)"$/\1/p' "$root/companion/prosperotube/__init__.py")
"$modules/.bin/esbuild" "$here/js/service.js" --bundle --format=iife --platform=neutral --target=es2022 \
  --main-fields=module,main --conditions=module,import --define:global=globalThis \
  "--define:__TUBE_VERSION__=\"${version:-0.0.0}\"" \
  "--alias:youtubei.js=$modules/youtubei.js/dist/src/platform/lib.js" \
  "--alias:youtubei-utils=$modules/youtubei.js/dist/src/utils/Utils.js" \
  "--alias:node:crypto=$here/js/node_crypto.js" \
  --legal-comments=inline --log-level=warning ${TUBE_JS_MINIFY:+--minify} --outfile="$out/tube_js.bundle.js"

# The engine: one C11 translation unit, no JIT, no `std`/`os` modules.
if [ ! -f "$out/quickjs.o" ] || [ "$quickjs/quickjs-amalgam.c" -nt "$out/quickjs.o" ]; then
  "$CC" -std=c11 -O2 -g0 -D_GNU_SOURCE -ffunction-sections -fdata-sections -w \
    -c "$quickjs/quickjs-amalgam.c" -o "$out/quickjs.o"
fi
"$CC" "${warnings[@]}" -O2 "${curl_cflags[@]}" "${sources[@]}" "$out/quickjs.o" -o "$out/tube_js" \
  -Wl,--gc-sections "${curl_libs[@]}" -lm -lpthread

if [ -n "${TUBE_JS_SANITIZE:-}" ]; then
  mkdir -p "$out/asan"
  sanitize=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined -g -O1)
  if [ ! -f "$out/asan/quickjs.o" ] || [ "$quickjs/quickjs-amalgam.c" -nt "$out/asan/quickjs.o" ]; then
    "$CC" -std=c11 -D_GNU_SOURCE -w "${sanitize[@]}" -c "$quickjs/quickjs-amalgam.c" -o "$out/asan/quickjs.o"
  fi
  "$CC" "${warnings[@]}" "${sanitize[@]}" "${curl_cflags[@]}" "${sources[@]}" "$out/asan/quickjs.o" -o "$out/asan/tube_js" \
    "${curl_libs[@]}" -lm -lpthread
  cp "$out/tube_js.bundle.js" "$out/asan/tube_js.bundle.js"
fi

ls -l "$out/tube_js" "$out/tube_js.bundle.js" | awk '{ print $5, $9 }'
