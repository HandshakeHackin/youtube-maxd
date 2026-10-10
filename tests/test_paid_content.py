import time
import pytest
from fastapi.testclient import TestClient
from yt_dlp.extractor.youtube import YoutubeIE
from prosperotube.app import create_app
from prosperotube.paid_content import disclosure, player_disclosure
from prosperotube.live_hls import LiveHLSYoutubeIE
from prosperotube.oauth_video import TVOAuthYoutubeIE
from test_companion import AUTH, TOKEN, ID, FakeExtractor, info


def response(value, identity=ID):
    return {"videoDetails": {"videoId": identity}, "paidContentOverlay": {
        "paidContentOverlayRenderer": {"text": value}}}


@pytest.mark.parametrize("value,expected", [
    ({"simpleText": "Contains paid promotion"}, "Contains paid promotion"),
    ({"runs": [{"text": "含有"}, {"text": "付费推广"}]}, "含有付费推广"),
    ({"runs": [None, {"text": 42}, {"text": "Promotion"}]}, "Promotion"),
    ({"runs": "bad"}, ""), ({}, ""), ([], "")])
def test_localized_disclosure(value, expected):
    assert player_disclosure(response(value), ID) == expected
    assert player_disclosure(response(value, "other-video"), ID) == ""


@pytest.mark.parametrize("value", [None, [], {"videoDetails": []},
    {"videoDetails": {"videoId": ID}, "paidContentOverlay": []},
    {"videoDetails": {"videoId": ID}, "paidContentOverlay": {"paidContentOverlayRenderer": [1]}}])
def test_malformed_optional_metadata_is_ignored(value):
    assert player_disclosure(value, ID) == ""


def test_notice_is_bounded_utf8_and_plain_text():
    result = disclosure("\x00\x01 日本語 \n " * 200)
    assert len(result.encode("utf-8")) <= 240 and "\x00" not in result and "\x01" not in result
    assert disclosure({"text": "wrong type"}) == ""
    assert disclosure("\ud800") == "?"


@pytest.mark.parametrize("account", [False, True])
def test_shipping_extractor_preserves_metadata_and_resets_each_video(monkeypatch, account):
    captured = [response({"simpleText": "Promotion — 日本語"})]
    monkeypatch.setattr(YoutubeIE, "_extract_player_responses", lambda *a, **k: (captured, "player-url"))
    def extraction(self, url):
        self._extract_player_responses([], ID, None)
        return {"id": ID, "formats": [{"format_id": "same"}]}
    monkeypatch.setattr(YoutubeIE, "_real_extract", extraction)
    extractor = TVOAuthYoutubeIE("test-token") if account else LiveHLSYoutubeIE()
    result = extractor._real_extract("fixture")
    assert result["paid_content_text"] == "Promotion — 日本語" and result["formats"] == [{"format_id": "same"}]
    captured.clear()
    assert extractor._real_extract("fixture")["paid_content_text"] == ""


def test_both_shipping_resolve_routes_return_disclosure_without_sponsorblock():
    data = info(); data["paid_content_text"] = "Contains paid promotion — 日本語"
    with TestClient(create_app(extractor=FakeExtractor(data), token=TOKEN)) as client:
        result = client.post("/v1/resolve", headers=AUTH, json={"id": ID})
        assert result.status_code == 200 and result.json()["paid_content_text"] == data["paid_content_text"]
        job = client.post("/v1/jobs", headers=AUTH, json={"action": "resolve", "playback": {"id": ID}}).json()["job"]
        for _ in range(100):
            result = client.get("/v1/jobs/"+job, headers=AUTH).json()
            if result["state"] != "pending": break
            time.sleep(.01)
        assert result["state"] == "done" and result["result"]["paid_content_text"] == data["paid_content_text"]
