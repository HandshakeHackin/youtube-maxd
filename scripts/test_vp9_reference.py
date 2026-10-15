#!/usr/bin/env python3
"""Build/run the offline software reference against a known lossless VP9 fixture."""
import csv
import hashlib
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
binary = root / "build/tests/vp9_reference"
binary.parent.mkdir(parents=True, exist_ok=True)
flags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs",
    "libavformat", "libavcodec", "libavutil"], text=True))
subprocess.run(shlex.split(os.environ.get("CC", "cc")) + ["-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
    "-fsanitize=undefined", "-I", str(root / "native"), str(root / "tools/diagnostics/vp9_reference.c"),
    "-o", str(binary)] + flags, check=True)

def fingerprint(y):
    result = 14695981039346656037
    for gy in range(9):
        row = ((2 * gy + 1) * 72) // 18
        for gx in range(16):
            x = ((2 * gx + 1) * 128) // 32
            start = row * 128 + (x // 64) * 64
            for byte in y[start:start+64]:
                result = ((result ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return result

with tempfile.TemporaryDirectory(prefix="vp9-reference-") as folder:
    folder = Path(folder)
    media = folder / "fixture.ivf"
    frames = []
    for frame in range(12):
        n = 2 if frame == 3 else frame
        y = bytes(16 + ((x * 3 + row * 5 + n * 11) % 220) for row in range(72) for x in range(128))
        frames.append(y)
    raw = b"".join(y + bytes([128]) * (128 * 72 // 2) for y in frames)
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-f", "rawvideo", "-pixel_format", "yuv420p",
        "-video_size", "128x72", "-framerate", "24", "-i", "pipe:0", "-an", "-c:v", "libvpx-vp9",
        "-lossless", "1", "-lag-in-frames", "0", "-auto-alt-ref", "0", "-threads", "1", "-f", "ivf", str(media)],
        input=raw, check=True)
    data = bytearray(media.read_bytes())
    assert data[:4] == b"DKIF" and struct.unpack_from("<II", data, 16) == (24, 1)
    at = 32
    packet_hashes = []
    for i in range(12):
        size = struct.unpack_from("<I", data, at)[0]
        # Equal timestamps are valid occurrences; never deduplicate or invent a replacement PTS.
        struct.pack_into("<Q", data, at + 4, i // 2)
        packet_hashes.append(hashlib.sha256(data[at+12:at+12+size]).hexdigest())
        at += 12 + size
    assert at == len(data)
    media.write_bytes(data)
    out = folder / "frames.csv"
    packets = folder / "packets.csv"
    subprocess.run([str(binary), str(media), str(out), str(packets)], check=True)
    rows = list(csv.DictReader(out.open()))
    compressed = list(csv.DictReader(packets.open()))
    assert len(rows) == len(compressed) == 12
    for i, (row, packet, y) in enumerate(zip(rows, compressed, frames)):
        pts = ((i // 2) * 1000000 + 12) // 24
        assert int(row["sequence"]) == i+1 and int(packet["packet"]) == i+1
        assert int(row["pts_us"]) == int(packet["pts_us"]) == pts
        assert int(row["luma_hash"]) == fingerprint(y) and row["hash_valid"] == "1"
        assert row["pitch_bytes"] == "128" and row["ten_bit"] == "0"
        assert packet["sha256"] == packet_hashes[i] and packet["sha256_valid"] == "1"
    assert rows[2]["luma_hash"] == rows[3]["luma_hash"]
    short = folder / "short.csv"
    short_packets = folder / "short-packets.csv"
    subprocess.run([str(binary), str(media), str(short), str(short_packets), "0.1"], check=True)
    assert len(list(csv.DictReader(short.open()))) == 6
    # Existing evidence is never overwritten, and invalid durations fail before output creation.
    saved = out.read_bytes()
    assert subprocess.run([str(binary), str(media), str(out), str(packets)], capture_output=True).returncode != 0
    assert out.read_bytes() == saved
    assert subprocess.run([str(binary), str(media), str(folder / "bad.csv"), str(folder / "bad-p.csv"), "nan"], capture_output=True).returncode != 0
    assert not (folder / "bad.csv").exists()
print("Software VP9 reference: known luma, equal-PTS occurrences, packet SHA256, bounds and output preservation PASS")
print(binary)
