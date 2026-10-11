"""Third batch: proxy, dislike relay, subscription imports, translated captions, details and one-attempt changes. No network use."""
import asyncio
import json
import sys
import time

import httpx
import pytest
from fastapi.testclient import TestClient

from prosperotube.app import create_app
from prosperotube.description import describe, details
from prosperotube.dislikes import Dislikes, votes
from prosperotube.extractor import Extractor, ProfileExtractors
from prosperotube.library import LibraryService
from prosperotube.models import ResolveRequest, ServiceError
from prosperotube.proxy import MESSAGE, proxy_url
from prosperotube.sessions import Sessions
from prosperotube.subscriptions_import import MAX_CHANNELS, channel_id, parse
from test_companion import AUTH, BASE, TOKEN, ID, FakeExtractor, info, app as plain_app

CHANNEL = "UC" + "c" * 22


def channel(number):
    return "UC" + str(number).rjust(22, "0")


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


# --- Web proxy ---------------------------------------------------------------

def test_proxy_addresses_are_normalized_and_errors_never_repeat_them():
    assert proxy_url("http://proxy.example") == "http://proxy.example:80"
    assert proxy_url("https://Proxy.Example/") == "https://proxy.example:443"
    assert proxy_url("http://10.0.0.2:3128") == "http://10.0.0.2:3128"
    assert proxy_url("http://[::1]:8080") == "http://[::1]:8080"
    assert proxy_url("http://us%40er:p%3Ass%2Fword@proxy.example:8080") == "http://us%40er:p%3Ass%2Fword@proxy.example:8080"
    assert proxy_url("http://user@proxy.example:1") == "http://user:@proxy.example:1"
    secret = "hunter2-secret"
    for value in ["", "proxy.example:3128", f"socks5://user:{secret}@proxy.example:1080", f"socks5h://{secret}@h:1",
                  f"ftp://{secret}@proxy.example", f"http://user:{secret}@proxy.example/path",
                  f"http://user:{secret}@proxy.example?x=1", f"http://user:{secret}@proxy.example#f",
                  f"http://user:{secret}@proxy.example:0", f"http://user:{secret}@proxy.example:70000",
                  f"http://user:{secret}@bad_host", f"http://user:{secret}@", f"http://user:{secret}@proxy.example:80 ",
                  "http://" + "a" * 2050, None, 7]:
        with pytest.raises(ValueError) as error:
            proxy_url(value)
        assert str(error.value) == MESSAGE and secret not in str(error.value)


def test_health_reports_only_whether_a_proxy_is_used_and_every_component_receives_it(monkeypatch):
    address = "http://user:proxy-secret@127.0.0.1:3128"
    with TestClient(plain_app()) as client:
        health = client.get("/v1/health").json()
        assert health["proxy"] is False and "proxy" not in health["features"]
    with TestClient(plain_app(proxy=address)) as client:
        response = client.get("/v1/health")
        assert response.json()["proxy"] is True and "proxy" in response.json()["features"]
        assert "proxy-secret" not in response.text and "3128" not in response.text
    with pytest.raises(ValueError):
        plain_app(proxy="socks5://127.0.0.1:1080")
    # The relay client is built with the proxy when the host supplies no client of its own.
    built = {}
    original = httpx.AsyncClient
    def capture(*args, **kwargs):
        built.update(kwargs)
        return original(*args, **{k: v for k, v in kwargs.items() if k != "proxy"})
    monkeypatch.setattr(httpx, "AsyncClient", capture)
    create_app(extractor=FakeExtractor(), token=TOKEN, proxy=address)
    assert built["proxy"] == address
    # Profile resolvers and the library worker inherit it; the worker gets it in its environment only.
    guest = Extractor(proxy=address)
    profiles = ProfileExtractors(guest, [{"id": "signed", "cookie": "SID=synthetic", "account_index": 0}])
    assert profiles.for_profile("signed").proxy == address


def test_the_resolver_worker_receives_the_proxy_and_the_translation_flag(tmp_path, monkeypatch):
    worker = tmp_path / "worker.py"
    worker.write_text("import json,sys\nrequest=json.load(sys.stdin)\n"
                      "print(json.dumps({'id':'abcdefghijk','echo':request}),flush=True)\n", encoding="utf-8")
    original = asyncio.create_subprocess_exec
    spawned = []
    async def spawn(*args, **kwargs):
        spawned.append(args)
        return await original(sys.executable, str(worker), **kwargs)
    monkeypatch.setattr(asyncio, "create_subprocess_exec", spawn)

    async def run():
        target = "https://www.youtube.com/watch?v=" + ID
        proxied = Extractor(proxy="http://user:proxy-secret@127.0.0.1:3128")
        plain = (await proxied.extract(target))["echo"]
        assert plain["proxy"] == "http://user:proxy-secret@127.0.0.1:3128" and "translated_subs" not in plain
        # The translated listing is a separate cache entry.
        assert (await proxied.extract(target, translated=True))["echo"]["translated_subs"] is True
        assert "translated_subs" not in (await proxied.extract(target))["echo"]
        assert len(spawned) == 2 and not any("proxy-secret" in str(part) for call in spawned for part in call)
        direct = (await Extractor().extract(target))["echo"]
        assert "proxy" not in direct
    asyncio.run(run())


