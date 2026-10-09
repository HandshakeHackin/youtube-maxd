"""Original offline library transport/API acceptance; no real account use."""
import asyncio
import json
from pathlib import Path
import sys
import time

import pytest
from fastapi.testclient import TestClient

from prosperotube.app import create_app
from prosperotube.library import LibraryService, load_profiles
from prosperotube.models import ServiceError

TOKEN = "library-test-only-token"
AUTH = {"Authorization": "Bearer " + TOKEN}


def fake_worker(tmp_path):
    path = tmp_path / "worker.py"
    path.write_text('''import json, sys, time
for line in sys.stdin:
    value = json.loads(line)
    if value["operation"] == "init":
        result = {"ready": True}
    else:
        request = value["request"]
        if request.get("action") == "hang": time.sleep(30)
        result = {"api_version": 1, "title": request.get("title", ""), "profile": request.get("profile", "guest")}
    print(json.dumps({"id": value["id"], "result": result}), flush=True)
''', encoding="utf-8")
    return path


def history_worker(tmp_path):
    path = tmp_path / "history-worker.py"
    calls = tmp_path / "history-calls.jsonl"
    path.write_text('''import json, sys, time
from pathlib import Path
calls = Path(__file__).with_name("history-calls.jsonl")
for line in sys.stdin:
    value = json.loads(line)
    if value["operation"] == "init": result = {"ready": True}
    else:
        request = value["request"]
        with calls.open("a") as f: f.write(json.dumps(request) + "\\n")
        if request.get("position_sec") == 2 or request.get("action") == "hang": time.sleep(30)
        result = {"api_version": 1, "success": True, "position_sec": request.get("position_sec", 0)}
    print(json.dumps({"id": value["id"], "result": result}), flush=True)
''', encoding="utf-8")
    return path, calls


def test_history_uncertainty_survives_worker_restarts_and_blocks_queued_reports(tmp_path):
    async def run():
        worker, calls = history_worker(tmp_path)
        service = LibraryService(sys.executable, worker=worker, timeout=2)
        service.profiles = [{"id": "one", "cookie": "synthetic-only"}]
        request = {"action": "watch_progress", "profile": "one", "id": "abcdefghijk",
                   "session": "playback-one", "position_sec": 1, "state": "playing"}
        assert (await service.execute(request))["success"]
        service.timeout = 1
        first = asyncio.create_task(service.execute({**request, "position_sec": 2}))
        # Confirm the first request reached the live worker before queueing stop.
        for _ in range(100):
            if len(calls.read_text().splitlines()) == 2: break
            await asyncio.sleep(0.005)
        assert len(calls.read_text().splitlines()) == 2
        assert not first.done() and service.process.returncode is None
        queued = asyncio.create_task(service.execute({**request, "position_sec": 3, "state": "stopped"}))
        results = await asyncio.gather(first, queued, return_exceptions=True)
        assert [r.code for r in results] == ["library_timeout", "history_unconfirmed"]
        assert service.process is None and len(calls.read_text().splitlines()) == 2
        service.timeout = 2
        with pytest.raises(ServiceError) as error: await service.execute(request)
        assert error.value.code == "history_unconfirmed" and service.process is None
        assert (await service.execute({**request, "session": "new-playback"}))["success"]
        assert len(calls.read_text().splitlines()) == 3
        # A loss caused by another operation also invalidates retained tracking.
        service.timeout = 0.1
        with pytest.raises(ServiceError): await service.execute({"action": "hang"})
        service.timeout = 2
        with pytest.raises(ServiceError) as error: await service.execute({**request, "session": "new-playback"})
        assert error.value.code == "history_unconfirmed"
        await service.close()
    asyncio.run(run())


def test_history_guests_and_unknown_profiles_fail_before_starting_worker(tmp_path):
    async def run():
        worker, _ = history_worker(tmp_path)
        service = LibraryService(sys.executable, worker=worker)
        request = {"action": "watch_progress", "id": "abcdefghijk", "session": "playback", "position_sec": 1}
        for profile, code in (("guest", "account_required"), ("unknown", "unknown_profile")):
            with pytest.raises(ServiceError) as error: await service.execute({**request, "profile": profile})
            assert error.value.code == code and service.process is None
        assert not service.watch_states
        await service.close()
    asyncio.run(run())


def test_profile_credentials_are_explicit_and_bounded(tmp_path):
    cookie = tmp_path / "account.cookies"
    cookie.write_text("SAPISID=synthetic-only; SESSION=not-a-real-account\n")
    config = tmp_path / "profiles.json"
    config.write_text(json.dumps([{"id": "one", "name": "One", "cookie_file": cookie.name}]))
    loaded = load_profiles(config)
    assert loaded[0]["cookie"] == "SAPISID=synthetic-only; SESSION=not-a-real-account"
    assert loaded[0]["account_index"] == 0
    assert load_profiles(None) == []
    config.write_text(json.dumps([{"id": "guest", "cookie_file": cookie.name}]))
    with pytest.raises(ValueError):
        load_profiles(config)
    cookie.write_text("header\ninjection")
    config.write_text(json.dumps([{"id": "one", "cookie_file": cookie.name}]))
    with pytest.raises(ValueError):
        load_profiles(config)


