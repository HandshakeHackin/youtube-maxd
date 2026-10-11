"""Release selection excludes local dependencies, credentials and dirty source."""
import importlib.util
from pathlib import Path
import subprocess

import pytest

spec = importlib.util.spec_from_file_location("tube_packaging", Path(__file__).resolve().parents[1] / "scripts/package_release.py")
packaging = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packaging)


def source_tree(root):
    subprocess.run(["git", "init", "-q", str(root)], check=True)
    sources = ["companion/client.py", "docs/example.md", "README.md", "LICENSE",
               "THIRD_PARTY_NOTICES.md", "pyproject.toml", "scripts/generate_fixtures.py"]
    for name in sources:
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("committed source\n")
    subprocess.run(["git", "-C", str(root), "add", "."], check=True)
    subprocess.run(["git", "-C", str(root), "-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid",
                    "commit", "-qm", "Fixture source"], check=True)
    fixtures = root / "fixtures/generated"
    fixtures.mkdir(parents=True)
    for name in ["video.mp4", "video-av1.mp4", "video-hevc.mp4", "audio.m4a", "video.webm", "audio.webm", "muxed.mp4",
                 "fixture.m3u8", "thumb.png", "captions.vtt", "fixture0.ts"]:
        (fixtures / name).write_bytes(b"synthetic fixture")
    return fixtures


def test_package_uses_committed_source_and_allowlisted_fixtures(tmp_path):
    fixtures = source_tree(tmp_path)
    for name in ["companion/private.cookies", "companion/node_modules/secret.txt",
                 "companion/innertube/worker.bundle.mjs", "fixtures/generated/private.json"]:
        path = tmp_path / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("must not distribute")
    (tmp_path / "companion/client.py").write_text("dirty working source")
    entries = dict(packaging.companion_entries(tmp_path, "HEAD"))
    assert entries["companion/client.py"] == b"committed source\n"
    assert entries["fixtures/generated/fixture0.ts"] == b"synthetic fixture"
    assert len(entries) == 18
    assert not any(b"must not distribute" in body for body in entries.values())


def test_fixture_symlink_cannot_package_a_private_file(tmp_path):
    fixtures = source_tree(tmp_path)
    private = tmp_path / "private.cookies"
    private.write_text("must not distribute")
    (fixtures / "video.mp4").unlink()
    (fixtures / "video.mp4").symlink_to(private)
    with pytest.raises(ValueError, match="regular local file"):
        dict(packaging.companion_entries(tmp_path, "HEAD"))


def test_every_advertised_demo_has_packaged_media_and_captions(tmp_path):
    import asyncio
    from prosperotube.demo import FixtureExtractor, ORIGIN
    source_tree(tmp_path)
    entries = dict(packaging.companion_entries(tmp_path, "HEAD"))
    catalog = asyncio.run(FixtureExtractor().extract("", flat=True))
    assert len(catalog["entries"]) == 5
    for item in catalog["entries"]:
        media = [fmt["url"] for fmt in item["formats"]]
        media += [track["url"] for tracks in item["subtitles"].values() for track in tracks]
        for url in media:
            assert url.startswith(ORIGIN)
            assert "fixtures/generated/" + url.removeprefix(ORIGIN) in entries
        assert "fixtures/generated/" + item["thumbnail"].removeprefix("/fixtures/") in entries
