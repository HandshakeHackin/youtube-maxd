"""Generate staged PS5 video shaders with quarter-turn/mirror support.

Uses pinned EVO generators; does not install headers or expose unfinished controls.
"""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import subprocess
from prepare_native import EVO, PIN

TRANSFORM = """
// Transform positions only: texture coordinates and fragment math stay unchanged.
vec2 tube_transform(vec2 point, float encoded) {
    int code = int(encoded);
    if ((code & 4) != 0) point.x = -point.x;
    int turns = code & 3;
    if (turns == 1) return vec2(point.y, -point.x);
    if (turns == 2) return -point;
    if (turns == 3) return vec2(-point.y, point.x);
    return point;
}
"""

def replace_once(source, old, new):
    if source.count(old) != 1:
        raise ValueError(f"Unexpected EVO vertex layout: {old}")
    return source.replace(old, new)

def vertex(source, family):
    if family == "video":
        source = replace_once(source, "    float uSlope;", "    float uSlope;\n    float uTransform;")
        expression = "(p * 2.0 - 1.0) * video.uScale"
        encoded = "video.uTransform"
    else:
        if family == "upscale":
            source = replace_once(source, "} pass;", "    vec4 uParams; // Reserved to match interpolation ABI.\n} pass;")
        source = replace_once(source, "} pass;", "    vec4 uTransform;\n} pass;")
        expression = "p * 2.0 - 1.0"
        encoded = "pass.uTransform.x"
    source = replace_once(source, "void main() {", TRANSFORM + "\nvoid main() {")
    return replace_once(source, f"vec4({expression}, 0.0, 1.0)",
                        f"vec4(tube_transform({expression}, {encoded}), 0.0, 1.0)")

def generate(output):
    output = output.resolve()
    if output.is_relative_to(EVO.resolve()):
        raise ValueError("Staged shader output must not overwrite pinned EVO")
    actual = subprocess.check_output(["git", "-C", str(EVO), "rev-parse", "HEAD"], text=True).strip()
    if actual != PIN:
        raise ValueError(f"Unexpected EVO revision: {actual}")
    tracked = ["tools/gen_video_pipes.py", "tools/gen_upscale_pipes.py",
               "tools/gen_interp_pipes.py", "third_party/anime4k"]
    subprocess.run(["git", "-C", str(EVO), "diff", "--quiet", PIN, "--", *tracked], check=True)
    output.mkdir(parents=True, exist_ok=True)
    for family in ("video", "upscale", "interp"):
        path = EVO / "tools" / f"gen_{family}_pipes.py"
        spec = importlib.util.spec_from_file_location(f"tube_gen_{family}", path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        module.VS = vertex(module.VS, family)
        module.SHADER_DIR = output
        module.main()
    manifest = {"evo_revision": PIN, "video_uniform_bytes": 32,
                "pass_uniform_bytes": 48, "pipes": {
                    p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in sorted(output.glob("*.pipe"))}}
    (output / "tube_transform_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    generate(args.output)
