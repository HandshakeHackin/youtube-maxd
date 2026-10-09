"""Verify FFmpeg's full-file separate-track DASH path with generated media."""
from pathlib import Path
import contextlib
import functools
import http.server
import json
import subprocess
import threading

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "fixtures/generated"


def run(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    run("ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i",
        "testsrc2=size=640x360:rate=30", "-t", "8", "-an", "-c:v",
        "libx264", "-g", "30", "-movflags", "+faststart", str(OUT / "video.mp4"))
    run("ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i",
        "sine=frequency=880:sample_rate=48000", "-t", "8", "-vn", "-c:a",
        "aac", "-movflags", "+faststart", str(OUT / "audio.m4a"))
    xml = '''<?xml version="1.0"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" mediaPresentationDuration="PT8S" minBufferTime="PT1S" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011">
<Period duration="PT8S">
<AdaptationSet contentType="video" mimeType="video/mp4" codecs="avc1.64001e">
<Representation id="v" bandwidth="1000000" width="640" height="360">
<BaseURL>video.mp4</BaseURL><SegmentList duration="8" timescale="1"><SegmentURL media="video.mp4"/></SegmentList>
</Representation></AdaptationSet>
<AdaptationSet contentType="audio" mimeType="audio/mp4" codecs="mp4a.40.2">
<Representation id="a" bandwidth="128000" audioSamplingRate="48000">
<BaseURL>audio.m4a</BaseURL><SegmentList duration="8" timescale="1"><SegmentURL media="audio.m4a"/></SegmentList>
</Representation></AdaptationSet>
</Period></MPD>'''
    (OUT / "test.mpd").write_text(xml)
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=OUT)
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        url = f"http://127.0.0.1:{server.server_port}/test.mpd"
        p = run("ffprobe", "-v", "error", "-show_streams", "-of", "json", url)
        streams = json.loads(p.stdout)["streams"]
        assert {s["codec_type"] for s in streams} == {"video", "audio"}, streams
        run("ffmpeg", "-v", "error", "-i", url, "-map", "0:v", "-map", "0:a", "-f", "null", "-")
        run("ffmpeg", "-v", "error", "-ss", "4", "-i", url, "-t", "2", "-map", "0:v", "-map", "0:a", "-f", "null", "-")
        print(json.dumps({"result": "pass", "streams": [{"codec": s["codec_name"], "type": s["codec_type"]} for s in streams], "checks": ["probe", "complete audio+video decode", "seek to 4 seconds"]}, indent=2))
    finally:
        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    main()
