import asyncio
import copy
import json
import time
import xml.etree.ElementTree as ET
import httpx
import pytest
from fastapi.testclient import TestClient

from prosperotube.app import create_app
from prosperotube.captions import vtt_to_srt
from prosperotube.catalog import item, page_for
from prosperotube.models import ResolveRequest, ServiceError, safe_remote
from prosperotube.sessions import Sessions, dash_manifest

ID = "abcdefghijk"
TOKEN = "test-token-never-a-real-account-secret"
AUTH = {"Authorization": "Bearer " + TOKEN}
BASE = "http://testserver"


def info():
    return {"id": ID, "title": "A synthetic video — 日本語", "duration": 20,
            "channel": "Fixture channel", "channel_id": "UC" + "a" * 22,
            "http_headers": {"User-Agent": "fixture", "Cookie": "host-only-cookie"},
            "formats": [
                {"format_id": "v", "url": "https://r1.googlevideo.com/video?signature=private", "ext": "mp4",
                 "protocol": "https", "vcodec": "avc1.640028", "acodec": "none", "width": 1920,
                 "height": 1080, "fps": 30, "tbr": 2500, "dynamic_range": "SDR"},
                {"format_id": "a", "url": "https://r1.googlevideo.com/audio?signature=private", "ext": "m4a",
                 "protocol": "https", "vcodec": "none", "acodec": "mp4a.40.2", "abr": 128, "asr": 48000,
                 "language": "en"},
                {"format_id": "p", "url": "https://r1.googlevideo.com/muxed", "ext": "mp4",
                 "protocol": "https", "vcodec": "avc1.64001f", "acodec": "mp4a.40.2",
                 "width": 1280, "height": 720, "tbr": 1000}],
            "subtitles": {"en": [{"ext": "vtt", "url": "https://www.youtube.com/api/timedtext?v=" + ID}]}}


class FakeExtractor:
    def __init__(self, result=None):
        self.result = result or info()
        self.calls = []

    async def extract(self, target, **kw):
        self.calls.append((target, kw))
        if kw.get("flat"):
            return {"entries": [copy.deepcopy(self.result)]}
        return copy.deepcopy(self.result)


@pytest.mark.parametrize("query,target", [
    ("https://youtu.be/abcdefghijk", "https://www.youtube.com/watch?v=abcdefghijk"),
    ("video:abcdefghijk", "https://www.youtube.com/watch?v=abcdefghijk"),
    ("https://www.youtube.com/playlist?list=PLabcdefghijk", "https://www.youtube.com/playlist?list=PLabcdefghijk"),
    ("channel:UC" + "a" * 22, "https://www.youtube.com/channel/UC" + "a" * 22 + "/videos"),
])
def test_search_opens_stable_links(query, target):
    extractor = FakeExtractor()
    page = asyncio.run(page_for(extractor, query=query))
    assert len(page.items) == 1 and extractor.calls[0][0] == target


def test_malformed_remote_url_is_service_error():
    for url in ("https://www.youtube.com:bad/video", "https://[broken/video"):
        with pytest.raises(ServiceError): safe_remote(url)


class Chunks(httpx.AsyncByteStream):
    def __init__(self, data): self.data = data
    async def __aiter__(self):
        for start in range(0, len(self.data), 3):
            yield self.data[start:start + 3]


def app(extractor=None, handler=None, **kw):
    client = httpx.AsyncClient(transport=httpx.MockTransport(handler or (lambda req: httpx.Response(404))))
    return create_app(extractor=extractor or FakeExtractor(), token=TOKEN,
                      pairing_code="123456", http_client=client, **kw)


def test_pairing_and_authorization():
    with TestClient(app()) as client:
        assert client.get("/v1/health").json()["api_version"] == 1
        assert client.get("/v1/search?q=fixture").status_code == 401
        assert client.post("/v1/pair", json={"code": "000000"}).status_code == 403
        assert client.post("/v1/pair", json={"code": "123456"}).json()["token"] == TOKEN
        assert client.get("/v1/search?q=fixture", headers=AUTH).json()["items"][0]["id"] == ID


def test_pair_code_expiry_and_rate_limit():
    application = app()
    with TestClient(application) as client:
        for _ in range(5):
            assert client.post("/v1/pair", json={"code": "000000"}).status_code == 403
        assert client.post("/v1/pair", json={"code": "123456"}).status_code == 429
        application.state.pair_attempts.clear()
        application.state.pairing_deadline = 0
        assert client.post("/v1/pair", json={"code": "123456"}).status_code == 410


