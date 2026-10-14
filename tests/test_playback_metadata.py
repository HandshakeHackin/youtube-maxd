"""Loudness, Shorts flag, chapter titles and relayed seek-preview storyboards; no network use."""
import math

import httpx
import pytest
from fastapi.testclient import TestClient
from yt_dlp.extractor.youtube import YoutubeIE

from prosperotube.live_hls import LiveHLSYoutubeIE
from prosperotube.models import ResolveRequest
from prosperotube.oauth_video import TVOAuthYoutubeIE
from prosperotube.player_metadata import codec_range, loudness, player_loudness, player_ranges
from prosperotube.sessions import Sessions
from test_companion import AUTH, BASE, ID, Chunks, FakeExtractor, info, app


def board(identifier, width, height, columns, rows, frames, duration=20, host="i.ytimg.com"):
    sheets = math.ceil(frames / (columns * rows))
    return {"format_id": identifier, "format_note": "storyboard", "ext": "mhtml", "protocol": "mhtml",
            "acodec": "none", "vcodec": "none", "width": width, "height": height, "fps": frames / duration,
            "rows": rows, "columns": columns, "url": f"https://{host}/sb/{ID}/{identifier}/M$M.jpg?sqp=x&sigh=private",
            "fragments": [{"url": f"https://{host}/sb/{ID}/{identifier}/M{n}.jpg?sqp=x&sigh=private-{identifier}",
                           "duration": duration / sheets} for n in range(sheets)]}


def response(value, identity=ID):
    return {"videoDetails": {"videoId": identity}, "playerConfig": {"audioConfig": {"loudnessDb": value}}}


@pytest.mark.parametrize("value,expected", [(-3.25, -3.25), (0, 0.0), (7, 7.0), (60, 60.0), (-60, -60.0),
    (60.5, None), (-61, None), (float("nan"), None), (float("inf"), None), ("4", None), (True, None), (None, None)])
def test_loudness_is_a_bounded_number_from_the_matching_player_response(value, expected):
    assert loudness(value) == expected
    assert player_loudness(response(value), ID) == expected
    assert player_loudness(response(value, "other-video"), ID) is None


@pytest.mark.parametrize("value", [None, [], {"videoDetails": []}, {"videoDetails": {"videoId": ID}},
    {"videoDetails": {"videoId": ID}, "playerConfig": []}, {"videoDetails": {"videoId": ID}, "playerConfig": {"audioConfig": 3}}])
def test_missing_or_malformed_loudness_is_unknown(value):
    assert player_loudness(value, ID) is None


def test_explicit_youtube_transfer_overrides_profile_based_hdr_guess(monkeypatch):
    captured = [response(None) | {"streamingData": {"adaptiveFormats": [
        {"itag": 337, "colorInfo": {"transferCharacteristics": "COLOR_TRANSFER_CHARACTERISTICS_ARIB_STD_B67"}},
        {"itag": 701, "colorInfo": {"transferCharacteristics": "COLOR_TRANSFER_CHARACTERISTICS_SMPTEST2084"}},
        {"itag": 313, "colorInfo": {"transferCharacteristics": "COLOR_TRANSFER_CHARACTERISTICS_BT709"}},
    ]}}]
    assert player_ranges(captured[0], ID) == {"337": "HLG", "701": "HDR10", "313": "SDR"}
    assert player_ranges(captured[0], "other-video") == {}
    assert player_ranges({"videoDetails": {"videoId": ID}, "streamingData": {"adaptiveFormats": [
        {"itag": 337, "colorInfo": {"transferCharacteristics": []}}]}}, ID) == {}
    assert codec_range("vp09.02.51.10.01.09.18.09.00") == "HLG"
    assert codec_range("av01.0.12M.10.0.110.09.16.09.0") == "HDR10"
    assert codec_range("vp9.2") is None
    monkeypatch.setattr(YoutubeIE, "_extract_player_responses", lambda *a, **k: (captured, "player-url"))
    def extraction(self, url):
        self._extract_player_responses([], ID, None)
        return {"id": ID, "formats": [
            {"format_id": "337", "vcodec": "vp9.2", "dynamic_range": "HDR10"},
            {"format_id": "701", "vcodec": "av01.0.12M.10", "dynamic_range": "HDR10"},
            {"format_id": "642", "vcodec": "vp09.02.51.10.01.09.18.09.00", "dynamic_range": "HDR10"},
            {"format_id": "313", "vcodec": "vp9", "dynamic_range": "HDR10"},
            {"format_id": "999", "vcodec": "vp9.2", "protocol": "https", "dynamic_range": "HDR10"},
            {"format_id": "998", "vcodec": "vp9.2", "protocol": "m3u8_native", "dynamic_range": "HDR10"},
            {"format_id": "997", "vcodec": "vp9.2", "protocol": "m3u8_native", "dynamic_range": "HDR10",
             "_tube_hls_video_range": "HLG"}]}
    monkeypatch.setattr(YoutubeIE, "_real_extract", extraction)
    for extractor in (LiveHLSYoutubeIE(), TVOAuthYoutubeIE("test-token")):
        ranges = [fmt["dynamic_range"] for fmt in extractor._real_extract("fixture")["formats"]]
        assert ranges == ["HLG", "HDR10", "HLG", "SDR", "UNKNOWN", "UNKNOWN", "HLG"]


