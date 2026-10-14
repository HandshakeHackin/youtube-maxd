#!/usr/bin/env python3
"""Seek and refusal laboratory: how an FFmpeg build reads the service's two
manifest shapes from a local, range-capable, paced HTTP server.

    python3 standalone/test/seek_lab.py --ffmpeg patched=~/.cache/prosperotube/ffmpeg-host/patched/ffmpeg [--ffmpeg name=path ...]

Generates ten-minute H.264/AAC (fragmented MP4 with one `sidx`, as YouTube
serves) and VP9/Opus (WebM with Cues in front) files once, serves them the way
a media server does (one long answer is paced; any request can be refused
with 403), builds the classic and the ranged manifest with `tube_js manifest`,
and measures seeks and refusals. Nothing leaves the machine.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import re
import struct
import subprocess
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
WORK = Path.home() / ".cache/prosperotube/seek-lab"
DURATION = 600
FILES = {
    "video.mp4": (["testsrc2=size=320x180:rate=15", "-an", "-c:v", "libx264", "-preset", "ultrafast", "-g", "75", "-b:v", "500k"],
                  ["-movflags", "+empty_moov+frag_keyframe+default_base_moof+global_sidx"], 'video/mp4; codecs="avc1.42c00c"'),
    "audio.m4a": (["sine=frequency=880:sample_rate=44100", "-vn", "-c:a", "aac", "-b:a", "128k"],
                  ["-movflags", "+empty_moov+frag_keyframe+default_base_moof+global_sidx", "-frag_duration", "5000000"], 'audio/mp4; codecs="mp4a.40.2"'),
    "video.webm": (["testsrc2=size=320x180:rate=15", "-an", "-c:v", "libvpx-vp9", "-deadline", "realtime", "-cpu-used", "8", "-g", "75", "-b:v", "500k"],
                   ["-f", "webm", "-dash", "1", "-cues_to_front", "1"], 'video/webm; codecs="vp9"'),
    "audio.webm": (["sine=frequency=880:sample_rate=48000", "-vn", "-c:a", "libopus", "-b:a", "128k"],
                   ["-f", "webm", "-dash", "1", "-cues_to_front", "1", "-cluster_time_limit", "5000"], 'audio/webm; codecs="opus"'),
}


def ranges(path):
    """init_range and index_range as YouTube lists them for an adaptive file."""
    data = path.read_bytes()[:2 << 20]
    if path.suffix in (".mp4", ".m4a"):
        at = 0
        while at + 8 <= len(data):
            size, kind = struct.unpack(">I4s", data[at:at + 8])
            if kind == b"sidx":
                return (0, at - 1), (at, at + size - 1)
            at += size
        raise SystemExit(f"{path.name}: no sidx")
    def element(at):
        width = lambda lead: next(n for n in range(1, 9) if lead & (0x80 >> (n - 1)))
        id_size = width(data[at]); size_size = width(data[at + id_size])
        size = data[at + id_size] & (0xff >> size_size)
        for i in range(1, size_size):
            size = size * 256 + data[at + id_size + i]
        return int.from_bytes(data[at:at + id_size], "big"), size, at + id_size + size_size
    _, size, body = element(0)
    _, _, at = element(body + size)          # Segment
    while at < len(data):
        identity, size, body = element(at)
        if identity == 0x1C53BB6B:
            return (0, at - 1), (at, body + size - 1)
        at = body + size
    raise SystemExit(f"{path.name}: no Cues in front")


def fixtures():
    WORK.mkdir(parents=True, exist_ok=True)
    for name, (source, container, _) in FILES.items():
        if not (WORK / name).exists():
            subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i", source[0], "-t", str(DURATION), *source[1:], *container, str(WORK / name)], check=True)
    return {name: (WORK / name).read_bytes() for name in FILES}


class Server:
    """Range-capable; paces every answer after a burst; can refuse media."""
    def __init__(self, resources, pace=120_000, burst=400_000):
        self.resources, self.pace, self.burst = resources, pace, burst
        self.reset()
        lab = self
        class Handler(BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"
            def log_message(self, *args): pass
            def do_GET(self):
                key = self.path.lstrip("/").split("?")[0]
                data = lab.resources.get(key)
                media = not key.endswith(".mpd")
                if data is None:
                    self.send_error(404); return
                if media:
                    lab.requests += 1
                    if lab.refuse is not None and lab.requests > lab.refuse:
                        lab.refused += 1
                        self.send_response(403); self.send_header("Content-Length", "0"); self.send_header("Connection", "close"); self.end_headers(); return
                start, end, raw = 0, len(data) - 1, self.headers.get("Range", "")
                if media:
                    lab.ranges.append(raw)
                if raw:
                    match = re.fullmatch(r"bytes=(\d+)-(\d*)", raw)
                    if not match or int(match[1]) > end:
                        self.send_error(416); return
                    start = int(match[1]); end = min(int(match[2]), end) if match[2] else end
                self.send_response(206 if raw else 200)
                self.send_header("Accept-Ranges", "bytes"); self.send_header("Content-Length", str(end - start + 1))
                self.send_header("Content-Type", "application/dash+xml" if not media else "application/octet-stream")
                if raw: self.send_header("Content-Range", f"bytes {start}-{end}/{len(data)}")
                self.send_header("Connection", "close"); self.end_headers()
                sent = 0
                try:
                    for offset in range(start, end + 1, 16384):
                        part = data[offset:min(offset + 16384, end + 1)]
                        self.wfile.write(part); self.wfile.flush()
                        sent += len(part)
                        if media:
                            lab.bytes += len(part)
                            if sent > lab.burst: time.sleep(len(part) / lab.pace)   # one long answer is paced, like the real servers
                except (BrokenPipeError, ConnectionResetError):
                    pass
        self.httpd = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.httpd.daemon_threads = True
        threading.Thread(target=self.httpd.serve_forever, daemon=True).start()
        self.base = f"http://127.0.0.1:{self.httpd.server_port}"
    def reset(self, refuse=None):
        self.requests = self.bytes = self.refused = 0
        self.ranges = []
        self.refuse = refuse


def manifests(server, tube_js, container, segment_seconds):
    video, audio = ("video.mp4", "audio.m4a") if container == "mp4" else ("video.webm", "audio.webm")
    tracks = []
    for kind, name in (("video", video), ("audio", audio)):
        init, index = ranges(WORK / name)
        data = (WORK / name).read_bytes()
        (WORK / f"{name}.head").write_bytes(data[:init[1] + 1])
        (WORK / f"{name}.index").write_bytes(data[index[0]:index[1] + 1])
        tracks.append({"kind": kind, "url": f"{server.base}/{name}?a=1&b=2", "head_file": str(WORK / f"{name}.head"),
                       "index_file": str(WORK / f"{name}.index"), "mime_type": FILES[name][2],
                       "init_range": {"start": init[0], "end": init[1]}, "index_range": {"start": index[0], "end": index[1]},
                       "content_length": (WORK / name).stat().st_size, "width": 320, "height": 180, "bitrate": 500000, "audio_sample_rate": 44100})
    built = {}
    for shape, seconds in (("classic", 0), ("ranged", 0), (f"ranged{segment_seconds}", segment_seconds)):
        request = {"duration": DURATION, "shape": shape[:6] if shape != "classic" else "classic", "tracks": tracks, "segment_seconds": seconds}
        answer = json.loads(subprocess.run([tube_js, "manifest", json.dumps(request)], check=True, capture_output=True, text=True).stdout)
        key = f"{container}-{shape}.mpd"
        server.resources[key] = answer["manifest"].encode()
        built[shape] = (key, answer["manifest"].count("<SegmentURL "))
    return built


def play(ffmpeg, url, seek, seconds=3, limit=25, evo=True):
    options = ["-allowed_extensions", "ALL", "-reconnect", "1", "-reconnect_streamed", "1", "-reconnect_on_network_error", "1",
               "-reconnect_delay_max", "2", "-rw_timeout", "5000000", "-reconnect_max_retries", "3", "-reconnect_delay_total_max", "6"] if evo else ["-allowed_extensions", "ALL"]
    command = [ffmpeg, "-hide_banner", "-nostdin", "-v", "error", *options, "-ss", str(seek), "-i", url, "-t", str(seconds), "-c", "copy", "-f", "null", "-"]
    started = time.monotonic()
    try:
        run = subprocess.run(command, capture_output=True, text=True, timeout=limit)
        code, errors = run.returncode, run.stderr
    except subprocess.TimeoutExpired as error:
        code, errors = "timeout", (error.stderr or b"").decode(errors="replace")
    return code, time.monotonic() - started, len(re.findall(r"403 Forbidden", errors))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--ffmpeg", action="append", required=True, help="name=path of an ffmpeg to measure")
    parser.add_argument("--tube-js", default=str(ROOT / "build/tube_js"))
    parser.add_argument("--segment-seconds", type=int, default=30)
    parser.add_argument("--only", default="", help="seek, refuse or expire")
    args = parser.parse_args()
    server = Server(fixtures())
    builds = [(item.split("=", 1)[0], str(Path(item.split("=", 1)[1]).expanduser())) for item in args.ffmpeg]
    shapes = {container: manifests(server, args.tube_js, container, args.segment_seconds) for container in ("mp4", "webm")}
    print(f"files: " + ", ".join(f"{name} {len(data) // 1024} KiB" for name, data in server.resources.items() if not name.endswith(".mpd")))
    print("segments: " + ", ".join(f"{container}/{shape} {count}" for container, built in shapes.items() for shape, (_, count) in built.items()))
    if args.only in ("", "seek"):
        print(f"\n{'ffmpeg':9} {'file':5} {'manifest':9} {'seek':>5}  {'result':8} {'wall s':>6} {'media KiB':>9} {'requests':>8}")
        for name, ffmpeg in builds:
            for container, built in shapes.items():
                for shape, (key, _) in built.items():
                    for seek in (0, DURATION // 2, DURATION - 20):
                        server.reset()
                        code, wall, _ = play(ffmpeg, f"{server.base}/{key}", seek)
                        print(f"{name:9} {container:5} {shape:9} {seek:5}  {('ok' if code == 0 else str(code)):8} {wall:6.1f} {server.bytes // 1024:9} {server.requests:8}")
    if args.only in ("", "refuse"):
        # Every media request refused from the start (expired links at open) and after the first few (expiry during play).
        print(f"\n{'ffmpeg':9} {'file':5} {'manifest':9} {'refused after':>13}  {'result':8} {'wall s':>6} {'requests':>8} {'refused':>7}")
        for name, ffmpeg in builds:
            for container, built in shapes.items():
                for shape, (key, _) in built.items():
                    for allowed in (0, 6):
                        server.reset(refuse=allowed)
                        code, wall, _ = play(ffmpeg, f"{server.base}/{key}", 0, seconds=DURATION, limit=40)
                        print(f"{name:9} {container:5} {shape:9} {allowed:13}  {('ok' if code == 0 else str(code)):8} {wall:6.1f} {server.requests:8} {server.refused:7}")


if __name__ == "__main__":
    main()