def test_the_library_worker_gets_the_proxy_through_its_environment(tmp_path):
    path = tmp_path / "env-worker.py"
    path.write_text('''import json, os, sys
for line in sys.stdin:
    value = json.loads(line)
    result = {"ready": True} if value["operation"] == "init" else {"proxy": os.environ.get("PROSPEROTUBE_PROXY", ""), "argv": sys.argv}
    print(json.dumps({"id": value["id"], "result": result}), flush=True)
''', encoding="utf-8")

    async def run():
        address = "http://user:proxy-secret@127.0.0.1:3128"
        service = LibraryService(sys.executable, worker=path, timeout=5, proxy=address)
        seen = await service.execute({"action": "profiles"})
        assert seen["proxy"] == address and not any("proxy-secret" in part for part in seen["argv"])
        await service.close()
        direct = LibraryService(sys.executable, worker=path, timeout=5)
        assert (await direct.execute({"action": "profiles"}))["proxy"] == ""
        await direct.close()
    asyncio.run(run())


# --- Return YouTube Dislike --------------------------------------------------

def test_dislike_counts_are_validated_strictly():
    assert votes({"id": ID, "likes": 10, "dislikes": 2, "rating": 4.5, "extra": "ignored"}, ID) == {"likes": 10, "dislikes": 2}
    assert votes({"id": ID, "likes": 0, "dislikes": 0}, ID) == {"likes": 0, "dislikes": 0}
    for payload in [None, [], "x", {"id": "zzzzzzzzzzz", "likes": 1, "dislikes": 1}, {"likes": 1, "dislikes": 1},
                    {"id": ID, "likes": True, "dislikes": 1}, {"id": ID, "likes": 1.0, "dislikes": 1},
                    {"id": ID, "likes": "1", "dislikes": 1}, {"id": ID, "likes": 1, "dislikes": -1},
                    {"id": ID, "likes": 10 ** 12 + 1, "dislikes": 1}, {"id": ID, "likes": 1}]:
        assert votes(payload, ID) is None


def test_the_dislike_relay_is_off_by_default_and_never_calls_out_unless_enabled():
    requests = []
    replies = {"good": httpx.Response(200, json={"id": ID, "likes": 120, "dislikes": 7, "viewCount": 9}),
               "other": httpx.Response(200, json={"id": "zzzzzzzzzzz", "likes": 1, "dislikes": 1}),
               "error": httpx.Response(500, text="private upstream detail"),
               "text": httpx.Response(200, text="not json"),
               "large": httpx.Response(200, content=b"[" + b"1," * 40000 + b"1]")}
    mode = {"value": "good"}
    def handler(request):
        requests.append(request)
        return replies[mode["value"]]
    with TestClient(plain_app(handler=handler)) as client:
        assert client.get(f"/v1/dislikes/{ID}", headers=AUTH).json() == {"api_version": 1, "enabled": False}
        assert "dislikes" not in client.get("/v1/health").json()["features"] and not requests
    with TestClient(plain_app(handler=handler, return_youtube_dislike=True)) as client:
        assert "dislikes" in client.get("/v1/health").json()["features"]
        assert client.get(f"/v1/dislikes/{ID}").status_code == 401
        assert client.get("/v1/dislikes/short", headers=AUTH).status_code == 400 and not requests
        expected = {"api_version": 1, "enabled": True, "id": ID, "available": True, "likes": 120, "dislikes": 7}
        assert client.get(f"/v1/dislikes/{ID}", headers=AUTH).json() == expected
        assert client.get(f"/v1/dislikes/{ID}", headers=AUTH).json() == expected and len(requests) == 1
        assert str(requests[0].url) == "https://returnyoutubedislikeapi.com/votes?videoId=" + ID
        assert "authorization" not in requests[0].headers and "cookie" not in requests[0].headers
        for index, name in enumerate(("other", "error", "text", "large")):
            mode["value"] = name
            other = f"{index}bcdefghijk"
            reply = client.get(f"/v1/dislikes/{other}", headers=AUTH)
            assert reply.json() == {"api_version": 1, "enabled": True, "id": other, "available": False}
            assert "private" not in reply.text