@pytest.mark.parametrize("account", [False, True])
def test_shipping_extractors_attach_loudness_and_reset_it_for_each_video(monkeypatch, account):
    captured = [response(None), response(-4.5)]
    monkeypatch.setattr(YoutubeIE, "_extract_player_responses", lambda *a, **k: (captured, "player-url"))
    def extraction(self, url):
        self._extract_player_responses([], ID, None)
        return {"id": ID, "formats": [{"format_id": "same"}]}
    monkeypatch.setattr(YoutubeIE, "_real_extract", extraction)
    extractor = TVOAuthYoutubeIE("test-token") if account else LiveHLSYoutubeIE()
    result = extractor._real_extract("fixture")
    assert result["loudness_db"] == -4.5 and result["paid_content_text"] == "" and result["formats"] == [{"format_id": "same"}]
    captured.clear()
    assert "loudness_db" not in extractor._real_extract("fixture")


def test_resolve_adds_optional_metadata_without_changing_stream_choices():
    plain = Sessions().create(info(), ResolveRequest(id=ID), BASE)
    assert plain.loudness_db is None and plain.is_short is False and plain.storyboards == [] and plain.chapters == []
    source = info()
    source.update(loudness_db=-2.5, media_type="short", chapters=[
        {"title": "Intro — 日本語", "start_time": 0, "end_time": 7.5}, {"title": "x" * 200, "start_time": 7.5, "end_time": 20},
        {"start_time": 20, "end_time": 20}])
    source["formats"] += [board("sb0", 320, 180, 3, 3, 20), board("sb2", 48, 27, 10, 10, 100), board("sb1", 160, 90, 5, 5, 20)]
    sessions = Sessions()
    playback = sessions.create(source, ResolveRequest(id=ID), BASE)
    assert [choice.model_dump(exclude={"url"}) for choice in playback.choices] == \
        [choice.model_dump(exclude={"url"}) for choice in plain.choices]
    assert playback.loudness_db == -2.5 and playback.is_short is True
    assert playback.chapters == [{"title": "Intro — 日本語", "start": 0, "end": 7.5},
                                 {"title": "x" * 96, "start": 7.5, "end": 20}, {"title": "", "start": 20, "end": 20}]
    session = next(iter(sessions.sessions.values()))
    prefix = f"{BASE}/media/{session.token}"
    assert [level.model_dump() for level in playback.storyboards] == [
        dict(width=48, height=27, columns=10, rows=10, count=100, sheets=1, interval_ms=200, url=prefix + "/sb0-$M.jpg"),
        dict(width=160, height=90, columns=5, rows=5, count=20, sheets=1, interval_ms=1000, url=prefix + "/sb1-$M.jpg"),
        dict(width=320, height=180, columns=3, rows=3, count=20, sheets=3, interval_ms=1000, url=prefix + "/sb2-$M.jpg")]
    assert "private" not in playback.model_dump_json() and "ytimg" not in playback.model_dump_json()
    assert session.tracks["sb2-2.jpg"].url.endswith("/sb0/M2.jpg?sqp=x&sigh=private-sb0")
    assert session.tracks["sb2-2.jpg"].headers == {"User-Agent": "fixture"} and "sb2-3.jpg" not in session.tracks
    for bad in (float("nan"), 99, "3", True):
        source["loudness_db"] = bad
        assert Sessions().create(source, ResolveRequest(id=ID), BASE).loudness_db is None


