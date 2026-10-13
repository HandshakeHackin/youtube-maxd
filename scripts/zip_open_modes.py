"""Rewrite a release ZIP so every entry is stored open to all (0777).

The console only starts an app whose files are open to every user. A tool that
unpacks the release ZIP and keeps its permissions would otherwise leave 0644
files, and the console answers "Can't start the game or app". The contents are
not changed, only the stored permissions.

    zip_open_modes.py ARCHIVE.zip [--label TEXT]

--label also adds <app folder>/build-label.txt holding TEXT, which names a
pull request's build inside the app folder.

Adapted from tools/zip-open-modes.py of ps5-native-app-boilerplate.
Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later
"""
import os
import sys
import zipfile


def rewrite(path, label=None):
    temporary = path + ".tmp"
    with zipfile.ZipFile(path) as source, zipfile.ZipFile(temporary, "w") as target:
        top = source.namelist()[0].split("/")[0]
        for info in source.infolist():
            data = source.read(info)
            folder = info.is_dir()
            info.create_system = 3  # Unix: the high half of external_attr is the mode
            info.external_attr = ((0o040777 if folder else 0o100777) << 16) | (0x10 if folder else 0)
            target.writestr(info, data, compress_type=info.compress_type)
        if label is not None:
            info = zipfile.ZipInfo(top + "/build-label.txt")
            info.create_system = 3
            info.external_attr = 0o100777 << 16
            target.writestr(info, label.strip() + "\n", compress_type=zipfile.ZIP_DEFLATED)
    os.replace(temporary, path)
    with zipfile.ZipFile(path) as check:
        wrong = [i.filename for i in check.infolist() if (i.external_attr >> 16) & 0o777 != 0o777]
        if wrong or check.testzip() is not None:
            raise SystemExit(f"ZIP check failed: {wrong[:3]}")
        print(f"{path}: {len(check.infolist())} entries stored as 0777")


if __name__ == "__main__":
    if len(sys.argv) == 2:
        rewrite(sys.argv[1])
    elif len(sys.argv) == 4 and sys.argv[2] == "--label":
        rewrite(sys.argv[1], sys.argv[3])
    else:
        raise SystemExit(__doc__)
