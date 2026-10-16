#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
out="${root}/.deps/update-tests"
mkdir -p "${out}"
flags=(-DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror)
kit="${root}/native/update/kit"
helper="${root}/native/update/helper"
for name in miniz miniz_tinfl miniz_tdef miniz_zip; do
    cc -O1 -g -fsanitize=address,undefined -w -I"${root}/native/update/third_party" -c "${root}/native/update/third_party/miniz/${name}.c" -o "${out}/${name}.o"
done
g++ -Wno-error=ignored-attributes -Wno-error=unused-result -std=c++20 "${flags[@]}" -I"${kit}" -I"${root}/native/update/third_party" \
    "${root}/tests/test_update_kit.cpp" "${helper}/archive.cpp" "${helper}/files.cpp" "${helper}/updater.cpp" \
    "${out}"/miniz*.o -pthread -o "${out}/test_update_kit"
ASAN_OPTIONS=detect_leaks=1 "${out}/test_update_kit"
cc -std=c11 "${flags[@]}" -DUPDATE_CHECK_NO_NETWORK -I"${root}/native" -I"${kit}" \
    "${root}/tests/test_update_adapter.c" "${root}/native/tube_update.c" "${root}/native/tube_update_app.c" \
    "${kit}/self_update.c" "${kit}/self_update_sha256.c" "${kit}/update_check.c" -pthread -o "${out}/test_update_adapter"
ASAN_OPTIONS=detect_leaks=1 "${out}/test_update_adapter"
python3 "${root}/scripts/test_update_provenance.py"

g++ -std=c++17 "${flags[@]}" -I"${root}/native" -I"${kit}" "${root}/tests/test_update_prompt.cpp" -o "${out}/test_update_prompt"
ASAN_OPTIONS=detect_leaks=1 "${out}/test_update_prompt"
cc -std=c11 "${flags[@]}" -I"${root}/native" -I"${kit}" "${root}/tests/test_update_app_flow.c" "${root}/native/tube_update_app.c" -o "${out}/test_update_app_flow"
ASAN_OPTIONS=detect_leaks=1 "${out}/test_update_app_flow"
