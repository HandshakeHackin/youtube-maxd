#!/usr/bin/env bash
# Offline test: engine, shims, the session and one parser path against canned
# responses. Never touches the network. SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
[ -x "$here/build/tube_js" ] || "$here/build_host.sh" >/dev/null
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
result=$(TUBE_JS_FIXTURES="$here/test/fixtures" TUBE_JS_CACHE_DIR="$scratch" "$here/build/tube_js" selftest)
echo "$result"
case "$result" in *'"ok":true'*'"failed":[]'*) ;; *) echo "standalone self-test FAILED" >&2; exit 1 ;; esac
version=$(TUBE_JS_FIXTURES="$here/test/fixtures" "$here/build/tube_js" version)
case "$version" in *'"engine":"quickjs-ng '*) ;; *) echo "version check FAILED" >&2; exit 1 ;; esac
echo "standalone self-test passed"