def test_unusable_storyboards_are_skipped_and_levels_are_bounded():
    def levels(*boards, **extra):
        source = info(); source.update(extra); source["formats"] += list(boards)
        sessions = Sessions()
        playback = sessions.create(source, ResolveRequest(id=ID), BASE)
        return playback.storyboards, next(iter(sessions.sessions.values())).tracks
    assert levels(board("sb0", 160, 90, 5, 5, 20), is_live=True)[0] == []
    assert levels(board("sb0", 160, 90, 5, 5, 20), duration=0)[0] == []
    assert levels(board("sb0", 160, 90, 5, 5, 20, host="evil.test"))[0] == []
    assert levels(board("sb0", 160, 90, 5, 5, 20) | {"fragments": "bad"})[0] == []
    assert levels(board("sb0", 160, 90, 5, 5, 20) | {"fragments": [{"duration": 1}]})[0] == []
    assert levels(board("sb0", 160, 90, 5, 5, 40) | {"fragments": [{"url": f"https://i.ytimg.com/sb/{ID}/x.jpg"}]})[0] == []
    assert levels(board("sb0", 0, 90, 5, 5, 20))[0] == [] and levels(board("sb0", 160, 90, 5, 5, 20) | {"columns": 0})[0] == []
    assert levels(board("sb0", 160, 90, 1, 1, 513, duration=513))[0] == []
    large, tracks = levels(board("sb0", 160, 90, 1, 1, 512, duration=512), duration=512)
    assert large[0].sheets == 512 and "sb0-511.jpg" in tracks
    many, tracks = levels(*(board(f"sb{n}", 16 * (n + 1), 9 * (n + 1), 5, 5, 20) for n in range(6)))
    assert [level.width for level in many] == [16, 32, 48, 64] and not any(key.startswith("sb4") for key in tracks)
    mixed, _ = levels(board("sb0", 160, 90, 5, 5, 20, host="evil.test"), board("sb1", 80, 45, 5, 5, 20))
    assert [level.url.rsplit("/", 1)[1] for level in mixed] == ["sb0-$M.jpg"] and mixed[0].width == 80


def test_storyboard_sheets_are_relayed_without_exposing_the_signed_address():
    source = info(); source["formats"].append(board("sb0", 160, 90, 5, 5, 60))
    seen = []
    def upstream(request):
        seen.append(request)
        if "/sb/" not in request.url.path:
            return httpx.Response(404)
        return httpx.Response(200, stream=Chunks(b"\xff\xd8\xffsheet" + request.url.path[-6:].encode()), headers={"content-type": "image/jpeg"})
    with TestClient(app(FakeExtractor(source), handler=upstream)) as client:
        playback = client.post("/v1/resolve", headers=AUTH, json={"id": ID}).json()
        level = playback["storyboards"][0]
        assert level["sheets"] == 3 and level["count"] == 60 and level["interval_ms"] == 333
        for sheet in range(3):
            reply = client.get(level["url"].replace("$M", str(sheet)))
            assert reply.status_code == 200 and reply.headers["content-type"] == "image/jpeg"
            assert reply.content == b"\xff\xd8\xffsheet" + f"M{sheet}.jpg".encode()
        assert seen[-1].url.params["sigh"] == "private-sb0" and "cookie" not in seen[-1].headers
        assert client.get(level["url"].replace("$M", "3")).status_code == 404
        assert client.get(level["url"]).status_code == 404
        job = client.post("/v1/jobs", headers=AUTH, json={"action": "resolve", "playback": {"id": ID}}).json()["job"]
        for _ in range(200):
            result = client.get("/v1/jobs/" + job, headers=AUTH).json()
            if result["state"] != "pending": break
        assert result["result"]["storyboards"][0]["sheets"] == 3 and result["result"]["loudness_db"] is None
