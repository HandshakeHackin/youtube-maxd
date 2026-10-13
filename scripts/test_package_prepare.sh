#!/usr/bin/env bash
# The package form of the generated tree: every overlay must apply with the
# built-in service on, as scripts/build_ps5.sh prepares it. Host tests prepare
# the companion form, so an overlay that only the package applies is otherwise
# first exercised by a PS5 build.
# Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
bundle="${root}/standalone/build/tube_js.bundle.js"
if [[ ! -f "${bundle}" ]]; then
    [[ -d "${root}/companion/innertube/node_modules" ]] || (cd "${root}/companion/innertube" && npm ci --ignore-scripts > /dev/null)
    bash "${root}/standalone/build_bundle.sh" > /dev/null
fi
work="${PROSPEROTUBE_NATIVE_WORK:-${HOME}/.cache/prosperotube/native}-package-check"
prepared="$(TUBE_EMBEDDED_SERVICE=1 PROSPEROTUBE_NATIVE_WORK="${work}" python3 "${root}/scripts/prepare_native.py")"
[[ "${prepared}" == "${work}" ]]
grep -q 'DTUBE_EMBEDDED_SERVICE=1' "${work}/projects/evoplayer/Makefile"
! grep -q 'PAIR COMPANION' "${work}/projects/evoplayer/assets/rml/youtube.rml"
echo "PASS: the package overlays apply with the built-in service"
