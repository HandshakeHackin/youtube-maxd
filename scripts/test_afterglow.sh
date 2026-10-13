#!/usr/bin/env bash
# The Afterglow interface on the PC: builds it against the pinned UI kit and
# runs every board of tests/afterglow/boards in the four looks. Each board is
# brought up from fixtures, driven by its steps and checked: what has the focus
# and where, the state the screen reports, no text across the edge of the
# screen, the looks' text contrast, and a picture that is not blank. Pictures
# land in build/afterglow/pictures. Needs Mesa's surfaceless EGL (llvmpipe).
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
pictures="${root}/build/afterglow/pictures"
rm -rf "${pictures}"
mkdir -p "${pictures}"
bash "${root}/scripts/afterglow_host.sh" "${pictures}" --look all
