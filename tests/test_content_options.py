"""Content locale, hide switches, channel pages and thumbnail source; no network use."""
import asyncio
import hashlib
import time

import httpx
import pytest
from fastapi.testclient import TestClient

from prosperotube.app import create_app, sponsorblock_origin
from prosperotube.catalog import item, page_for
from prosperotube.content_locale import COUNTRIES, LANGUAGES, extractor_language, locale_options
from prosperotube.description import bounded, describe
from prosperotube.extractor import Extractor, ProfileExtractors
from prosperotube.models import ResolveRequest, ServiceError
from prosperotube.thumbnail_source import apply as thumbnail_apply
from prosperotube.worker import language_arguments
from test_companion import AUTH, TOKEN, ID, FakeExtractor, info, app as plain_app

CHANNEL = "UC" + "c" * 22
SUPPORTED = ["en", "en-GB", "fr", "iw", "no", "pt", "pt-PT", "ro", "zh-TW"]


class Library:
    def __init__(self, result=None):
        self.calls = []
        self.result = result or dict(api_version=1, kind="catalog", items=[], cursor="", has_more=False)
    async def execute(self, request, operation="execute"):
        self.calls.append((request, operation))
        return self.result
    async def close(self): pass


def finished(client, job):
    for _ in range(200):
        result = client.get("/v1/jobs/" + job, headers=AUTH).json()
        if result["state"] != "pending":
            return result
        time.sleep(.005)
    raise AssertionError("job did not finish")


def test_locale_tables_match_the_reference_lists_and_the_extractor_mapping():
    assert len(LANGUAGES) == 49 and len(COUNTRIES) == 89
    assert {LANGUAGES[k] for k in ("he", "nb", "mo", "pt-BR", "en-NZ")} == {"iw", "no", "ro", "pt", "en-GB"}
    assert locale_options() == {} and locale_options("fr", "") == {"hl": "fr"}
    assert locale_options("", "JP") == {"gl": "JP"} and locale_options("he", "IL") == {"hl": "he", "gl": "IL"}
    for hl, gl in [("xx", ""), ("EN", ""), ("en_US", ""), ("", "us"), ("", "GB-SCT"), (7, ""), ("", None)]:
        with pytest.raises(ServiceError) as error:
            locale_options(hl, gl)
        assert error.value.code == "invalid_locale" and error.value.status == 400
    assert [extractor_language(tag, SUPPORTED) for tag in ("he", "pt-BR", "fr", "tjl", "", "de", None)] == \
        ["iw", "pt", "fr", "", "", "", ""]
    configured = {"youtube": {"player_client": ["tv"]}, "other": {"x": ["1"]}}
    assert language_arguments(configured, "", SUPPORTED) is configured
    assert language_arguments(configured, "de", SUPPORTED) is configured
    assert language_arguments(configured, "nb", SUPPORTED) == {
        "youtube": {"player_client": ["tv"], "lang": ["no"]}, "other": {"x": ["1"]}}
    assert "lang" not in configured["youtube"]


def test_yt_dlp_knows_every_mapped_language_except_the_unsupported_tag():
    from yt_dlp.extractor.youtube import YoutubeIE
    assert {tag for tag in LANGUAGES if not extractor_language(tag, YoutubeIE._SUPPORTED_LANG_CODES)} == {"tjl"}


@pytest.mark.parametrize("operation", [
    dict(action="home", hl="fr", gl="BE", hide=["shorts_home", "watched_home"]),
    dict(action="home", hide=[]), dict(action="search", query="cats", hl="pt-BR", hide=["shorts_search", "mixes"]),
    dict(action="subscriptions", profile="signed", gl="JP", hide=["streams_subscriptions", "upcoming_subscriptions"]),
    dict(action="topics", section="trending", hl="he", hide=["shorts_trending"]),
    dict(action="channel", channel=CHANNEL), dict(action="channel", channel=CHANNEL, tab="shorts", channel_sort="popular", hl="de"),
    dict(action="channel_search", channel=CHANNEL, query="cats", hide=["upcoming_channel"]),
    dict(action="feedback", profile="signed", id=ID, cursor="12345678-1234-1234-1234-123456789abc", feedback="hide"),
    dict(action="playlist_save", profile="signed", playlist="PLabcdefghijk", session="intent-1"),
    dict(action="playlist_unsave", profile="signed", playlist="PLabcdefghijk", session="intent-2"),
    dict(action="history_pause", profile="signed", session="intent-3"),
    dict(action="history_resume", profile="signed", session="intent-4"),
    dict(action="history_clear", profile="signed", session="intent-5")])
