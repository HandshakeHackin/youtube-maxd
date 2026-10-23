"""Bake Cyrillic coverage into playback/UI atlases, without changing kit inputs."""
import hashlib
import os
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import sys

NAME = "montserrat-medium.huifont"
FACES = {
    NAME: "Montserrat-Medium.ttf",
    "inter-regular.huifont": "Inter-Regular.ttf",
    "inter-semibold.huifont": "Inter-SemiBold.ttf",
    "dejavu-sans-mono.huifont": "DejaVuSansMono.ttf",
}


def stage(kit, destination, cache, name=NAME):
    kit, destination, cache = Path(kit), Path(destination), Path(cache)
    inputs = [kit / path for path in (
        "tools/font-baker/bake_font.cpp", "third_party/stb/stb_truetype.h",
        "src/gfx/font_format.hpp", "third_party/fonts/" + FACES[name])]
    original = (kit / "assets/fonts" / name).read_bytes()
    pixel, spread = struct.unpack_from("<ff", original, 12)
    # The pinned baker's european set includes А–я and Ё/ё. Its larger atlas
    # keeps the original pixel size and SDF spread (and thus text metrics).
    args = [str(pixel), str(int(spread)), "2048", "european"]
    compiler = shlex.split(os.environ.get("HOST_CXX", "c++"))
    digest = hashlib.sha256((repr(args) + repr(compiler)).encode())
    for path in inputs:
        digest.update(path.read_bytes())
    folder = cache / digest.hexdigest()
    baked = folder / name
    if not baked.exists():
        folder.mkdir(parents=True, exist_ok=True)
        executable = folder / "bake-font"
        subprocess.run([*compiler, "-std=c++17", "-O2", str(inputs[0]), "-o", str(executable)], check=True, stdout=sys.stderr)
        temporary = folder / (name + ".tmp")
        subprocess.run([str(executable), str(inputs[-1]), str(temporary), *args], check=True, stdout=sys.stderr)
        data = temporary.read_bytes()
        count = struct.unpack_from("<I", data, 32)[0]
        points = {struct.unpack_from("<I", data, 40 + 24 * i)[0] for i in range(count)}
        required = set(range(0x410, 0x450)) | {0x401, 0x451}
        if not required <= points:
            raise RuntimeError("Playback display font is missing Russian glyphs")
        temporary.replace(baked)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(baked, destination)
    return destination
