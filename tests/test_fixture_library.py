"""Acceptance of offline account UI fixtures through the shipping API/jobs."""
import asyncio
import time

import pytest
from fastapi.testclient import TestClient

from prosperotube.demo import create_demo_app, FixtureExtractor
from prosperotube.fixture_library import FixtureLibrary, CHANNEL, VIDEOS
from prosperotube.models import ServiceError

AUTH = {"Authorization": "Bearer fixture-test-token"}


def test_fixture_account_api_and_mutations_are_profile_isolated(tmp_path, monkeypatch):
    # Any accidental real network client creation in fixture mode fails this test.
    import httpx
    original = httpx.AsyncClient
    def offline_client(*args, **kwargs):
        assert isinstance(kwargs.get("transport"), httpx.MockTransport)
        return original(*args, **kwargs)
    monkeypatch.setattr(httpx, "AsyncClient", offline_client)
    app = create_demo_app(tmp_path, token="fixture-test-token")
    with TestClient(app) as client:
        def call(action, **kwargs):
            return client.post("/v1/library", headers=AUTH,
                               json={"action": action, "profile": "fixture_one", **kwargs})
        health = client.get("/v1/health").json()
        assert health["mode"] == "fixtures" and "library" in health["features"]
        assert client.post("/v1/library", json={"action": "profiles"}).status_code == 401
        profiles = call("profiles").json()["profiles"]
        assert [p["id"] for p in profiles] == ["guest", "fixture_one", "fixture_two"]
        assert profiles[0]["account_configured"] is False
        assert call("like", profile="guest", id=VIDEOS[0]).status_code == 401
        assert call("like", profile="unknown", id=VIDEOS[0]).status_code == 404
        assert call("like", id="abcdefghijk").status_code == 404
        assert call("like", id=VIDEOS[0]).json()["success"]
        assert call("playlist", id="LL").json()["items"][0]["id"] == VIDEOS[0]
        assert call("playlist", profile="fixture_two", id="LL").json()["items"] == []
        assert call("dislike", id=VIDEOS[0]).json()["success"]
        assert call("playlist", id="LL").json()["items"] == []
        assert call("unrate", id=VIDEOS[0]).json()["success"]
        for action in ("subscribe", "unsubscribe"):
            assert call(action, id=CHANNEL).json()["success"]
            assert len(call("subscriptions").json()["items"]) == (3 if action == "subscribe" else 0)
        for action in ("watch_later_add", "watch_later_remove"):
            assert call(action, id=VIDEOS[0]).json()["success"]
            assert len(call("watch_later").json()["items"]) == (1 if action.endswith("add") else 0)
        title = 'Private "café" ' + "😀" * 130
        created = call("playlist_create", title=title, id=VIDEOS[0]).json()["playlist_id"]
        assert app.state.library.accounts["fixture_one"]["playlists"][created]["privacy"] == "PRIVATE"
        assert call("playlist", profile="fixture_two", id=created).status_code == 404
        assert call("playlist_add", playlist=created, id=VIDEOS[1]).json()["success"]
        assert [i["id"] for i in call("playlist", id=created).json()["items"]] == list(VIDEOS[:2])
        assert call("playlist_remove", playlist=created, id=VIDEOS[0]).json()["success"]
        assert call("playlist", id=created).json()["items"][0]["id"] == VIDEOS[1]
        assert call("playlist_rename", playlist=created, title="Renamed").json()["success"]
        assert call("playlist_delete", playlist=created).json()["success"]
        assert call("playlist", id=created).status_code == 404
        assert call("playlist_delete", playlist="WL").status_code == 404
        assert call("playlist_create", title="bad\nname").status_code == 400
        assert call("playlist_create", title="😀" * 151).status_code == 422
    assert app.state.library.closed