def test_new_library_requests_reach_the_worker_boundary_directly_and_as_jobs(operation):
    library = Library()
    with TestClient(create_app(library=library, token=TOKEN)) as client:
        direct = client.post("/v1/library", json=operation, headers=AUTH)
        assert direct.status_code == 200
        job = client.post("/v1/jobs", json=dict(action="library", library=operation), headers=AUTH).json()["job"]
        assert finished(client, job)["result"] == direct.json()
        assert len(library.calls) == 2
        for sent, _ in library.calls:
            assert all(sent[key] == value for key, value in operation.items())
            assert "thumbnails" not in sent
            assert sent["hide"] == operation.get("hide") and sent["hl"] == operation.get("hl", "")


@pytest.mark.parametrize("request_body", [
    dict(action="home", hl="xx"), dict(action="home", hl="en_US"), dict(action="home", gl="us"),
    dict(action="home", gl="USA"), dict(action="home", hide=["shorts"]), dict(action="home", hide="mixes"),
    dict(action="home", hide=["mixes", "mixes"]), dict(action="home", hide=["mixes"] * 17),
    dict(action="home", thumbnails="best"), dict(action="home", tab="videos"), dict(action="home", channel_sort="popular"),
    dict(action="home", channel=CHANNEL), dict(action="channel"), dict(action="channel", channel="UCshort"),
    dict(action="channel", channel=CHANNEL, tab="about"), dict(action="channel", channel=CHANNEL, channel_sort="rating"),
    dict(action="channel", channel=CHANNEL, id=ID), dict(action="channel", channel=CHANNEL, query="cats"),
    dict(action="channel_search", channel=CHANNEL), dict(action="channel_search", channel=CHANNEL, query="  "),
    dict(action="channel_search", channel=CHANNEL, query="cats", tab="videos"),
    dict(action="playlist_save", playlist="PLabcdefghijk"), dict(action="playlist_save", session="intent"),
    dict(action="playlist_save", playlist="WL", session="intent"), dict(action="playlist_unsave", playlist="LL", session="intent"),
    dict(action="history_clear"), dict(action="history_pause", session="bad session"),
    dict(action="feedback", id=ID, cursor="reference", feedback="unhide")])
def test_invalid_new_requests_are_rejected_before_the_library(request_body):
    library = Library()
    with TestClient(create_app(library=library, token=TOKEN)) as client:
        for path, body in [("/v1/library", request_body), ("/v1/jobs", dict(action="library", library=request_body))]:
            assert client.post(path, json=body, headers=AUTH).status_code == 422
        assert not library.calls


