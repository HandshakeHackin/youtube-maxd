#!/usr/bin/env bash
# Build the Afterglow interface for the PC and run its host harness
# (tests/afterglow/host_main.cpp): the frame and its screens on the pinned UI
# kit's own OpenGL renderer, under Mesa's surfaceless EGL, fed from fixtures.
#
# usage: scripts/afterglow_host.sh <output dir> [harness options]
#        scripts/afterglow_host.sh --build-only
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
kit="$(python3 "${root}/scripts/ui_kit.py")"
build="${root}/build/afterglow"
mkdir -p "${build}/obj"
cxx="${HOST_CXX:-g++}"
cc="${HOST_CC:-gcc}"

# The kit: everything that builds or draws a draw list, its OpenGL renderer and
# the host's system services. Not its app shell, designs, demo data or music.
sources=("${kit}/host/platform_host.cpp" "${kit}/src/audio/cues.cpp" "${kit}/src/audio/mixer.cpp" "${kit}/src/audio/wav.cpp")
while IFS= read -r -d '' source; do sources+=("${source}"); done < <(find "${kit}/src/gfx" "${kit}/src/ui" "${kit}/src/core" -name '*.cpp' -print0 | sort -z)
# The interface, except what only the app has (the mount and the provider's source).
while IFS= read -r -d '' source; do sources+=("${source}"); done < <(find "${root}/native/afterglow" -name '*.cpp' ! -path '*/app/*' -print0 | sort -z)
while IFS= read -r -d '' source; do sources+=("${source}"); done < <(find "${root}/tests/afterglow" -maxdepth 1 -name '*.cpp' -print0 | sort -z)
sources+=("${root}/native/vendor/cJSON/cJSON.c")

makefile="${build}/Makefile"
{
    printf 'CXXFLAGS := -std=c++20 -O2 -g -Wall -Wextra -DGL_GLEXT_PROTOTYPES=1 -I%s/src -I%s -I%s/third_party -I%s/native -I%s/native/vendor/cJSON -I%s/third_party/evo/projects/evoplayer -I%s/tests/afterglow\n' \
        "${kit}" "${kit}" "${kit}" "${root}" "${root}" "${root}" "${root}"
    printf 'all: %s/afterglow_host\n' "${build}"
    objects=()
    for source in "${sources[@]}"; do
        name="${source#"${root}/"}"; name="${name#"${kit}/"}"
        object="${build}/obj/${name//\//_}.o"
        objects+=("${object}")
        if [[ "${source}" == *.c ]]; then
            printf '%s: %s\n\t@%s -std=c11 -O2 -w -MD -MP -c %s -o %s\n' "${object}" "${source}" "${cc}" "${source}" "${object}"
        else
            printf '%s: %s\n\t@%s $(CXXFLAGS) -MD -MP -c %s -o %s\n' "${object}" "${source}" "${cxx}" "${source}" "${object}"
        fi
    done
    printf '%s/afterglow_host: %s\n\t@%s %s -lEGL -lGL -lm -o %s/afterglow_host\n' "${build}" "${objects[*]}" "${cxx}" "${objects[*]}" "${build}"
    printf -- '-include %s/obj/*.d\n' "${build}"
} > "${makefile}"
if ! make -s -j"$(nproc)" -f "${makefile}" > "${build}/build.log" 2>&1; then
    grep -E 'error|Error|undefined' -A4 "${build}/build.log" | head -80 >&2
    exit 1
fi
[[ "${1:-}" == "--build-only" ]] && exit 0

output="${1:?output directory}"
shift
EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
    "${build}/afterglow_host" "${kit}" "${root}/tests/afterglow/fixtures" "${root}/tests/afterglow/boards" "${output}" \
    --catalogs "${root}/native/i18n" "$@"
