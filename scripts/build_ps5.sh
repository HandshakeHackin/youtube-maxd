#!/usr/bin/env bash
# Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-${HOME}/.cache/prosperotube/sdk}"
export PATH="${PS5_PAYLOAD_SDK}/bin:${PATH}"
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
ffmpeg_identity="7.1.1-minimal-EVO-9715f6e-dash-${ffmpeg_patch}"
if [[ ! -f "${PS5_PAYLOAD_SDK}/PROSPEROTUBE_FFMPEG" ]] ||
   [[ "$(cat "${PS5_PAYLOAD_SDK}/PROSPEROTUBE_FFMPEG")" != "${ffmpeg_identity}" ]]; then
    bash "${work}/scripts/build-ffmpeg.sh" --install --jobs 8
    printf '%s\n' "${ffmpeg_identity}" > "${PS5_PAYLOAD_SDK}/PROSPEROTUBE_FFMPEG"
fi
bash "${work}/scripts/package-app.sh" --zip "$@"
destination="${PROSPEROTUBE_PS5_DIST:-${root}/dist}"
mkdir -p "${destination}"
cp -a "${work}/output/app/PPSA99142" "${destination}/"
find "${work}/output" -maxdepth 3 -type f \( -name '*.zip' -o -name '*.ffpfsc' \) -exec cp -t "${destination}/" {} +
printf '%s\n' "ProsperoTube PS5 artifacts are in ${destination}. No console deployment performed."
