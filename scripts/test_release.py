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
assert title == "PPSA99142" and re.fullmatch(r"\d\d\.\d{3}\.\d{3}", content) and re.fullmatch(r"\d+\.\d+\.\d+", version)
assert run(ROOT / "scripts/release_version.py", "--tag").strip() == "v" + content
# The version About shows and the one the release is named after are the same.
assert f'#define TUBE_ABOUT_CHANGELOG_VERSION "{version}"' in (ROOT / "native/tube_about_data.h").read_text()
notes = run(ROOT / "scripts/release_version.py", "--notes")
written = ROOT / "docs/releases" / (content + ".md")
if written.is_file():
    assert notes.strip() == written.read_text().strip()
else:
    assert "- " in notes and "## " not in notes and notes.strip() in (ROOT / "CHANGELOG.md").read_text()

# The tag the workflow builds is the tag the app understands, and the address it asks is the one constant.
header = (ROOT / "native/tube_update.h").read_text()
assert header.count("api.github.com") == 1
assert '#define TUBE_UPDATE_MANIFEST_URL "https://api.github.com/repos/blackbearreloaded/ProsperoTube/releases?per_page=10"' in header
assert f'#define TUBE_UPDATE_TITLE "{title}"' in header
for source in ("tube_update.c", "tube_update_app.c", "tube_update_http.c", "tube_update_zip.c", "tube_provider.c"):
    assert "api.github.com/repos" not in (ROOT / "native" / source).read_text(), source

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

workflow = (ROOT / ".github/workflows/build.yml").read_text()
trigger = workflow[workflow.index("\non:\n"):workflow.index("\npermissions:")]
# Pushes build nothing but version tags; pull requests and a manual run build.
assert "branches" not in trigger and "pull_request:" in trigger and "workflow_dispatch:" in trigger
assert re.findall(r'^\s+- "(.+)"$', trigger, re.M) == ["v[0-9][0-9].[0-9][0-9][0-9].[0-9][0-9][0-9]"]
assert "schedule" not in trigger
uses = re.findall(r"uses: (\S+)", workflow)
assert uses and all(re.fullmatch(r"actions/[a-z-]+@[0-9a-f]{40}", action) for action in uses), uses
assert "clang-tidy is a local check" in workflow and "tidy" not in workflow.replace("clang-tidy is a local check", "")
assert "--prerelease" in workflow and "--verify-tag" in workflow and "--clobber" not in workflow and "delete-asset" not in workflow
assert "zip_open_modes.py" in workflow and "sha256sum \"$TITLE_ID.zip\" > SHA256SUMS" in workflow
assert "secrets." not in workflow   # nothing but the run's own token
assert "ffpfsc" not in workflow     # no image files from CI
print(f"PASS: release {title} {content} ({version}): tag, notes, ZIP entries 0777, workflow triggers and pinned actions")