def test_dislike_answers_are_cached_and_bounded():
    async def run():
        calls = []
        async def get(url, **kwargs):
            calls.append(kwargs["params"]["videoId"])
            if kwargs["params"]["videoId"] == "failfailfai":
                raise httpx.ConnectError("private")
            return httpx.Response(200, json={"id": kwargs["params"]["videoId"], "likes": 1, "dislikes": 2})
        now = [0.0]
        relay = Dislikes(type("Http", (), {"get": staticmethod(get)})(), True, clock=lambda: now[0])
        assert (await relay.get(ID))["dislikes"] == 2 and (await relay.get(ID))["likes"] == 1 and calls == [ID]
        assert (await relay.get("failfailfai"))["available"] is False
        now[0] = 301  # an unavailable answer is retried after five minutes, a known one after an hour
        await relay.get("failfailfai"); await relay.get(ID)
        assert calls == [ID, "failfailfai", "failfailfai"]
        now[0] = 3601
        await relay.get(ID)
        assert calls[-1] == ID and len(calls) == 4
        for index in range(200):
            await relay.get(f"v{index:010}")
        assert len(relay.cache) == 128
    asyncio.run(run())


# --- Subscriptions import ----------------------------------------------------

def test_channel_addresses_are_recognized_only_by_identity():
    assert channel_id(CHANNEL) == CHANNEL
    for address in (f"https://www.youtube.com/channel/{CHANNEL}", f"http://youtube.com/channel/{CHANNEL}/",
                    f" https://m.youtube.com/channel/{CHANNEL}?view=0 "):
        assert channel_id(address) == CHANNEL
    for address in ("", None, 7, "https://www.youtube.com/@handle", "https://www.youtube.com/user/name", "https://www.youtube.com/c/name",
                    f"https://example.com/channel/{CHANNEL}", f"https://notyoutube.com/channel/{CHANNEL}",
                    f"ftp://www.youtube.com/channel/{CHANNEL}", f"https://www.youtube.com/channel/{CHANNEL}/videos",
                    "https://www.youtube.com/channel/UCshort", "https://[bad/channel/x", "UC" + "c" * 23, "x" * 3000):
        assert channel_id(address) == ""


def test_the_three_export_formats_become_one_bounded_channel_list():
    newpipe = json.dumps({"app_version": "0.27.2", "app_version_int": 999, "subscriptions": [
        {"service_id": 0, "url": f"https://www.youtube.com/channel/{channel(1)}", "name": " First \x00channel "},
        {"service_id": 0, "url": f"https://www.youtube.com/channel/{channel(1)}", "name": "Duplicate"},
        {"service_id": 1, "url": f"https://www.youtube.com/channel/{channel(2)}", "name": "Another service"},
        {"service_id": 0, "url": "https://www.youtube.com/user/legacy", "name": "No identity"},
        {"service_id": 0, "url": f"https://www.youtube.com/channel/{channel(3)}", "name": 7}, "junk"]})
    assert parse(newpipe) == {"api_version": 1, "kind": "subscriptions_import", "format": "newpipe", "skipped": 3, "truncated": False,
                              "channels": [{"channel_id": channel(1), "title": "First channel"}, {"channel_id": channel(3), "title": ""}]}
    pockettube = json.dumps({"Music": [channel(1), channel(2)], "News \u65e5\u672c": [channel(2), channel(3), "not-a-channel"],
                             "ysc_collection": {"Music": {"order": 0}}, "ysc_settings": "x"})
    assert parse(pockettube.encode()) == {
        "api_version": 1, "kind": "subscriptions_import", "format": "pockettube", "skipped": 1, "truncated": False,
        "channels": [{"channel_id": channel(1), "title": "", "group": "Music"}, {"channel_id": channel(2), "title": "", "group": "Music"},
                     {"channel_id": channel(3), "title": "", "group": "News \u65e5\u672c"}]}
    grayjay = json.dumps([f"https://www.youtube.com/channel/{channel(1)}",
                          json.dumps({"channel": {"url": f"https://www.youtube.com/channel/{channel(2)}", "name": "Serialized"}}),
                          {"channel": {"url": f"https://www.youtube.com/channel/{channel(3)}", "name": "Record"}},
                          {"url": f"https://www.youtube.com/channel/{channel(4)}", "name": "Flat"},
                          "https://rumble.com/c/other", "{broken", 5])
    result = parse(b"\xef\xbb\xbf" + grayjay.encode())
    assert result["format"] == "grayjay" and result["skipped"] == 3
    assert result["channels"] == [{"channel_id": channel(1), "title": ""}, {"channel_id": channel(2), "title": "Serialized"},
                                  {"channel_id": channel(3), "title": "Record"}, {"channel_id": channel(4), "title": "Flat"}]
    many = parse(json.dumps([channel(n) for n in range(MAX_CHANNELS + 5)]))
    assert len(many["channels"]) == MAX_CHANNELS and many["truncated"] is True
    assert parse("[]")["channels"] == [] and parse('{"subscriptions": []}')["format"] == "newpipe"


@pytest.mark.parametrize("raw", ["", "not json", "7", '"text"', "null", "{}", '{"a": 1}', '{"Group": "UC"}', '{"Group": ["x"]}',
                                 '{"ysc_collection": {}}', "[" * 100000, b"\xff\xfe", 7, None])
