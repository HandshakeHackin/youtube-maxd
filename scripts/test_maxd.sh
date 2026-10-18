#!/usr/bin/env bash
# Fork integration checks; no console or network required.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$root/build/tests"
cc -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$root/native/maxd" -I"$root/third_party/evo/projects/evoplayer/ui/include" \
  "$root/native/maxd/evo_feedback.c" "$root/native/maxd/lightbar.c" "$root/tests/test_maxd_feedback.c" \
  -lm -o "$root/build/tests/test-maxd-feedback"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "$root/build/tests/test-maxd-feedback"
python3 - "$root" <<'PY'
import json,pathlib,sys
root=pathlib.Path(sys.argv[1]);sys.path.insert(0,str(root/'scripts'))
from release_version import identity
title,content,version=identity()
assert (title,content,version)==('PPSA99991','01.003.000','1.003.000')
assert json.loads((root/'UPSTREAM.json').read_text())['title_id']==title
source=(root/'native/tube_update_app.c').read_text()
assert '#ifndef TUBE_MAXD_ENABLE_SELF_UPDATE\n    return;' in source
assert '#define TUBE_UPDATE_TITLE "PPSA99991"' in (root/'native/tube_update.h').read_text()
assert '/data/prosperotube' not in (root/'native/elevation/tube_jailbreak.cpp').read_text()
print("Max'd identity, storage and updater isolation: passed")
PY
