#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
destination="${root}/build/tests/player-extra-overlay"
mkdir -p "$destination"
python3 - "$work" "$destination" <<'PY'
from pathlib import Path
import sys
evo = Path(sys.argv[1]) / 'projects/evoplayer'
output = Path(sys.argv[2])
screen = (evo / 'core/src/screens/PlayerScreen.cpp').read_text()
picker = (evo / 'core/src/screens/AudioTrackPickerScreen.cpp').read_text()
for source, tag, name in ((screen, 'player exit', 'exit.inc'), (screen, 'D-pad actions', 'dpad.inc'),
                          (picker, 'extra option lists', 'lists.inc'), (picker, 'extra option rows', 'rows.inc'),
                          (picker, 'extra option selection', 'selection.inc'), (picker, 'extra option back', 'back.inc')):
    start = source.index('// ProsperoTube ' + tag)
    end = source.index('// End ProsperoTube ' + tag, start)
    (output / name).write_text(source[start:end])
# The blocks must sit where the player consults them: exit ahead of the stop
# prompt, D-pad actions ahead of seeking, captions and the live-key filter.
assert screen.index('// ProsperoTube player exit') < screen.index('screenMgr->navigateTo(ScreenId::ExitConfirm);')
dpad = screen.index('// ProsperoTube D-pad actions')
for later in ('    /* Live TV has no timeline', 'playback->moveScrub(-playback->getSeekIntervalSeconds());', 'prospero_subtitle_toggle();'):
    assert dpad < screen.index(later)
assert screen.index('playback->updateScrubHold(held);') < dpad
PY
flags=(-O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer
       -I"${root}/native" -I"${root}/native/vendor/cJSON")
cc -std=c11 -D_DEFAULT_SOURCE "${flags[@]}" -c "${root}/native/tube_player_extra.c" -o "$destination/tube_player_extra.o"
cc -std=c11 "${flags[@]}" -c "${root}/native/vendor/cJSON/cJSON.c" -o "$destination/cJSON.o"
g++ -std=c++17 "${flags[@]}" -I"$destination" "${root}/tests/test_player_extra_overlay.cpp" \
    "$destination/tube_player_extra.o" "$destination/cJSON.o" -lm -o "$destination/test"
ASAN_OPTIONS=detect_leaks=1 "$destination/test"