def test_unrecognized_exports_are_refused(raw):
    with pytest.raises(ServiceError) as error:
        parse(raw)
    assert error.value.code == "invalid_import" and error.value.status == 400


def test_the_import_endpoint_accepts_the_file_body_up_to_one_mebibyte():
    library = Library()
    with TestClient(plain_app(library=library)) as client:
        body = json.dumps({"subscriptions": [{"service_id": 0, "url": f"https://www.youtube.com/channel/{CHANNEL}", "name": "Imported"}]})
        assert client.post("/v1/subscriptions/import", content=body).status_code == 401
        reply = client.post("/v1/subscriptions/import", content=body, headers=AUTH)
        assert reply.status_code == 200 and reply.json()["channels"] == [{"channel_id": CHANNEL, "title": "Imported"}]
        large = json.dumps([f"https://www.youtube.com/channel/{channel(n)}" for n in range(9000)])
        assert 8192 < len(large) < 1024 * 1024
        assert len(client.post("/v1/subscriptions/import", content=large, headers=AUTH).json()["channels"]) == MAX_CHANNELS
        assert client.post("/v1/subscriptions/import", content=b"x" * (1024 * 1024 + 1), headers=AUTH).status_code == 413
        bad = client.post("/v1/subscriptions/import", content="oops", headers=AUTH)
        assert bad.status_code == 400 and bad.json()["error"]["code"] == "invalid_import"
        assert "subscriptions_import" in client.get("/v1/health").json()["features"]
        # Other routes keep their small limit, and nothing reached the account worker.
        assert client.post("/v1/library", content=b" " * 9000, headers=AUTH).status_code == 413
        assert library.calls == []
    with pytest.raises(ServiceError) as error:
        parse(b"x" * (1024 * 1024 + 1))
    assert error.value.code == "import_too_large" and error.value.status == 413


# --- Translated captions -----------------------------------------------------

def caption_info():
    source = info()
    timed = "https://www.youtube.com/api/timedtext?v=" + ID
    source["subtitles"] = {"en": [{"ext": "vtt", "url": timed + "&lang=en", "name": "English"}]}
    source["automatic_captions"] = {
        "de-orig": [{"ext": "vtt", "url": timed + "&lang=de&kind=asr", "name": "German (Original)"}],
        "de": [{"ext": "vtt", "url": timed + "&lang=de&kind=asr", "name": "German"}],
        "fr": [{"ext": "json3", "url": timed + "&fmt=json3&tlang=fr"}, {"ext": "vtt", "url": timed + "&lang=de&kind=asr&tlang=fr", "name": "French"}],
        "ja-en": [{"ext": "vtt", "url": timed + "&lang=en&tlang=ja", "name": "Japanese from English"}],
        "en": [{"ext": "vtt", "url": timed + "&lang=de&kind=asr&tlang=en", "name": "English"}],
        "xx": [{"ext": "vtt", "url": "https://example.com/api/timedtext?tlang=xx"}],
        "bad": "not a list"}
    return source


def test_translated_tracks_are_listed_on_request_and_flagged():
    def listing(**options):
        playback = Sessions().create(caption_info(), ResolveRequest(id=ID, **options), BASE)
        return [(track.language, track.automatic, track.translated) for track in playback.captions]
    assert listing() == [("en", False, False)]
    # Earlier clients: the automatic listing is unchanged apart from the added flag.
    assert listing(automatic_captions=True) == [("en", False, False), ("de", True, False), ("de-orig", True, False),
                                                ("fr", True, True), ("ja-en", True, True)]
    # "More subtitles" alone adds the machine translations only.
    assert listing(translated_captions=True) == [("en", False, False), ("fr", True, True), ("ja-en", True, True)]
    assert listing(translated_captions=True, caption_language="ja-en")[0] == ("ja-en", True, True)
    assert listing(automatic_captions=True, translated_captions=True) == listing(automatic_captions=True)
    source = caption_info()
    source["automatic_captions"] = {f"t{n:03}": [{"ext": "vtt", "url": f"https://www.youtube.com/api/timedtext?v={ID}&tlang=t{n:03}"}]
                                    for n in range(200)}
    playback = Sessions().create(source, ResolveRequest(id=ID, translated_captions=True), BASE)
    assert len(playback.captions) == 129 and sum(track.translated for track in playback.captions) == 128
    assert all(track.url.startswith(BASE + "/media/") and track.url.endswith(".srt") for track in playback.captions)
    with pytest.raises(ValueError):
        ResolveRequest(id=ID, translated_captions="yes")


