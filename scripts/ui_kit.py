#!/usr/bin/env python3
"""The UI kit (ps5-homebrew-ui) as a pinned build input.

ProsperoTube never carries the kit in its own tree: this fetches the commit
named below into .deps/ui-kit/<commit> and checks every file the build uses
against the hashes recorded in scripts/ui_kit.lock.json. The commit is taken
from the first of these that has it:

  PROSPEROTUBE_UI_KIT   a local clone or a git address (optional)
  ~/ps5-homebrew-ui     the owner's clone, when it exists
  the kit's GitHub address, with whatever credentials git has

`python3 scripts/ui_kit.py` prints the directory of the checked kit.
`python3 scripts/ui_kit.py --lock` writes the lock file again after a pin change.
"""
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tarfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
COMMIT = "bbe19054ee2e7ec069254775273aeb8846c4bd3e"
ADDRESS = "https://github.com/blackbearreloaded/ps5-homebrew-ui.git"
LOCK = ROOT / "scripts/ui_kit.lock.json"
# What the app and its tests take from the kit; nothing else is hashed or used.
USED = ("src/", "assets/fonts/", "assets/audio/", "host/platform_host.cpp", "third_party/stb/", "tools/font-baker/",
        "third_party/fonts/", "LICENSE", "THIRD_PARTY_NOTICES.md")


def directory():
    return ROOT / ".deps/ui-kit" / COMMIT


def sources():
    chosen = os.environ.get("PROSPEROTUBE_UI_KIT")
    if chosen:
        yield chosen
    bundled = ROOT / "third_party/maxd-ui-kit-source"
    if (bundled / ".git").exists():
        yield str(bundled)
    home = pathlib.Path.home() / "ps5-homebrew-ui"
    if (home / ".git").exists():
        yield str(home)
    yield ADDRESS


def has_commit(clone):
    return subprocess.run(["git", "-C", clone, "cat-file", "-e", COMMIT + "^{commit}"], capture_output=True).returncode == 0


def fetch(target):
    staging = target.with_name(target.name + ".tmp")
    shutil.rmtree(staging, ignore_errors=True)
    staging.mkdir(parents=True)
    archive = staging.with_suffix(".tar")
    for source in sources():
        clone = source
        if not pathlib.Path(source).exists():
            # A git address: fetch the one commit into a scratch repository.
            clone = str(staging.with_suffix(".git"))
            shutil.rmtree(clone, ignore_errors=True)
            subprocess.run(["git", "init", "--quiet", "--bare", clone], check=True)
            got = subprocess.run(["git", "-C", clone, "fetch", "--quiet", "--depth", "1", source, COMMIT], capture_output=True)
            if got.returncode != 0:
                shutil.rmtree(clone, ignore_errors=True)
                continue
        if not has_commit(clone):
            continue
        subprocess.run(["git", "-C", clone, "archive", "--format=tar", "-o", str(archive), COMMIT], check=True)
        with tarfile.open(archive) as tar:
            tar.extractall(staging, filter="data")
        archive.unlink()
        shutil.rmtree(staging.with_suffix(".git"), ignore_errors=True)
        shutil.rmtree(target, ignore_errors=True)
        staging.rename(target)
        return
    shutil.rmtree(staging, ignore_errors=True)
    raise RuntimeError(f"UI kit commit {COMMIT} was not found: set PROSPEROTUBE_UI_KIT to a clone of ps5-homebrew-ui that has it")


def hashes(target):
    found = {}
    for path in sorted(target.rglob("*")):
        relative = path.relative_to(target).as_posix()
        if path.is_file() and (relative in USED or any(relative.startswith(prefix) for prefix in USED if prefix.endswith("/"))):
            found[relative] = hashlib.sha256(path.read_bytes()).hexdigest()
    return found


def checkout():
    """The directory of the pinned kit, fetched and verified."""
    target = directory()
    stamp = target / ".prosperotube-verified"
    if stamp.exists():
        return target
    if not target.exists():
        fetch(target)
    lock = json.loads(LOCK.read_text(encoding="utf-8"))
    if lock["commit"] != COMMIT:
        raise RuntimeError("scripts/ui_kit.lock.json is for another commit: run python3 scripts/ui_kit.py --lock")
    found = hashes(target)
    if found != lock["files"]:
        wrong = sorted(name for name in set(found) | set(lock["files"]) if found.get(name) != lock["files"].get(name))
        raise RuntimeError(f"UI kit files differ from the lock file: {', '.join(wrong[:8])}")
    stamp.write_text(COMMIT + "\n", encoding="utf-8")
    return target


def main():
    if sys.argv[1:] == ["--lock"]:
        target = directory()
        shutil.rmtree(target, ignore_errors=True)
        fetch(target)
        files = hashes(target)
        LOCK.write_text(json.dumps({"commit": COMMIT, "files": files}, indent=1) + "\n", encoding="utf-8")
        print(f"{LOCK.relative_to(ROOT)}: {len(files)} files of {COMMIT[:7]}")
        return
    print(checkout())


if __name__ == "__main__":
    main()
