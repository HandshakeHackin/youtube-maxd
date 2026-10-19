#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$root/build/tests"
flags=(-std=c11 -D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -I"$root/native" -I"$root/native/vendor/cJSON")
cc "${flags[@]}" "$root/tests/test_maxd_phone_remote.c" "$root/native/maxd/phone_remote.c" "$root/native/maxd/diagnostics.c" "$root/native/vendor/cJSON/cJSON.c" -lm -o "$root/build/tests/maxd-phone-remote"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$root/build/tests/maxd-phone-remote"
cc "${flags[@]}" "$root/tests/test_maxd_diagnostics.c" "$root/native/maxd/diagnostics.c" "$root/native/vendor/cJSON/cJSON.c" -lm -o "$root/build/tests/maxd-diagnostics"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$root/build/tests/maxd-diagnostics"
python3 "$root/scripts/generate_phone_remote.py" --check
node --test "$root/tests/test_maxd_phone_page.mjs"
