#!/usr/bin/env python3
"""Restore Git metadata for a downloaded source ZIP without replacing source.

Dependency files are supplied in the source archive; pinned Git objects are
fetched to satisfy upstream's archive/pin checks. Existing work stays intact.
SPDX-License-Identifier: GPL-3.0-or-later
"""
from pathlib import Path
import json
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PINS = [
    ("third_party/evo", "https://github.com/sainsaji/EVO-PLAYER-PS5.git", "9715f6ee9bf9b53461d6fdf9a2e11b4c48357783"),
    ("third_party/evo/third_party/ps5-homebrew-ui", "https://github.com/blackbearreloaded/ps5-homebrew-ui.git", "4bd942579dd981b3df9c740438489ca6a614ddc0"),
    ("third_party/maxd-ui-kit-source", "https://github.com/blackbearreloaded/ps5-homebrew-ui.git", "bbe19054ee2e7ec069254775273aeb8846c4bd3e"),
]


def git(folder, *args):
    return subprocess.run(["git", "-C", str(folder), *args], check=True)


def main():
    # ZIP clients such as Python's extractall can discard all executable bits,
    # including the project's directly invoked fetch/build scripts.
    manifest = ROOT / "SOURCE_MANIFEST.json"
    if manifest.exists():
        for entry in json.loads(manifest.read_text()):
            target = (ROOT / entry["file"]).resolve()
            if not target.is_relative_to(ROOT):
                raise ValueError("Source manifest path escapes the checkout")
            executable = int(entry.get("mode", 0)) & 0o111
            if executable and target.is_file():
                target.chmod(target.stat().st_mode | executable)
    for name, address, revision in PINS:
        folder = ROOT / name
        folder.mkdir(parents=True, exist_ok=True)
        if (folder / ".git").exists():
            continue
        git(folder, "init", "-q")
        git(folder, "remote", "add", "origin", address)
        git(folder, "fetch", "--depth=1", "origin", revision)
        # Index only: supplied source is never overwritten by this command.
        git(folder, "reset", "--mixed", revision)
        # Some ZIP extractors discard executable modes; restore them from the
        # authentic pinned index before checking that source content matches.
        listing = subprocess.check_output(["git", "-C", str(folder), "ls-files", "-s", "-z"])
        for row in listing.split(b"\0"):
            if row.startswith(b"100755 "):
                target = folder / row.split(b"\t", 1)[1].decode()
                if target.is_file():
                    target.chmod(target.stat().st_mode | 0o111)
        git(folder, "diff", "--quiet", "--ignore-submodules=all", revision)
    if not (ROOT / ".git").exists():
        git(ROOT, "init", "-q")
        # The substituted kit is a separately checked local source input, not
        # another root submodule; keep it out of the snapshot's Git index.
        exclude = ROOT / ".git/info/exclude"
        with exclude.open("a") as out:
            out.write("\n/third_party/maxd-ui-kit-source/\n")
        git(ROOT, "add", ".")
        git(ROOT, "-c", "user.name=Source snapshot", "-c", "user.email=snapshot@localhost",
            "commit", "-q", "-m", "YouTube Max’d source archive snapshot")
    print("Pinned source checkout ready; run make deps and make zip.")


if __name__ == "__main__":
    main()
