#!/usr/bin/env python3
"""CPU-only static Whisper library for Linux tests or public PS5 SDK.
SPDX-License-Identifier: GPL-3.0-or-later
"""
from pathlib import Path
import argparse
import os
import subprocess
from bootstrap_local_voice import CACHE, REVISION
from patch_local_voice import apply

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ps5", action="store_true")
    parser.add_argument("--jobs", type=int, default=3)
    args = parser.parse_args()
    source = CACHE / ("whisper.cpp-" + REVISION)
    if not (source / "include/whisper.h").exists():
        raise RuntimeError("Run scripts/bootstrap_local_voice.py first")
    apply(source)
    build = CACHE / ("ps5" if args.ps5 else "host")
    options = ["-DBUILD_SHARED_LIBS=OFF", "-DCMAKE_BUILD_TYPE=Release",
               "-DWHISPER_BUILD_TESTS=OFF", "-DWHISPER_BUILD_EXAMPLES=OFF", "-DWHISPER_BUILD_IS_DEV=OFF",
               "-DGGML_NATIVE=OFF", "-DGGML_OPENMP=OFF", "-DGGML_BLAS=OFF", "-DGGML_BACKEND_DL=OFF",
               "-DGGML_CUDA=OFF", "-DGGML_VULKAN=OFF", "-DGGML_METAL=OFF", "-DGGML_RPC=OFF",
               "-DGGML_CPU_REPACK=OFF", "-DGGML_CPU_KLEIDIAI=OFF", "-DGGML_CCACHE=OFF",
               "-DGGML_AVX=ON", "-DGGML_AVX2=ON", "-DGGML_F16C=ON", "-DGGML_FMA=ON",
               "-DGGML_AVX512=OFF", "-DGGML_AVX_VNNI=OFF", "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"]
    if args.ps5:
        sdk = Path(os.environ.get("PS5_PAYLOAD_SDK", str(ROOT / ".deps/sdk"))).resolve()
        toolchain = CACHE / "ps5-toolchain.cmake"
        toolchain.write_text(f'''set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER "{sdk}/bin/prospero-clang")
set(CMAKE_CXX_COMPILER "{sdk}/bin/prospero-clang++")
set(CMAKE_AR "{sdk}/bin/prospero-ar")
set(CMAKE_RANLIB "{sdk}/bin/prospero-ranlib")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_FLAGS_INIT "-DEVO_TARGET_PS5=1 -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_INIT "-DEVO_TARGET_PS5=1 -ffunction-sections -fdata-sections")
''')
        options += ["-DCMAKE_TOOLCHAIN_FILE=" + str(toolchain)]
    subprocess.run(["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja", *options], check=True)
    subprocess.run(["cmake", "--build", str(build), "--target", "whisper", "--parallel", str(max(1, args.jobs))], check=True)
    for name in ("libwhisper.a", "libggml.a", "libggml-base.a", "libggml-cpu.a"):
        found = list(build.rglob(name))
        if len(found) != 1:
            raise RuntimeError("Missing/ambiguous voice archive " + name)
        if args.ps5:
            symbols = subprocess.check_output([str(sdk / "bin/prospero-nm"), "-u", str(found[0])], text=True)
            forbidden = {"fork", "vfork", "execlp", "execve", "waitpid", "popen", "system", "readlink"}
            if any(line.split()[-1] in forbidden for line in symbols.splitlines() if line.split()):
                raise RuntimeError("Desktop process helper was linked into " + name)
    print("Local voice static archives ready: " + str(build))


if __name__ == "__main__":
    main()