@pytest.mark.parametrize("url", ["file:///etc/passwd", "http://youtube.com/a", "https://youtube.com.evil.test/",
                                     "https://127.0.0.1/a", "https://u:p@youtube.com/a", "https://youtube.com:8443/a"])
def test_remote_origin_scope(url):
    with pytest.raises(ServiceError): safe_remote(url)


def test_catalog_stable_ids_and_unicode():
    row = item(info())
    assert row.id == ID and "日本語" in row.title
    assert row.art_url == f"https://i.ytimg.com/vi/{ID}/hqdefault.jpg"
    assert item({"id": "too-short", "title": "ignored"}) is None


def test_paging_deduplicates_and_preserves_order():
    class Pages(FakeExtractor):
        async def extract(self, target, **kw):
            self.calls.append((target, kw))
            return {"entries": [info(), info(), {**info(), "id": "12345678901"}] + [None] * 22}
    source = Pages()
    page = asyncio.run(page_for(source, query="Unicode café", page=2))
    assert [row.id for row in page.items] == [ID, "12345678901"]
    assert page.has_more
    assert source.calls[0][1] == {"flat": True, "start": 49, "end": 73}


def test_catalog_targets_are_not_arbitrary_urls():
    source = FakeExtractor()
    with pytest.raises(ServiceError): asyncio.run(page_for(source, parent="http://localhost/admin"))
    asyncio.run(page_for(source, parent="channel:UC" + "a" * 22))
    assert source.calls[-1][0].endswith("/videos")


def test_resolution_keeps_secrets_on_host_and_builds_two_track_dash():
    sessions = Sessions()
    playback = sessions.create(info(), ResolveRequest(id=ID), BASE)
    serialized = playback.model_dump_json()
    assert "signature" not in serialized and "host-only-cookie" not in serialized
    assert playback.choices[0].container == "dash"
    assert playback.choices[1].container == "mp4"
    session = next(iter(sessions.sessions.values()))
    manifest = ET.fromstring(next(iter(session.manifests.values())))
    ns = {"d": "urn:mpeg:dash:schema:mpd:2011"}
    assert len(manifest.findall(".//d:AdaptationSet", ns)) == 2
    assert all(u.attrib["media"].startswith(BASE + "/media/") for u in manifest.findall(".//d:SegmentURL", ns))
    assert any(r.headers.get("Cookie") == "host-only-cookie" for r in session.tracks.values())


def test_quality_and_hdr_av1_filtering():
    source = info()
    source["formats"] += [dict(source["formats"][0], height=2160, vcodec="av01.0.12M.10"),
                           dict(source["formats"][0], height=2160, dynamic_range="HDR10")]
    playback = Sessions().create(source, ResolveRequest(id=ID, max_height=720, codec="avc"), BASE)
    assert all(c.height <= 720 and c.video_codec == "h264" for c in playback.choices)


def test_vp9_opus_mapping_and_audio_language_preference():
    source = info()
    source["formats"][0].update(vcodec="vp09.00.31.08", ext="webm")
    source["formats"][1].update(acodec="opus", ext="webm", language="ja")
    playback = Sessions().create(source, ResolveRequest(id=ID, codec="vp9", audio_language="ja"), BASE)
    assert playback.choices[0].video_codec == "vp9" and playback.choices[0].audio_codec == "opus"


def test_unsupported_transport_is_explained():
    source = info()
    for f in source["formats"]: f["protocol"] = "sabr"
    with pytest.raises(ServiceError, match="No compatible"):
        Sessions().create(source, ResolveRequest(id=ID), BASE)


def test_session_expiry_and_capacity_do_not_evict_active_playback():
    sessions = Sessions(max_sessions=1)
    playback = sessions.create(info(), ResolveRequest(id=ID), BASE)
    with pytest.raises(ServiceError) as exc: sessions.create(info(), ResolveRequest(id=ID), BASE)
    assert exc.value.status == 503
    token = playback.choices[0].url.split("/")[4]
    sessions.sessions[token].deadline = 0
    with pytest.raises(ServiceError) as exc: sessions.get(token)
    assert exc.value.status == 410
    sessions.create(info(), ResolveRequest(id=ID), BASE)


