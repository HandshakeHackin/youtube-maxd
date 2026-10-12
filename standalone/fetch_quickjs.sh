#!/usr/bin/env bash
# Fetch the pinned QuickJS-NG amalgamation and print its directory. One script
# serves the host and the console build: standalone/ps5/fetch_quickjs.sh holds
# the tag and the checksum; this keeps the host build's cache next to it.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
TUBE_JS_CACHE=${TUBE_JS_CACHE:-$here/.cache} exec "$here/ps5/fetch_quickjs.sh"
