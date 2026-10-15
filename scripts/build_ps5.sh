#!/usr/bin/env bash
# Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
for option in "$@"; do
    if [[ "$option" == "--ffpfsc" ]]; then
        echo 'ProsperoTube packages the title as a ZIP; omit --ffpfsc.' >&2
        exit 2
    fi
done
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
# Every application package must include the renderer used by its display controls.
# An explicit artifact is preflighted by prepare_native; otherwise compile/reuse
# the complete verified set. A missing compiler or failed pipeline stops the build.
if [[ -z "${PROSPEROTUBE_TRANSFORM_ARTIFACT:-}" ]]; then
    PROSPEROTUBE_TRANSFORM_ARTIFACT="$(python3 "${root}/scripts/build_video_transform_shaders.py")"
    export PROSPEROTUBE_TRANSFORM_ARTIFACT
fi
# The package carries the built-in YouTube service unless told otherwise:
# TUBE_EMBEDDED_SERVICE=0 builds the companion-only app.
# Host tests call prepare_native.py without this and keep the companion.
export TUBE_EMBEDDED_SERVICE="${TUBE_EMBEDDED_SERVICE:-1}"
# The service bundle is built here when it is missing or older than anything it
# is made from; PROSPEROTUBE_JS_BUNDLE names another one. Its digest is logged.
if [[ "${TUBE_EMBEDDED_SERVICE}" == 1 ]]; then
    bundle="${PROSPEROTUBE_JS_BUNDLE:-${root}/standalone/build/tube_js.bundle.js}"
    if [[ -z "${PROSPEROTUBE_JS_BUNDLE:-}" ]]; then
        newer=""
        [[ -f "${bundle}" ]] && newer="$(find "${root}/standalone/js" "${root}/standalone/build_bundle.sh" \
            "${root}/companion/innertube" "${root}/companion/prosperotube/__init__.py" -maxdepth 1 -type f \
            \( -name '*.js' -o -name '*.mjs' -o -name '*.json' -o -name '*.sh' -o -name '*.py' \) -newer "${bundle}" -print -quit)"
        if [[ ! -f "${bundle}" || -n "${newer}" ]]; then
            [[ -d "${root}/companion/innertube/node_modules" ]] || (cd "${root}/companion/innertube" && npm ci --ignore-scripts)
            bash "${root}/standalone/build_bundle.sh" > /dev/null
        fi
    fi
    [[ -f "${bundle}" ]] || { echo "The service bundle ${bundle} is missing." >&2; exit 2; }
    printf 'Service bundle: %s  %s bytes  sha256 %s\n' "${bundle}" "$(stat -c %s "${bundle}")" "$(sha256sum "${bundle}" | cut -d ' ' -f 1)"
fi
work="$(python3 "${root}/scripts/prepare_native.py")"
# The exact-title upstream Lapy helper, built and verified as ProsperoEden does it.
python3 "${root}/scripts/build_lapy_helper.py" PPSA99142 "${work}/prosperotube-lapy"
if [[ "${TUBE_EMBEDDED_SERVICE}" == 1 ]]; then
    make -C "${root}/native/update/helper" OUTPUT="${work}/prosperotube-update/self-updater.elf"
fi
if [[ ! -f "${PS5_PAYLOAD_SDK}/target/user/homebrew/lib/libdav1d.a" ]]; then
    bash "${work}/scripts/build-dav1d.sh" --install --jobs 8
fi
ffmpeg_patch=$(cat "${root}/scripts/patch_ffmpeg_dash_seek.py" "${root}/scripts/patch_ffmpeg_hls_dvr.py" "${root}/scripts/ffmpeg_hls_dvr.inc" | sha256sum | cut -d ' ' -f 1)
ffmpeg_identity="7.1.1-minimal-EVO-9715f6e-atempo-v1-dash-hls-dvr-${ffmpeg_patch}"
if [[ ! -f "${PS5_PAYLOAD_SDK}/PROSPEROTUBE_FFMPEG" ]] ||
   [[ "$(cat "${PS5_PAYLOAD_SDK}/PROSPEROTUBE_FFMPEG")" != "${ffmpeg_identity}" ]]; then
    bash "${work}/scripts/build-ffmpeg.sh" --install --jobs 8
    [[ -f "${PS5_PAYLOAD_SDK}/target/user/homebrew/lib/libavfilter.a" ]]
    grep -q '^#define CONFIG_ATEMPO_FILTER 1$' "${work}/build/ffmpeg/7.1.1-minimal/config_components.h"
    printf '%s\n' "${ffmpeg_identity}" > "${PS5_PAYLOAD_SDK}/PROSPEROTUBE_FFMPEG"
fi
# A stamped SDK must expose the checked window contract, not just HLS decoding.
"${PS5_PAYLOAD_SDK}/bin/prospero-nm" --defined-only "${PS5_PAYLOAD_SDK}/target/user/homebrew/lib/libavformat.a" > "${root}/.deps/ps5-format-symbols.txt"
for symbol in av_hls_get_dvr_window av_hls_dvr_prepare_seek av_hls_dvr_point_identity av_hls_dvr_pts_to_wall av_hls_dvr_wall_to_pts; do
    grep -Eq "[[:space:]][Tt][[:space:]]+${symbol}$" "${root}/.deps/ps5-format-symbols.txt" || {
        echo "The PS5 FFmpeg archive lacks required checked HLS DVR entry point ${symbol}." >&2; exit 2;
    }
done
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
python3 "${root}/scripts/build_lapy_helper.py" PPSA99142 --check-app "${work}/output/app/PPSA99142"
destination="${PROSPEROTUBE_PS5_DIST:-${root}/dist}"
mkdir -p "${destination}"
cp -a "${work}/output/app/PPSA99142" "${destination}/"
find "${work}/output" -maxdepth 3 -type f -name '*.zip' -exec cp -t "${destination}/" {} +
printf '%s\n' "ProsperoTube PS5 artifacts are in ${destination}. No console deployment performed."
