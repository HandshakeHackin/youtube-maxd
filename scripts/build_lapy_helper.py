#!/usr/bin/env python3
# ps5-native-app-boilerplate - Build the pinned upstream Lapy one-shot helper.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fetch, build, and verify upstream Lapy's exact-title elfldr helper."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import urllib.request
import zipfile


ROOT = Path(__file__).resolve().parents[1]
CACHE = ROOT / ".deps/lapy"
LAPY_COMMIT = "57fcc19b7f57e46c823d6f2a6c6233ebf3288079"
LAPY = CACHE / f"PS5-Lapy-JB-Daemon-{LAPY_COMMIT[:7]}"
SDK = CACHE / "ps5-payload-sdk-v0.42"
SDK_URL = "https://github.com/ps5-payload-dev/sdk/releases/download/v0.42/ps5-payload-sdk.zip"
SDK_SHA256 = "8cfbc7cd5811e719eb4f0c47eea668d3dc7b40bc8ab11c4a5031d40c23ec02da"
PS5LOG = CACHE / "ps5log-1ae1f918"
PS5LOG_URL = (
    "https://raw.githubusercontent.com/mpereiraesaa/ps5-agc-gears/"
    "1ae1f9182abd2770c131b97419034fb85173c2dc/native/ps5log/ps5log.h"
)
PS5LOG_SHA256 = "394af67d0f8b60b3335deb53396e52855ea2daa50ca914a456ea7663f48900c6"
PROTOCOL_SHA256 = "bb02c4aa814eaba7a7a423a31b29ff41f786212c2953678cf29434e85fa0f869"

# Observe only snapshots already read while the target is ptrace-stopped.
# Keep the pinned upstream checkout clean and its stop predicate unchanged.
STOP_CHECK = "    return before->cred_refs == 2 && same_stop(before, stopped) ? 0 : EBUSY;"
STOP_DIAGNOSTICS = '''    ps5log_printf(PS5LOG_MARK,
                  "target_stop_state build=%s pid=%d fd_refs=%u fd_refs_after=%u cred_refs=%u cred_refs_after=%u threads=%u threads_after=%u suspended=%u suspended_after=%u proc_same=%d fd_same=%d cred_same=%d prison_same=%d root_same=%d jail_same=%d cwd_same=%d thread_list_same=%d stable=%d",
                  LAPY_OWNED_ID, pid, before->fd_refs, stopped->fd_refs,
                  before->cred_refs, stopped->cred_refs,
                  before->count, stopped->count,
                  before->suspended, stopped->suspended,
                  before->proc == stopped->proc, before->fd == stopped->fd,
                  before->ucred == stopped->ucred,
                  before->prison == stopped->prison,
                  before->root == stopped->root, before->jail == stopped->jail,
                  before->cwd == stopped->cwd,
                  before->count == stopped->count &&
                  !memcmp(before->threads, stopped->threads,
                          before->count * sizeof(before->threads[0])),
                  same_stop(before, stopped));
'''


def diagnostic_source():
    """Copy pinned inputs and add observations without changing stop policy."""
    staging = CACHE / f"stop-diagnostics-{LAPY_COMMIT[:7]}"
    staging.mkdir(parents=True, exist_ok=True)
    for directory in ("source", "tools"):
        shutil.copytree(LAPY / directory, staging / directory, dirs_exist_ok=True)
    source = staging / "source/owned_root_daemon.c"
    original = source.read_text()
    if original.count(STOP_CHECK) != 1:
        raise RuntimeError("Pinned target-stop check changed; refusing diagnostic overlay")
    patched = original.replace(STOP_CHECK, STOP_DIAGNOSTICS + STOP_CHECK)
    if patched.replace(STOP_DIAGNOSTICS, "", 1) != original:
        raise RuntimeError("Diagnostic overlay changed upstream behavior")
    source.write_text(patched)
    return staging


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def download(url, path, expected):
    if path.is_file() and digest(path) == expected:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".download")
    request = urllib.request.Request(url, headers={"User-Agent": "ps5-native-app-boilerplate"})
    with urllib.request.urlopen(request, timeout=120) as response, temporary.open("wb") as output:
        shutil.copyfileobj(response, output)
    actual = digest(temporary)
    if actual != expected:
        temporary.unlink(missing_ok=True)
        raise RuntimeError(f"{url}: SHA-256 {actual}, expected {expected}")
    temporary.replace(path)


def fetch_lapy():
    if LAPY.exists():
        head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=LAPY, text=True).strip()
        if head != LAPY_COMMIT:
            raise RuntimeError(f"{LAPY} is at {head}, expected {LAPY_COMMIT}")
        if subprocess.check_output(
            ["git", "status", "--porcelain", "--untracked-files=no"],
            cwd=LAPY,
            text=True,
        ):
            raise RuntimeError(f"{LAPY} has local changes; refusing to build privileged code")
        return
    CACHE.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix="lapy-git-", dir=CACHE))
    try:
        subprocess.run(["git", "init", "-q"], cwd=staging, check=True)
        subprocess.run(["git", "remote", "add", "origin",
                        "https://github.com/blackbearreloaded/PS5-Lapy-JB-Daemon.git"],
                       cwd=staging, check=True)
        subprocess.run(["git", "fetch", "-q", "--depth", "1", "origin", LAPY_COMMIT],
                       cwd=staging, check=True)
        subprocess.run(["git", "-c", "advice.detachedHead=false", "checkout", "-q", "--detach",
                        "FETCH_HEAD"], cwd=staging, check=True)
        staging.rename(LAPY)
    finally:
        if staging.exists():
            shutil.rmtree(staging)