def test_a_translated_track_is_served_by_the_caption_relay_and_asks_the_resolver_for_translations():
    seen = []
    def handler(request):
        seen.append(str(request.url))
        return httpx.Response(200, text="WEBVTT\n\n00:00.000 --> 00:01.000\nBonjour\n", headers={"content-type": "text/vtt"})
    extractor = FakeExtractor(caption_info())
    with TestClient(plain_app(extractor, handler)) as client:
        playback = client.post("/v1/resolve", json={"id": ID, "translated_captions": True}, headers=AUTH).json()
        assert extractor.calls[-1][1] == {"fresh": False, "translated": True}
        track = next(track for track in playback["captions"] if track["language"] == "fr")
        assert track == {"language": "fr", "name": "French", "url": track["url"], "automatic": True, "translated": True}
        body = client.get(track["url"].replace(BASE, "")).text
        assert "Bonjour" in body and "00:00:00,000 --> 00:00:01,000" in body
        assert seen == ["https://www.youtube.com/api/timedtext?v=" + ID + "&lang=de&kind=asr&tlang=fr"]
        # Without the flag the resolver is asked exactly as before.
        client.post("/v1/resolve", json={"id": ID}, headers=AUTH)
        assert extractor.calls[-1][1] == {"fresh": False}
        job = client.post("/v1/jobs", json={"action": "resolve", "playback": {"id": ID, "translated_captions": True, "hl": "fr"}},
                          headers=AUTH).json()["job"]
        assert finished(client, job)["state"] == "done" and extractor.calls[-1][1] == {"fresh": False, "hl": "fr", "translated": True}
        assert "translated_captions" in client.get("/v1/health").json()["features"]


# --- Details and likes -------------------------------------------------------

def test_details_and_like_counts_come_from_one_extraction_without_the_worker():
    source = info()
    source.update(description="Line one\n" + "x" * 700, upload_date="20240506", view_count=1234567, like_count=4321,
                  channel_follower_count=1200000, live_status="is_upcoming")
    source["formats"].append({"format_id": "u", "url": "https://r1.googlevideo.com/uhd", "vcodec": "vp09", "height": 2160})
    summary = details(source, ID)
    assert summary == {"api_version": 1, "kind": "video_details", "id": ID, "title": source["title"],
                       "channel": {"id": "UC" + "a" * 22, "name": "Fixture channel", "subscribers": "1200000", "art_url": ""},
                       "published": "2024-05-06", "views": "1234567", "like_count": "4321",
                       "description": ("Line one\n" + "x" * 700)[:600], "description_truncated": True,
                       "duration_sec": 20, "badges": ["upcoming", "4k"], "has_related": False}
    assert describe(source, ID)["like_count"] == "4321" and describe(info(), ID)["like_count"] == ""
    bare = details({"id": ID, "like_count": True, "is_live": True, "formats": "bad", "duration": float("inf")}, ID)
    assert bare["like_count"] == "" and bare["badges"] == ["live"] and bare["duration_sec"] == 0
    assert bare["channel"] == {"id": "", "name": "", "subscribers": "", "art_url": ""}
    with pytest.raises(ServiceError) as error:
        details(source, "zzzzzzzzzzz")
    assert error.value.code == "upstream_format"
    extractor = FakeExtractor(source)
    with TestClient(plain_app(extractor)) as client:
        assert client.post("/v1/library", json={"action": "video_details", "id": ID, "hl": "fr"}, headers=AUTH).json() == summary
        assert extractor.calls[-1][1] == {"hl": "fr"}
        job = client.post("/v1/jobs", json={"action": "library", "library": {"action": "video_details", "id": ID}}, headers=AUTH).json()["job"]
        assert finished(client, job)["result"]["like_count"] == "4321"
        assert client.post("/v1/library", json={"action": "video_details", "id": "short"}, headers=AUTH).status_code == 422
        assert client.post("/v1/library", json={"action": "comments", "id": ID}, headers=AUTH).status_code == 503
        features = client.get("/v1/health").json()["features"]
        assert "video_details" in features and "kids" not in features


# --- Request validation ------------------------------------------------------

