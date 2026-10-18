#!/usr/bin/env python3
"""Collect matching native dependency source/notices for binary redistribution.

No downloaded source is executed here. Recipes/patches are also retained.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import concurrent.futures
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request
from bootstrap_local_voice import SOURCE_URL, MODEL_URL, MODEL_SHA, ASSETS

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "runtime-sources"
URLS = {
    "whisper.cpp": SOURCE_URL,
    "SDL2": "https://github.com/ps5-payload-dev/SDL/archive/0baf4ac49.tar.gz",
    "FFmpeg": "https://ffmpeg.org/releases/ffmpeg-7.1.1.tar.xz",
    "dav1d": "https://github.com/videolan/dav1d/archive/refs/tags/1.5.1.tar.gz",
    "FreeType": "https://download.savannah.gnu.org/releases/freetype/freetype-2.13.2.tar.gz",
    "HarfBuzz": "https://github.com/harfbuzz/harfbuzz/releases/download/9.0.0/harfbuzz-9.0.0.tar.xz",
    "FriBidi": "https://github.com/fribidi/fribidi/releases/download/v1.0.15/fribidi-1.0.15.tar.xz",
    "libass": "https://github.com/libass/libass/releases/download/0.17.3/libass-0.17.3.tar.xz",
    "libxml2": "https://github.com/GNOME/libxml2/archive/refs/tags/v2.13.5.tar.gz",
    "libsamplerate": "https://github.com/libsndfile/libsamplerate/releases/download/0.2.2/libsamplerate-0.2.2.tar.xz",
    "zlib": "https://zlib.net/zlib-1.3.2.tar.gz",
    "libpng": "https://download.sourceforge.net/libpng/libpng-1.6.43.tar.gz",
    "bzip2": "https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz",
    "XZ": "https://github.com/tukaani-project/xz/releases/download/v5.4.6/xz-5.4.6.tar.xz",
    "libiconv": "https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.17.tar.gz",
    "curl": "https://curl.se/download/curl-8.18.0.tar.xz",
    "OpenSSL": "https://github.com/openssl/openssl/releases/download/openssl-3.5.2/openssl-3.5.2.tar.gz",
    "libpsl": "https://github.com/rockdaboot/libpsl/releases/download/0.21.5/libpsl-0.21.5.tar.gz",
    "zstd": "https://github.com/facebook/zstd/releases/download/v1.5.6/zstd-1.5.6.tar.gz",
    "OpenLibm": "https://github.com/JuliaMath/openlibm/archive/refs/tags/v0.8.6.tar.gz",
    "PacBrew": "https://github.com/ps5-payload-dev/pacbrew-repo/archive/refs/tags/v0.39.tar.gz",
    "PS5-Payload-SDK": "https://github.com/ps5-payload-dev/sdk/archive/refs/tags/v0.42.tar.gz",
    "RmlUi": "https://github.com/mikke89/RmlUi/archive/refs/tags/6.2.tar.gz",
    "protobuf-es": "https://github.com/bufbuild/protobuf-es/archive/refs/tags/v2.16.0.tar.gz",
}
NOTICE_NAMES = {"license", "license.txt", "license.md", "licence", "copying", "copying.txt", "copying.lib",
                "copying.lgplv2.1", "copying.gplv2", "copying.gplv3", "copying.lgplv3", "copyright",
                "ftl.txt", "bsd", "apache2", "notice", "unlicense", "license.apache-2.0"}


def sha(path):
    with path.open("rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()


def collect(item):
    name, url = item
    path = OUT / (name + (".tar.xz" if url.endswith(".xz") else ".tar.gz"))
    if not path.exists():
        temporary = path.with_suffix(path.suffix + ".download")
        with urllib.request.urlopen(url, timeout=90) as response, temporary.open("wb") as f:
            shutil.copyfileobj(response, f)
        temporary.replace(path)
    with tarfile.open(path) as archive:
        for member in archive:
            parts = Path(member.name).parts
            notice_name = Path(member.name).name.lower()
            is_notice = notice_name in NOTICE_NAMES or notice_name.startswith(("license_", "license-", "copying_", "copying-", "notice_", "notice-"))
            if (member.isfile() and is_notice and
                    len(parts) <= 5 and member.size < 250000):
                dest = OUT / "licenses" / name / "--".join(parts[1:])
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(archive.extractfile(member).read())
    return {"name": name, "url": url, "file": path.name, "sha256": sha(path)}


def main():
    OUT.mkdir(exist_ok=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        records = list(pool.map(collect, URLS.items()))
    # Source for the exact-title helper is separate from the app's editable code.
    lapy = next((ROOT / ".deps/lapy").glob("PS5-Lapy-JB-Daemon-*"))
    path = OUT / "Lapy-57fcc19.tar"
    subprocess.run(["git", "-C", str(lapy), "archive", "--format=tar", "-o", str(path),
                    "57fcc19b7f57e46c823d6f2a6c6233ebf3288079"], check=True)
    records.append({"name": "Lapy", "commit": "57fcc19b7f57e46c823d6f2a6c6233ebf3288079", "file": path.name, "sha256": sha(path)})
    header = ROOT / ".deps/lapy/ps5log-1ae1f918/ps5log.h"
    shutil.copyfile(header, OUT / "ps5log.h")
    records.append({"name": "ps5log", "commit": "1ae1f9182abd2770c131b97419034fb85173c2dc", "file": "ps5log.h", "sha256": sha(OUT / "ps5log.h")})
    quickjs = ROOT / "standalone/.cache/quickjs-amalgam-v0.17.0.zip"
    shutil.copyfile(quickjs, OUT / quickjs.name)
    records.append({"name": "QuickJS-NG", "version": "0.17.0", "file": quickjs.name, "sha256": sha(OUT / quickjs.name)})
    ca = ROOT / "standalone/ps5/.cache/cacert-2026-09-25.pem"
    assert sha(ca) == "a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505"
    shutil.copyfile(ca, OUT / ca.name)
    records.append({"name": "Mozilla CA certificates", "url": "https://curl.se/ca/cacert-2026-09-25.pem",
                    "file": ca.name, "sha256": sha(OUT / ca.name)})
    llvm_license = OUT / "licenses/LLVM/LICENSE.txt"
    llvm_license.parent.mkdir(parents=True, exist_ok=True)
    if not llvm_license.exists():
        with urllib.request.urlopen("https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-19.1.7/compiler-rt/LICENSE.TXT", timeout=90) as response:
            llvm_license.write_bytes(response.read())
    lock = json.loads((ROOT / "companion/innertube/package-lock.json").read_text())
    npm = OUT / "YouTube-js-18.1.0-and-runtime-dependencies.tar.gz"
    with tarfile.open(npm, "w:gz") as archive:
        for name, package in lock["packages"].items():
            if name and not package.get("dev"):
                folder = ROOT / "companion/innertube" / name
                archive.add(folder, arcname=name)
                # Keep each linked JavaScript dependency's notice in the app,
                # in addition to its complete npm source input in this archive.
                for notice in folder.iterdir():
                    if notice.is_file() and notice.name.lower().split(".")[0] in {"license", "licence", "copying", "notice"}:
                        dest = OUT / "licenses/npm" / name.removeprefix("node_modules/") / notice.name
                        dest.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copyfile(notice, dest)
    records.append({"name": "YouTube.js and runtime dependencies", "file": npm.name, "sha256": sha(npm)})
    # protobuf's npm artifact omits standalone LICENSE files. Its varint module
    # retains Google's full BSD notice; the source archive supplies Apache 2.0.
    varint = ROOT / "companion/innertube/node_modules/@bufbuild/protobuf/dist/esm/wire/varint.js"
    header = varint.read_text().split("/**", 1)[0]
    assert "Copyright 2008 Google Inc." in header and "Redistribution" in header
    notice = OUT / "licenses/npm/@bufbuild/protobuf/Google-BSD-3-Clause.txt"
    notice.parent.mkdir(parents=True, exist_ok=True)
    notice.write_text("\n".join(line.removeprefix("//").removeprefix(" ") for line in header.splitlines()) + "\n")
    model = ASSETS / "ggml-tiny.bin"
    if sha(model) != MODEL_SHA:
        raise ValueError("Run bootstrap_local_voice.py to verify the voice model")
    records.append({"name": "Whisper tiny model", "url": MODEL_URL,
                    "file": "../native/maxd/voice/ggml-tiny.bin", "sha256": MODEL_SHA})
    (OUT / "SOURCES.json").write_text(json.dumps(records, indent=2) + "\n")
    print(f"Collected {len(records)} source inputs and notices")


if __name__ == "__main__":
    main()