def test_search_catalog_and_resolve_carry_only_requested_locale_and_filters():
    extractor = FakeExtractor()
    with TestClient(plain_app(extractor)) as client:
        assert client.get("/v1/search?q=fixture", headers=AUTH).status_code == 200
        assert client.get("/v1/search?q=fixture&hl=fr&gl=CA", headers=AUTH).status_code == 200
        assert client.get(f"/v1/catalog?parent=channel:{CHANNEL}&gl=JP", headers=AUTH).status_code == 200
        assert client.get("/v1/video/" + ID + "?hl=he", headers=AUTH).status_code == 200
        assert client.post("/v1/resolve", headers=AUTH, json={"id": ID}).status_code == 200
        assert client.post("/v1/resolve", headers=AUTH, json={"id": ID, "hl": "pt-BR"}).status_code == 200
        for body in (dict(action="search", query="fixture", hl="de", gl="AT", hide=["shorts_search"]),
                     dict(action="catalog", parent="channel:" + CHANNEL, hide=[]),
                     dict(action="details", id=ID, hl="ja"),
                     dict(action="resolve", playback={"id": ID, "hl": "ko"})):
            assert finished(client, client.post("/v1/jobs", headers=AUTH, json=body).json()["job"])["state"] == "done"
        options = [{k: v for k, v in kw.items() if k in ("hl", "gl", "hide")} for _, kw in extractor.calls]
        assert options == [{}, {"hl": "fr", "gl": "CA"}, {"gl": "JP"}, {"hl": "he"}, {}, {"hl": "pt-BR"},
                           {"hl": "de", "gl": "AT", "hide": ["shorts_search"]}, {"hide": []}, {"hl": "ja"}, {"hl": "ko"}]
        for url in ("/v1/search?q=fixture&hl=xx", "/v1/search?q=fixture&gl=us", f"/v1/catalog?parent=channel:{CHANNEL}&hl=EN",
                    "/v1/video/" + ID + "?hl=xx"):
            reply = client.get(url, headers=AUTH)
            assert reply.status_code == 400 and reply.json()["error"]["code"] == "invalid_locale"
        assert client.post("/v1/resolve", headers=AUTH, json={"id": ID, "hl": "xx"}).status_code == 422
        for body in (dict(action="search", query="fixture", hl="xx"), dict(action="search", query="fixture", hide=["x"]),
                     dict(action="resolve", playback={"id": ID, "hl": "en_US"})):
            assert client.post("/v1/jobs", headers=AUTH, json=body).status_code == 422
        assert len(extractor.calls) == 10
    with pytest.raises(ValueError):
        ResolveRequest(id=ID, hl="xx")


def test_worker_catalogs_and_caches_never_mix_locales_or_filter_selections():
    class Worker:
        def __init__(self): self.calls = []
        async def execute(self, request, operation="execute"):
            self.calls.append(request)
            tag = f"{request.get('hl', '')}/{request.get('gl', '')}/{','.join(request.get('hide') or [])}"
            return {"items": [{"id": ID, "title": tag, "is_short": request.get("hl") == "fr"}], "has_more": False, "cursor": ""}
    class Video(Extractor):
        def __init__(self):
            super().__init__(); self.calls = []
        async def extract(self, target, **kwargs):
            self.calls.append(kwargs); return {"id": ID, "title": "Video"}
    async def run():
        worker, video = Worker(), Video()
        resolver = ProfileExtractors(video, [], worker).for_profile("guest")
        titles = []
        for options in ({}, {"hl": "fr"}, {"hl": "fr", "gl": "BE"}, {"hl": "fr", "gl": "BE", "hide": ["shorts_search"]},
                        {"hide": []}, {"hl": "fr"}, {}):
            page = await page_for(resolver, query="sample", options=options)
            titles.append(page.items[0].title)
        assert titles == ["//", "fr//", "fr/BE/", "fr/BE/shorts_search", "//", "fr//", "//"]
        assert len(worker.calls) == 5  # the two repeats are served from their own cached catalogs
        assert [("hl" in call, "gl" in call, "hide" in call) for call in worker.calls] == [
            (False, False, False), (True, False, False), (True, True, False), (True, True, True), (False, False, True)]
        assert (await page_for(resolver, query="sample", options={"hl": "fr"})).items[0].is_short is True
        assert (await page_for(resolver, query="sample")).items[0].is_short is False
        await resolver.extract("https://www.youtube.com/watch?v=" + ID)
        await resolver.extract("https://www.youtube.com/watch?v=" + ID, hl="he", gl="IL")
        assert video.calls == [{"fresh": False}, {"fresh": False, "hl": "he"}]
    asyncio.run(run())


