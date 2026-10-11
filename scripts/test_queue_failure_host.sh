#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
destination="${root}/build/tests/queue-failure"
python3 "${root}/scripts/extract_queue_failure_test.py" "${work}/projects/evoplayer" "$destination"
g++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"$destination" "${root}/tests/test_queue_failure_host.cpp" -o "${root}/build/tests/test_queue_failure_host"
ASAN_OPTIONS=detect_leaks=1 "${root}/build/tests/test_queue_failure_host"
