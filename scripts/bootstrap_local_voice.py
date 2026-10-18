#!/usr/bin/env python3
"""Pinned local Whisper source/model inputs; no speech leaves the console.
SPDX-License-Identifier: GPL-3.0-or-later
"""
from pathlib import Path
import hashlib
import json
import shutil
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
REVISION = "d1be6fde11ac6e0407606b4e42fe72d34add8037"
SOURCE_SHA = "645445da884df6dbd52b30f82d7be7c01d1767c938714e477e786b88d529e9f4"
MODEL_REVISION = "5359861c739e955e79d9a303bcbc70fb988958b1"
MODEL_SHA = "be07e048e1e599ad46341c8d2a135645097a538221678b7acdd1b1919c6e1b21"
MODEL_SIZE = 77691713
SOURCE_URL = f"https://github.com/ggml-org/whisper.cpp/archive/{REVISION}.tar.gz"
MODEL_URL = f"https://huggingface.co/ggerganov/whisper.cpp/resolve/{MODEL_REVISION}/ggml-tiny.bin"
CACHE = ROOT / ".deps/voice"
ASSETS = ROOT / "native/maxd/voice"


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def download(url, path, expected=None):
    if not path.is_file():
        print("Fetching " + path.name, flush=True)
        temporary = path.with_suffix(path.suffix + ".download")
        with urllib.request.urlopen(url, timeout=60) as source, temporary.open("wb") as target:
            shutil.copyfileobj(source, target)
        if expected and sha(temporary) != expected:
            temporary.unlink()
            raise ValueError("Checksum mismatch: " + path.name)
        temporary.replace(path)
    if expected and sha(path) != expected:
        raise ValueError("Checksum mismatch: " + str(path))


def header(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(f'''/* Generated from pinned Whisper inputs; GPL-3.0-or-later. */
#pragma once
#define MAXD_VOICE_MODEL_SHA256 "{MODEL_SHA}"
#define MAXD_VOICE_MODEL_SIZE {MODEL_SIZE}u
#define MAXD_VOICE_MODEL_PATH "/app0/assets/voice/ggml-tiny.bin"
''')


def main():
    CACHE.mkdir(parents=True, exist_ok=True)
    ASSETS.mkdir(parents=True, exist_ok=True)
    archive = CACHE / "whisper.cpp.tar.gz"
    supplied = ROOT / "runtime-sources/whisper.cpp.tar.gz"
    if not archive.exists() and supplied.exists():
        shutil.copyfile(supplied, archive)
    download(SOURCE_URL, archive, SOURCE_SHA)
    source = CACHE / ("whisper.cpp-" + REVISION)
    if not (source / "include/whisper.h").exists():
        with tarfile.open(archive) as tar:
            tar.extractall(CACHE, filter="data")
    model = ASSETS / "ggml-tiny.bin"
    download(MODEL_URL, model, MODEL_SHA)
    if model.stat().st_size != MODEL_SIZE:
        raise ValueError("Incorrect voice model size")
    license_file = ASSETS / "OPENAI-WHISPER-MIT.txt"
    download("https://raw.githubusercontent.com/openai/whisper/main/LICENSE", license_file)
    if "MIT License" not in license_file.read_text() or "OpenAI" not in license_file.read_text():
        raise ValueError("Whisper model license is missing")
    shutil.copyfile(source / "LICENSE", ASSETS / "WHISPER-CPP-MIT.txt")
    manifest = {"engine": {"name": "whisper.cpp", "version": "1.9.5", "commit": REVISION,
                           "url": SOURCE_URL, "sha256": SOURCE_SHA},
                "model": {"name": "tiny", "multilingual": True, "file": model.name,
                          "revision": MODEL_REVISION, "url": MODEL_URL,
                          "sha256": MODEL_SHA, "size": MODEL_SIZE},
                "model_license": {"file": license_file.name, "sha256": sha(license_file)}}
    (ASSETS / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    header(CACHE / "include/maxd_voice_model.h")
    runtime = ROOT / "runtime-sources"
    runtime.mkdir(exist_ok=True)
    shutil.copyfile(archive, supplied)
    licenses = runtime / "licenses/whisper.cpp"
    licenses.mkdir(parents=True, exist_ok=True)
    for name in ("OPENAI-WHISPER-MIT.txt", "WHISPER-CPP-MIT.txt"):
        shutil.copyfile(ASSETS / name, licenses / name)
    records_path = runtime / "SOURCES.json"
    if records_path.exists():
        records = [r for r in json.loads(records_path.read_text()) if r["name"] not in {"whisper.cpp", "Whisper tiny model"}]
        records.extend([
            {"name": "whisper.cpp", "commit": REVISION, "url": SOURCE_URL, "file": supplied.name, "sha256": SOURCE_SHA},
            {"name": "Whisper tiny model", "url": MODEL_URL, "file": "../native/maxd/voice/ggml-tiny.bin", "sha256": MODEL_SHA}])
        records_path.write_text(json.dumps(records, indent=2) + "\n")
    print("Pinned Whisper source, multilingual tiny model and MIT notices verified", flush=True)


if __name__ == "__main__":
    main()
