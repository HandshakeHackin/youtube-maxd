#!/usr/bin/env bash
# Build the Afterglow interface for the PC and run its host harness
# (tests/afterglow/host_main.cpp): the frame and its screens on the pinned UI
# kit's own OpenGL renderer, under Mesa's surfaceless EGL, fed from fixtures.
#
# usage: scripts/afterglow_host.sh <output dir> [harness options]
#        scripts/afterglow_host.sh --build-only
#        scripts/afterglow_host.sh --run-only <output dir> [harness options]
# --run-only runs what the last build left, so that several runs (one per look)
# can share one build and go at the same time.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
kit="$(python3 "${root}/scripts/ui_kit.py")"
build="${root}/build/afterglow"

build_host() {
    mkdir -p "${build}/obj"
    # The kit's font with the door for the text engine: a patched copy, ahead of the kit on the include path.
    python3 "${root}/scripts/kit_text_patch.py" "${kit}" "${build}/kit-overlay"
    # HarfBuzz, FreeType and FriBidi at the versions the PS5 SDK has, built once (scripts/text_deps.sh).
    local text_deps
    text_deps="$(bash "${root}/scripts/text_deps.sh" plain)"
    local cxx="${HOST_CXX:-g++}"
    local cc="${HOST_CC:-gcc}"

    # The kit: everything that builds or draws a draw list, its OpenGL renderer and
    # the host's system services. Not its app shell, designs, demo data or music.
    local sources=("${kit}/host/platform_host.cpp" "${kit}/src/audio/cues.cpp" "${kit}/src/audio/mixer.cpp" "${kit}/src/audio/wav.cpp")
    local source
    while IFS= read -r -d '' source; do sources+=("${source}"); done < <(find "${kit}/src/gfx" "${kit}/src/ui" "${kit}/src/core" -name '*.cpp' ! -path "${kit}/src/gfx/font.cpp" -print0 | sort -z)
    sources+=("${build}/kit-overlay/gfx/font.cpp")
    while IFS= read -r -d '' source; do sources+=("${source}"); done < <(find "${root}/native/text" -maxdepth 1 -name '*.cpp' -print0 | sort -z)
    # The interface, except what only the app has (the mount and the provider's source).
    while IFS= read -r -d '' source; do sources+=("${source}"); done < <(find "${root}/native/afterglow" -name '*.cpp' ! -path '*/app/*' -print0 | sort -z)
    while IFS= read -r -d '' source; do sources+=("${source}"); done < <(find "${root}/tests/afterglow" -maxdepth 1 -name '*.cpp' -print0 | sort -z)
    sources+=("${root}/native/vendor/cJSON/cJSON.c")

    local makefile="${build}/Makefile"
    {
        printf 'CXXFLAGS := -std=c++20 -O2 -g -Wall -Wextra -DGL_GLEXT_PROTOTYPES=1 -I%s/kit-overlay -I%s/src -I%s -I%s/third_party -I%s/native -I%s/native/text -I%s/native/vendor/cJSON -I%s/third_party/evo/projects/evoplayer -I%s/tests/afterglow -I%s/tests %s\n' \
            "${build}" "${kit}" "${kit}" "${kit}" "${root}" "${root}" "${root}" "${root}" "${root}" "${root}" "$(tr '\n' ' ' < "${text_deps}/include.flags")"
        printf 'all: %s/afterglow_host\n' "${build}"
        local objects=()
        local name object
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
        printf '%s/afterglow_host: %s\n\t@%s %s %s -lEGL -lGL -lm -o %s/afterglow_host\n' "${build}" "${objects[*]}" "${cxx}" "${objects[*]}" "$(tr '\n' ' ' < "${text_deps}/lib.flags")" "${build}"
        printf -- '-include %s/obj/*.d\n' "${build}"
    } > "${makefile}"
    if ! make -s -j"$(nproc)" -f "${makefile}" > "${build}/build.log" 2>&1; then
        grep -E 'error|Error|undefined' -A4 "${build}/build.log" | head -80 >&2
        exit 1
    fi
}

if [[ "${1:-}" == "--run-only" ]]; then
    shift
else
    build_host
fi
[[ "${1:-}" == "--build-only" ]] && exit 0

output="${1:?output directory}"
shift
EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
    "${build}/afterglow_host" "${kit}" "${root}/tests/afterglow/fixtures" "${root}/tests/afterglow/boards" "${output}" \
    --catalogs "${root}/native/i18n" "$@"
