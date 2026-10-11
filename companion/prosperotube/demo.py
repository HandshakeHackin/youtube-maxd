"""Explicit generated-fixture mode: exercises the real API/relay with no internet."""
import copy
from pathlib import Path
import re
from urllib.parse import urlsplit
import httpx
from fastapi.responses import FileResponse
from .app import create_app
from .models import ServiceError

ORIGIN = "https://fixtures.googlevideo.com/"
CHANNEL = "UC" + "f" * 22


def fixture_info(identifier):
    vp9 = identifier == "DEMOVP90001"
    av1 = identifier == "DEMOAV10001"
    hevc = identifier == "DEMOHEVC001"
    hls = identifier == "DEMOHLS0001"
    info = {"id": identifier, "title": "Offline fixture: " + ("HEVC / AAC" if hevc else "AV1 / AAC" if av1 else "VP9 / Opus" if vp9 else "HLS" if hls else "AVC / AAC"),
            "channel": "Generated test media", "channel_id": CHANNEL, "duration": 8,
            "description": "Synthetic, redistributable media. This fixture does not establish YouTube or PS5 compatibility.",
            "thumbnail": ORIGIN + "thumb.png", "is_live": hls,
            "chapters": [{"title": "First section", "start_time": 0, "end_time": 4}, {"title": "Seek target", "start_time": 4, "end_time": 8}],
            "subtitles": {"en": [{"ext": "vtt", "url": ORIGIN + "captions.vtt"}]},
            "formats": [
                {"format_id": "video", "url": ORIGIN + ("video-hevc.mp4" if hevc else "video-av1.mp4" if av1 else "video.webm" if vp9 else "video.mp4"),
                 "protocol": "https", "ext": "webm" if vp9 else "mp4", "vcodec": "hvc1.1.6.L93.B0" if hevc else "av01.0.01M.08" if av1 else "vp9" if vp9 else "avc1.64001e",
                 "acodec": "none", "height": 360, "width": 640, "fps": 30, "tbr": 800, "dynamic_range": "SDR"},
                {"format_id": "audio", "url": ORIGIN + ("audio.webm" if vp9 else "audio.m4a"),
                 "protocol": "https", "ext": "webm" if vp9 else "m4a", "vcodec": "none",
                 "acodec": "opus" if vp9 else "mp4a.40.2", "asr": 48000, "abr": 128, "language": "en"}]}
    if not vp9 and not av1 and not hevc:
        info["formats"].append({"format_id": "progressive", "url": ORIGIN + "muxed.mp4", "protocol": "https",
                                "ext": "mp4", "vcodec": "avc1.64001e", "acodec": "mp4a.40.2", "height": 360,
                                "width": 640, "tbr": 800, "dynamic_range": "SDR"})
    if hls:
        info["formats"] = [{"format_id": "hls", "url": ORIGIN + "fixture.m3u8", "protocol": "m3u8_native",
                            "ext": "mp4", "vcodec": "avc1.64001e", "acodec": "mp4a.40.2", "height": 360,
                            "width": 640, "tbr": 800, "dynamic_range": "SDR"}]
    return info


class FixtureExtractor:
    fixture_mode = True
    base = ""
    def for_profile(self, profile):
        if profile not in {"guest", "fixture_one", "fixture_two"}:
            raise ServiceError("unknown_profile", "Select an available offline profile.", 404)
        return self
    def info(self, identifier):
        info = fixture_info(identifier)
        info["thumbnail"] = self.base + "/fixtures/thumb.png"
        return info
    async def extract(self, target, *, flat=False, **_):
        if flat:
            return {"entries": [self.info(i) for i in ("DEMOAVC0001", "DEMOVP90001", "DEMOHLS0001", "DEMOAV10001", "DEMOHEVC001")]}
        identifier = target.split("v=")[-1]
        if identifier not in ("DEMOAVC0001", "DEMOVP90001", "DEMOHLS0001", "DEMOAV10001", "DEMOHEVC001"):
            raise ServiceError("fixture_only", "Fixture mode only plays the generated demo videos.", 404)
        return self.info(identifier)


class FileChunks(httpx.AsyncByteStream):
    def __init__(self, path, start, count): self.path, self.start, self.count = path, start, count
    async def __aiter__(self):
        with self.path.open("rb") as f:
            f.seek(self.start)
            remaining = self.count
            while remaining:
                chunk = f.read(min(65536, remaining))
                if not chunk: break
                remaining -= len(chunk)
                yield chunk


def create_demo_app(directory, **kwargs):
    root = Path(directory).resolve()
    def serve(request):
        name = urlsplit(str(request.url)).path.lstrip("/")
        if request.url.host != "fixtures.googlevideo.com" or not re.fullmatch(r"[a-zA-Z0-9_-]+\.(mp4|m4a|webm|m3u8|ts|vtt|png)", name):
            return httpx.Response(404)
        path = root / name
        if not path.is_file(): return httpx.Response(404)
        size = path.stat().st_size; start, end, status = 0, size - 1, 200
        byte_range = request.headers.get("range")
        if byte_range:
            match = re.fullmatch(r"bytes=([0-9]*)-([0-9]*)", byte_range)
            if not match: return httpx.Response(416)
            start = int(match[1]) if match[1] else max(0, size - int(match[2]))
            end = min(size - 1, int(match[2])) if match[2] and match[1] else size - 1
            if start > end or start >= size: return httpx.Response(416)
            status = 206
        types = {".m3u8": "application/vnd.apple.mpegurl", ".vtt": "text/vtt", ".png": "image/png"}
        headers = {"Content-Type": types.get(path.suffix, "application/octet-stream"),
                   "Content-Length": str(end - start + 1), "Accept-Ranges": "bytes"}
        if status == 206: headers["Content-Range"] = f"bytes {start}-{end}/{size}"
        return httpx.Response(status, headers=headers, stream=FileChunks(path, start, 0 if request.method == "HEAD" else end - start + 1))
    client = httpx.AsyncClient(transport=httpx.MockTransport(serve))
    extractor = FixtureExtractor()
    from .fixture_library import FixtureLibrary
    if "library" in kwargs:
        raise ValueError("Fixture mode uses only its in-memory library.")
    app = create_app(extractor=extractor, http_client=client, library=FixtureLibrary(extractor), **kwargs)
    @app.middleware("http")
    async def fixture_base(request, call_next):
        extractor.base = kwargs.get("public_base") or str(request.base_url).rstrip("/")
        return await call_next(request)
    @app.get("/fixtures/thumb.png")
    async def thumbnail():
        return FileResponse(root / "thumb.png", media_type="image/png")
    return app
