#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$root/build/tests"
cc -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$root/native" -I"$root/native/vendor/cJSON" "$root/tests/test_maxd_feed_policy.c" \
  "$root/native/vendor/cJSON/cJSON.c" -lm -o "$root/build/tests/test-maxd-feed-policy"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$root/build/tests/test-maxd-feed-policy"
