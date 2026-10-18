#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
temporary="$(mktemp -d)"
trap 'rm -rf "$temporary"' EXIT
python3 - "$root/scripts" "$temporary/maxd_voice_model.h" <<'PY'
import sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
from bootstrap_local_voice import header
header(Path(sys.argv[2]))
PY
"${CC:-cc}" -std=c11 -D_POSIX_C_SOURCE=200809L -DTUBE_LOCAL_VOICE=1 -Wall -Wextra -Werror \
    -g -fsanitize=address,undefined -I"$temporary" -I"$root/native" -I"$root/native/maxd" \
    -I"$root/native/vendor/cJSON" -I"$root/third_party/evo/projects/evoplayer/addons/include" \
    "$root/native/tube_voice.c" "$root/native/vendor/cJSON/cJSON.c" \
    "$root/tests/native/test_local_voice_driver.c" -lm -o "$temporary/test"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$temporary/test"
