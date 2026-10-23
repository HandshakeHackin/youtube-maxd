#!/usr/bin/env bash
# Restore the existing pins through a public mirror of the unavailable UI repo.
# No branch or dependency revision is advanced. ui_kit.py separately verifies
# the substituted kit against scripts/ui_kit.lock.json.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
git -C "$root" -c url.https://github.com/LedyBacer/.insteadOf=https://github.com/blackbearreloaded/ submodule update --init --recursive
