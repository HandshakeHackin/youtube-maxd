"""Actual pinned TV extraction boundaries and profile routing; no accounts/network."""
import asyncio
import json
import sys
from pathlib import Path

import httpx
import pytest
from yt_dlp import YoutubeDL
from yt_dlp.utils import ExtractorError

from prosperotube.catalog import page_for
from prosperotube.extractor import Extractor, ProfileExtractors
from prosperotube.models import ServiceError
from prosperotube.oauth_video import TVOAuthYoutubeIE, strip_oauth_headers

TOKEN = "synthetic-private-oauth-token"
ID = "abcdefghijk"


def test_actual_pinned_tv_extractor_headers_context_fallback_and_script():
    with YoutubeDL({"quiet": True, "skip_download": True}) as ydl:
        ie = TVOAuthYoutubeIE(TOKEN); ydl.add_info_extractor(ie)
        assert ydl.get_info_extractor("Youtube") is ie and ie.is_authenticated
        assert ie._get_requested_clients("https://www.youtube.com/watch?v=" + ID, {"is_music_url": True}, True) == ["tv", "tv_downgraded"]
        for client in ("tv", "tv_downgraded"):
            headers = ie.generate_api_headers(default_client=client)
            assert headers["Authorization"] == "Bearer " + TOKEN
            assert headers["X-YouTube-Client-Name"] == "7"
        assert "Authorization" not in ie.generate_api_headers(default_client="web")
        with pytest.raises(ExtractorError): ie.generate_api_headers(default_client="tv", api_hostname="evil.example")
        with pytest.raises(ExtractorError): ie._extract_player_response("web_embedded", ID)
        tv = {"INNERTUBE_CONTEXT": {"client": {"clientName": "TVHTML5"}},
              "PLAYER_JS_URL": "/s/player/879f7172/tv-player-ias-tcl.vflset/tv-player-ias-tcl.js"}
        assert ie._extract_player_url({"PLAYER_JS_URL": "/s/player/879f7172/player_ias.vflset/en_US/base.js"}, tv).endswith("tv-player-ias-tcl.js")
        assert ie._construct_player_url(player_id="879f7172").endswith("tv-player-ias.js")
        assert "base.js" not in ie._construct_player_url(player_url="https://www.youtube.com/s/player/879f7172/player_ias.vflset/en_US/base.js")
        with pytest.raises(ExtractorError): ie._construct_player_url(player_url="https://evil.example/s/player/879f7172/tv-player-ias.vflset/tv-player-ias.js")


def test_actual_pinned_tv_timestamp_uses_matching_script_not_web_config(monkeypatch):
    with YoutubeDL({"quiet": True, "skip_download": True,
                    "extractor_args": {"youtube": {"player_js_version": ["11111@aaaaaaaa"]}}}) as ydl:
        ie = TVOAuthYoutubeIE(TOKEN); ydl.add_info_extractor(ie)
        scripts = []
        def load(video_id, player_url, fatal=True):
            scripts.append(player_url)
            return "var cfg={signatureTimestamp:22222};" if "ias-tcl" in player_url else "var cfg={signatureTimestamp:33333};"
        monkeypatch.setattr(ie, "_load_player", load)
        first = "https://www.youtube.com/s/player/879f7172/tv-player-ias-tcl.vflset/tv-player-ias-tcl.js"
        second = "https://www.youtube.com/s/player/879f7172/tv-player-ias.vflset/tv-player-ias.js"
        # WEB STS, TV config STS and caller STS@HASH cannot substitute for JS.
        assert ie._extract_signature_timestamp(ID, first, {"STS": 11111}) == 22222
        assert ie._extract_signature_timestamp(ID, second, {"STS": 44444}) == 33333
        assert ie._extract_signature_timestamp(ID, first, {"STS": 55555}) == 22222
        assert scripts == [first, second]  # variant-scoped pinned cache
        assert ie._get_player_js_version() == (None, None)
        for url in ("https://evil.example/s/player/879f7172/tv-player-ias.js",
                    "https://www.youtube.com/s/player/879f7172/player_ias.vflset/en_US/base.js"):
            with pytest.raises(ExtractorError): ie._extract_signature_timestamp(ID, url, {"STS": 11111})
        monkeypatch.setattr(ie, "_load_player", lambda *args, **kwargs: "var cfg={};")
        missing = "https://www.youtube.com/s/player/aaaaaaaa/tv-player-ias.vflset/tv-player-ias.js"
        assert ie._extract_signature_timestamp(ID, missing, {"STS": 11111}) is None