def fetch_sdk():
    stamp = SDK / ".archive-sha256"
    if (SDK / "bin/prospero-clang").is_file() and stamp.is_file() and \
            stamp.read_text().strip() == SDK_SHA256:
        return
    if SDK.exists():
        raise RuntimeError(f"{SDK} is incomplete or does not match the pinned archive")
    archive = CACHE / "ps5-payload-sdk-v0.42.zip"
    download(SDK_URL, archive, SDK_SHA256)
    staging = Path(tempfile.mkdtemp(prefix="lapy-sdk-", dir=CACHE))
    try:
        with zipfile.ZipFile(archive) as source:
            for member in source.infolist():
                parts = Path(member.filename).parts
                if not parts or parts[0] != "ps5-payload-sdk" or ".." in parts:
                    raise RuntimeError(f"unexpected SDK archive path: {member.filename}")
                relative = parts[1:]
                if not relative:
                    continue
                target = staging.joinpath(*relative)
                if member.is_dir():
                    target.mkdir(parents=True, exist_ok=True)
                    continue
                target.parent.mkdir(parents=True, exist_ok=True)
                with source.open(member) as input_file, target.open("wb") as output:
                    shutil.copyfileobj(input_file, output)
                mode = member.external_attr >> 16
                if mode:
                    target.chmod(mode)
        (staging / ".archive-sha256").write_text(SDK_SHA256 + "\n")
        staging.rename(SDK)
    finally:
        if staging.exists():
            shutil.rmtree(staging)


def fetch_ps5log():
    header = PS5LOG / "ps5log.h"
    download(PS5LOG_URL, header, PS5LOG_SHA256)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("title")
    parser.add_argument("output", type=Path)
    parser.add_argument("--result-file", choices=["/data/prosperotube/lapy-helper.log"],
                        help="diagnostic build only; installer must create the app data directory")
    parser.add_argument("--target-stop-diagnostics", action="store_true",
                        help="log stopped snapshots without changing upstream acceptance checks")
    args = parser.parse_args()
    if not re.fullmatch(r"PPSA[0-9]{5}", args.title):
        parser.error("title must use PPSA followed by five digits")
    if args.target_stop_diagnostics and not args.result_file:
        parser.error("--target-stop-diagnostics requires --result-file")

    fetch_lapy()
    fetch_sdk()
    fetch_ps5log()
    build_source = diagnostic_source() if args.target_stop_diagnostics else LAPY
    environment = os.environ.copy()
    environment.update(PS5_PAYLOAD_SDK=str(SDK), LOGGING_CLIENT=str(PS5LOG))
    command = ["python3", "tools/build_owned_daemon.py", "--sdk", str(SDK),
               "--logging-client", str(PS5LOG), "--elf-helper", "--title", args.title]
    if args.result_file:
        command += ["--result-file", args.result_file]
    subprocess.run(command, cwd=build_source, env=environment, check=True)

    source = build_source / f"build/owned_root_helper-{args.title}"
    elf = source / "lapy.elf"
    manifest_path = source / "lapy-manifest.json"
    manifest = json.loads(manifest_path.read_text())
    full_manifest = json.loads((source / "manifest.json").read_text())
    if (full_manifest.get("result_file") != args.result_file or
            full_manifest.get("target_snapshot_delay_us") is not None):
        raise RuntimeError("Unexpected helper logging path or test-only snapshot delay")
    expected = {
        "schema": "lapy-owned-build/1",
        "target_title": args.title,
        "mode": "elf-helper",
        "max_requests": 1,
        "service": False,
        "require_client_result": False,
        "console_validated": False,
    }
    for key, value in expected.items():
        if manifest.get(key) != value:
            raise RuntimeError(f"Lapy manifest {key}: {manifest.get(key)!r}, expected {value!r}")
    if manifest.get("features") != ["root_layout_probe_retry"]:
        raise RuntimeError("Lapy helper lacks the required root-layout retry feature")
    if manifest.get("elf_sha256") != digest(elf):
        raise RuntimeError("Lapy helper differs from its generated manifest")
    if manifest.get("protocol_sha256") != PROTOCOL_SHA256:
        raise RuntimeError("Lapy helper protocol differs from the pinned upstream protocol")
    if elf.read_bytes()[:6] != b"\x7fELF\x02\x01":
        raise RuntimeError("Lapy helper is not a little-endian ELF64")

    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    shutil.copy2(elf, output / "lapy.elf")
    manifest["source_commit"] = LAPY_COMMIT
    manifest["result_file"] = args.result_file
    manifest["target_stop_diagnostics"] = args.target_stop_diagnostics
    manifest["upstream_daemon_sha256"] = digest(LAPY / "source/owned_root_daemon.c")
    manifest["compiled_daemon_sha256"] = digest(build_source / "source/owned_root_daemon.c")
    (output / "lapy-manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    shutil.copy2(LAPY / "LICENSE", output / "Lapy-MIT.txt")
    print(f"Lapy helper verified: {manifest['build_id']} {manifest['elf_sha256']}")


if __name__ == "__main__":
    main()
