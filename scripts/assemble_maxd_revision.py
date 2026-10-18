#!/usr/bin/env python3
"""Assemble the 1.3.0 preview's verified binaries, source and review evidence.

Does not compile, execute a PS5 binary, publish, or replace a release asset.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys
import zipfile

from bootstrap_local_voice import MODEL_SHA, MODEL_SIZE
from verify_app_zip import verify

ROOT = Path(__file__).resolve().parents[1]
TAG = "v1.3.0-preview.1"
SOURCE_COMMIT = "78718a740137f1a111aa73926d74d22d8af8b1d4"
APP_SHA = "22b4bf2c8b7fa67f3e11198645fab9746f7381170b12dfc82ce709814792e593"
EBOOT_SHA = "f539b2007b205c2623524c1946dc5cbc21b2a84070341a4f49e499a8a8c507f2"


def sha(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def assemble(source_dir, out, evidence):
    source_dir, out, evidence = source_dir.resolve(), out.resolve(), evidence.resolve()
    binary = source_dir / "YouTube-Maxd-PS5.zip"
    source = source_dir / "YouTube-Maxd-source.zip"
    icon = source_dir / "YouTube-Maxd-icon.png"
    if source_dir == out:
        raise ValueError("Output must be separate from the input distributions")
    if sha(binary) != APP_SHA:
        raise ValueError("This publisher accepts only the verified 1.3.0 preview app ZIP")
    out.mkdir(parents=True, exist_ok=True)
    allowed = {"PPSA99991.zip", source.name, icon.name, "RELEASE_NOTES.md", "TESTING.txt",
               "TEST_RESULTS.txt", "catalog-scan.txt", "native-validation.json", "SHA256SUMS"}
    if any(p.name not in allowed or not p.is_file() for p in out.iterdir()):
        raise ValueError("Output contains unrelated files")
    shutil.copyfile(binary, out / "PPSA99991.zip")
    verify(out / "PPSA99991.zip")
    with zipfile.ZipFile(binary) as archive:
        meta = json.loads(archive.read("PPSA99991/sce_sys/param.json"))
        if meta["localizedParameters"]["en-US"]["titleName"] != "YouTube Max’d":
            raise ValueError("Unexpected application name")
        executable = archive.read("PPSA99991/eboot.bin")
        if hashlib.sha256(executable).hexdigest() != EBOOT_SHA:
            raise ValueError("Unexpected executable")
        model = archive.read("PPSA99991/assets/voice/ggml-tiny.bin")
        if len(model) != MODEL_SIZE or hashlib.sha256(model).hexdigest() != MODEL_SHA:
            raise ValueError("Wrong voice model")
        if archive.read("PPSA99991/sce_sys/icon0.png") != icon.read_bytes():
            raise ValueError("Release icon differs from the packaged icon")
        helper_paths = ("PPSA99991/lapy.elf", "PPSA99991/self-updater.elf")
        helpers = [{"file": p, "sha256": hashlib.sha256(archive.read(p)).hexdigest()}
                   for p in helper_paths]
        entries = len(archive.infolist())
    with zipfile.ZipFile(source) as archive:
        if archive.testzip() is not None:
            raise ValueError("Damaged source distribution")
        manifest = json.loads(archive.read("YouTube-Maxd/SOURCE_MANIFEST.json"))
        if len({r["file"] for r in manifest}) != len(manifest):
            raise ValueError("Duplicate source manifest paths")
        for record in manifest:
            body = archive.read("YouTube-Maxd/" + record["file"])
            if hashlib.sha256(body).hexdigest() != record["sha256"]:
                raise ValueError("Source manifest hash mismatch: " + record["file"])
        upstream = json.loads(archive.read("YouTube-Maxd/UPSTREAM.json"))
        if (upstream["title_id"], upstream["content_version"]) != ("PPSA99991", "01.003.000"):
            raise ValueError("Source identity differs from the app")
    shutil.copyfile(source, out / source.name)
    shutil.copyfile(icon, out / icon.name)
    shutil.copyfile(ROOT / "docs/releases/v1.3.0-preview.1.md", out / "RELEASE_NOTES.md")
    for name in ("TESTING.txt", "TEST_RESULTS.txt", "catalog-scan.txt"):
        shutil.copyfile(evidence / name, out / name)
    validation = {
        "application": "YouTube Max’d", "version": "1.3.0", "release_tag": TAG,
        "title_id": "PPSA99991", "content_version": "01.003.000",
        "application_source_commit": SOURCE_COMMIT,
        "source_archive_commit": "1435ccdbad5f2d815de8f3440c3765ab785319fe",
        "pip_default_enabled": False, "linked_google_profiles": "research only",
        "target": {"firmware": "11.00", "launcher": "ShadowMount+"},
        "hardware_tested": False, "live_google_account_tested": False,
        "live_youtube_playback_tested": False,
        "app_zip": {"file": "PPSA99991.zip", "size": binary.stat().st_size,
                    "sha256": APP_SHA, "entries": entries, "zip_crc_verified": True,
                    "identity_verified": True, "entry_permissions": "0777"},
        "executable_sha256": EBOOT_SHA,
        "source_archive": {"file": source.name, "size": source.stat().st_size,
                           "sha256": sha(source), "manifest_files_verified": len(manifest)},
        "icon": {"file": icon.name, "sha256": sha(icon), "matches_app": True},
        "voice_model": {"size": MODEL_SIZE, "sha256": MODEL_SHA, "verified": True},
        "helpers": helpers,
        "host_test_evidence": {
            "kind": "Recorded development results; not rerun by release assembly",
            "reference": "docs/MAXD_NEXT_VALIDATION.md", "dates": ["2026-10-17", "2026-10-18"]},
        "catalog_scan": {"file": "catalog-scan.txt", "static_only": True,
                         "catalog_commit": "c3e61941fda62b4624f088a0f5344ff729bcd051"},
        "store_publication": "Catalog update submitted separately after immutable release publication",
    }
    (out / "native-validation.json").write_text(json.dumps(validation, indent=2, ensure_ascii=False) + "\n")
    names = sorted(allowed - {"SHA256SUMS"})
    (out / "SHA256SUMS").write_text("".join(f"{sha(out / n)}  {n}\n" for n in names))
    print(f"Assembled {len(allowed)} release files; {entries} app entries and {len(manifest)} source files verified")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dist", type=Path, default=ROOT / "dist")
    parser.add_argument("--out", type=Path, default=ROOT / "dist/release-v1.3.0-preview.1")
    parser.add_argument("--evidence", type=Path, default=ROOT / "docs/releases/v1.3.0-preview.1")
    args = parser.parse_args()
    try:
        assemble(args.dist, args.out, args.evidence)
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        sys.exit(str(error))
