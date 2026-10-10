"""Acceptance of offline account UI fixtures through the shipping API/jobs."""
import asyncio
import time

import pytest
from fastapi.testclient import TestClient

from prosperotube.demo import create_demo_app, FixtureExtractor
from prosperotube.fixture_library import FixtureLibrary, CHANNEL, VIDEOS
from prosperotube.models import ServiceError

AUTH = {"Authorization": "Bearer fixture-test-token"}


def test_curated_section_api_keeps_rows_and_profile_scopes(tmp_path):
    app = create_demo_app(tmp_path, token="fixture-test-token")
    with TestClient(app) as client:
        def call(action, **kw):
            return client.post("/v1/library", headers=AUTH, json={"action": action, **kw})
        for section in ("music", "gaming", "news", "sports", "live", "movies"):
            root = call("topics", section=section).json()
            row = root["items"][0]
            assert row["is_folder"] and row["id"].startswith(f"section:{section}:")
            key = row["id"].split(":")[2]
            opened = call("topic_open", section=section, id=key)
            assert opened.status_code == 200 and len(opened.json()["items"]) == 3
            assert call("topic_open", section=section, id=key, profile="fixture_one").status_code == 409
            assert call("continue", cursor=key).status_code == 409
        job = client.post("/v1/jobs", headers=AUTH, json={"action": "library", "library": {
            "action": "topics", "section": "music"}}).json()
        for _ in range(100):
            result = client.get("/v1/jobs/" + job["job"], headers=AUTH).json()
            if result["state"] != "pending":
                break
            time.sleep(.01)
        assert result["state"] == "done" and result["result"]["items"][0]["is_folder"]
        assert call("topics", section="arbitrary").status_code == 422


def test_rapid_six_section_job_navigation_does_not_exhaust_admission(tmp_path):
    with TestClient(create_demo_app(tmp_path, token="fixture-test-token")) as client:
        def job(request):
            response = client.post("/v1/jobs", headers=AUTH, json={"action": "library", "library": request})
            assert response.status_code == 200
            path = "/v1/jobs/" + response.json()["job"]
            for _ in range(100):
                reply = client.get(path, headers=AUTH).json()
                if reply["state"] != "pending":
                    break
                time.sleep(.001)
            assert reply["state"] == "done"
            return reply["result"]
        for section in ("gaming", "news", "sports", "live", "movies", "music"):
            root = job({"action": "topics", "section": section})
            row = job({"action": "topic_open", "section": section, "id": root["items"][0]["id"].split(":")[2]})
            assert len(row["items"]) == 3
            assert job({"action": "topics", "section": section})["items"][0]["is_folder"]


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


def test_fixture_chat_send_through_shipping_api_and_jobs(tmp_path):
    app = create_demo_app(tmp_path, token="fixture-test-token")
    with TestClient(app) as client:
        def call(action, **kwargs):
            return client.post("/v1/library", headers=AUTH, json={"action": action,
                "profile": "fixture_one", "id": VIDEOS[2], **kwargs})
        chat = call("live_chat").json()
        request = {"action": "chat_send", "profile": "fixture_one", "id": VIDEOS[2],
                   "cursor": chat["cursor"], "session": "fixture-send-1", "text": "<b>Literal</b> & café 日本語 😀"}
        first = client.post("/v1/library", headers=AUTH, json=request).json()
        assert first == {"api_version": 1, "success": True, "id": VIDEOS[2],
                         "session": request["session"], "message_id": first["message_id"]}
        assert client.post("/v1/library", headers=AUTH, json=request).json() == first
        job = client.post("/v1/jobs", headers=AUTH, json={"action": "library", "library": request}).json()
        for _ in range(100):
            result = client.get("/v1/jobs/" + job["job"], headers=AUTH).json()
            if result["state"] == "done": break
            time.sleep(0.01)
        assert result["state"] == "done" and result["result"] == first
        rows = call("chat_poll", cursor=chat["cursor"]).json()["items"]
        assert sum(row["id"] == first["message_id"] for row in rows) == 1
        assert rows[-1]["content"] == request["text"]
        assert rows[-1]["author"] == "Offline account one"
        assert call("chat_send", **{k: v for k, v in request.items() if k not in {"action", "profile", "id"}}, profile="guest").status_code == 401
        for changed in ({"text": "Changed"}, {"id": VIDEOS[0]}, {"cursor": "different"}):
            response = client.post("/v1/library", headers=AUTH, json={**request, **changed})
            assert response.status_code == 409 and response.json()["error"]["code"] == "chat_unconfirmed"
        second = call("live_chat", profile="fixture_two").json()
        assert call("chat_poll", profile="fixture_two", cursor=second["cursor"]).json()["items"] == second["items"]
        assert call("chat_send", profile="fixture_two", cursor=chat["cursor"], session="foreign", text="No").status_code == 409
        assert call("chat_close", cursor=chat["cursor"]).json()["success"]
        assert client.post("/v1/library", headers=AUTH, json=request).json() == first
        assert call("chat_send", cursor=chat["cursor"], session="after-close", text="No").status_code == 409


def test_fixture_chat_send_limits_expiry_and_removal():
    async def run():
        clock = [0]
        library = FixtureLibrary(FixtureExtractor(), clock=lambda: clock[0])
        chat = await library.execute({"action": "live_chat", "profile": "fixture_one", "id": VIDEOS[2]})
        request = {"action": "chat_send", "profile": "fixture_one", "id": VIDEOS[2],
                   "cursor": chat["cursor"], "session": "one", "text": "😀" * 200}
        first = await library.execute(request)
        for i, text in enumerate(("", " \u2003", "x\n", "x\x7f", "\ud800", "😀" * 201)):
            with pytest.raises(ServiceError) as error:
                await library.execute({**request, "session": f"invalid-{i}", "text": text})
            assert error.value.code == "invalid_message"
        clock[0] = 3
        rows = (await library.execute({"action": "chat_poll", "profile": "fixture_one", "cursor": chat["cursor"]}))["items"]
        assert rows[-1]["id"] == first["message_id"] and rows[-1]["content"] == request["text"]
        for i in range(1, 256):
            await library.execute({**request, "session": f"bounded-{i}"})
        assert len(library.chat_sends) == 256
        with pytest.raises(ServiceError) as error:
            await library.execute({**request, "session": "overflow"})
        assert error.value.code == "chat_busy"
        # Confirmed repeats survive chat closure until the intent expires.
        clock[0] = 1801
        with pytest.raises(ServiceError) as error:
            await library.execute(request)
        assert error.value.code == "cursor_expired"
        assert len(library.chat_sends) == 256  # later intents still have three seconds
        await library.execute({"action": "profile_remove", "profile": "fixture_one"})
        assert not library.chat_sends and not library.chats
        await library.close()
        assert not library.references and not library.chat_sends
    asyncio.run(run())
