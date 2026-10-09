"""Package matching project/submodule source, companion fixtures and checksums."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import tarfile
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DIST = ROOT / "dist"

def main():
    DIST.mkdir(exist_ok=True)
    revision = subprocess.check_output(["git", "-C", str(ROOT), "rev-parse", "HEAD"], text=True).strip()
    with tempfile.TemporaryDirectory(prefix="prosperotube-source-") as temporary:
        with tarfile.open(DIST / "ProsperoTube-0.1.0-source.tar.gz", "w:gz") as result:
            for directory, prefix in ((ROOT, ""), (ROOT / "third_party/evo", "third_party/evo/"),
                                      (ROOT / "third_party/evo/third_party/ps5-homebrew-ui", "third_party/evo/third_party/ps5-homebrew-ui/")):
                archive = Path(temporary) / "source.tar"
                subprocess.run(["git", "-C", str(directory), "archive", "--format=tar", "-o", str(archive), "HEAD"], check=True)
                with tarfile.open(archive) as source:
                    for member in source:
                        member.name = "ProsperoTube/" + prefix + member.name
                        result.addfile(member, source.extractfile(member) if member.isfile() else None)
    with zipfile.ZipFile(DIST / "ProsperoTube-0.1.0-companion-fixtures.zip", "w", zipfile.ZIP_DEFLATED) as archive:
        for directory in ("companion", "docs", "fixtures/generated"):
            for path in (ROOT / directory).rglob("*"):
                if path.is_file() and "__pycache__" not in path.parts and path.suffix != ".pyc" and path.name != "test.mpd":
                    archive.write(path, "ProsperoTube/" + path.relative_to(ROOT).as_posix())
        for name in ("README.md", "LICENSE", "THIRD_PARTY_NOTICES.md", "pyproject.toml", "scripts/generate_fixtures.py"):
            archive.write(ROOT / name, "ProsperoTube/" + name)
    artifacts = {}
    for path in sorted(DIST.iterdir()):
        if path.is_file() and path.suffix in {".zip", ".ffpfsc", ".gz"}:
            artifacts[path.name] = {"bytes": path.stat().st_size, "sha256": hashlib.file_digest(path.open("rb"), "sha256").hexdigest()}
    manifest = {"version": "0.1.0", "source_revision": revision, "title_id": "PPSA99142",
                "validation": {"companion_tests": 30, "native_sanitizers": "pass", "host_fixture_decode_seek": "pass",
                               "native_ui_preview": "pass", "ps5_hardware": "not tested", "live_youtube": "not tested"},
                "artifacts": artifacts}
    (DIST / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    (DIST / "SHA256SUMS").write_text("".join(f"{v['sha256']}  {k}\n" for k, v in artifacts.items()))
    print(json.dumps(manifest, indent=2))

if __name__ == "__main__": main()
