#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="${PROSPEROTUBE_NATIVE_WORK:-${HOME}/.cache/prosperotube/native}"
destination="${root}/build/tests/sponsor-review"
python3 "${root}/scripts/extract_sponsor_review_test.py" "${work}/projects/evoplayer" "${destination}"
g++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"${destination}" "${root}/tests/test_sponsor_review.cpp" -o "${destination}/test"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "${destination}/test"
