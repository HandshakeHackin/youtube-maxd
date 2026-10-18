"""The release side of updates: the tag a tree is released under, its notes,
the ZIP's permissions, and the rules the build workflow has to keep."""
from pathlib import Path
import json
import re
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def run(*arguments):
    return subprocess.run([sys.executable, *map(str, arguments)], check=True, capture_output=True, text=True).stdout


title, content, version = run(ROOT / "scripts/release_version.py").split()
assert title == "PPSA99991" and re.fullmatch(r"\d\d\.\d{3}\.\d{3}", content) and re.fullmatch(r"\d+\.\d+\.\d+", version)
major, minor, patch = content.split(".")
assert version == f"{int(major)}.{minor}.{patch}"
assert f'__version__ = "{version}"' in (ROOT / "companion/prosperotube/__init__.py").read_text()
assert f'version = "{version}"' in (ROOT / "pyproject.toml").read_text()
assert run(ROOT / "scripts/release_version.py", "--tag").strip() == "v" + content
# The version About shows and the one the release is named after are the same.
assert f'#define TUBE_ABOUT_CHANGELOG_VERSION "{version}"' in (ROOT / "native/tube_about_data.h").read_text()
notes = run(ROOT / "scripts/release_version.py", "--notes")
written = ROOT / "docs/releases" / (content + ".md")
if written.is_file():
    assert notes.strip() == written.read_text().strip()
else:
    assert "- " in notes and "## " not in notes and notes.strip() in (ROOT / "CHANGELOG.md").read_text()

# Releases stay on GitHub; update authority is exclusively the signed catalog.
header = (ROOT / "native/tube_update.h").read_text()
assert f'#define TUBE_UPDATE_TITLE "{title}"' in header
for source in ("tube_update.c", "tube_update_app.c", "tube_provider.c"):
    assert "api.github.com/repos" not in (ROOT / "native" / source).read_text(), source
assert 'https://homebrew.page/api/v1/' in (ROOT / "native/update/kit/self_update.h").read_text()

with tempfile.TemporaryDirectory(prefix="tube-release-") as directory:
    archive = Path(directory) / (title + ".zip")
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as made:
        made.writestr(title + "/", "")
        for name, mode in (("eboot.bin", 0o100755), ("sce_sys/param.json", 0o100644), ("assets/a.txt", 0o100600)):
            info = zipfile.ZipInfo(f"{title}/{name}")
            info.external_attr = mode << 16
            made.writestr(info, json.dumps({"name": name}))
    before = {i.filename: zipfile.ZipFile(archive).read(i) for i in zipfile.ZipFile(archive).infolist()}
    run(ROOT / "scripts/zip_open_modes.py", archive)
    with zipfile.ZipFile(archive) as check:
        assert all((i.external_attr >> 16) & 0o777 == 0o777 for i in check.infolist()) and len(check.infolist()) == 4
        assert {i.filename: check.read(i) for i in check.infolist()} == before
    run(ROOT / "scripts/zip_open_modes.py", archive, "--label", "PR 7, abc1234")
    with zipfile.ZipFile(archive) as check:
        assert check.read(title + "/build-label.txt") == b"PR 7, abc1234\n"
        assert all((i.external_attr >> 16) & 0o777 == 0o777 for i in check.infolist()) and len(check.infolist()) == 5

# Local candidates must be installable by tools that retain ZIP permissions,
# just like CI releases, and must not copy unrelated stale archives.
build = (ROOT / "scripts/build_ps5.sh").read_text()
normalize = 'python3 "${root}/scripts/zip_open_modes.py" "${work}/output/app/PPSA99991.zip"'
copy_zip = 'cp "${work}/output/app/PPSA99991.zip" "${destination}/"'
assert build.index(normalize) < build.index(copy_zip)
assert "-name '*.zip'" not in build

workflows = sorted((ROOT / ".github/workflows").glob("*.yml"))
assert [path.name for path in workflows] == ["build-cache.yml", "build.yml", "publish-maxd-preview.yml", "publish-maxd-revision.yml"]
for path in workflows:
    text = path.read_text()
    assert "# SPDX-License-Identifier: GPL-3.0-or-later\n" in text, path.name
    if not path.name.startswith("publish-maxd-"):
        assert text.startswith("# ProsperoTube - "), path.name
        assert "# Copyright (C) 2026 BlackBearReloaded\n" in text, path.name
    uses = re.findall(r"uses: (\S+)", text)
    assert all(re.fullmatch(r"actions/[a-z-]+(/[a-z-]+)?@[0-9a-f]{40}", action) for action in uses), uses
    assert "secrets." not in text   # nothing but the run's own token
    assert "ffpfsc" not in text     # no image files from CI

workflow = (ROOT / ".github/workflows/build.yml").read_text()
trigger = workflow[workflow.index("\non:\n"):workflow.index("\npermissions:")]
# Pushes build nothing but version tags; pull requests and a manual run build.
assert "branches" not in trigger and "pull_request:" in trigger and "workflow_dispatch:" in trigger
assert re.findall(r'^\s+- "(.+)"$', trigger, re.M) == ["v[0-9][0-9].[0-9][0-9][0-9].[0-9][0-9][0-9]"]
assert "schedule" not in trigger and "snapshot:" in trigger
assert "clang-tidy is a local check" in workflow and "tidy" not in workflow.replace("clang-tidy is a local check", "")
assert "--prerelease" in workflow and "--verify-tag" in workflow
# The build never writes to a release that already has files; build-cache.yml replaces snapshots.
assert "--clobber" not in workflow and "delete-asset" not in workflow and "gh release upload build-cache" not in workflow
assert "zip_open_modes.py" in workflow and "make verify" in workflow and "make zip" in workflow
assert '--label "$BUILD_LABEL"' in workflow and 'artifact=${GITHUB_REPOSITORY##*/}-PR$PR_NUMBER-$short' in workflow
attest = workflow[workflow.index("- name: Attest the release ZIP"):]
assert attest.startswith("- name: Attest the release ZIP\n        if: github.event_name != 'pull_request' && !github.event.repository.private\n"
                         "        uses: actions/attest@1e69f48acb82d1966a394da916b4c1698aa569d6 # v4.2.2\n")
assert workflow.index("- name: Label and check the release ZIP") < workflow.index("- name: Attest the release ZIP") < workflow.index("- name: Upload build")
assert "id-token: write" in workflow and "attestations: write" in workflow
for snapshot in ("ccache-snapshot", "shader-compiler-snapshot"):
    assert f"name: {snapshot}\n" in workflow
cache = (ROOT / ".github/workflows/build-cache.yml").read_text()
assert "workflow_run:" in cache and 'workflows: ["Build"]' in cache and cache.count("--clobber") == 2
print(f"PASS: release {title} {content} ({version}): tag, notes, ZIP entries 0777, workflow triggers and pinned actions")
