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
if [[ ! -f "${PS5_PAYLOAD_SDK}/target/user/homebrew/lib/libdav1d.a" ]]; then
    bash "${work}/scripts/build-dav1d.sh" --install --jobs 8
fi
if [[ ! -f "${PS5_PAYLOAD_SDK}/PROSPEROTUBE_FFMPEG" ]]; then
    bash "${work}/scripts/build-ffmpeg.sh" --install --jobs 8
    printf '%s\n' '7.1.1-minimal-EVO-9715f6e' > "${PS5_PAYLOAD_SDK}/PROSPEROTUBE_FFMPEG"
fi
bash "${work}/scripts/package-app.sh" --zip "$@"
mkdir -p "${root}/dist"
cp -a "${work}/output/app/PPSA99142" "${root}/dist/"
find "${work}/output" -maxdepth 3 -type f \( -name '*.zip' -o -name '*.ffpfsc' \) -exec cp -t "${root}/dist/" {} +
printf '%s\n' 'ProsperoTube PS5 artifacts are in dist/. No console deployment performed.'
