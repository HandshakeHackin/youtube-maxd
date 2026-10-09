"""Durable account removal, stale request/capability isolation and offline UI API."""
import asyncio
import json
import os
from pathlib import Path
import time

import httpx
import pytest
from fastapi.testclient import TestClient

from prosperotube.app import create_app
from prosperotube.device_auth import DeviceAuth, OAuthProfiles
from prosperotube.extractor import Extractor, ProfileExtractors
from prosperotube.library import LibraryService
from prosperotube.models import ResolveRequest, ServiceError

ID = "abcdefghijk"
URL = "https://www.youtube.com/watch?v=" + ID
AUTH = {"Authorization": "Bearer removal-test-token"}


def credentials(owner):
    return {"access_token": f"private-access-{owner}", "refresh_token": f"private-refresh-{owner}",
            "expiry_date": "2099-01-01T00:00:00.000Z",
            "client": {"client_id": "private-client", "client_secret": "private-secret"}}


def info():
    return {"id": ID, "title": "Private video", "duration": 8, "formats": [{
        "format_id": "p", "url": "https://r1.googlevideo.com/video", "ext": "mp4", "protocol": "https",
        "vcodec": "avc1.64001e", "acodec": "mp4a.40.2", "height": 360, "width": 640}]}


def test_private_store_removal_preserves_peers_and_rejects_foreign_writes(tmp_path):
    store = OAuthProfiles(tmp_path)
    one = store.append(credentials("one"), "One")
    two = store.append(credentials("two"), "Two")
    saved = store.path.read_bytes()
    store.lock_path.write_text("foreign-owner")
    with pytest.raises(ServiceError) as error: store.remove(one["id"])
    assert error.value.code == "auth_storage_busy" and store.path.read_bytes() == saved
    assert store.lock_path.read_text() == "foreign-owner"
    store.lock_path.unlink()  # this test created the foreign-lock fixture
    store.remove(one["id"])
    assert OAuthProfiles(tmp_path).profiles == [two]
    assert b"private-access-one" not in store.path.read_bytes()
    assert b"private-refresh-one" not in store.path.read_bytes()
    with pytest.raises(ServiceError): store.remove(one["id"])
    other = OAuthProfiles(tmp_path)
    other.update(two["id"], credentials("new-two"))
    changed = store.path.read_bytes()
    with pytest.raises(ServiceError) as error: store.remove(two["id"])
    assert error.value.code == "auth_storage_changed" and store.path.read_bytes() == changed


def test_removal_storage_failure_keeps_runtime_and_callback_unmodified(tmp_path):
    async def run():
        store = OAuthProfiles(tmp_path)
        one = store.append(credentials("one"), "One")
        library = LibraryService(oauth_store=store)
        marker = object(); library.process = marker
        callbacks = []; library.on_profile_removed = callbacks.append
        store.lock_path.write_text("test-foreign-lock")
        with pytest.raises(ServiceError) as error:
            await library.execute({"action": "profile_remove", "profile": one["id"]})
        assert error.value.code == "auth_storage_busy"
        assert library.process is marker and library.has_profile(one["id"]) and not callbacks
        assert not library.pending and library.profile_revision == 0
        store.lock_path.unlink()
        library.process = None
        for identity, code in (("guest", "account_required"), ("missing", "unknown_profile")):
            with pytest.raises(ServiceError) as error:
                await library.execute({"action": "profile_remove", "profile": identity})
            assert error.value.code == code
        library.profiles.append({"id": "external", "cookie": "private-cookie", "name": "External"})
        with pytest.raises(ServiceError) as error:
            await library.execute({"action": "profile_remove", "profile": "external"})
        assert error.value.code == "profile_managed" and library.has_profile("external")
        await library.close()
    asyncio.run(run())