def test_tv_api_transport_blocks_redirects_other_origins_and_web_context(monkeypatch):
    requests = []
    response = [httpx.Response(200, json={"videoDetails": {"videoId": ID}})]
    def handler(request):
        requests.append(request)
        assert request.headers["Authorization"] == "Bearer " + TOKEN
        return response[0]
    original = httpx.Client
    def client(**kwargs):
        assert kwargs["follow_redirects"] is False and kwargs["timeout"] == 12
        return original(**kwargs, transport=httpx.MockTransport(handler))
    monkeypatch.setattr(httpx, "Client", client)
    ie = TVOAuthYoutubeIE(TOKEN)
    data = json.dumps({"context": {"client": {"clientName": "TVHTML5"}}, "videoId": ID}).encode()
    headers = {"Authorization": "Bearer " + TOKEN}
    api = "https://www.youtube.com/youtubei/v1/player"
    assert ie._download_json(api, ID, headers=headers, data=data)["videoDetails"]["videoId"] == ID
    for bad in ("http://www.youtube.com/youtubei/v1/player", "https://evil.example/youtubei/v1/player", "https://www.youtube.com/watch", "https://www.youtube.com:443/youtubei/v1/player"):
        with pytest.raises(ExtractorError): ie._download_json(bad, ID, headers=headers, data=data)
    with pytest.raises(ExtractorError): ie._download_json(api, ID, headers=headers, data=b'{"context":{"client":{"clientName":"WEB"}}}')
    assert len(requests) == 1
    for status in (302, 401, 403, 429, 500):
        response[0] = httpx.Response(status, headers={"Location": "https://evil.example/steal"})
        with pytest.raises(ExtractorError): ie._download_json(api, ID, headers=headers, data=data)
    assert len(requests) == 6
    response[0] = httpx.Response(200, content=b"x" * (8 * 1024 * 1024 + 1))
    with pytest.raises(ExtractorError): ie._download_json(api, ID, headers=headers, data=data)
    value = {"http_headers": {"Authorization": "Bearer " + TOKEN}, "formats": [{"http_headers": {"authorization": TOKEN, "User-Agent": "TV"}}], "subtitles": {"en": [{"http_headers": {"Authorization": TOKEN}}]}}
    strip_oauth_headers(value)
    assert TOKEN not in json.dumps(value) and value["formats"][0]["http_headers"]["User-Agent"] == "TV"


def test_actual_pinned_player_request_is_tv_and_authenticated(monkeypatch):
    original = httpx.Client
    captured = []
    def handler(request):
        captured.append(request)
        return httpx.Response(200, json={"videoDetails": {"videoId": ID}, "playabilityStatus": {"status": "OK"}})
    monkeypatch.setattr(httpx, "Client", lambda **kwargs: original(**kwargs, transport=httpx.MockTransport(handler)))
    with YoutubeDL({"quiet": True, "skip_download": True}) as ydl:
        ie = TVOAuthYoutubeIE(TOKEN); ydl.add_info_extractor(ie)
        monkeypatch.setattr(ie, "_load_player", lambda *args, **kwargs: "var cfg={signatureTimestamp:22222};")
        script = "https://www.youtube.com/s/player/879f7172/tv-player-ias.vflset/tv-player-ias.js"
        result = ie._extract_player_response("tv", ID, {"STS": 11111}, ie._get_default_ytcfg("tv"), script, None, None, None, None)
        assert result["videoDetails"]["videoId"] == ID
    assert len(captured) == 1
    payload = json.loads(captured[0].content)
    assert payload["context"]["client"]["clientName"] == "TVHTML5"
    assert payload["playbackContext"]["contentPlaybackContext"]["signatureTimestamp"] == 22222
    assert captured[0].headers["Authorization"] == "Bearer " + TOKEN


