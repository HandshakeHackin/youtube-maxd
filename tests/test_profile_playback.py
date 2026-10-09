"""Selected-account extraction and credential boundaries, without network use."""
import asyncio
import json

import httpx
import pytest
from fastapi.testclient import TestClient

from prosperotube.app import create_app
from prosperotube.extractor import Extractor, ProfileExtractors
from prosperotube.models import ServiceError
from prosperotube.worker import install_profile_cookies, attach_media_cookies

ID = "abcdefghijk"
AUTH = {"Authorization": "Bearer profile-test-token"}


def source(label):
    return {"id": ID, "title": label, "duration": 8, "formats": [{
        "format_id": "p", "url": "https://r1.googlevideo.com/video", "ext": "mp4", "protocol": "https",
        "vcodec": "avc1.64001e", "acodec": "mp4a.40.2", "height": 360, "width": 640}]}


def test_extractor_payload_cache_and_profile_credentials_are_partitioned(monkeypatch):
    calls = []
    class Process:
        returncode = None
        def __init__(self):
            self.stdin = self
            self.stdout = self
            self.body = None
            self.sent = False
        def write(self, value):
            self.body = json.loads(value)
            calls.append(self.body)
        async def drain(self): pass
        def close(self): pass
        async def read(self, size):
            if self.sent: return b""
            self.sent = True
            return json.dumps(source(self.body["cookie_header"] or "Guest")).encode()
        async def wait(self): self.returncode = 0
        def kill(self): self.returncode = -9
    async def spawn(*args, **kwargs):
        assert args[-2:] == ("-m", "prosperotube.worker")
        assert not any("synthetic-secret" in str(arg) for arg in args)
        return Process()
    monkeypatch.setattr(asyncio, "create_subprocess_exec", spawn)
    async def run():
        guest = Extractor()
        profiles = ProfileExtractors(guest, [
            {"id": "one", "cookie": "SESSION=synthetic-secret-one", "account_index": 0},
            {"id": "two", "cookie": "SESSION=synthetic-secret-two", "account_index": 0},
            {"id": "secondary", "cookie": "SESSION=synthetic-secret-other", "account_index": 1},
            {"id": "brand", "cookie": "SESSION=synthetic-secret-brand", "channel": "UC" + "a" * 22}])
        results = []
        for owner in ("one", "two", "guest", "one"):
            resolver = profiles.for_profile(owner)
            assert resolver.semaphore is guest.semaphore
            results.append(await resolver.extract("https://www.youtube.com/watch?v=" + ID))
        assert len(calls) == 3
        assert [r["title"] for r in results] == ["SESSION=synthetic-secret-one", "SESSION=synthetic-secret-two", "Guest", "SESSION=synthetic-secret-one"]
        assert all(r["cookies"] is None for r in calls)
        for owner, code in (("unknown", "unknown_profile"), ("secondary", "playback_account_context"), ("brand", "playback_account_context")):
            with pytest.raises(ServiceError) as error: profiles.for_profile(owner)
            assert error.value.code == code
    asyncio.run(run())


def test_pinned_ytdlp_cookie_jar_scopes_and_media_handoff():
    from yt_dlp import YoutubeDL
    with YoutubeDL({"quiet": True, "skip_download": True}) as ydl:
        install_profile_cookies(ydl, "SAPISID=synthetic-secret; SESSION=synthetic-session")
        jar = ydl.cookiejar
        assert "synthetic-secret" in jar.get_cookie_header("https://www.youtube.com/watch?v=" + ID)
        for url in ("https://youtube.com.evil.example/", "https://r1.googlevideo.com/video", "http://youtube.com/"):
            assert not jar.get_cookie_header(url)
        info = source("Private fixture")
        info["url"] = "https://www.youtube.com/watch?v=" + ID
        info["subtitles"] = {"en": [{"url": "https://www.youtube.com/api/timedtext?v=" + ID},
                                     {"url": "https://evil.example/captions"}]}
        attach_media_cookies(ydl, info)
        assert "Cookie" not in info["formats"][0].get("http_headers", {})
        assert "Cookie" not in info.get("http_headers", {})
        assert "synthetic-secret" in info["subtitles"]["en"][0]["http_headers"]["Cookie"]
        assert "http_headers" not in info["subtitles"]["en"][1]
        with pytest.raises(ValueError): install_profile_cookies(ydl, "invalid\nheader")
        with pytest.raises(ValueError): install_profile_cookies(ydl, "")


def test_selected_profile_routes_all_api_and_job_extraction_without_public_credentials():
    calls = []
    class Resolver:
        def __init__(self, owner): self.owner = owner
        async def extract(self, target, **kwargs):
            calls.append((self.owner, target))
            info = source(self.owner)
            info["http_headers"] = {"Cookie": "synthetic-account-secret"}
            return {"entries": [info]} if kwargs.get("flat") else info
    class Routing:
        def for_profile(self, profile):
            if profile not in ("guest", "one", "two"):
                raise ServiceError("unknown_profile", "Select a profile.", 404)
            return Resolver(profile)
    transport = httpx.AsyncClient(transport=httpx.MockTransport(lambda r: httpx.Response(404)))
    app = create_app(extractor=Routing(), token="profile-test-token", http_client=transport)
    with TestClient(app) as client:
        for route in ("/v1/search?q=fixture", "/v1/catalog?parent=playlist:PLabcdefghijk", "/v1/video/" + ID):
            response = client.get(route + ("&" if "?" in route else "?") + "profile=one", headers=AUTH)
            assert response.status_code == 200 and "synthetic-account-secret" not in response.text
            assert calls[-1][0] == "one"
        response = client.post("/v1/resolve", headers=AUTH, json={"id": ID, "profile": "two"})
        assert response.status_code == 200 and response.json()["title"] == "two", response.text
        assert "synthetic-account-secret" not in response.text
        assert client.post("/v1/resolve", headers=AUTH, json={"id": ID, "profile": "unknown"}).status_code == 404
        assert client.get("/v1/video/" + ID, headers=AUTH).json()["item"]["title"] == "guest"
        for action in ("search", "catalog", "details", "resolve"):
            payload = {"action": action, "profile": "one", "query": "fixture", "parent": "playlist:PLabcdefghijk", "id": ID,
                       "playback": {"id": ID, "profile": "two"}}
            created = client.post("/v1/jobs", headers=AUTH, json=payload).json()
            # The job coroutine completes immediately with this resolver.
            result = client.get("/v1/jobs/" + created["job"], headers=AUTH).json()
            assert result["state"] == "done"
            assert calls[-1][0] == ("two" if action == "resolve" else "one")
            assert "synthetic-account-secret" not in json.dumps(result)
