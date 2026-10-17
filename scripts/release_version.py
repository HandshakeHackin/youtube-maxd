"""What a release is called, read from the files that define it.

    release_version.py           prints "PPSA99012 01.000.000 1.000.000": title, content version, version
    release_version.py --tag     prints the tag a release of this tree carries: "v01.000.000"
    release_version.py --notes   prints the release notes: docs/releases/<content version>.md
                                 when it exists, else the CHANGELOG.md section of the version

The content version is what the installed app compares with a release's tag
(native/tube_update.h), so the tag is always "v" + the content version that
scripts/prepare_native.py writes into sce_sys/param.json.
SPDX-License-Identifier: GPL-3.0-or-later
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def identity():
    source = (ROOT / "scripts/prepare_native.py").read_text(encoding="utf-8")
    title = re.search(r'^TITLE = "(PPSA\d{5})"$', source, re.M)
    content = re.search(r'contentVersion="(\d\d\.\d{3}\.\d{3})"', source)
    version = re.search(r'"VERSION"\)\.write_text\("(\d+\.\d+\.\d+)\\n"\)', source)
    if not title or not content or not version:
        raise SystemExit("scripts/prepare_native.py does not name the title, content version and version")
    return title.group(1), content.group(1), version.group(1)


def notes(content, version):
    written = ROOT / "docs/releases" / (content + ".md")
    if written.is_file():
        return written.read_text(encoding="utf-8").strip() + "\n"
    lines, found = [], False
    for line in (ROOT / "CHANGELOG.md").read_text(encoding="utf-8").splitlines():
        if line.strip() == "## " + version:
            found = True
        elif found and line.startswith("## "):
            break
        elif found:
            lines.append(line)
    text = "\n".join(lines).strip()
    if not text:
        raise SystemExit(f"No release notes: neither docs/releases/{content}.md nor a CHANGELOG.md section '## {version}'")
    return text + "\n"


def main(arguments):
    title, content, version = identity()
    if arguments == ["--tag"]:
        print("v" + content)
    elif arguments == ["--notes"]:
        sys.stdout.write(notes(content, version))
    elif not arguments:
        print(title, content, version)
    else:
        raise SystemExit(__doc__)


if __name__ == "__main__":
    main(sys.argv[1:])
