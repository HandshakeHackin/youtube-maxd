#!/usr/bin/env bash
# Build the service bundle standalone/build/tube_js.bundle.js: youtubei.js
# (platform-neutral entry), the companion's library modules and the shims, as
# one classic script. Needs Node with companion/innertube/node_modules
# (npm ci --ignore-scripts). TUBE_JS_MINIFY=1 minifies. Prints the bundle's path.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(dirname "$here")
out=$here/build
modules=$root/companion/innertube/node_modules
mkdir -p "$out"
version=$(sed -n 's/^__version__ = "\(.*\)"$/\1/p' "$root/companion/prosperotube/__init__.py")
"$modules/.bin/esbuild" "$here/js/service.js" --bundle --format=iife --platform=neutral --target=es2022 \
  --main-fields=module,main --conditions=module,import --define:global=globalThis \
  "--define:__TUBE_VERSION__=\"${version:-0.0.0}\"" \
  "--alias:youtubei.js=$modules/youtubei.js/dist/src/platform/lib.js" \
  "--alias:youtubei-utils=$modules/youtubei.js/dist/src/utils/Utils.js" \
  "--alias:node:crypto=$here/js/node_crypto.js" \
  --legal-comments=inline --log-level=warning ${TUBE_JS_MINIFY:+--minify} --outfile="$out/tube_js.bundle.js" >&2
echo "$out/tube_js.bundle.js"
