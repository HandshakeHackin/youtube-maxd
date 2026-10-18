#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "${root}/build/tests"
# The committed header must be what CHANGELOG.md generates, and the build and
# packaging scripts must name the same version.
python3 "${root}/scripts/generate_about.py" --check
flags=(-D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -I"${root}/native")
cc -std=c11 "${flags[@]}" "${root}/tests/test_about.c" -o "${root}/build/tests/test_about"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "${root}/build/tests/test_about" "${root}/THIRD_PARTY_NOTICES.md" "${root}/CHANGELOG.md"
# The console build takes the version from the packaged VERSION file instead.
cc -std=c11 "${flags[@]}" -DEVO_PLAYER_VERSION="\"$(sed -n 's/^## \([0-9][0-9.]*\)$/\1/p' "${root}/CHANGELOG.md" | head -n 1)\"" \
  "${root}/tests/test_about.c" -o "${root}/build/tests/test_about_packaged"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "${root}/build/tests/test_about_packaged" "${root}/THIRD_PARTY_NOTICES.md" "${root}/CHANGELOG.md"
