"""Package matching project/submodule source, companion fixtures and checksums."""
from pathlib import Path
import hashlib
import io
import json
import os
import subprocess
import tarfile
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DIST = ROOT / "dist"


def companion_entries(root, revision):
    """Source bytes come from Git; generated media uses an explicit allowlist."""
    source = subprocess.check_output(["git", "-C", str(root), "archive", revision,
        "companion", "docs", "README.md", "LICENSE", "THIRD_PARTY_NOTICES.md",
        "pyproject.toml", "scripts/generate_fixtures.py"])
    with tarfile.open(fileobj=io.BytesIO(source)) as archive:
        for member in archive:
            if member.isfile():
                yield member.name, archive.extractfile(member).read()
    fixtures = root / "fixtures/generated"
    names = ["video.mp4", "video-av1.mp4", "video-hevc.mp4", "audio.m4a", "video.webm", "audio.webm", "muxed.mp4",
             "fixture.m3u8", "thumb.png", "captions.vtt"]
    names.extend(f"fixture{index}.ts" for index in range(1000)
                 if (fixtures / f"fixture{index}.ts").exists())
    for name in names:
        path = fixtures / name
        if path.is_symlink() or not path.resolve().is_relative_to(fixtures.resolve()):
            raise ValueError("Generated fixture must be a regular local file")
        yield "fixtures/generated/" + name, path.read_bytes()

def main():
    DIST.mkdir(exist_ok=True)
    revision = subprocess.check_output(["git", "-C", str(ROOT), "rev-parse", "HEAD"], text=True).strip()
    with tempfile.TemporaryDirectory(prefix="prosperotube-source-") as temporary:
        with tarfile.open(DIST / "ProsperoTube-1.000.000-source.tar.gz", "w:gz") as result:
            for directory, prefix in ((ROOT, ""), (ROOT / "third_party/evo", "third_party/evo/"),
                                      (ROOT / "third_party/evo/third_party/ps5-homebrew-ui", "third_party/evo/third_party/ps5-homebrew-ui/")):
                archive = Path(temporary) / "source.tar"
                subprocess.run(["git", "-C", str(directory), "archive", "--format=tar", "-o", str(archive), "HEAD"], check=True)
                with tarfile.open(archive) as source:
                    for member in source:
                        member.name = "ProsperoTube/" + prefix + member.name
                        result.addfile(member, source.extractfile(member) if member.isfile() else None)
    with zipfile.ZipFile(DIST / "ProsperoTube-1.000.000-companion-fixtures.zip", "w", zipfile.ZIP_DEFLATED) as archive:
        for name, body in companion_entries(ROOT, revision):
            archive.writestr("ProsperoTube/" + name, body)
    artifacts = {}
    for path in sorted(DIST.iterdir()):
        if path.is_file() and path.suffix in {".zip", ".gz"}:
            artifacts[path.name] = {"bytes": path.stat().st_size, "sha256": hashlib.file_digest(path.open("rb"), "sha256").hexdigest()}
    manifest = {"version": "1.004.000", "source_revision": revision, "title_id": "PPSA99991",
                "validation": {"status": "Packaging records checksums; consult the validation record for test evidence.",
                               "ps5_hardware": "not tested", "live_youtube": "not tested"},
                "artifacts": artifacts}
    (DIST / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    (DIST / "SHA256SUMS").write_text("".join(f"{v['sha256']}  {k}\n" for k, v in artifacts.items()))
    print(json.dumps(manifest, indent=2))

if __name__ == "__main__": main()