def test_fixture_paging_reply_sort_and_jobs(tmp_path):
    app = create_demo_app(tmp_path, token="fixture-test-token")
    with TestClient(app) as client:
        def call(action, **kwargs):
            return client.post("/v1/library", headers=AUTH,
                               json={"action": action, "profile": "fixture_one", **kwargs})
        first = call("playlists").json()
        assert len(first["items"]) == 24 and first["has_more"]
        second = call("continue", cursor=first["cursor"]).json()
        assert len(second["items"]) == 1 and not second["has_more"]
        assert second == call("continue", cursor=first["cursor"]).json()
        assert call("continue", profile="fixture_two", cursor=first["cursor"]).status_code == 409
        comments = call("comments", id=VIDEOS[0]).json()
        pinned = comments["items"][0]
        assert pinned["pinned"] and len(pinned["content"]) > 2000
        assert "<b>markup</b>" in comments["items"][1]["content"]
        replies = call("replies", cursor=pinned["reply_cursor"]).json()
        assert len(replies["items"]) == 3 and replies["kind"] == "comments"
        assert replies == call("replies", cursor=pinned["reply_cursor"]).json()
        assert call("replies", profile="fixture_two", cursor=pinned["reply_cursor"]).status_code == 409
        assert call("continue", cursor=pinned["reply_cursor"]).status_code == 409
        assert call("replies", cursor=comments["cursor"]).status_code == 409
        newest = call("comments", id=VIDEOS[0], sort="newest").json()
        assert newest["items"][0]["id"] == "fixture-comment-24"
        job = client.post("/v1/jobs", headers=AUTH, json={"action": "library", "library": {
            "action": "playlist_create", "profile": "fixture_one", "title": "Job-created"}}).json()
        for _ in range(100):
            result = client.get("/v1/jobs/" + job["job"], headers=AUTH).json()
            if result["state"] == "done":
                break
            time.sleep(0.01)
        assert result["state"] == "done" and result["result"]["success"]
        assert call("playlist", id=result["result"]["playlist_id"]).json()["items"] == []


def test_fixture_references_expire_and_are_bounded():
    async def run():
        clock = [0]
        library = FixtureLibrary(FixtureExtractor(), clock=lambda: clock[0])
        first = await library.execute({"action": "playlists", "profile": "fixture_one"})
        clock[0] = 121
        with pytest.raises(ServiceError) as error:
            await library.execute({"action": "continue", "profile": "fixture_one", "cursor": first["cursor"]})
        assert error.value.code == "cursor_expired"
        for _ in range(300):
            await library.execute({"action": "comments", "id": VIDEOS[0]})
        assert len(library.references) == 256
        await library.close()
        assert not library.references
        with pytest.raises(ServiceError) as error:
            await library.execute({"action": "home"})
        assert error.value.code == "library_closed"
    asyncio.run(run())


def test_fixture_watch_progress_contract_and_profile_history(tmp_path):
    app = create_demo_app(tmp_path, token="fixture-test-token")
    with TestClient(app) as client:
        def call(action="watch_progress", **kwargs):
            return client.post("/v1/library", headers=AUTH, json={"action": action,
                "profile": "fixture_one", "id": VIDEOS[0], "session": "test-playback",
                "position_sec": 1, "state": "playing", **kwargs})
        assert call().json() == {"api_version": 1, "success": True, "position_sec": 1}
        assert call(position_sec=7.5).json()["success"]
        history = call("history").json()["items"]
        assert len(history) == 1 and history[0]["resume_pos_sec"] == 7
        assert call("history", profile="fixture_two").json()["items"] == []
        assert call(profile="guest").status_code == 401
        assert call(id=VIDEOS[1]).status_code == 400
        assert call(position_sec=8, state="stopped").json()["success"]
        assert call(position_sec=8, state="stopped").json()["success"]
        assert call(position_sec=9).status_code == 409
        assert call(session="").status_code == 400
        assert call(session="bad/session").status_code == 422
        assert call(position_sec=-1).status_code == 422
        assert call(position_sec=1_000_001).status_code == 422
        assert call(state="paused").status_code == 422
        for i in range(1, 64):
            assert call(session=f"fixture-{i}").status_code == 200
        assert call(session="overflow").status_code == 429