def test_plain_extractor_cache_is_partitioned_by_language(monkeypatch):
    payloads = []
    class Process:
        returncode = 0
        pid = None
        def __init__(self):
            self.stdin = type("In", (), {"write": lambda s, data: payloads.append(data), "drain": self.drain, "close": lambda s: None})()
            self.stdout = type("Out", (), {"read": self.read})()
            self.sent = False
        async def drain(self): pass
        async def read(self, _size):
            if self.sent: return b""
            self.sent = True; return b'{"id":"abcdefghijk"}'
        async def wait(self): return 0
        def kill(self): pass
    async def spawn(*args, **kwargs): return Process()
    monkeypatch.setattr(asyncio, "create_subprocess_exec", spawn)
    async def run():
        import json
        resolver = Extractor()
        url = "https://www.youtube.com/watch?v=" + ID
        for options in ({}, {"hl": "fr"}, {"hl": "fr", "gl": "BE", "hide": ["mixes"]}, {}, {"hl": "he"}):
            await resolver.extract(url, **options)
        sent = [json.loads(payload) for payload in payloads]
        assert [value.get("hl") for value in sent] == [None, "fr", "he"]
        assert all("gl" not in value and "hide" not in value for value in sent)
        assert (url, False, 1, 24) in resolver.cache and (url, False, 1, 24, "fr") in resolver.cache
    asyncio.run(run())


def test_classification_of_shorts_in_catalog_rows():
    assert item({**info(), "is_short": True}).is_short is True
    assert item({**info(), "media_type": "short"}).is_short is True
    assert item({**info(), "url": "https://www.youtube.com/shorts/" + ID}).is_short is True
    for extra in ({}, {"is_short": "yes"}, {"media_type": "video"}, {"url": "https://www.youtube.com/watch?v=" + ID},
                  {"url": "https://evil.test/https://www.youtube.com/shorts/x"}, {"url": 7}):
        assert item({**info(), **extra}).is_short is False


def test_thumbnail_source_rewrites_only_ordinary_youtube_video_art():
    rows = [dict(id=ID, art_url=f"https://i.ytimg.com/vi/{ID}/hqdefault.jpg?sqp=signed&rs=signed"),
            dict(id=ID, art_url=f"https://i9.ytimg.com/vi_webp/{ID}/sddefault.webp", is_live=True),
            dict(id=ID, art_url=f"https://i.ytimg.com/vi/{ID}/hq720.jpg", is_upcoming=True),
            dict(id="library:playlist:PLabcdefghijk", art_url=f"https://i.ytimg.com/vi/{ID}/hqdefault.jpg", is_folder=True),
            dict(id=ID, art_url="http://testserver/fixtures/thumb.png"), dict(id=ID, art_url="https://ytimg.com.evil.test/a.jpg"),
            dict(id=ID, art_url=""), dict(id=ID, art_url="https://[broken/a.jpg"), "not-a-row"]
    page = dict(api_version=1, kind="catalog", items=rows, cursor="", has_more=False)
    assert thumbnail_apply(page, "default") is page
    for source, name in (("start", "hq1"), ("middle", "hq2"), ("end", "hq3")):
        changed = thumbnail_apply(page, source)
        assert changed["items"][0]["art_url"] == f"https://i.ytimg.com/vi/{ID}/{name}.jpg"
        assert changed["items"][1:] == rows[1:] and changed is not page
    assert rows[0]["art_url"].endswith("rs=signed")  # the cached page is never modified
    assert thumbnail_apply(dict(kind="comments", items=rows), "start")["items"] == rows
    assert thumbnail_apply({"item": dict(rows[0]), "chapters": []}, "end")["item"]["art_url"].endswith("/hq3.jpg")
    assert thumbnail_apply(dict(kind="description", id=ID), "end") == dict(kind="description", id=ID)


