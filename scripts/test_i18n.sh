#!/usr/bin/env bash
# Interface localization: the template and the generated tables are current,
# every catalog covers the template, the module passes its unit test under the
# sanitizers, and the provider marks interface rows only.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
python3 "${root}/scripts/i18n_extract.py" --check
python3 "${root}/scripts/i18n_build.py" --check
python3 - "${root}" <<'PY'
import json
import subprocess
import sys
root = sys.argv[1]
report = json.loads(subprocess.check_output([sys.executable, root + "/scripts/i18n_build.py", "--report", "--check"], text=True))
assert len(report) >= 21, "catalogs are missing"
# The texts only the Afterglow interface shows are translated in their own pass, after its screens: until
# then a catalog is held to the classic interface's texts, and Afterglow shows what is missing in English.
# Completed catalogs must also cover every Afterglow text. Add each language here when its pass is finished.
AFTERGLOW_TRANSLATED = {"cs", "da", "de", "es", "fi", "fr", "hu", "id", "it", "nb", "nl", "pl", "pt-BR", "pt-PT", "ro", "sv", "tr", "vi"}
for row in report:
    print(f"{row['tag']:6} {row['name']:22} {row['coverage']:6.2f}%  missing {row['missing']}  (Afterglow: {row['missing_afterglow']} of {row['afterglow']})  stale {row['stale']}")
    complete = row["tag"] in AFTERGLOW_TRANSLATED
    held = row["coverage"] if complete else row["coverage_classic"]
    assert not complete or not row["missing_afterglow"], f"{row['tag']}: Afterglow texts are missing"
    assert held >= 99.0 and not row["stale"], f"{row['tag']}: the catalog does not cover the template"
# The same rule marks and extracts the texts of the browser document.
sys.path.insert(0, root + "/scripts")
from i18n_overlay import rml_localize, rml_texts
document = open(root + "/native/youtube.rml", encoding="utf-8").read()
texts = rml_texts(document)
marked = rml_localize(document)
assert len(texts) > 30 and "Nothing here yet" in texts and "ProsperoTube" not in texts and "PT" not in texts
assert marked.count("| tr(i18n) }}") >= len(texts) and rml_texts(marked) == [] and rml_localize(marked) == marked
assert "{{ 'Nothing here yet' | tr(i18n) }}" in marked and "{{ breadcrumb }}" in marked and "{{ row.title }}" in marked
assert rml_localize("<p>It's here</p>") == "<p>{{ 'It\\'s here' | tr(i18n) }}</p>"
assert rml_localize("<p>  Two\n   lines </p>") == "<p>  {{ 'Two lines' | tr(i18n) }} </p>"
template = {entry["source"] for entry in json.load(open(root + "/native/i18n/template.json", encoding="utf-8"))["strings"]}
assert all(text in template for text in texts)
PY
work="$(python3 "${root}/scripts/prepare_native.py")"
evo="${work}/projects/evoplayer"
destination="${root}/build/tests/i18n"
mkdir -p "${destination}"
flags=(-std=c11 -D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer
       -I"${root}/native" -I"${evo}/include" -I"${evo}/addons/include")
cc "${flags[@]}" "${root}/tests/test_i18n.c" "${root}/native/tube_i18n.c" -pthread -o "${destination}/test_i18n"
ASAN_OPTIONS=detect_leaks=1 "${destination}/test_i18n"
# The same test with the player interface path forced on: no catalog uses it yet.
cc "${flags[@]}" -DTUBE_I18N_TEST_KIT "${root}/tests/test_i18n.c" "${root}/native/tube_i18n.c" -pthread -o "${destination}/test_i18n_kit"
ASAN_OPTIONS=detect_leaks=1 "${destination}/test_i18n_kit"
# What prepare_native.py hands to the PS5 build is the module and tables tested here.
for name in tube_i18n.h tube_i18n_data.h; do cmp "${root}/native/${name}" "${evo}/addons/include/${name}"; done
cmp "${root}/native/tube_i18n.c" "${evo}/addons/src/tube_i18n.c"
grep -q "addons/src/tube_i18n.c" "${evo}/Makefile"
# Toasts are drawn by the player interface: both of their lines are asked for with its flag, so
# they stay English until its fonts hold the letters of a language.
[[ "$(grep -c 'tube_i18n_field(prospero_toast_.*TUBE_I18N_KIT);' "${evo}/src/evo_toast.c")" == 2 ]]
objects=()
for source in "${root}/native/tube_provider.c" "${root}/native/tube_remote.c" "${root}/native/tube_player_extra.c" "${root}/native/tube_backup.c" \
              "${root}/native/tube_phone.c" "${root}/native/tube_i18n.c" "${evo}/addons/src/cJSON.c" "${evo}/addons/src/evo_provider_mgr.c" "${evo}/src/evo_data_path.c"; do
  object="${destination}/$(basename "${source}" .c).o"
  cc "${flags[@]}" -c "${source}" -o "${object}"
  objects+=("${object}")
done
cc "${flags[@]}" -I"${root}/tests" "${root}/tests/test_i18n_provider.c" "${objects[@]}" -lm -pthread -o "${destination}/test_i18n_provider"
ASAN_OPTIONS=detect_leaks=1 "${destination}/test_i18n_provider"
