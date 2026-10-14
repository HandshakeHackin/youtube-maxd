#!/usr/bin/env bash
# Exercise the real Player spring before the video blit and after the UI update.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
bash "$root/scripts/afterglow_host.sh" --build-only
kit="$(python3 "$root/scripts/ui_kit.py")"
build="$root/build/afterglow"
text_deps="$(bash "$root/scripts/text_deps.sh" plain)"
objects=()
shopt -s dotglob
for object in "$build"/obj/*.o; do
    [[ "$object" == *tests_afterglow_host_main.cpp.o ]] || objects+=("$object")
done
g++ -std=c++20 -g -Wall -Wextra -I"$build/kit-overlay" -I"$kit/src" -I"$kit" -I"$root/native" $(cat "$text_deps/include.flags") "$root/tests/test_afterglow_theatre.cpp" "${objects[@]}" $(cat "$text_deps/lib.flags") -lEGL -lGL -lm -o "$build/test-theatre"
"$build/test-theatre"
echo 'PASS real Player pre-blit theatre motion, matching UI rectangle/radius, both sides and immediate restore'
