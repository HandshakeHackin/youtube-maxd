#!/bin/bash
# Offline capture lifecycle test. No console or microphone is accessed.
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
binary=$(mktemp "${TMPDIR:-/tmp}/prosperotube-microphone.XXXXXX")
trap 'rm -f -- "$binary"' EXIT
"${CC:-cc}" -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror \
    -g -fsanitize=address,undefined -pthread -I"$root/native" \
    "$root/native/tube_microphone.c" "$root/tests/native/test_microphone.c" \
    -Wl,--wrap=clock_gettime -Wl,--wrap=pthread_create -o "$binary"
"$binary"