def test_library_transport_serializes_concurrent_requests_and_unicode(tmp_path):
    async def run():
        service = LibraryService(sys.executable, worker=fake_worker(tmp_path))
        results = await asyncio.gather(*(service.execute({"action": "home", "title": f"日本語 café {i}"}) for i in range(5)))
        assert [result["title"] for result in results] == [f"日本語 café {i}" for i in range(5)]
        process = service.process
        await service.close()
        assert process.returncode is not None and service.pending == 0
        with pytest.raises(ServiceError) as error:
            await service.execute({"action": "home"})
        assert error.value.code == "library_closed" and service.process is None
    asyncio.run(run())


def test_timeout_kills_owned_worker_and_a_later_request_recovers(tmp_path):
    async def run():
        service = LibraryService(sys.executable, worker=fake_worker(tmp_path), timeout=0.2)
        with pytest.raises(ServiceError) as error:
            await service.execute({"action": "hang"})
        assert error.value.code == "library_timeout" and service.process is None
        service.timeout = 3
        assert (await service.execute({"action": "home"}))["api_version"] == 1
        await service.close()
    asyncio.run(run())


def test_cancelled_operation_reaps_worker(tmp_path):
    async def run():
        service = LibraryService(sys.executable, worker=fake_worker(tmp_path))
        task = asyncio.create_task(service.execute({"action": "hang"}))
        while service.process is None:
            await asyncio.sleep(0.01)
        process = service.process
        task.cancel()
        with pytest.raises(asyncio.CancelledError):
            await task
        assert process.returncode is not None and service.pending == 0
        await service.close()
    asyncio.run(run())


def test_library_requests_are_authenticated_and_closed_with_app():
    class Library:
        closed = False
        calls = []
        async def execute(self, request):
            self.calls.append(request)
            return {"api_version": 1, "items": [], "has_more": False}
        async def close(self):
            self.closed = True
    library = Library()
    with TestClient(create_app(token=TOKEN, library=library)) as client:
        assert client.post("/v1/library", json={"action": "home"}).status_code == 401
        assert not library.calls
        assert client.post("/v1/library", headers=AUTH, json={"action": "home"}).status_code == 200
        assert client.post("/v1/library", headers=AUTH, json={"action": "init"}).status_code == 422
        assert client.post("/v1/library", headers=AUTH, json={"action": "home", "cookie_file": "/private/file"}).status_code == 422
        assert "library" in client.get("/v1/health").json()["features"]
    assert library.closed and len(library.calls) == 1


def test_disabled_library_has_explicit_service_error():
    with TestClient(create_app(token=TOKEN)) as client:
        response = client.post("/v1/library", headers=AUTH, json={"action": "home"})
        assert response.status_code == 503
        assert response.json()["error"]["code"] == "library_unavailable"
        assert "library" not in client.get("/v1/health").json()["features"]


def test_reply_browse_passes_only_bounded_profile_reference():
    class Library:
        calls = []
        async def execute(self, request):
            self.calls.append(request)
            return {"api_version": 1, "kind": "comments", "items": [], "has_more": False}
        async def close(self):
            pass
    library = Library()
    with TestClient(create_app(token=TOKEN, library=library)) as client:
        assert client.post("/v1/library", headers=AUTH, json={"action": "replies", "profile": "one", "cursor": "opaque-thread"}).status_code == 200
        assert library.calls[0]["action"] == "replies" and library.calls[0]["profile"] == "one"
        assert library.calls[0]["cursor"] == "opaque-thread"
        assert client.post("/v1/library", headers=AUTH, json={"action": "replies", "cursor": "x" * 65}).status_code == 422
        assert len(library.calls) == 1


def test_native_library_jobs_poll_without_blocking_or_using_extractor():
    class Library:
        async def execute(self, request):
            await asyncio.sleep(0.05)
            return {"api_version": 1, "items": [{"id": "abcdefghijk", "title": "Fixture"}], "has_more": False}
        async def close(self):
            pass
    with TestClient(create_app(token=TOKEN, library=Library())) as client:
        assert client.post("/v1/jobs", headers=AUTH, json={"action": "library"}).status_code == 400
        created = client.post("/v1/jobs", headers=AUTH, json={"action": "library", "library": {"action": "home"}}).json()
        path = "/v1/jobs/" + created["job"]
        assert client.get(path, headers=AUTH).json()["state"] == "pending"
        result = None
        for _ in range(50):
            result = client.get(path, headers=AUTH).json()
            if result["state"] != "pending":
                break
            time.sleep(0.01)
        assert result["state"] == "done" and result["result"]["items"][0]["id"] == "abcdefghijk"


def test_real_worker_initializes_without_any_network_or_credentials():
    # Optional local runtime: exercising the actual dependency import does not
    # create an InnerTube client; profiles is a local-only operation.
    node = Path.home() / ".cache/prosperotube/node/node-v24.10.0-linux-x64/bin/node"
    dependency = Path(__file__).resolve().parents[1] / "companion/innertube/worker.bundle.mjs"
    if not node.is_file() or not dependency.is_file():
        pytest.skip("optional library runtime not installed")
    async def run():
        service = LibraryService(node)
        result = await service.execute({"action": "profiles"})
        assert result == {"api_version": 1, "profiles": [{"id": "guest", "name": "Guest", "account_configured": False}]}
        await service.close()
    asyncio.run(run())
