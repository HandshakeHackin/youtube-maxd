#!/usr/bin/env bash
# Source this: fetches and builds the pinned host RmlUi once, then sets
# rmlui_include and rmlui_library for the UI preview and UI tests.
rmlui_cache="${HOME}/.cache/prosperotube/rmlui"
rmlui_pin=2230d1a6e8e0848ed87a5761e2a5160b2a175ba4
rmlui_library="${rmlui_cache}/build/librmlui.a"
rmlui_include="${rmlui_cache}/RmlUi-6.2/Include"
[[ -d "${rmlui_include}" ]] || rmlui_include="${rmlui_cache}/RmlUi-${rmlui_pin}/Include"
if [[ ! -f "${rmlui_library}" || ! -f "${rmlui_include}/RmlUi/Core.h" ]]; then
    mkdir -p "${rmlui_cache}"
    curl -fsSL "https://github.com/mikke89/RmlUi/archive/${rmlui_pin}.tar.gz" -o "${rmlui_cache}/source.tar.gz"
    tar -xzf "${rmlui_cache}/source.tar.gz" -C "${rmlui_cache}"
    rmlui_include="${rmlui_cache}/RmlUi-${rmlui_pin}/Include"
    cmake -S "${rmlui_cache}/RmlUi-${rmlui_pin}" -B "${rmlui_cache}/build" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DRMLUI_SAMPLES=OFF -DRMLUI_TESTS=OFF
    cmake --build "${rmlui_cache}/build" -j 8
fi