def test_real_node_removal_forgets_tokens_jobs_auth_results_and_media(tmp_path):
    node = Path.home() / ".cache/prosperotube/node/node-v24.10.0-linux-x64/bin/node"
    worker = Path(__file__).resolve().parents[1] / "companion/innertube/worker.bundle.mjs"
    if not node.is_file() or not worker.is_file(): pytest.skip("Build pinned Node worker first")
    wrapper = tmp_path / "offline.mjs"
    wrapper.write_text("globalThis.fetch=async()=>{throw new Error('Network forbidden');};\nawait import(" + json.dumps(worker.as_uri()) + ");\n")
    async def run():
        store = OAuthProfiles(tmp_path / "private")
        one = store.append(credentials("one"), "One")
        two = store.append(credentials("two"), "Two")
        library = LibraryService(node, worker=wrapper, oauth_store=store)
        profiles = ProfileExtractors(Extractor(), lambda: library.profiles, library)
        first = profiles.for_profile(one["id"])
        peer = profiles.for_profile(two["id"])
        for resolver in (first, peer, profiles.guest):
            resolver.cache[(URL, False, 1, 24)] = (time.monotonic() + 120, info())
        async def forbidden(request): raise AssertionError("No device/account network")
        auth = DeviceAuth(forbidden, store, library.add_oauth_profile, lambda: True)
        session = "f" * 32
        auth.sessions[session] = {"id": session, "state": "authorized", "profile": {"id": one["id"], "name": "One"}}
        app = create_app(extractor=profiles, library=library, device_auth=auth, token="removal-test-token",
                         http_client=httpx.AsyncClient(transport=httpx.MockTransport(lambda request: httpx.Response(200, stream=httpx.ByteStream(b"peer fixture bytes")))))
        async with app.router.lifespan_context(app):
            async with httpx.AsyncClient(transport=httpx.ASGITransport(app=app), base_url="http://test") as client:
                await library.execute({"action": "credentials", "profile": one["id"]}, operation="playback")
                original = library.process
                jobs = []
                for payload in ({"action": "resolve", "playback": {"id": ID, "profile": one["id"]}},
                                {"action": "auth", "auth": {"action": "status", "session": session}}):
                    reply = await client.post("/v1/jobs", headers=AUTH, json=payload)
                    job = reply.json()["job"]; await app.state.jobs[job][1]; jobs.append(job)
                before = [app.state.sessions.create(info(), ResolveRequest(id=ID, profile=p), "http://test")
                          for p in (one["id"], two["id"], "guest")]
                denied = await client.post("/v1/library", json={"action": "profile_remove", "profile": one["id"]})
                assert denied.status_code == 401 and library.has_profile(one["id"])
                removed = await client.post("/v1/library", headers=AUTH, json={"action": "profile_remove", "profile": one["id"]})
                assert removed.json() == {"api_version": 1, "success": True, "profile": one["id"]}
                assert original.returncode is not None and library.process is None
                assert first.revoked and not first.cache and not first.catalogs and not peer.revoked
                for job in jobs: assert (await client.get("/v1/jobs/" + job, headers=AUTH)).status_code == 410
                assert (await auth.status(session))["state"] == "canceled"
                for playback, status in zip(before, (410, 200, 200)):
                    assert (await client.get(playback.choices[0].url)).status_code == status
                assert (await client.post("/v1/resolve", headers=AUTH, json={"id": ID, "profile": two["id"]})).status_code == 200
                assert (await client.post("/v1/resolve", headers=AUTH, json={"id": ID, "profile": one["id"]})).status_code == 404
                with pytest.raises(ServiceError): await first.extract(URL)
                public = (await client.post("/v1/library", headers=AUTH, json={"action": "profiles"})).json()
                assert {p["id"] for p in public["profiles"]} == {"guest", two["id"]}
                assert "private-" not in json.dumps([public, removed.json()])
                assert OAuthProfiles(store.directory).profiles == [two]
                # Removal submitted as a job must retain its own confirmation.
                reply = await client.post("/v1/jobs", headers=AUTH,
                    json={"action": "library", "library": {"action": "profile_remove", "profile": two["id"]}})
                job = reply.json()["job"]; await app.state.jobs[job][1]
                assert (await client.get("/v1/jobs/" + job, headers=AUTH)).json()["result"]["success"]
                assert not OAuthProfiles(store.directory).profiles
        assert library.process is None
    asyncio.run(run())


def test_queued_revoked_extraction_never_starts_a_worker(monkeypatch):
    async def forbidden(*args, **kwargs): raise AssertionError("revoked worker started")
    monkeypatch.setattr(asyncio, "create_subprocess_exec", forbidden)
    async def run():
        resolver = Extractor(workers=1)
        await resolver.semaphore.acquire()
        task = asyncio.create_task(resolver.extract(URL))
        await asyncio.sleep(0)
        resolver.invalidate(); resolver.semaphore.release()
        with pytest.raises(ServiceError) as error: await task
        assert error.value.code == "unknown_profile" and not resolver.processes
    asyncio.run(run())