PLAYLIST = "PLabcdefghijk"
VALID = [
    dict(action="topics", section="kids"), dict(action="topic_open", section="kids", id="x"),
    dict(action="stream_reminder", id=ID, reminder="status"),
    dict(action="stream_reminder", id=ID, reminder="set", session="intent-1", profile="signed"),
    dict(action="playlist_order", playlist=PLAYLIST, profile="signed"), dict(action="playlist_order", playlist="WL", profile="signed"),
    dict(action="playlist_order", playlist=PLAYLIST, playlist_order="published_oldest", session="intent-1", profile="signed"),
    dict(action="subscribe", id=CHANNEL, notifications="all", profile="signed"),
    dict(action="subscribe_notifications", id=CHANNEL, notifications="none", profile="signed"),
    dict(action="search", query="play this", first_playable=True), dict(action="channels", new_content=True, profile="signed"),
    dict(action="video_details", id=ID),
    dict(action="comment_post", id=ID, text="x" * 1000, session="intent-1", profile="signed"),
    dict(action="comment_post", id=ID, text="two\nlines \u65e5\u672c\u8a9e", session="intent-1", profile="signed"),
    dict(action="comment_reply", id=ID, text="reply", cursor="reference-1", session="intent-1", profile="signed"),
]
INVALID = [
    dict(action="topics", section="adults"), dict(action="stream_reminder", id=ID), dict(action="stream_reminder", id=ID, reminder="toggle"),
    dict(action="stream_reminder", id="short", reminder="status"), dict(action="stream_reminder", id=ID, reminder="set"),
    dict(action="home", reminder="status"), dict(action="playlist_order", playlist="short"),
    dict(action="playlist_order", playlist=PLAYLIST, playlist_order="manual", session="s"),
    dict(action="playlist_order", playlist=PLAYLIST, playlist_order="popular"),
    dict(action="playlist_order", playlist=PLAYLIST, id=ID), dict(action="playlist", id=PLAYLIST, playlist_order="popular"),
    dict(action="subscribe", id=CHANNEL, notifications="loud"), dict(action="unsubscribe", id=CHANNEL, notifications="all"),
    dict(action="subscribe_notifications", id=CHANNEL), dict(action="subscribe_notifications", id=ID, notifications="all"),
    dict(action="home", first_playable=True), dict(action="search", query="x", first_playable="true"),
    dict(action="home", new_content=True), dict(action="channels", new_content=1), dict(action="video_details", id="short"),
    dict(action="comment_post", id=ID, text="x" * 1001, session="s"), dict(action="comment_post", id=ID, text="  ", session="s"),
    dict(action="comment_post", id=ID, text="tab\there", session="s"), dict(action="comment_post", id=ID, text="ok"),
    dict(action="comment_post", id="short", text="ok", session="s"), dict(action="comment_reply", id=ID, text="ok", session="s"),
    dict(action="comment_reply", id=ID, text="ok", session="s", cursor="bad cursor"),
    dict(action="chat_send", id=ID, text="x" * 201, cursor="c", session="s"), dict(action="home", text="x" * 201),
]


def test_new_request_fields_validate_before_the_worker_and_arrive_unchanged():
    library = Library()
    with TestClient(plain_app(library=library)) as client:
        for operation in VALID:
            assert client.post("/v1/library", json=operation, headers=AUTH).status_code == 200, operation
            sent = library.calls[-1][0]
            assert all(sent[key] == value for key, value in operation.items()), operation
        sent = len(library.calls)
        for operation in INVALID:
            assert client.post("/v1/library", json=operation, headers=AUTH).status_code == 422, operation
            assert client.post("/v1/jobs", json={"action": "library", "library": operation}, headers=AUTH).status_code == 422
        assert len(library.calls) == sent
        # Earlier clients send none of the new fields and get the earlier defaults.
        client.post("/v1/library", json={"action": "subscribe", "id": CHANNEL, "profile": "signed"}, headers=AUTH)
        defaults = library.calls[-1][0]
        assert (defaults["notifications"], defaults["reminder"], defaults["playlist_order"]) == ("", "", "")
        assert defaults["first_playable"] is False and defaults["new_content"] is False
        features = client.get("/v1/health").json()["features"]
        assert {"kids", "stream_reminder", "playlist_order", "subscribe_notifications", "comment_post", "search_first",
                "channel_new_content", "video_details", "translated_captions"} <= set(features)


# --- One attempt per confirmation across worker restarts ----------------------

def intent_worker(tmp_path):
    path = tmp_path / "intent-worker.py"
    path.write_text('''import json, sys, time
from pathlib import Path
calls = Path(__file__).with_name("intent-calls.jsonl")
for line in sys.stdin:
    value = json.loads(line)
    if value["operation"] == "init":
        print(json.dumps({"id": value["id"], "result": {"ready": True}}), flush=True); continue
    request = value["request"]
    with calls.open("a") as f: f.write(json.dumps(request) + "\\n")
    session, action = request.get("session", ""), request["action"]
    if session == "hang": time.sleep(30)
    if session == "missing":
        code = "reminder_unavailable" if action == "stream_reminder" else "playlist_order_unavailable"
        print(json.dumps({"id": value["id"], "error": {"code": code, "message": "Not offered.", "status": 404}}), flush=True); continue
    wrong = session.startswith("wrong")
    if action == "stream_reminder" and request["reminder"] == "status":
        result = {"api_version": 1, "kind": "stream_reminder", "id": request["id"], "upcoming": True, "available": True, "reminder_set": False}
    elif action == "stream_reminder":
        result = {"api_version": 1, "success": True, "id": request["id"], "reminder_set": (request["reminder"] == "set") != wrong}
    elif action == "playlist_order" and not request.get("playlist_order"):
        result = {"api_version": 1, "kind": "playlist_order", "playlist_id": request["playlist"], "editable": True, "orders": ["popular"], "order": ""}
    elif action == "playlist_order":
        result = {"api_version": 1, "success": True, "playlist_id": request["playlist"], "order": "popular" if wrong else request["playlist_order"]}
    else:
        result = {"api_version": 1, "success": True, "id": "zzzzzzzzzzz" if wrong else request["id"], "session": session,
                  "reply": action == "comment_reply"}
    print(json.dumps({"id": value["id"], "result": {**result, "private": "never-forwarded"}}), flush=True)
''', encoding="utf-8")
    service = LibraryService(sys.executable, worker=path, timeout=2)
    service.profiles = [{"id": "signed", "cookie": "synthetic-only"}, {"id": "other", "cookie": "synthetic-only"}]
    return service, tmp_path / "intent-calls.jsonl"


