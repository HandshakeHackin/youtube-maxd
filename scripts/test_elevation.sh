#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "${root}/build/tests"
for case in elevation elevation_listing; do
    c++ -std=c++20 -O1 -g -Wall -Wextra -Werror \
        "${root}/tests/test_${case}.cpp" -o "${root}/build/tests/test_${case}"
    "${root}/build/tests/test_${case}"
done
printf '%s\n' 'Lapy existing/resident/helper and mounted-data listing regressions passed.'