def test_caption_timing_unicode_and_markup_conversion():
    result = vtt_to_srt(b"WEBVTT\n\ncue-a\n00:01.250 --> 00:03.500 align:start\n<v Narrator>Hello &amp; <00:02.000>world</v>\n\n")
    assert result == "1\n00:00:01,250 --> 00:00:03,500\nHello & world\n"
    assert "日本語" in vtt_to_srt("WEBVTT\n\n01:00:00.000 --> 01:00:01.000\n日本語\n".encode())


def test_media_relay_preserves_ranges_and_scoped_headers():
    observed = []
    def handler(request):
        observed.append(request)
        return httpx.Response(206, headers={"Content-Range": "bytes 2-5/20", "Content-Length": "4", "Content-Type": "video/mp4"}, stream=Chunks(b"2345"))
    with TestClient(app(handler=handler)) as client:
        playback = client.post("/v1/resolve", headers=AUTH, json={"id": ID}).json()
        manifest = client.get(playback["choices"][0]["url"]).text
        ns = {"d": "urn:mpeg:dash:schema:mpd:2011"}
        url = ET.fromstring(manifest).find(".//d:SegmentURL", ns).attrib["media"]
        reply = client.get(url, headers={"Range": "bytes=2-5", "Authorization": "Bearer unrelated-client-value"})
        assert reply.status_code == 206 and reply.content == b"2345"
        assert reply.headers["content-range"] == "bytes 2-5/20"
        assert observed[-1].headers["range"] == "bytes=2-5"
        assert observed[-1].headers["cookie"] == "host-only-cookie"
        assert "authorization" not in observed[-1].headers
        assert client.get(url, headers={"Range": "bytes=0-1,4-5"}).status_code == 416


def test_redirect_drops_cookie_and_rejects_local_origin():
    observed = []
    def handler(request):
        observed.append(request)
        if len(observed) == 1:
            return httpx.Response(302, headers={"Location": "https://r2.googlevideo.com/video"})
        return httpx.Response(200, stream=Chunks(b"media"))
    with TestClient(app(handler=handler)) as client:
        playback = client.post("/v1/resolve", headers=AUTH, json={"id": ID}).json()
        reply = client.get(playback["choices"][1]["url"])
        assert reply.content == b"media"
        assert "cookie" in observed[0].headers and "cookie" not in observed[1].headers
    with TestClient(app(handler=lambda request: httpx.Response(302, headers={"Location": "https://127.0.0.1/private"}))) as client:
        playback = client.post("/v1/resolve", headers=AUTH, json={"id": ID}).json()
        assert client.get(playback["choices"][1]["url"]).status_code == 502


def test_expired_upstream_requires_resolution_refresh():
    with TestClient(app(handler=lambda request: httpx.Response(403))) as client:
        playback = client.post("/v1/resolve", headers=AUTH, json={"id": ID}).json()
        assert client.get(playback["choices"][1]["url"]).status_code == 410


def test_caption_relay_converts_to_srt():
    raw = b"WEBVTT\n\n00:00.000 --> 00:01.000\nFixture cue\n"
    with TestClient(app(handler=lambda request: httpx.Response(200, stream=Chunks(raw)))) as client:
        playback = client.post("/v1/resolve", headers=AUTH, json={"id": ID}).json()
        reply = client.get(playback["captions"][0]["url"])
        assert "00:00:00,000 --> 00:00:01,000" in reply.text


def test_jobs_complete_fail_and_cancel_without_blocking_api():
    application = app()
    with TestClient(application) as client:
        job = client.post("/v1/jobs", headers=AUTH, json={"action": "search", "query": "fixture"}).json()["job"]
        for _ in range(100):
            reply = client.get("/v1/jobs/" + job, headers=AUTH).json()
            if reply["state"] != "pending": break
        assert reply["state"] == "done" and reply["result"]["items"][0]["id"] == ID
        assert client.delete("/v1/jobs/" + job, headers=AUTH).json()["state"] == "cancelled"
        assert client.get("/v1/jobs/" + job, headers=AUTH).status_code == 410
        assert client.post("/v1/jobs", headers=AUTH, json={"action": "resolve"}).status_code == 400