def test_oauth_profile_refresh_private_subprocess_cache_and_paged_tv_search(monkeypatch):
    requests, bodies = [], []
    class Library:
        async def execute(self, request, *, operation):
            assert operation == "playback"; requests.append(request)
            if request["action"] == "credentials":
                return {"access_token": TOKEN, "expiry_date": "2099-01-01T00:00:00.000Z"}
            if request.get("cursor"):
                return {"items": [{"id": "zyxwvutsrqp", "title": "Second"}], "has_more": False, "cursor": ""}
            return {"items": [{"id": ID, "title": "First"}], "has_more": True, "cursor": "opaque_tv_cursor"}
    class Process:
        returncode = None
        def __init__(self): self.stdin = self; self.stdout = self; self.sent = False
        def write(self, data): bodies.append(json.loads(data))
        async def drain(self): pass
        def close(self): pass
        async def read(self, size):
            if self.sent: return b""
            self.sent = True
            return json.dumps({"id": ID, "title": "TV authorized", "formats": []}).encode()
        async def wait(self): self.returncode = 0
        def kill(self): self.returncode = -9
    async def spawn(*args, **kwargs):
        assert TOKEN not in json.dumps(args); return Process()
    monkeypatch.setattr(asyncio, "create_subprocess_exec", spawn)
    async def run():
        profiles = ProfileExtractors(Extractor(), [{"id": "one", "cookie": "", "oauth": {}}], Library())
        # Presence of OAuth, including malformed material, cannot become Guest.
        with pytest.raises(ServiceError): profiles.for_profile("one")
        profiles.profile_source = lambda: [{"id": "one", "cookie": "", "oauth": {
            "access_token": TOKEN, "refresh_token": "synthetic-refresh", "expiry_date": "2099-01-01T00:00:00.000Z",
            "client": {"client_id": "synthetic-client", "client_secret": "synthetic-secret"}}}]
        resolver = profiles.for_profile("one")
        assert resolver.semaphore is profiles.guest.semaphore
        first = await resolver.extract("https://www.youtube.com/watch?v=" + ID)
        assert first["title"] == "TV authorized"
        await resolver.extract("https://www.youtube.com/watch?v=" + ID)
        assert len(bodies) == 1 and bodies[0]["oauth_access_token"] == TOKEN
        assert bodies[0]["cookies"] is None and bodies[0]["cookie_header"] is None
        result = await page_for(resolver, query="TV search")
        assert result.has_more and result.items[0].id == ID
        result = await page_for(resolver, query="TV search", page=1)
        assert not result.has_more and result.items[0].id == "zyxwvutsrqp"
        await page_for(resolver, query="TV search", page=1)
        assert len([r for r in requests if r["action"] == "catalog"]) == 2
        assert all(r["profile"] == "one" for r in requests)
        assert requests[-1]["cursor"] == "opaque_tv_cursor"
    asyncio.run(run())


