#!/usr/bin/env bash
# Fetch the pinned QuickJS-NG amalgamation into the build cache (never committed)
# and print its directory. QuickJS-NG is MIT licensed.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
QJS_TAG=v0.17.0
QJS_SHA256=a0955463c74809173a253ff87365095e6972e8b171cfb6aa8a1e781adf35cfeb
here=$(cd "$(dirname "$0")" && pwd)
cache=${TUBE_JS_CACHE:-$here/.cache}
dir=$cache/quickjs-$QJS_TAG
zip=$cache/quickjs-amalgam-$QJS_TAG.zip
mkdir -p "$cache"
if [ ! -f "$zip" ]; then
  curl -fsSL -o "$zip.part" "https://github.com/quickjs-ng/quickjs/releases/download/$QJS_TAG/quickjs-amalgam.zip"
  mv "$zip.part" "$zip"
fi
echo "$QJS_SHA256  $zip" | sha256sum -c - >&2
if [ ! -f "$dir/.unpacked-$QJS_SHA256" ]; then
  rm -rf "$dir"
  mkdir -p "$dir"
  unzip -q -o "$zip" -d "$dir"
  for f in quickjs-amalgam.c quickjs.h; do
    [ -f "$dir/$f" ] || { echo "fetch_quickjs: $f missing from the archive" >&2; exit 1; }
  done
  : > "$dir/.unpacked-$QJS_SHA256"
fi
echo "$dir"
