#!/usr/bin/env bash
# Fetch the pinned Mozilla root-certificate bundle (curl.se's dated PEM extract
# of Mozilla's certdata.txt; the certificate data is MPL-2.0) into the build
# cache (never committed) and print its path. To move the pin: pick a newer
# date from https://curl.se/docs/caextract.html and put its SHA-256 here.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
CA_DATE=2026-09-25
CA_SHA256=a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505
here=$(cd "$(dirname "$0")" && pwd)
cache=${TUBE_JS_CACHE:-$here/.cache}
file=$cache/cacert-$CA_DATE.pem
mkdir -p "$cache"
if [ ! -f "$file" ]; then
  curl -fsSL -o "$file.part" "https://curl.se/ca/cacert-$CA_DATE.pem"
  mv "$file.part" "$file"
fi
echo "$CA_SHA256  $file" | sha256sum -c - >&2
echo "$file"
