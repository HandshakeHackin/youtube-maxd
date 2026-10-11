#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "${root}/build/tests"
flags=(-D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -I"${root}/native")
cc -std=c11 "${flags[@]}" "${root}/tests/test_qr.c" -o "${root}/build/tests/test_qr"
ASAN_OPTIONS=detect_leaks=1 "${root}/build/tests/test_qr"
# The screens include the same header from C++.
g++ -std=c++17 "${flags[@]}" -x c++ "${root}/tests/test_qr.c" -o "${root}/build/tests/test_qr_cxx"
ASAN_OPTIONS=detect_leaks=1 "${root}/build/tests/test_qr_cxx"
