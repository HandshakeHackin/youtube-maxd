#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
destination="${root}/build/tests/speed-switch"
mkdir -p "$destination"
python3 - "$work" "$destination" <<'PY'
from pathlib import Path
import sys
source=(Path(sys.argv[1])/'projects/evoplayer/core/src/services/PlaybackController.cpp').read_text()
start=source.index('// ProsperoTube speed reopen ')
end=source.index('// End ProsperoTube speed reopen.',start)
(Path(sys.argv[2])/'speed-switch.inc').write_text(source[start:end])
start=source.index('    if (m_source.isProvider()',source.index('void PlaybackController::stopPlayback()'))
end=source.index('    m_playbackFsm.postEvent',start)
(Path(sys.argv[2])/'stop-progress.inc').write_text(source[start:end])
PY
g++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer -I"$destination" "$root/tests/test_speed_switch.cpp" -o "$destination/test"
ASAN_OPTIONS=detect_leaks=1 "$destination/test"
