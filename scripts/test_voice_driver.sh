#!/bin/bash
# Deterministic native upload/job test; no console or network is accessed.
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
binary=$(mktemp "${TMPDIR:-/tmp}/prosperotube-voice.XXXXXX")
trap 'rm -f -- "$binary"' EXIT
"${CC:-cc}" -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror \
    -g -fsanitize=address,undefined -I"$root/native" -I"$root/native/vendor/cJSON" \
    -I"$root/third_party/evo/projects/evoplayer/addons/include" \
    "$root/native/tube_voice.c" "$root/native/vendor/cJSON/cJSON.c" \
    "$root/tests/native/test_voice_driver.c" -lm -o "$binary"
"$binary"
