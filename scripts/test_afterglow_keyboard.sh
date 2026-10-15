#!/usr/bin/env bash
# Keyboard-to-screen regression against the real Afterglow frame and components.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
# --reuse-host is for a coordinated batch that already rebuilt these objects.
[[ "${1:-}" == --reuse-host ]] || bash "$root/scripts/afterglow_host.sh" --build-only
kit="$(python3 "$root/scripts/ui_kit.py")"
build="$root/build/afterglow"
text_deps="$(bash "$root/scripts/text_deps.sh" plain)"
objects=()
shopt -s dotglob
for object in "$build"/obj/*.o; do
    [[ "$object" == *tests_afterglow_host_main.cpp.o ]] || objects+=("$object")
done
g++ -std=c++20 -g -Wall -Wextra -I"$build/kit-overlay" -I"$kit/src" -I"$kit" -I"$root/native" $(cat "$text_deps/include.flags") "$root/tests/test_afterglow_keyboard.cpp" "${objects[@]}" $(cat "$text_deps/lib.flags") -lEGL -lGL -lm -o "$build/test-keyboard"
"$build/test-keyboard" "$root/tests/afterglow/fixtures/signed-in"
echo 'PASS: physical text uses live Search debounce, submit and limits; hidden-page isolation; TextSheet UTF-8 backspace, submit, cancel and controller coexistence'
