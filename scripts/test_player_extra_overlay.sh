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
                          (picker, 'extra option selection', 'selection.inc'), (picker, 'extra option back', 'back.inc'),
                          (picker, 'picker entry', 'entry.inc'), (picker, 'video action lists', 'menu_lists.inc'),
                          (picker, 'video action rows', 'menu_rows.inc'), (picker, 'player buttons filter', 'filter.inc'),
                          (picker, 'video action selection', 'menu_selection.inc'), (picker, 'video action back', 'menu_back.inc'),
                          (picker, 'video action list refresh', 'menu_refresh.inc'),
                          (screen, 'chapter notification', 'chapter.inc'), (screen, 'error fixer: decoder failure', 'decoder.inc'),
                          (screen, 'error fixer: long buffering', 'buffering.inc')):
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
# In-player actions: their lists come before every other row, the button filter
# sees the complete list, and the fixer acts before the player gives up.
host = (evo / 'core/src/screens/ProviderHostScreen.cpp').read_text()
app = (evo / 'core/src/Application.cpp').read_text()
browser = (evo / 'ui_rml/src/evo_rmlui_provider.cpp').read_text()
refresh = picker.index('void AudioTrackPickerScreen::refreshTracks() {')
navigate = picker.index('void AudioTrackPickerScreen::navigate(int delta) {')
assert picker.index('// End ProsperoTube extra option lists.') < picker.index('// ProsperoTube video action lists') < \
    picker.index('// ProsperoTube video action rows') < picker.index('const auto variants = playback->getVideoVariants();')
assert picker.index('// End ProsperoTube extra option rows.') < picker.index('// ProsperoTube player buttons filter') < navigate
assert picker.rindex('m_tracks.push_back', refresh, navigate) < picker.index('// ProsperoTube player buttons filter')
assert picker.index('// ProsperoTube video action selection') < picker.index('// ProsperoTube extra option selection')
assert picker.index('// ProsperoTube video action back') < picker.index('// ProsperoTube extra option back')
assert picker.count('tube_player_menu_close();   /* the browser') == 1 and picker.count('tube_player_remember_height(v.height);') == 1
assert screen.index('if (evo_pb_decode_fatal()) {') < screen.index('// ProsperoTube error fixer: decoder failure') < \
    screen.index('toast("AUDIO STOPPED"')
assert screen.index('const bool stalled = readerStalled();') < screen.index('// ProsperoTube error fixer: long buffering') < \
    screen.index('// ProsperoTube controls visibility.')
assert screen.count('tube_extra_dim_percent() > 0') == 2
assert host.count('tube_player_take_launch(&queued) || tube_take_next(&queued)') == 1
assert host.count('tube_error_fix_relaxed() ? tube_error_fix_choice(choices, count)') == 1
assert host.index('// ProsperoTube player browse request') < host.index('    evo_provider_selection_t sel;\n    evo_provider_item_t queued;')
assert browser.count('void EvoRmlProviderHost::PushFolder(const char* parent, const char* title)') == 1
# Dimming: input is taken before any screen sees it, the level is decided once a
# frame ahead of the redraw decision, and each surface has its black layer.
assert app.index('// ProsperoTube screen dimming input.') < app.index('// ProsperoTube screen dimming: one idle clock') < \
    app.index('|| (jb_repaint > 0) || tubeDimChanged;')
for name, marker in (('assets/rml/playback.rml', '<div id="tube-dim"></div>\n</body>'), ('assets/rml/youtube.rml', '<div id="tube-dim"></div>\n</body>'),
                     ('assets/rml/playback.rcss', '#tube-dim.shown {'), ('assets/rml/youtube.rcss', '#tube-dim.shown {'),
                     ('ui_kit/src/evo_hui_osd.cpp', '// ProsperoTube screen dimming HUI.'),
                     ('ui_rml/src/evo_rmlui_app.cpp', 'dim->SetClass("shown", percent > 0);'),
                     ('ui_rml/src/evo_rmlui_provider.cpp', 'dim->SetClass("shown", percent > 0);')):
    assert (evo / name).read_text().count(marker) == 1, name
PY
flags=(-O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer
       -I"${root}/native" -I"${root}/native/vendor/cJSON" -I"${work}/projects/evoplayer/addons/include")
cc -std=c11 -D_DEFAULT_SOURCE "${flags[@]}" -c "${root}/native/tube_player_extra.c" -o "$destination/tube_player_extra.o"
cc -std=c11 "${flags[@]}" -c "${root}/native/tube_background_play.c" -o "$destination/tube_background_play.o"
cc -std=c11 "${flags[@]}" -c "${root}/native/vendor/cJSON/cJSON.c" -o "$destination/cJSON.o"
g++ -std=c++17 "${flags[@]}" -I"$destination" "${root}/tests/test_player_extra_overlay.cpp" \
    "$destination/tube_player_extra.o" "$destination/tube_background_play.o" "$destination/cJSON.o" -lm -o "$destination/test"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "$destination/test"
