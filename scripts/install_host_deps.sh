#!/usr/bin/env bash
# Explicit host setup for Ubuntu 24.04 / WSL; never run implicitly by make zip.
# Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
source /etc/os-release
if [[ "${ID:-}" != ubuntu || "${VERSION_ID:-}" != 24.04 ]]; then
    echo 'Host dependency installation supports Ubuntu 24.04 (including WSL) only.' >&2
    exit 2
fi
# Keep build, shader-toolchain and offline-test dependencies together for CI and local builds.
packages=(
    build-essential clang-18 lld-18 llvm-18 cmake ninja-build make git
    python3 python3-venv python3-yaml python3-jinja2 python3-ruamel.yaml python3-pil
    pkg-config ccache wget curl ca-certificates unzip zip tar xz-utils util-linux meson nasm yasm
    libssl-dev libcurl4-openssl-dev zlib1g-dev libfreetype-dev libzstd-dev libbz2-dev liblzma-dev
    ffmpeg libavformat-dev libavcodec-dev libavutil-dev libavfilter-dev libswresample-dev libswscale-dev
    glslang-tools spirv-tools openssl
    g++-multilib gcc-multilib libwayland-dev libx11-dev libx11-xcb-dev libxcb1-dev libxext-dev
    libxcb-dri3-dev libxcb-dri2-0-dev libxcb-present-dev libxcb-randr0-dev libxcb-sync-dev libxcb-shm0-dev
    libxcb-glx0-dev libxcb-xfixes0-dev libxshmfence-dev libxrandr-dev libdrm-dev x11proto-dri2-dev
    libegl1-mesa-dev libgl-dev libgl1-mesa-dri
)
privilege=()
if (( EUID != 0 )); then privilege=(sudo); fi
"${privilege[@]}" apt-get update
"${privilege[@]}" apt-get install --yes --no-install-recommends "${packages[@]}"
printf '%s\n' 'Host dependencies installed. Builds use LLVM 18 from /usr/lib/llvm-18/bin.'
