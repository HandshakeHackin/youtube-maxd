"""Validate and copy a complete PS5 transform artifact into generated native sources."""
from pathlib import Path
import contextlib
import io
import json
import shutil
import tempfile
from build_video_transform_shaders import digest, verify
from prepare_video_transform_shaders import generate, EVO, PIN

def current_sources():
    with tempfile.TemporaryDirectory(prefix="prosperotube-transform-source-") as directory:
        with contextlib.redirect_stdout(io.StringIO()):
            return generate(Path(directory))

def validate(artifact):
    artifact = Path(artifact).resolve()
    manifest = json.loads((artifact/"manifest.json").read_text())
    expected = manifest["build"]
    if expected["sources"] != current_sources():
        raise ValueError("Transform artifact is stale for the current generated shaders")
    if expected["packager_sha256"] != digest(EVO/"tools/build_agc_pipes.py"):
        raise ValueError("Transform artifact uses a different PS5 metadata packager")
    verify(artifact, expected)
    return manifest

def install(artifact, native_root):
    artifact, native_root = Path(artifact).resolve(), Path(native_root).resolve()
    if native_root.is_relative_to(EVO.resolve()):
        raise ValueError("Cannot install transform shaders into pinned EVO")
    if (native_root/".prosperotube-source").read_text().strip() != PIN:
        raise ValueError("Transform destination is not the matching generated native tree")
    manifest = validate(artifact)
    target = native_root/"projects/evoplayer/shaders/agc"
    if not target.is_dir():
        raise ValueError("Generated native shader directory is missing")
    # Verify every input before changing any destination; retain unrelated UI pipes.
    for name in manifest["headers"]:
        shutil.copyfile(artifact/"headers"/name, target/name)
    return manifest
