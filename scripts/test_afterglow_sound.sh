#!/usr/bin/env bash
# The Afterglow interface's sound and vibration on the PC
# (tests/afterglow/sound/sound_test.cpp): native/afterglow/sound against the
# original procedural ProsperoTube sound assets, with a recorder in place of the console's
# port and pad. Which recording serves which cue, the rules of
# docs/afterglow/sound-design.md, vibration, and what a failing device leaves
# behind. Built twice: with the sanitizers, and plain with every allocation
# counted (a cue must not allocate). Nothing is played and nothing is heard.
# Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
kit="$(python3 "${root}/scripts/ui_kit.py")"
destination="${root}/build/tests/afterglow-sound"
mkdir -p "${destination}"
sources=("${kit}/src/audio/cues.cpp" "${kit}/src/audio/mixer.cpp" "${kit}/src/audio/wav.cpp" "${kit}/src/core/save_file.cpp")
while IFS= read -r -d '' source; do sources+=("${source}"); done < <(find "${root}/native/afterglow/sound" -maxdepth 1 -name '*.cpp' -print0 | sort -z)
sources+=("${root}/tests/afterglow/sound/sound_test.cpp")
flags=(-std=c++20 -Wall -Wextra -Werror -I"${root}/native" -I"${kit}/src" -I"${kit}")
g++ "${flags[@]}" -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer "${sources[@]}" -lm -o "${destination}/sound_test"
g++ "${flags[@]}" -O2 -Wno-mismatched-new-delete -DAFTERGLOW_COUNT_ALLOCATIONS "${sources[@]}" -lm -o "${destination}/sound_test_plain"
ASAN_OPTIONS=detect_leaks=1 "${destination}/sound_test" "${root}"
"${destination}/sound_test_plain" "${root}"
# Every original asset the vocabulary names is present in package preparation.
python3 - "${root}" "${kit}" "${destination}/sound_test" <<'PY'
import pathlib
import subprocess
import sys

root, kit, test = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), sys.argv[3]
sys.path.insert(0, str(root / "scripts"))
import afterglow_overlay

shipped = {name for name, _ in afterglow_overlay.sound_files(root, kit)}
takes = subprocess.check_output([test, "--takes"], text=True).split()
missing = []
for take in takes:
    group, cue = take.split("/")
    files=sorted((root / "native/afterglow/sound/recordings" / group).glob(cue + "_[0-9][0-9].wav"))
    if not files:
        missing.append(take)
    for file in files:
        if f"{group}/{file.name}" not in shipped:
            missing.append(f"{group}/{file.name}")
if missing:
    sys.exit("the package would not ship: " + ", ".join(missing))
if not shipped:
    sys.exit("the package ships no recording at all")
assert len(shipped)==34 and all(name.startswith('prosperotube/') for name in shipped)
print(f"PASS: the package ships the {len(shipped)} original assets the complete vocabulary can play")
PY

python3 "${root}/scripts/generate_afterglow_sounds.py" --check
python3 "${root}/scripts/test_afterglow_sound_assets.py"
