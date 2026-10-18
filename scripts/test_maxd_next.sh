#!/usr/bin/env bash
# Local policy and real loopback HTTP checks; no upstream account or YouTube calls.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$root/build/tests"
flags=(-std=c11 -D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -I"$root/native" -I"$root/native/vendor/cJSON")
cc "${flags[@]}" "$root/tests/test_maxd_next_policy.c" "$root/native/vendor/cJSON/cJSON.c" "$root/native/tube_background_play.c" "$root/native/tube_video_destination.c" -lm -o "$root/build/tests/maxd-next-policy"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$root/build/tests/maxd-next-policy"
cc "${flags[@]}" "$root/tests/test_maxd_party.c" "$root/native/maxd/party_server.c" "$root/native/vendor/cJSON/cJSON.c" -o "$root/build/tests/maxd-party"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$root/build/tests/maxd-party"
node --test "$root/tests/test_maxd_party_page.mjs"
