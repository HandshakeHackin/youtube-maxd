"""Public script probe cannot send credentials or follow other origins."""
import importlib.util
from pathlib import Path

import httpx
import pytest


def test_public_tv_script_transport_is_bounded_anonymous_and_nonredirecting(monkeypatch):
    path = Path(__file__).resolve().parents[1] / "scripts/probe_tv_signature.py"
    spec = importlib.util.spec_from_file_location("tv_signature_probe", path)
    probe = importlib.util.module_from_spec(spec); spec.loader.exec_module(probe)
    requests = []
    response = [httpx.Response(200, text="public script")]
    def handler(request):
        assert request.method == "GET"
        assert "Authorization" not in request.headers and "Cookie" not in request.headers
        assert request.headers["User-Agent"] == "TV test"
        requests.append(request)
        return response[0]
    original = httpx.Client
    def client(**kwargs):
        assert kwargs == {"timeout": 12, "follow_redirects": False, "trust_env": False}
        return original(**kwargs, transport=httpx.MockTransport(handler))
    monkeypatch.setattr(httpx, "Client", client)
    tv = "https://www.youtube.com/tv"
    js = "https://www.youtube.com/s/player/879f7172/tv-player-ias.vflset/tv-player-ias.js"
    assert probe.public_text(tv, "TV test") == "public script"
    assert probe.public_text(js, "TV test") == "public script"
    for url in ("http://www.youtube.com/tv", "https://evil.example/tv", tv + "?token=secret",
                tv + "#secret", "https://www.youtube.com:443/tv",
                "https://www.youtube.com/s/player/879f7172/player_ias.vflset/en_US/base.js",
                "https://www.youtube.com/youtubei/v1/player"):
        with pytest.raises(ValueError): probe.public_text(url, "TV test")
    assert len(requests) == 2
    for status in (302, 401, 500):
        response[0] = httpx.Response(status, headers={"Location": "https://evil.example"})
        with pytest.raises(ValueError): probe.public_text(tv, "TV test")
    assert len(requests) == 5
    response[0] = httpx.Response(200, content=b"x" * (8 * 1024 * 1024 + 1))
    with pytest.raises(ValueError): probe.public_text(js, "TV test")