@pytest.mark.parametrize("dearrow", [False, True])
def test_thumbnail_source_on_search_jobs_library_and_the_dearrow_fallback(dearrow):
    fetched = []
    def upstream(request):
        fetched.append(str(request.url))
        if request.url.host == "sponsor.ajay.app":
            return httpx.Response(200, json={ID: {"thumbnails": [{"timestamp": 3, "votes": 1, "original": False}]}} if dearrow else {})
        if request.url.host == "dearrow-thumb.ajay.app":
            return httpx.Response(204)
        return httpx.Response(200, content=b"\xff\xd8\xfffixture", headers={"content-type": "image/jpeg"})
    row = dict(id=ID, title="Row", art_url=f"https://i.ytimg.com/vi/{ID}/hqdefault.jpg", is_folder=False)
    library = Library(dict(api_version=1, kind="catalog", items=[row], cursor="", has_more=False))
    with TestClient(plain_app(FakeExtractor(), handler=upstream, library=library)) as client:
        expected = f"https://i.ytimg.com/vi/{ID}/hq2.jpg"
        assert client.get("/v1/search?q=fixture", headers=AUTH).json()["items"][0]["art_url"].endswith("/hqdefault.jpg")
        found = client.get("/v1/search?q=fixture&thumbnails=middle&dearrow_thumbnails=true", headers=AUTH).json()["items"][0]
        assert found["art_url"] == expected
        assert client.get(f"/v1/catalog?parent=channel:{CHANNEL}&thumbnails=middle", headers=AUTH).json()["items"][0]["art_url"] == expected
        job = client.post("/v1/jobs", headers=AUTH, json=dict(action="search", query="fixture", thumbnails="middle")).json()["job"]
        assert finished(client, job)["result"]["items"][0]["art_url"] == expected
        job = client.post("/v1/jobs", headers=AUTH, json=dict(action="details", id=ID, thumbnails="middle")).json()["job"]
        assert finished(client, job)["result"]["item"]["art_url"] == expected
        listed = client.post("/v1/library", headers=AUTH, json=dict(action="home", thumbnails="middle")).json()
        assert listed["items"][0]["art_url"] == expected and row["art_url"].endswith("/hqdefault.jpg")
        assert client.get("/v1/search?q=fixture&thumbnails=best", headers=AUTH).status_code == 422
        if dearrow:
            # A failed generated thumbnail falls back to the selected frame, not to the uploader's image.
            assert found["dearrow_art_url"].startswith("http://testserver/art/dearrow/")
            assert client.get(found["dearrow_art_url"]).content == b"\xff\xd8\xfffixture"
            assert fetched[-1] == expected
        else:
            assert "dearrow_art_url" not in found


def test_description_is_served_from_the_video_extraction_without_a_worker():
    long = "Line one\nLine <i>two</i> 日本語 😀\n" + "a" * 30000
    source = {**info(), "description": long, "upload_date": "20240506", "view_count": 1234567}
    extractor = FakeExtractor(source)
    with TestClient(plain_app(extractor)) as client:
        assert client.post("/v1/library", headers=AUTH, json=dict(action="home")).status_code == 503
        reply = client.post("/v1/library", headers=AUTH, json=dict(action="description", id=ID, hl="fr"))
        assert reply.status_code == 200
        result = reply.json()
        assert {k: v for k, v in result.items() if k != "description"} == dict(
            api_version=1, kind="description", id=ID, title=source["title"], channel="Fixture channel",
            channel_id="UC" + "a" * 22, published="2024-05-06", views="1234567", truncated=True)
        assert result["description"] == long[:len(result["description"])] and len(result["description"]) == 19999
        assert extractor.calls[-1] == ("https://www.youtube.com/watch?v=" + ID, {"hl": "fr"})
        job = client.post("/v1/jobs", headers=AUTH, json=dict(action="library", library=dict(action="description", id=ID))).json()["job"]
        assert finished(client, job)["result"]["description"] == result["description"]
        assert client.post("/v1/jobs", headers=AUTH, json=dict(action="library", library=dict(action="home"))).status_code == 400
        assert client.post("/v1/library", headers=AUTH, json=dict(action="description", id="bad")).status_code == 422
    assert bounded("😀" * 10001) == ("😀" * 10000, True) and bounded("b" * 20000) == ("b" * 20000, False)
    assert bounded(None) == ("", False) and bounded("a\x00b") == ("ab", False)
    bare = describe({"id": ID, "view_count": True, "upload_date": "2024", "channel_id": "bad"}, ID)
    assert bare == dict(api_version=1, kind="description", id=ID, title="", channel="", channel_id="", published="",
                        views="", description="", truncated=False)
    for wrong in ({"id": "12345678901"}, None, []):
        with pytest.raises(ServiceError) as error:
            describe(wrong, ID)
        assert error.value.code == "upstream_format"


