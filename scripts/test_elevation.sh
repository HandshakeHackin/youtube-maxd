#!/usr/bin/env bash
# The Lapy client and helper build checks, as ProsperoEden's tools/check-elevation-client.sh
# runs them: the client under AddressSanitizer and UBSan, then the helper verification.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "${root}/build/tests"
for case in elevation elevation_listing; do
    c++ -std=c++20 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
        -fno-sanitize-recover=all \
        "${root}/tests/test_${case}.cpp" -o "${root}/build/tests/test_${case}"
    "${root}/build/tests/test_${case}"
done
python3 -B "${root}/tests/test_lapy_helper.py"
printf '%s\n' 'Lapy existing/resident/helper, mounted-data listing and helper verification regressions passed.'
