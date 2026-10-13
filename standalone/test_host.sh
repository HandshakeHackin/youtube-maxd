#!/usr/bin/env bash
# Offline tests of the embedded companion. Nothing here touches the network:
# every upstream request is answered from standalone/fixtures.
#   1. engine self-test (shims, format policy, manifest, request models, one parser path)
#   2. HTTP service test: requests against `tube_js serve` on 127.0.0.1
#   3. accounts: sign-in, the credential file, account lists and changes (test/account_test.py)
#   4. all again under AddressSanitizer + UBSan with leak detection (TUBE_JS_SANITIZE=0 skips)
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
export TUBE_JS_SANITIZE=${TUBE_JS_SANITIZE:-1}
[ "$TUBE_JS_SANITIZE" = 0 ] && unset TUBE_JS_SANITIZE
"$here/build_host.sh" >/dev/null
export TUBE_JS_FIXTURES="$here/fixtures"
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=0 UBSAN_OPTIONS=print_stacktrace=1
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT

run() {
  local binary=$1 label=$2 result
  result=$(TUBE_JS_CACHE_DIR="$scratch/$label" "$binary" selftest)
  echo "$label self-test: $result"
  case "$result" in *'"ok":true'*'"failed":[]'*) ;; *) echo "standalone self-test FAILED ($label)" >&2; exit 1 ;; esac
  python3 "$here/test/service_test.py" "$binary"
  python3 "$here/test/account_test.py" "$binary"
}
run "$here/build/tube_js" plain
if [ -n "${TUBE_JS_SANITIZE:-}" ]; then run "$here/build/asan/tube_js" sanitized; fi
echo "standalone tests passed"
