#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "${root}/build/tests"
flags=(-D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -I"${root}/native")
# The screens include the same headers from C++, so each case runs as both.
for name in test_qr test_qr_page; do
  cc -std=c11 "${flags[@]}" "${root}/tests/${name}.c" -o "${root}/build/tests/${name}"
  ASAN_OPTIONS=detect_leaks=1 "${root}/build/tests/${name}"
  g++ -std=c++17 "${flags[@]}" -x c++ "${root}/tests/${name}.c" -o "${root}/build/tests/${name}_cxx"
  ASAN_OPTIONS=detect_leaks=1 "${root}/build/tests/${name}_cxx"
done