def test_committed_removal_still_invalidates_after_directory_sync_failure(tmp_path, monkeypatch):
    async def run():
        store = OAuthProfiles(tmp_path)
        one = store.append(credentials("one"), "One")
        library = LibraryService(oauth_store=store)
        callbacks = []; library.on_profile_removed = callbacks.append
        original = os.fsync
        def fsync(descriptor):
            import stat
            if stat.S_ISDIR(os.fstat(descriptor).st_mode): raise OSError("directory sync fixture failure")
            return original(descriptor)
        monkeypatch.setattr(os, "fsync", fsync)
        with pytest.raises(ServiceError) as error:
            await library.execute({"action": "profile_remove", "profile": one["id"]})
        assert error.value.code == "auth_storage_unconfirmed"
        assert callbacks == [one["id"]] and not library.has_profile(one["id"])
        assert not store.profiles and not OAuthProfiles(tmp_path).profiles
        assert b"private-access-one" not in store.path.read_bytes()
        await library.close()
    asyncio.run(run())


def test_active_media_stream_stops_and_closes_after_account_removal(tmp_path):
    from starlette.requests import Request
    async def run():
        closed = []
        class Stream(httpx.AsyncByteStream):
            async def __aiter__(self):
                yield b"first"
                yield b"second"
                yield b"must-not-deliver"
            async def aclose(self): closed.append(True)
        store = OAuthProfiles(tmp_path)
        one = store.append(credentials("one"), "One")
        library = LibraryService(oauth_store=store)
        app = create_app(library=library, token="removal-test-token",
            http_client=httpx.AsyncClient(transport=httpx.MockTransport(lambda request: httpx.Response(200, stream=Stream()))))
        async with app.router.lifespan_context(app):
            playback = app.state.sessions.create(info(), ResolveRequest(id=ID, profile=one["id"]), "http://test")
            path = playback.choices[0].url.split("/media/", 1)[1].split("/")
            endpoint = next(route.endpoint for route in app.routes if route.path == "/media/{session_token}/{key}")
            request = Request({"type": "http", "method": "GET", "headers": [], "scheme": "http",
                               "server": ("test", 80), "path": "/media/" + "/".join(path), "query_string": b""})
            response = await endpoint(*path, request)
            assert await anext(response.body_iterator) == b"first"
            app.state.sessions.get(path[0]).deadline = time.monotonic() - 1
            assert await anext(response.body_iterator) == b"second"  # open-stream TTL behavior retained
            await library.execute({"action": "profile_remove", "profile": one["id"]})
            with pytest.raises(StopAsyncIteration): await anext(response.body_iterator)
            assert closed == [True]
    asyncio.run(run())


def test_offline_fixture_removal_matches_shipping_account_api(tmp_path):
    from prosperotube.demo import create_demo_app
    app = create_demo_app(tmp_path, token="removal-test-token")
    with TestClient(app) as client:
        owner = "fixture_one"
        page = client.post("/v1/library", headers=AUTH, json={"action": "playlists", "profile": owner}).json()
        assert page["has_more"]
        result = client.post("/v1/library", headers=AUTH, json={"action": "profile_remove", "profile": owner})
        assert result.json()["success"]
        for payload in ({"action": "profile_remove", "profile": owner},
                        {"action": "continue", "profile": owner, "cursor": page["cursor"]}):
            assert client.post("/v1/library", headers=AUTH, json=payload).status_code == 404
        assert client.get("/v1/search", headers=AUTH, params={"q": "test", "profile": owner}).status_code == 404
        assert client.post("/v1/library", headers=AUTH, json={"action": "playlists", "profile": "fixture_two"}).status_code == 200


def test_removed_pending_job_is_retained_until_shutdown_reaps_it(tmp_path):
    from prosperotube.demo import create_demo_app
    async def run():
        app = create_demo_app(tmp_path, token="removal-test-token")
        entered = asyncio.Event(); cleaned = []
        async def extract(*args, **kwargs):
            entered.set()
            try:
                await asyncio.Event().wait()
            finally:
                await asyncio.sleep(0)
                cleaned.append(True)
        app.state.extractor.extract = extract
        async with app.router.lifespan_context(app):
            async with httpx.AsyncClient(transport=httpx.ASGITransport(app=app), base_url="http://test") as client:
                response = await client.post("/v1/jobs", headers=AUTH,
                    json={"action": "resolve", "playback": {"id": "DEMOAVC0001", "profile": "fixture_one"}})
                job = response.json()["job"]
                task = app.state.jobs[job][1]
                await entered.wait()
                await app.state.library.execute({"action": "profile_remove", "profile": "fixture_one"})
                assert job not in app.state.jobs and task in app.state.retired_tasks
        assert task.cancelled() and cleaned == [True] and not app.state.retired_tasks
    asyncio.run(run())
