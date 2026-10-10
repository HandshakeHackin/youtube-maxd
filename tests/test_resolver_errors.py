"""Private yt-dlp diagnostics become stable public errors without credential text."""
import asyncio
import io
import json
import sys

import pytest
from prosperotube.extractor import Extractor
from prosperotube.models import ServiceError
from prosperotube.resolver_errors import classify_error, service_error
from prosperotube import worker


def test_worker_protocol_uses_utf8_with_windows_code_page_streams(monkeypatch):
    import yt_dlp
    target = "ytsearch1:日本語 café"
    metadata = {"id": "abcdefghijk", "title": "日本語 café 🎥", "formats": []}
    class YDL:
        def __init__(self, _options): pass
        def __enter__(self): return self
        def __exit__(self, *_): pass
        def add_info_extractor(self, _ie): pass
        def extract_info(self, actual, **_kwargs):
            assert actual == target
            print("extractor output stays separate")
            return metadata
        def sanitize_info(self, info): return info
        cookiejar = None
    monkeypatch.setattr(yt_dlp, "YoutubeDL", YDL)
    request = json.dumps({"target": target, "flat": False, "start": 1,
                          "end": 24}, ensure_ascii=False).encode("utf-8")
    incoming = io.TextIOWrapper(io.BytesIO(request), encoding="cp1252")
    raw = io.BytesIO()
    outgoing = io.TextIOWrapper(raw, encoding="cp1252")
    errors = io.StringIO()
    monkeypatch.setattr(sys, "stdin", incoming)
    monkeypatch.setattr(sys, "stdout", outgoing)
    monkeypatch.setattr(sys, "stderr", errors)
    assert worker.main() == 0
    outgoing.flush()
    assert json.loads(raw.getvalue()) == metadata
    assert "extractor output" not in raw.getvalue().decode("utf-8")
    assert "extractor output" in errors.getvalue()


@pytest.mark.parametrize("message,code", [
    ("ERROR: This video is unavailable", "video_unavailable"),
    ("Video has been removed by the uploader", "video_unavailable"),
    ("Private video. Sign in if you've been granted access", "authentication_required"),
    ("Sign in to confirm your age", "authentication_required"),
    ("Sign in to confirm you're not a bot", "youtube_challenge"),
    ("HTTP Error 429: Too Many Requests", "youtube_rate_limited"),
    ("Unable to download API page: timed out", "resolver_network"),
    ("An unexpected extractor failure", "extraction_failed"),
])
def test_real_ytdlp_exception_boundary_is_allowlisted(monkeypatch, message, code):
    import yt_dlp
    from yt_dlp.utils import DownloadError
    secret = "https://r1.googlevideo.com/video?sig=synthetic-secret Cookie=synthetic-cookie"
    class YDL:
        def __init__(self, options):
            assert options["logger"].error(message) is None
        def __enter__(self): return self
        def __exit__(self, *_): pass
        def add_info_extractor(self, ie):
            from prosperotube.live_hls import LiveHLSYoutubeIE
            assert isinstance(ie, LiveHLSYoutubeIE)
        def extract_info(self, *_args, **_kwargs): raise DownloadError(message + " " + secret)
    monkeypatch.setattr(yt_dlp, "YoutubeDL", YDL)
    output, errors = io.StringIO(), io.StringIO()
    monkeypatch.setattr(sys, "stdin", io.StringIO(json.dumps({
        "target": "https://www.youtube.com/watch?v=abcdefghijk", "flat": False, "start": 1, "end": 24})))
    monkeypatch.setattr(sys, "stdout", output)
    monkeypatch.setattr(sys, "stderr", errors)
    assert worker.main() == 1
    assert json.loads(output.getvalue()) == {"worker_error": {"version": 1, "code": code}}
    assert "synthetic" not in output.getvalue() + errors.getvalue()
    public = service_error(code)
    assert public.code == code and "synthetic" not in public.message
    if code in ("authentication_required", "youtube_challenge"):
        assert public.status == 403  # 401 would wrongly discard LAN pairing.


@pytest.mark.parametrize("body,expected", [
    ({"worker_error": {"version": 1, "code": "video_unavailable"}}, "video_unavailable"),
    ({"worker_error": {"version": 1, "code": "youtube_rate_limited"}}, "youtube_rate_limited"),
    ({"worker_error": {"version": True, "code": "video_unavailable"}}, "extraction_failed"),
    ({"worker_error": {"version": 2, "code": "video_unavailable"}}, "extraction_failed"),
    ({"worker_error": {"version": 1, "code": {"secret": "synthetic-secret"}}}, "extraction_failed"),
    ({"worker_error": {"version": 1, "code": "synthetic-secret"}}, "extraction_failed"),
    ({"worker_error": {"version": 1, "code": "video_unavailable", "message": "synthetic-secret"}}, "extraction_failed"),
    (None, "extraction_failed"),
])
def test_failed_worker_results_are_checked_and_never_cached(monkeypatch, body, expected):
    class Process:
        returncode = None
        def __init__(self): self.stdin = self.stdout = self; self.sent = False
        def write(self, _): pass
        async def drain(self): pass
        def close(self): pass
        async def read(self, _):
            if self.sent: return b""
            self.sent = True
            return json.dumps(body).encode() if body is not None else b""
        async def wait(self): self.returncode = 1
        def kill(self): self.returncode = -9
    async def spawn(*_args, **_kwargs): return Process()
    monkeypatch.setattr(asyncio, "create_subprocess_exec", spawn)
    async def run():
        resolver = Extractor()
        with pytest.raises(ServiceError) as error:
            await resolver.extract("https://www.youtube.com/watch?v=abcdefghijk")
        assert error.value.code == expected
        assert "synthetic" not in error.value.message
        assert not resolver.cache
    asyncio.run(run())


def test_native_network_exception_and_unknown_codes_remain_private():
    assert classify_error(TimeoutError("synthetic-secret")) == "resolver_network"
    assert service_error(["synthetic-secret"]).code == "extraction_failed"
