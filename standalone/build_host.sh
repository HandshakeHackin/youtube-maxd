#!/usr/bin/env bash
# Build the host executable standalone/build/tube_js and its service bundle.
# Needs clang, libcurl development files, and Node with
# companion/innertube/node_modules installed (npm ci --ignore-scripts).
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(dirname "$here")
out=$here/build
modules=$root/companion/innertube/node_modules
CC=${CC:-clang}
mkdir -p "$out"

quickjs=$("$here/fetch_quickjs.sh")

# The engine: one C11 translation unit, no JIT, no `std`/`os` modules.
if [ ! -f "$out/quickjs.o" ] || [ "$quickjs/quickjs-amalgam.c" -nt "$out/quickjs.o" ]; then
  "$CC" -std=c11 -O2 -g0 -D_GNU_SOURCE -ffunction-sections -fdata-sections -w \
    -c "$quickjs/quickjs-amalgam.c" -o "$out/quickjs.o"
fi

# The service: youtubei.js (platform-neutral entry), the companion's library
# modules and the shims, as one classic script.
"$modules/.bin/esbuild" "$here/js/service.js" --bundle --format=iife --platform=neutral --target=es2022 \
  --main-fields=module,main --conditions=module,import --define:global=globalThis \
  "--alias:youtubei.js=$modules/youtubei.js/dist/src/platform/lib.js" \
  "--alias:youtubei-utils=$modules/youtubei.js/dist/src/utils/Utils.js" \
  "--alias:node:crypto=$here/js/node_crypto.js" \
  --legal-comments=inline --log-level=warning ${TUBE_JS_MINIFY:+--minify} --outfile="$out/tube_js.bundle.js"

"$CC" -std=c11 -O2 -Wall -Wextra -D_GNU_SOURCE -I"$quickjs" $(pkg-config --cflags libcurl) \
  "$here/src/tube_js.c" "$out/quickjs.o" -o "$out/tube_js" \
  -Wl,--gc-sections $(pkg-config --libs libcurl) -lm -lpthread

ls -l "$out/tube_js" "$out/tube_js.bundle.js" | awk '{ print $5, $9 }'