def test_worker_description_is_still_preferred_when_the_library_runs():
    library = Library(dict(api_version=1, kind="description", id=ID, description="from worker", truncated=False))
    extractor = FakeExtractor()
    with TestClient(plain_app(extractor, library=library)) as client:
        reply = client.post("/v1/library", headers=AUTH, json=dict(action="description", id=ID, thumbnails="end"))
        assert reply.json()["description"] == "from worker" and not extractor.calls


def test_alternative_sponsorblock_server_is_opt_in_validated_and_cached_separately():
    assert sponsorblock_origin("https://Sponsor.Example.org/") == "https://sponsor.example.org"
    assert sponsorblock_origin("https://sb.example.org:443") == "https://sb.example.org"
    for value in ("http://sb.example.org", "https://user:pw@sb.example.org", "https://sb.example.org:8443",
                  "https://sb.example.org/api", "https://sb.example.org/?x=1", "https://sb.example.org/#f", "https://",
                  "sb.example.org", "https://sb_example.org", "https://[::1]", "https://" + "a" * 300 + ".org", None, 7):
        with pytest.raises(ValueError):
            sponsorblock_origin(value)
    with pytest.raises(ValueError):
        create_app(token=TOKEN, sponsorblock_server="http://sb.example.org")
    hosts = []
    prefix = hashlib.sha256(ID.encode()).hexdigest()[:4]
    def upstream(request):
        hosts.append(request.url.host)
        assert request.url.path == "/api/skipSegments/" + prefix and "authorization" not in request.headers
        start = 2 if request.url.host == "sponsor.ajay.app" else 7
        return httpx.Response(200, json=[{"videoID": ID, "segments": [{"category": "sponsor", "segment": [start, start + 3]}]}])
    with TestClient(plain_app(sponsorblock=True, handler=upstream, sponsorblock_server="https://sb.example.org")) as client:
        assert client.get("/v1/health").json()["sponsorblock_alt"] is True
        primary = client.get("/v1/sponsor/" + ID, headers=AUTH).json()["segments"]
        alternative = client.get("/v1/sponsor/" + ID + "?alt=true", headers=AUTH).json()["segments"]
        assert primary[0]["start"] == 2 and alternative[0]["start"] == 7
        assert client.get("/v1/sponsor/" + ID + "?alt=true", headers=AUTH).json()["segments"] == alternative
        assert client.get("/v1/sponsor/" + ID + "?alt=false", headers=AUTH).json()["segments"] == primary
        assert hosts == ["sponsor.ajay.app", "sb.example.org"]
    hosts.clear()
    with TestClient(plain_app(sponsorblock=True, handler=upstream)) as client:
        assert client.get("/v1/health").json()["sponsorblock_alt"] is False
        assert client.get("/v1/sponsor/" + ID + "?alt=true", headers=AUTH).json()["segments"][0]["start"] == 2
        assert hosts == ["sponsor.ajay.app"]
    with TestClient(plain_app(handler=upstream, sponsorblock_server="https://sb.example.org")) as client:
        assert client.get("/v1/sponsor/" + ID + "?alt=true", headers=AUTH).json() == {"segments": [], "enabled": False}


def test_health_advertises_the_new_request_fields():
    with TestClient(plain_app()) as client:
        features = client.get("/v1/health").json()["features"]
        assert {"locale", "thumbnail_source", "storyboards"} <= set(features)
        assert not {"content_filters", "channel_pages"} & set(features)
    with TestClient(plain_app(library=Library())) as client:
        assert {"content_filters", "channel_pages", "library"} <= set(client.get("/v1/health").json()["features"])
