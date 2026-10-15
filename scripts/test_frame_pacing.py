#!/usr/bin/env python3
"""Run the pure deadline model; no console, SDK or generated-tree mutation."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def main():
    with tempfile.TemporaryDirectory(prefix="tube-frame-pacing-") as temporary:
        binary = Path(temporary) / "test_frame_pacing"
        subprocess.run([
            os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=undefined", "-I", str(ROOT / "native"),
            str(ROOT / "tests/test_frame_pacing.c"), "-lm", "-o", str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True)

if __name__ == "__main__":
    main()