def test_optional_sponsorblock_failure_does_not_affect_playback():
    with TestClient(app(sponsorblock=True, handler=lambda request: httpx.Response(503))) as client:
        assert client.get("/v1/sponsor/" + ID, headers=AUTH).json()["segments"] == []
        assert client.post("/v1/resolve", headers=AUTH, json={"id": ID}).status_code == 200


def test_privacy_preserving_sponsorblock_prefix_and_category_filter():
    observed = []
    def handler(request):
        observed.append(request)
        return httpx.Response(200, json=[{"videoID": ID, "segments": [
            {"category": "sponsor", "segment": [2, 5], "actionType": "skip"},
            {"category": "sponsor", "segment": [9, 7]},
            {"category": "intro", "segment": [1, 2]}]}])
    with TestClient(app(sponsorblock=True, handler=handler)) as client:
        result = client.get("/v1/sponsor/" + ID, headers=AUTH).json()
        assert result["segments"] == [{"start": 2, "end": 5, "category": "sponsor"}]
        assert ID not in str(observed[0].url)
        assert len(observed[0].url.path.split("/")[-1]) == 4


@pytest.mark.parametrize("category", ["sponsor", "selfpromo", "interaction", "intro", "outro", "preview", "music_offtopic", "filler"])
def test_sponsorblock_selected_category_and_invalid_timestamps(category):
    observed = []
    def handler(request):
        observed.append(request)
        return httpx.Response(200, json=[{"videoID": ID, "segments": [
            {"category": category, "segment": [2, 12], "actionType": "skip"},
            {"category": category, "segment": [True, 12], "actionType": "skip"},
            {"category": category, "segment": [2, False], "actionType": "skip"},
            {"category": category, "segment": [-1, 12], "actionType": "skip"},
            {"category": category, "segment": [2, 12], "actionType": "mute"}]}])
    with TestClient(app(sponsorblock=True, handler=handler)) as client:
        result = client.get("/v1/sponsor/" + ID, params={"categories": category}, headers=AUTH)
        assert result.status_code == 200
        assert result.json()["segments"] == [{"start": 2, "end": 12, "category": category}]
        assert json.loads(observed[0].url.params["categories"]) == [category]
        assert ID not in str(observed[0].url)
        client.get("/v1/sponsor/" + ID, params={"categories": category}, headers=AUTH)
        assert len(observed) == 1
        assert client.get("/v1/sponsor/" + ID, params={"categories": "invalid"}, headers=AUTH).status_code == 400


def test_post_body_limit():
    with TestClient(app()) as client:
        assert client.post("/v1/jobs", headers=AUTH, content=b"x" * 9000).status_code == 413


def test_sponsorblock_highlight_points_and_malformed_entries():
    observed = []
    def handler(request):
        observed.append(request)
        return httpx.Response(200, json=[None, "invalid", {"videoID": ID, "segments": [
            None, "invalid", {"category": "poi_highlight", "segment": [40, 40], "actionType": "poi"},
            {"category": "poi_highlight", "segment": [2, 5], "actionType": "poi"},
            {"category": "poi_highlight", "segment": [2, 2], "actionType": "skip"},
            {"category": "poi_highlight", "segment": [True, True], "actionType": "poi"},
            {"category": "poi_highlight", "segment": [10**1000, 10**1000], "actionType": "poi"},
            {"category": [], "segment": [2, 5], "actionType": "skip"},
            {"category": "sponsor", "segment": [2, 5], "actionType": "skip"}]}])
    with TestClient(app(sponsorblock=True, handler=handler)) as client:
        result = client.get("/v1/sponsor/" + ID, params={"categories": "poi_highlight,sponsor"}, headers=AUTH)
        assert result.status_code == 200
        assert result.json()["segments"] == [{"start": 40, "end": 40, "category": "poi_highlight"},
                                             {"start": 2, "end": 5, "category": "sponsor"}]
        assert json.loads(observed[0].url.params["actionTypes"]) == ["skip", "poi"]


@pytest.mark.parametrize("payload", [None, {}, "invalid", [None, "invalid", {"videoID": ID, "segments": {}}]])
def test_sponsorblock_malformed_payload_is_optional(payload):
    with TestClient(app(sponsorblock=True, handler=lambda request: httpx.Response(200, json=payload))) as client:
        result = client.get("/v1/sponsor/" + ID, headers=AUTH)
        assert result.status_code == 200 and result.json()["segments"] == []