def test_actual_pinned_extraction_normalizes_tv_formats_without_token_leak(monkeypatch):
    original = httpx.Client
    requests = []
    def handler(request):
        requests.append(request)
        assert request.url.path == "/youtubei/v1/player"
        assert request.headers["Authorization"] == "Bearer " + TOKEN
        assert json.loads(request.content)["context"]["client"]["clientName"] == "TVHTML5"
        return httpx.Response(200, json={"playabilityStatus": {"status": "OK"},
            "videoDetails": {"videoId": ID, "title": "TV test", "lengthSeconds": "8", "channelId": "UC" + "a" * 22,
                             "author": "TV creator", "isLive": False, "isLiveContent": False, "shortDescription": "Offline TV test"},
            "streamingData": {"expiresInSeconds": "3600", "adaptiveFormats": [
                {"itag": 137, "url": "https://r1.googlevideo.com/videoplayback?track=v", "mimeType": 'video/mp4; codecs="avc1.64001e"',
                 "bitrate": 100000, "width": 640, "height": 360, "fps": 30, "contentLength": "1000", "approxDurationMs": "8000"},
                {"itag": 140, "url": "https://r1.googlevideo.com/videoplayback?track=a", "mimeType": 'audio/mp4; codecs="mp4a.40.2"',
                 "bitrate": 64000, "audioSampleRate": "48000", "audioChannels": 2, "contentLength": "1000", "approxDurationMs": "8000"}]}})
    monkeypatch.setattr(httpx, "Client", lambda **kwargs: original(**kwargs, transport=httpx.MockTransport(handler)))
    with YoutubeDL({"quiet": True, "no_warnings": True, "skip_download": True,
                    "extractor_args": {"youtube": {"player_skip": ["webpage", "initial_data", "configs", "js"]}}}) as ydl:
        ydl.add_info_extractor(TVOAuthYoutubeIE(TOKEN))
        info = ydl.extract_info("https://www.youtube.com/watch?v=" + ID, download=False)
        info = ydl.sanitize_info(info)
    strip_oauth_headers(info)
    assert info["id"] == ID and info["title"] == "TV test"
    assert len(requests) == 2 and len(info["formats"]) == 2
    assert all(f["url"].startswith("https://r1.googlevideo.com/") for f in info["formats"])
    assert TOKEN not in json.dumps(info)
    from prosperotube.models import ResolveRequest
    from prosperotube.sessions import Sessions
    playback = Sessions().create(info, ResolveRequest(id=ID), "http://companion.test")
    assert playback.choices and playback.choices[0].container == "dash"
    assert TOKEN not in playback.model_dump_json()


def test_real_node_private_preflight_cannot_be_requested_through_http(tmp_path):
    from prosperotube.device_auth import OAuthProfiles
    from prosperotube.library import LibraryService
    from prosperotube.app import create_app
    node = Path.home() / ".cache/prosperotube/node/node-v24.10.0-linux-x64/bin/node"
    dependency = Path(__file__).resolve().parents[1] / "companion/innertube/worker.bundle.mjs"
    if not node.is_file() or not dependency.is_file(): pytest.skip("Build the pinned Node worker first")
    wrapper = tmp_path / "no-network.mjs"
    wrapper.write_text("globalThis.fetch = async () => { throw new Error('Network disabled'); };\nawait import(" + json.dumps(dependency.as_uri()) + ");\n")
    store = OAuthProfiles(tmp_path / "private")
    profile = store.append({"access_token": TOKEN, "refresh_token": "synthetic-refresh",
        "expiry_date": "2099-01-01T00:00:00.000Z", "client": {"client_id": "fixture-client", "client_secret": "fixture-secret"}}, "TV")
    async def run():
        library = LibraryService(node, worker=wrapper, oauth_store=store)
        app = create_app(token="test-token", library=library)
        async with app.router.lifespan_context(app):
            result = await library.execute({"action": "credentials", "profile": profile["id"]}, operation="playback")
            assert result["access_token"] == TOKEN and set(result) == {"access_token", "expiry_date"}
            async with httpx.AsyncClient(transport=httpx.ASGITransport(app=app), base_url="http://test") as client:
                headers = {"Authorization": "Bearer test-token"}
                public = await client.post("/v1/library", headers=headers, json={"action": "profiles"})
                assert public.status_code == 200 and TOKEN not in public.text
                for value in ({"action": "credentials", "profile": profile["id"]},
                              {"action": "profiles", "operation": "playback"}):
                    assert (await client.post("/v1/library", headers=headers, json=value)).status_code == 422
        assert library.process is None
    asyncio.run(run())
