#!/usr/bin/env bash
# Circle on the main menu and on typed search results: the generated blocks of
# the provider screen, compiled as they ship, and where they sit in it.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
destination="${root}/build/tests/ui-exit"
mkdir -p "$destination"
python3 - "$work" "$destination" <<'PY'
from pathlib import Path
import sys
evo = Path(sys.argv[1]) / 'projects/evoplayer'
output = Path(sys.argv[2])
host = (evo / 'core/src/screens/ProviderHostScreen.cpp').read_text()
browser = (evo / 'ui_rml/src/evo_rmlui_provider.cpp').read_text()
for tag, name in (('search exit', 'search_exit.inc'), ('app exit', 'app_exit.inc')):
    start = host.index('// ProsperoTube ' + tag)
    end = host.index('// End ProsperoTube ' + tag, start)
    (output / name).write_text(host[start:end])
# Search results are asked about before the browser takes Back; the app exit
# belongs to the main menu alone: after Back was declined, ahead of the setup screen.
back = host.index('        if (evo_rmlui_provider_key(EvoRmlProviderHost::KeyBack) != 0)\n            return true;')
assert host.index('evo_feedback(EVO_FB_CANCEL);\n        /*\n         * The exit contract.') < host.index('// ProsperoTube search exit') < back
assert back < host.index('if (m_providerId == "youtube") {\n            // ProsperoTube app exit') < host.index('// End ProsperoTube app exit.')
assert host.index('// End ProsperoTube app exit.') < host.index('            evo_rmlui_provider_show_setup(); return true;\n        }\n        if (sm) sm->navigateTo(ScreenId::MainMenu);')
assert host.count('tube_app_exit_press(') == 1 and host.count('tube_search_exit_press(') == 1 and host.count('requestSoftClose()') == 2
assert browser.count('bool SearchResultsShown() const') == 0 and browser.count('EvoRmlProviderHost::Instance().SearchResultsShown() ? 1 : 0') == 1
# The theme or scheme lends its accent to every part of the player controls: the
# palette is replaced once, ahead of all drawing.
hud = (evo / 'ui_kit/src/evo_hui_osd.cpp').read_text()
draw = hud.index('void OsdScreen::draw(DrawList &list, const Context &evo_ctx) const')
assert draw < hud.index('// ProsperoTube colour scheme HUI.') < hud.index('const Context ctx{evo_ctx.fonts, evo_ctx.textures, tube_palette, evo_ctx.time};') < \
    hud.index('        draw_bottom(list, ctx);')
assert hud.count('evo_ctx') == 5 and hud.count('tube_palette.accent = Color::rgb(g_tube_schemes[tube_scheme].accent);') == 1
# The interface lists are levels of their own; their values act in place.
for level in ('settings:ui', 'settings:menu', 'settings:ui:scale', 'settings:ui:grid', 'settings:ui:title-lines', 'settings:ui:title-scroll',
              'settings:ui:scroll-speed', 'settings:ui:subtitle-lines', 'settings:ui:rounded', 'settings:ui:scheme', 'settings:ui:exit',
              'settings:ui:search-exit'):
    assert browser.count(f'id != "{level}" && ') == 1, level
assert browser.count('!(id.rfind("settings:menu:", 0) == 0 && id != "settings:menu:reset" && id.find(\':\', 14) == std::string::npos) && ') == 1
PY
g++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"$destination" -I"${root}/native" "${root}/tests/test_ui_exit.cpp" -o "$destination/test"
ASAN_OPTIONS=detect_leaks=1 "$destination/test"
