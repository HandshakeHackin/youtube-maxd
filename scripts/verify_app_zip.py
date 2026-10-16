"""Verify the installable ZIP and publish its SHA256SUMS only after validation."""
from pathlib import Path, PurePosixPath
import hashlib
import json
import stat
import sys
import zipfile
from release_version import identity


def verify(archive):
    title, version, _ = identity()
    with zipfile.ZipFile(archive) as z:
        names = z.namelist()
        if len(names) != len(set(names)):
            raise ValueError("Duplicate ZIP entries")
        for item in z.infolist():
            path = PurePosixPath(item.filename)
            if (not item.filename.startswith(title + "/") or ".." in path.parts or
                    "\\" in item.filename or stat.S_ISLNK(item.external_attr >> 16)):
                raise ValueError("Unsafe ZIP entry")
            if (item.external_attr >> 16) & 0o777 != 0o777:
                raise ValueError("ZIP entry is not stored as 0777")
        if z.testzip() is not None:
            raise ValueError("Damaged ZIP entry")
        if not z.read(title + "/eboot.bin"):
            raise ValueError("Empty executable")
        meta = json.loads(z.read(title + "/sce_sys/param.json"))
        if meta.get("titleId") != title or meta.get("contentVersion") != version:
            raise ValueError("ZIP identity/version differs from the source")
    with archive.open("rb") as source:
        checksum = hashlib.file_digest(source, "sha256").hexdigest()
    (archive.parent / "SHA256SUMS").write_text(f"{checksum}  {archive.name}\n")
    print(f"{archive.name}: {len(names)} entries verified, version {version}; SHA256SUMS written")


if __name__ == "__main__":
    verify(Path(sys.argv[1]))
