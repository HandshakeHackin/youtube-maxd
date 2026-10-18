#!/usr/bin/env python3
"""Package the verified fork and complete corresponding project/source inputs.

No publication or deployment. Secrets, Git metadata and build caches are excluded.
SPDX-License-Identifier: GPL-3.0-or-later
"""
from pathlib import Path
import hashlib
import json
import os
import shutil
import io
import stat
import subprocess
import tarfile
import zipfile
from release_version import identity
from verify_app_zip import verify
from bootstrap_local_voice import MODEL_SHA, MODEL_SIZE

ROOT = Path(__file__).resolve().parents[1]
DIST = ROOT / "dist"
SKIP = {".git", ".deps", ".local", ".cache", ".venv", "node_modules", "__pycache__", ".pytest_cache", "build", "dist", "output"}
PRIVATE = {"google-oauth.json", "google-tokens.json", "oauth-profiles.json", "cookies.txt", "token", ".env", ".env.local"}
COMPRESSED = {".png", ".jpg", ".jpeg", ".webp", ".avif", ".mp4", ".webm", ".m4a", ".ogg", ".zip", ".xz", ".gz", ".woff2"}


def compression(name):
    return zipfile.ZIP_STORED if Path(name).suffix.lower() in COMPRESSED else zipfile.ZIP_DEFLATED


def sha(path):
    with path.open("rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()


def files(directory):
    for folder, dirs, names in os.walk(directory):
        dirs[:] = sorted(d for d in dirs if d not in SKIP and not (Path(folder) == ROOT / "fixtures" and d == "generated"))
        if Path(folder) == ROOT / "third_party":
            dirs[:] = [d for d in dirs if d not in {"evo", "maxd-ui-kit-source"}]
        for name in sorted(names):
            if Path(folder) == ROOT and name == "SOURCE_MANIFEST.json":
                continue  # Regenerate distribution metadata when repacking a source ZIP.
            if name in PRIVATE or name.endswith((".pyc", ".log", ".download")) or name == ".git":
                continue
            p = Path(folder) / name
            if p.is_symlink():
                raise ValueError(f"Unexpected source symlink: {p}")
            yield p


def source_archive(path):
    records = []
    seen = set()
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED, compresslevel=1) as archive:
        for p in files(ROOT):
            rel = p.relative_to(ROOT).as_posix()
            name = "YouTube-Maxd/" + rel
            if name in seen:
                raise ValueError(name)
            seen.add(name)
            archive.write(p, name, compress_type=compression(name))
            records.append({"file": rel, "sha256": sha(p), "mode": stat.S_IMODE(p.stat().st_mode)})
        # Include every tracked pinned dependency input, even directories named
        # build/output that would otherwise resemble generated working files.
        for repo, prefix, revision in (
            (ROOT / "third_party/evo", "third_party/evo/", "9715f6ee9bf9b53461d6fdf9a2e11b4c48357783"),
            (ROOT / "third_party/evo/third_party/ps5-homebrew-ui", "third_party/evo/third_party/ps5-homebrew-ui/", "4bd942579dd981b3df9c740438489ca6a614ddc0"),
        ):
            raw = subprocess.check_output(["git", "-C", str(repo), "archive", revision])
            with tarfile.open(fileobj=io.BytesIO(raw)) as dependency:
                for entry in dependency:
                    if entry.issym() or entry.islnk():
                        raise ValueError("Unsupported source link: " + entry.name)
                    if not entry.isfile():
                        continue
                    rel = prefix + entry.name
                    name = "YouTube-Maxd/" + rel
                    assert name not in seen
                    seen.add(name)
                    body = dependency.extractfile(entry).read()
                    info = zipfile.ZipInfo(name)
                    info.create_system = 3
                    info.external_attr = (stat.S_IFREG | entry.mode) << 16
                    archive.writestr(info, body, compress_type=compression(name), compresslevel=1)
                    records.append({"file": rel, "sha256": hashlib.sha256(body).hexdigest(), "mode": entry.mode})
        # The substituted UI kit is an independent pinned build input.
        kit = ROOT / ".deps/ui-kit/bbe19054ee2e7ec069254775273aeb8846c4bd3e"
        if not (kit / "LICENSE").exists():
            raise ValueError("Verified UI-kit source is missing")
        for p in files(kit):
            rel = "third_party/maxd-ui-kit-source/" + p.relative_to(kit).as_posix()
            archive.write(p, "YouTube-Maxd/" + rel, compress_type=compression(rel))
            records.append({"file": rel, "sha256": sha(p), "mode": stat.S_IMODE(p.stat().st_mode)})
        archive.writestr("YouTube-Maxd/SOURCE_MANIFEST.json", json.dumps(records, indent=2) + "\n")
        assert archive.testzip() is None


def main():
    title, content, _ = identity()
    DIST.mkdir(exist_ok=True)
    candidate = DIST / (title + ".zip")
    verify(candidate)
    sources = ROOT / "runtime-sources/SOURCES.json"
    if not sources.exists():
        raise ValueError("Collect native sources before distributing the binary")
    for entry in json.loads(sources.read_text()):
        p = sources.parent / entry["file"]
        if sha(p) != entry["sha256"]:
            raise ValueError("Source input changed: " + str(p))
    with zipfile.ZipFile(candidate) as archive:
        meta = json.loads(archive.read(title + "/sce_sys/param.json"))
        assert meta["localizedParameters"]["en-US"]["titleName"] == "YouTube Max’d"
        assert meta["contentVersion"] == content
        assert archive.read(title + "/sce_sys/icon0.png") == (ROOT / "native/branding/icon0.png").read_bytes()
        assert all(Path(name).name not in PRIVATE for name in archive.namelist())
        assert title + "/assets/prosperotube-notices/MAXD_CHANGES.md" in archive.namelist()
        shaders = json.loads(archive.read(title + "/assets/prosperotube-notices/MAXD-SHADERS.json"))
        assert shaders["evo_commit"] == "9715f6ee9bf9b53461d6fdf9a2e11b4c48357783"
        assert shaders["mode"] in {"evo-pinned", "transformed"}
        model = archive.read(title + "/assets/voice/ggml-tiny.bin")
        assert len(model) == MODEL_SIZE and hashlib.sha256(model).hexdigest() == MODEL_SHA
        assert title + "/assets/voice/WHISPER-CPP-MIT.txt" in archive.namelist()
        assert {"whisper.cpp", "Whisper tiny model"} <= {r["name"] for r in json.loads(sources.read_text())}
    binary, source = DIST / "YouTube-Maxd-PS5.zip", DIST / "YouTube-Maxd-source.zip"
    shutil.copyfile(candidate, binary)
    source_archive(source)
    icon = DIST / "YouTube-Maxd-icon.png"
    shutil.copyfile(ROOT / "native/branding/icon0.png", icon)
    for p in (binary, source, icon):
        print(f"{p.name}: {p.stat().st_size / 1024 / 1024:.1f} MiB")
    (DIST / "SHA256SUMS").write_text("".join(f"{sha(p)}  {p.name}\n" for p in (binary, source, icon)))


if __name__ == "__main__":
    main()