REMIND = dict(action="stream_reminder", profile="signed", id=ID, reminder="set", session="intent-1")
ORDER = dict(action="playlist_order", profile="signed", playlist=PLAYLIST, playlist_order="added_oldest", session="order-1")
POST = dict(action="comment_post", profile="signed", id=ID, text="First \u65e5\u672c\u8a9e", session="post-1")
REPLY = dict(action="comment_reply", profile="signed", id=ID, cursor="reference-1", text="A reply", session="reply-1")


def test_reminder_order_and_comment_changes_survive_worker_restart_and_are_never_repeated(tmp_path):
    async def run():
        service, calls = intent_worker(tmp_path)
        count = lambda: len(calls.read_text().splitlines())
        confirmed = [(REMIND, dict(api_version=1, success=True, id=ID, reminder_set=True)),
                     (ORDER, dict(api_version=1, success=True, playlist_id=PLAYLIST, order="added_oldest")),
                     (POST, dict(api_version=1, success=True, id=ID, session="post-1", reply=False)),
                     (REPLY, dict(api_version=1, success=True, id=ID, session="reply-1", reply=True))]
        for request, result in confirmed:
            assert await service.execute(dict(request)) == result
            await service._stop()
            assert await service.execute(dict(request)) == result and service.process is None
        assert count() == 4
        # The same confirmation never covers a different change.
        for request, patch, code in ((REMIND, dict(reminder="unset"), "reminder_unconfirmed"), (REMIND, dict(id="zzzzzzzzzzz"), "reminder_unconfirmed"),
                                     (ORDER, dict(playlist_order="popular"), "playlist_unconfirmed"), (POST, dict(text="Other text"), "comment_unconfirmed"),
                                     (REPLY, dict(cursor="reference-2"), "comment_unconfirmed"), (REPLY, dict(action="comment_post"), "comment_unconfirmed")):
            with pytest.raises(ServiceError) as error:
                await service.execute({**request, **patch})
            assert error.value.code == code and error.value.status == 409
        assert (await service.execute({**REMIND, "profile": "other"}))["reminder_set"] is True
        # Reads are not intents: no session, never cached.
        status = dict(action="stream_reminder", profile="guest", id=ID, reminder="status")
        assert (await service.execute(status))["available"] is True and (await service.execute(status))["kind"] == "stream_reminder"
        listing = dict(action="playlist_order", profile="signed", playlist=PLAYLIST, playlist_order="")
        assert (await service.execute(listing))["orders"] == ["popular"]
        assert count() == 8
        # A wrong confirmation, a timeout and a lost worker leave the intent unrepeatable.
        for request, code in ((dict(REMIND, session="wrong-1"), "reminder_unconfirmed"), (dict(ORDER, session="wrong-2"), "playlist_unconfirmed"),
                              (dict(POST, session="wrong-3"), "comment_unconfirmed")):
            with pytest.raises(ServiceError) as error:
                await service.execute(request)
            assert error.value.code == code and error.value.status == 502
            with pytest.raises(ServiceError) as error:
                await service.execute(request)
            assert error.value.code == code and error.value.status == 409
        service.timeout = .2
        with pytest.raises(ServiceError) as error:
            await service.execute(dict(POST, session="hang"))
        assert error.value.code == "library_timeout" and service.process is None
        service.timeout = 2
        with pytest.raises(ServiceError) as error:
            await service.execute(dict(POST, session="hang"))
        assert error.value.code == "comment_unconfirmed" and error.value.status == 409
        assert count() == 12
        # A control the worker did not find sent nothing, so the confirmation stays usable.
        for request, code in ((dict(REMIND, session="missing"), "reminder_unavailable"), (dict(ORDER, session="missing"), "playlist_order_unavailable")):
            for _ in range(2):
                with pytest.raises(ServiceError) as error:
                    await service.execute(request)
                assert error.value.code == code and error.value.status == 404
        assert count() == 16 and ("signed", "missing") not in service.session_intents
        await service.close()
    asyncio.run(run())


