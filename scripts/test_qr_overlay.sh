#!/usr/bin/env bash
# QR display on the real RmlUi host build. The overlay is applied to copies of
# the generated provider screen sources and layout, so the shared generated
# tree is left as prepare_native.py made it.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="${PROSPEROTUBE_NATIVE_WORK:-${HOME}/.cache/prosperotube/native}"
evo="${work}/projects/evoplayer"
source "${root}/scripts/rmlui_host.sh"
destination="${root}/build/tests/qr-ui"
rm -rf "${destination}"
mkdir -p "${destination}/objects"
python3 - "${root}" "${evo}" "${destination}" <<'PY'
from pathlib import Path
import shutil
import sys
root, evo, destination = (Path(value) for value in sys.argv[1:4])
sys.path.insert(0, str(root / "scripts"))
from qr_overlay import apply_qr_overlay
player = "projects/evoplayer/"
names = ("ui_rml/include/evo_rmlui_provider.h", "ui_rml/src/evo_rmlui_provider.cpp", "assets/rml/youtube.rml", "assets/rml/youtube.rcss")
for name in names:
    target = destination / player / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(evo / name, target)
(destination / player / "addons/include").mkdir(parents=True)


def replace(path, old, new, count=1):
    file = destination / path
    source = file.read_text()
    actual = source.count(old)
    if actual != count:
        raise RuntimeError(f"Overlay mismatch: {path}: expected {count} matches, found {actual}")
    file.write_text(source.replace(old, new))


if "UpdateQr" in (evo / names[1]).read_text():
    # prepare_native.py already applies the overlay: test the generated files.
    for name in ("tube_qr.h", "tube_qr_page.h"):
        shutil.copyfile(root / "native" / name, destination / player / "addons/include" / name)
    # A code on screen is never dimmed while the browser shows it.
    app = (evo / "core/src/Application.cpp").read_text()
    assert app.count("(current == ScreenId::EmbyBrowse && evo_rmlui_provider_qr_shown())) context = TUBE_DIM_NONE;") == 1
else:
    apply_qr_overlay(replace, player, destination, root)
screen = (destination / player / names[1]).read_text()
layout = (destination / player / names[2]).read_text()
style = (destination / player / names[3]).read_text()
assert screen.count("void EvoRmlProviderHost::UpdateQr()") == 1 and screen.count("    UpdateQr();\n}") == 1
assert layout.count('id="tube-qr-box"') == 1 and layout.count('id="tube-link-footer"') == 1
assert layout.index('id="tube-discussion-text"') < layout.index('id="tube-qr"') < layout.index('id="tube-footer"')
assert style.count("#tube-qr-symbol div") == 1
PY
copy="${destination}/projects/evoplayer"
for source in "${root}/native/tube_provider.c" "${root}/native/tube_remote.c" "${root}/native/tube_player_extra.c" "${root}/native/tube_backup.c" "${evo}/addons/src/cJSON.c" "${evo}/addons/src/evo_provider_mgr.c" "${evo}/src/evo_data_path.c"; do
  cc -std=c11 -D_DEFAULT_SOURCE -Dclock_gettime=tube_test_clock_gettime -O1 -g -ffunction-sections -fdata-sections \
    -I"${root}/native" -I"${evo}/include" -I"${evo}/addons/include" -c "${source}" \
    -o "${destination}/objects/$(basename "${source}" .c).o"
done
g++ -O1 -g -std=c++17 -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -I"${copy}/ui_rml/include" -I"${copy}/ui_rml/src" -I"${copy}/addons/include" -I"${root}/native" \
  -I"${rmlui_include}" -I"${evo}/ui_rml/include" -I"${evo}/ui_rml/src" -I"${evo}/addons/include" -I"${evo}/include" \
  "${root}/tests/test_qr_ui.cpp" "${evo}/ui_rml/src/evo_rmlui_render.cpp" "${evo}/ui_rml/src/evo_rmlui_system.cpp" \
  "${destination}/objects/"*.o "${rmlui_library}" $(pkg-config --libs freetype2) -pthread \
  -o "${destination}/test"
"${destination}/test" "${copy}/assets/rml" "${evo}/assets/fonts"
