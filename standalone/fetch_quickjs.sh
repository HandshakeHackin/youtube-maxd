#!/usr/bin/env bash
# Fetch the pinned QuickJS-NG amalgamation into the build cache (never committed).
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
QJS_TAG=v0.17.0
QJS_SHA256=a0955463c74809173a253ff87365095e6972e8b171cfb6aa8a1e781adf35cfeb
cache=${TUBE_JS_CACHE:-$(cd "$(dirname "$0")" && pwd)/.cache}
dir=$cache/quickjs-$QJS_TAG
if [ ! -f "$dir/quickjs-amalgam.c" ]; then
  mkdir -p "$dir"
  zip=$cache/quickjs-amalgam-$QJS_TAG.zip
  curl -fsSL -o "$zip" "https://github.com/quickjs-ng/quickjs/releases/download/$QJS_TAG/quickjs-amalgam.zip"
  echo "$QJS_SHA256  $zip" | sha256sum -c - >&2
  unzip -q -o "$zip" -d "$dir"
fi
echo "$dir"