def test_invalid_guest_and_unknown_changes_fail_before_starting_the_worker(tmp_path):
    async def run():
        service, calls = intent_worker(tmp_path)
        for request, patch, code in [(REMIND, dict(profile="guest"), "account_required"), (ORDER, dict(profile="missing"), "unknown_profile"),
                                     (POST, dict(session=""), "invalid_request"), (POST, dict(session="bad session"), "invalid_request"),
                                     (POST, dict(text=""), "invalid_request"), (POST, dict(text=7), "invalid_request"),
                                     (REPLY, dict(cursor=""), "invalid_request"), (REMIND, dict(id=None), "invalid_request"),
                                     (ORDER, dict(playlist=""), "invalid_request"), (REMIND, dict(profile="bad/profile"), "invalid_request")]:
            with pytest.raises(ServiceError) as error:
                await service.execute({**request, **patch})
            assert error.value.code == code, (patch, error.value.code)
        assert service.process is None and not calls.exists()
        for index in range(256):
            await service.execute({**POST, "session": f"fill-{index}"})
        with pytest.raises(ServiceError) as error:
            await service.execute({**POST, "session": "one-more"})
        assert error.value.code == "account_busy" and error.value.status == 429
        # Removing nothing but time frees the table; another profile's entries are separate keys.
        for state in service.session_intents.values():
            state["deadline"] = 0
        assert (await service.execute({**POST, "session": "one-more"}))["success"] is True
        await service.close()
    asyncio.run(run())


def test_real_worker_rejects_new_requests_offline_and_refuses_a_malformed_proxy():
    import shutil
    from pathlib import Path
    pinned = Path.home() / ".cache/prosperotube/node/node-v24.10.0-linux-x64/bin/node"
    node = pinned if pinned.is_file() else shutil.which("node")
    bundle = Path(__file__).resolve().parents[1] / "companion/innertube/worker.bundle.mjs"
    if not node or not bundle.is_file():
        pytest.skip("optional library runtime not installed")
    rejected = [(dict(action="stream_reminder", id="bad", reminder="status"), "invalid_identity"),
                (dict(action="stream_reminder", id=ID, reminder="set", session="x"), "account_required"),
                (dict(action="playlist_order", playlist=PLAYLIST), "account_required"),
                (dict(action="comment_post", id=ID, text="x", session="x"), "account_required"),
                (dict(action="comment_reply", id=ID, text="x", session="x", cursor="c"), "account_required"),
                (dict(action="subscribe_notifications", id=CHANNEL, notifications="all"), "account_required"),
                (dict(action="video_details", id="bad"), "invalid_identity"),
                (dict(action="search", query="x", first_playable="yes"), "invalid_search"),
                (dict(action="home", notifications="all"), "invalid_request")]

    async def run():
        # A well-formed proxy changes nothing for requests that fail validation: no connection is attempted.
        for proxy in (None, "http://user:proxy-secret@127.0.0.1:9"):
            service = LibraryService(node, proxy=proxy)
            for request, code in rejected:
                with pytest.raises(ServiceError) as error:
                    await service.execute(request)
                assert error.value.code == code, request
            await service.close()
        # A malformed address stops the worker instead of letting it connect directly.
        service = LibraryService(node, proxy="socks5://127.0.0.1:1080")
        with pytest.raises(ServiceError) as error:
            await service.execute(dict(action="profiles"))
        assert error.value.code == "library_transport" and "socks5" not in error.value.message
        await service.close()
    asyncio.run(run())


def test_fixture_mode_answers_the_host_side_features_and_reports_the_worker_ones_as_unavailable(tmp_path):
    from prosperotube.demo import create_demo_app
    from prosperotube.fixture_library import VIDEOS
    with TestClient(create_demo_app(tmp_path, token=TOKEN)) as client:
        def call(**operation):
            return client.post("/v1/library", headers=AUTH, json=operation)
        health = client.get("/v1/health").json()
        assert {"translated_captions", "video_details", "subscriptions_import", "library"} <= set(health["features"])
        assert not {"kids", "stream_reminder", "comment_post", "dislikes", "proxy"} & set(health["features"]) and health["proxy"] is False
        assert call(action="description", id=VIDEOS[0]).json()["like_count"] == "56"
        kids = call(action="topics", section="kids")
        assert kids.status_code == 400 and kids.json()["error"]["code"] == "invalid_catalog"
        for operation in (dict(action="video_details", id=VIDEOS[0]), dict(action="stream_reminder", id=VIDEOS[0], reminder="status"),
                          dict(action="playlist_order", playlist=PLAYLIST, profile="fixture_one"),
                          dict(action="subscribe_notifications", id=CHANNEL, notifications="all", profile="fixture_one"),
                          dict(action="comment_post", id=VIDEOS[0], text="x", session="s", profile="fixture_one"),
                          dict(action="comment_reply", id=VIDEOS[0], text="x", session="s", cursor="c", profile="fixture_one")):
            reply = call(**operation)
            assert reply.status_code == 400 and reply.json()["error"]["code"] == "unknown_operation", operation
        assert client.get(f"/v1/dislikes/{VIDEOS[0]}", headers=AUTH).json() == {"api_version": 1, "enabled": False}
        imported = client.post("/v1/subscriptions/import", content=json.dumps([CHANNEL]), headers=AUTH).json()
        assert imported["channels"] == [{"channel_id": CHANNEL, "title": ""}]
