"""Real HTTP -> API -> manifest -> byte-range relay -> host audio/video decode."""
import asyncio
from pathlib import Path
import json
import socket
import subprocess
import sys
import threading
import time
import urllib.request
import uvicorn
from prosperotube.demo import create_demo_app

ROOT = Path(__file__).resolve().parents[1]


def main():
    app = create_demo_app(ROOT / "fixtures/generated", token="offline-fixture-token", pairing_code="123456")
    sock = socket.socket(); sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
    server = uvicorn.Server(uvicorn.Config(app, log_level="warning", access_log=False))
    thread = threading.Thread(target=server.run, kwargs={"sockets": [sock]}, daemon=True); thread.start()
    while not server.started:
        if not thread.is_alive(): raise RuntimeError("fixture server failed")
        time.sleep(.01)
    base = f"http://127.0.0.1:{port}"
    def request(path, data=None):
        raw = json.dumps(data).encode() if data is not None else None
        req = urllib.request.Request(base + path, data=raw, headers={"Content-Type": "application/json", "Authorization": "Bearer offline-fixture-token"})
        with urllib.request.urlopen(req, timeout=5) as response: return json.load(response)
    results = []
    try:
        assert request("/v1/health")["mode"] == "fixtures"
        rows = request("/v1/search?q=fixture")["items"]
        assert len(rows) == 3
        for row in rows:
            assert row["art_url"].startswith(base + "/fixtures/")
            with urllib.request.urlopen(row["art_url"]) as response:
                assert response.read(8) == b"\x89PNG\r\n\x1a\n"
        for identifier, codec in (("DEMOAVC0001", "avc"), ("DEMOVP90001", "vp9"), ("DEMOHLS0001", "avc")):
            playback = request("/v1/resolve", {"id": identifier, "codec": codec})
            url = playback["choices"][0]["url"]
            probe = subprocess.run(["ffprobe", "-v", "error", "-show_streams", "-of", "json", url], capture_output=True, text=True, timeout=30, check=True)
            streams = json.loads(probe.stdout)["streams"]
            assert {s["codec_type"] for s in streams} >= {"video", "audio"}
            for offset in (0, 4):
                args = ["ffmpeg", "-v", "error"] + (["-ss", str(offset)] if offset else [])
                args += ["-i", url, "-t", "2", "-map", "0:v", "-map", "0:a", "-f", "null", "-"]
                subprocess.run(args, check=True, capture_output=True, timeout=30)
            if playback["captions"]:
                with urllib.request.urlopen(playback["captions"][0]["url"]) as response:
                    assert "00:00:04,000" in response.read().decode()
            results.append({"id": identifier, "transport": playback["choices"][0]["container"],
                            "codecs": [s["codec_name"] for s in streams], "decode_and_seek": "pass"})
        out = ROOT / "output/validation"; out.mkdir(parents=True, exist_ok=True)
        (out / "companion-media.json").write_text(json.dumps(results, indent=2) + "\n")
        print(json.dumps(results, indent=2))
    finally:
        server.should_exit = True; thread.join(timeout=10); sock.close()


if __name__ == "__main__": main()
