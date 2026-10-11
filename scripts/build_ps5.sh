#!/usr/bin/env bash
# Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-${HOME}/.cache/prosperotube/sdk}"
export PATH="${PS5_PAYLOAD_SDK}/bin:${PATH}"
# Resolve the SDK's preferred LLVM once; repeated PATH searches across WSL's
# Windows mounts otherwise dominate configure and per-object tool invocations.
# Keep an explicit caller selection intact.
if [[ -z "${LLVM_CONFIG:-}" ]]; then
    llvm_bindir=$("${PS5_PAYLOAD_SDK}/bin/prospero-llvm-config" --bindir)
    if [[ -x "${llvm_bindir}/llvm-config" ]]; then
        export LLVM_CONFIG="${llvm_bindir}/llvm-config"
    fi
fi
mkdir -p "${root}/.deps"
exec 9>"${root}/.deps/native-build.lock"
flock -n 9 || { echo 'Another ProsperoTube native build is already running.' >&2; exit 2; }
work="$(python3 "${root}/scripts/prepare_native.py")"
lapy_options=()
if [[ -n "${PROSPEROTUBE_LAPY_RESULT_FILE:-}" ]]; then
    lapy_options+=(--result-file "${PROSPEROTUBE_LAPY_RESULT_FILE}")
fi
if [[ "${PROSPEROTUBE_LAPY_STOP_DIAGNOSTICS:-0}" == 1 ]]; then
    lapy_options+=(--target-stop-diagnostics)
fi
python3 "${root}/scripts/build_lapy_helper.py" PPSA99142 "${work}/prosperotube-lapy" "${lapy_options[@]}"
if [[ ! -f "${PS5_PAYLOAD_SDK}/target/user/homebrew/lib/libdav1d.a" ]]; then
    bash "${work}/scripts/build-dav1d.sh" --install --jobs 8
fi
ffmpeg_patch=$(sha256sum "${root}/scripts/patch_ffmpeg_dash_seek.py" | cut -d ' ' -f 1)
ffmpeg_identity="7.1.1-minimal-EVO-9715f6e-atempo-v1-dash-${ffmpeg_patch}"
if [[ ! -f "${PS5_PAYLOAD_SDK}/PROSPEROTUBE_FFMPEG" ]] ||
   [[ "$(cat "${PS5_PAYLOAD_SDK}/PROSPEROTUBE_FFMPEG")" != "${ffmpeg_identity}" ]]; then
    bash "${work}/scripts/build-ffmpeg.sh" --install --jobs 8
    [[ -f "${PS5_PAYLOAD_SDK}/target/user/homebrew/lib/libavfilter.a" ]]
    grep -q '^#define CONFIG_ATEMPO_FILTER 1$' "${work}/build/ffmpeg/7.1.1-minimal/config_components.h"
    printf '%s\n' "${ffmpeg_identity}" > "${PS5_PAYLOAD_SDK}/PROSPEROTUBE_FFMPEG"
fi
"${PS5_PAYLOAD_SDK}/bin/prospero-nm" --defined-only "${PS5_PAYLOAD_SDK}/target/user/homebrew/lib/libavcodec.a" > "${root}/.deps/ps5-codec-symbols.txt"
grep -Eq '[[:space:]][Dd][[:space:]]+ff_libdav1d_decoder$' "${root}/.deps/ps5-codec-symbols.txt" || {
    echo 'The PS5 FFmpeg archive lacks the required libdav1d AV1 decoder.' >&2; exit 2;
}
for symbol in ff_hevc_decoder ff_hevc_mp4toannexb_bsf; do
    grep -Eq "[[:space:]][Dd][[:space:]]+${symbol}$" "${root}/.deps/ps5-codec-symbols.txt" || {
        echo "The PS5 FFmpeg archive lacks required HEVC component ${symbol}." >&2; exit 2;
    }
done
bash "${work}/scripts/package-app.sh" --zip "$@"
destination="${PROSPEROTUBE_PS5_DIST:-${root}/dist}"
mkdir -p "${destination}"
cp -a "${work}/output/app/PPSA99142" "${destination}/"
find "${work}/output" -maxdepth 3 -type f \( -name '*.zip' -o -name '*.ffpfsc' \) -exec cp -t "${destination}/" {} +
printf '%s\n' "ProsperoTube PS5 artifacts are in ${destination}. No console deployment performed."
