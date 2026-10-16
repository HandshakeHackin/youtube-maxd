"""Compile every staged transform pipeline with EVO's PS5 metadata packager.

Publishes an immutable, content-keyed artifact only after all pipelines pass.
The existing application shader tree is never changed by this command.
"""
from pathlib import Path
import argparse
import contextlib
import fcntl
import hashlib
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
from prepare_video_transform_shaders import generate, EVO, PIN

def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()

def verify(artifact, expected):
    """Reject stale, incomplete or corrupted artifacts before runtime copying."""
    manifest = json.loads((artifact / "manifest.json").read_text())
    if manifest["build"] != expected:
        raise ValueError("Transform artifact does not match the requested build")
    names = {Path(name).stem for name in expected["sources"]["pipes"]}
    if set(manifest["headers"]) != {name+"_pipe.h" for name in names}:
        raise ValueError("Incomplete transform header set")
    if set(manifest["metadata"]) != {name+".manifest.json" for name in names}:
        raise ValueError("Incomplete transform metadata set")
    for group in ("headers", "metadata"):
        for name, checksum in manifest[group].items():
            if digest(artifact / group / name) != checksum:
                raise ValueError(f"Transform artifact checksum mismatch: {name}")
    for name in names:
        metadata = json.loads((artifact / "metadata" / (name+".manifest.json")).read_text())
        if (metadata["target"] != "gfx1013" or metadata["gfxip"] != "10.1.3" or
            metadata["pipeline_type"] != "Ngg" or metadata["no_relocations"] is not True or
            metadata["source_sha256"] != expected["sources"]["pipes"][name+".pipe"]):
            raise ValueError(f"Invalid PS5 transform pipeline metadata: {name}")
    return artifact

def build(cache, compiler, library):
    cache, compiler, library = cache.resolve(), compiler.resolve(), library.resolve()
    if cache.is_relative_to(EVO.resolve()):
        raise ValueError("Compiler output must not overwrite pinned EVO")
    # Check dependencies before beginning a potentially expensive generation/build.
    compiler_hash, library_hash = digest(compiler), digest(library)
    cache.mkdir(parents=True, exist_ok=True)
    with (cache / "build.lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        with tempfile.TemporaryDirectory(prefix=".build-", dir=cache) as temporary:
            work = Path(temporary)
            sources = generate(work / "sources")
            packager = EVO / "tools/build_agc_pipes.py"
            subprocess.run(["git", "-C", str(EVO), "diff", "--quiet", PIN, "--",
                            "tools/build_agc_pipes.py"], check=True)
            expected = {"sources": sources, "compiler_sha256": compiler_hash,
                        "spvgen_sha256": library_hash, "packager_sha256": digest(packager)}
            key = hashlib.sha256(json.dumps(expected, sort_keys=True).encode()).hexdigest()
            artifact = cache / key
            if artifact.exists():
                return verify(artifact, expected)
            spec = importlib.util.spec_from_file_location("tube_agc_packager", packager)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            staging = work / "complete"
            headers, metadata = staging / "headers", staging / "metadata"
            headers.mkdir(parents=True)
            module.SHADER_DIR = headers
            # Subprocesses need spvgen; retain any caller-provided library paths.
            previous = os.environ.get("LD_LIBRARY_PATH")
            os.environ["LD_LIBRARY_PATH"] = str(library.parent) + (":"+previous if previous else "")
            try:
                for name in sorted(sources["pipes"]):
                    module.build_pipe(work / "sources" / name, metadata, str(compiler),
                                      "llvm-readelf-18", "llvm-objcopy-18")
            finally:
                if previous is None:
                    os.environ.pop("LD_LIBRARY_PATH", None)
                else:
                    os.environ["LD_LIBRARY_PATH"] = previous
            # Keep metadata/ELFs for auditing; the header set is copied at integration.
            manifest = {"build": expected,
                        "headers": {p.name: digest(p) for p in sorted(headers.glob("*_pipe.h"))},
                        "metadata": {p.name: digest(p) for p in sorted(metadata.glob("*.manifest.json"))}}
            (staging / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True)+"\n")
            verify(staging, expected)
            staging.rename(artifact)
            return artifact

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    tools = Path(os.environ.get("PROSPEROTUBE_SHADER_TOOLS", Path.home() / ".cache/prosperotube/shader-tools"))
    parser.add_argument("--cache", type=Path, default=Path.home()/".cache/prosperotube/video-transform-compiled")
    parser.add_argument("--compiler", type=Path, default=tools/"bin/amdllpc")
    parser.add_argument("--spvgen", type=Path, default=tools/"lib/libspvgen.so")
    args = parser.parse_args()
    # Keep stdout machine-readable for the PS5 build; progress belongs on stderr.
    with contextlib.redirect_stdout(sys.stderr):
        artifact = build(args.cache, args.compiler, args.spvgen)
    print(artifact)
