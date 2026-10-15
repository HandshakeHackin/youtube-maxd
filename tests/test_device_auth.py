"""Offline device authorization, persistence, cancellation and API privacy."""
import asyncio
import json
import os
from pathlib import Path
import stat
import sys

import httpx

from fastapi.testclient import TestClient
import pytest

from prosperotube.app import create_app
from prosperotube.device_auth import DeviceAuth, OAuthProfiles, credentials_valid
from prosperotube.library import LibraryService
from prosperotube.models import ServiceError

TOKENS = {"access_token": "private-access-token", "refresh_token": "private-refresh-token",
          "expiry_date": "2099-01-01T00:00:00.000Z",
          "client": {"client_id": "private-client-id", "client_secret": "private-client-secret"}}
PENDING = {"client": TOKENS["client"], "device_code": "private-device-code", "user_code": "ABCD-EFGH",
           "verification_url": "https://www.google.com/device", "expires_in": 900, "interval": 5}
AUTH = {"Authorization": "Bearer test-device-auth-token"}


def test_auth_jobs_nonblocking_delivery_abandonment_and_cancel(tmp_path):
    async def run():
        service, store, now, calls, profiles = flow(tmp_path)
        app = create_app(token="test-device-auth-token", device_auth=service)
        async with app.router.lifespan_context(app):
            async with httpx.AsyncClient(transport=httpx.ASGITransport(app=app), base_url="http://test") as client:
                async def submit(operation):
                    reply = await client.post("/v1/jobs", headers=AUTH, json={"action": "auth", "auth": operation})
                    assert reply.status_code == 200
                    return reply.json()["job"]
                async def ready(job):
                    await app.state.jobs[job][1]
                    return (await client.get("/v1/jobs/" + job, headers=AUTH)).json()["result"]
                assert (await client.post("/v1/jobs", json={"action": "auth", "auth": {"action": "start"}})).status_code == 401
                assert not calls
                assert (await client.post("/v1/jobs", headers=AUTH, json={"action": "auth"})).status_code == 400
                assert (await client.post("/v1/jobs", headers=AUTH, json={"action": "auth", "auth": {"action": "status"}})).status_code == 400
                assert (await client.post("/v1/jobs", headers=AUTH, json={"action": "auth", "auth": {"action": "start", "credentials": TOKENS}})).status_code == 422

                abandoned = await submit({"action": "start"})
                await app.state.jobs[abandoned][1]  # Completed, but never delivered to caller.
                orphan = service.active["id"]
                await client.delete("/v1/jobs/" + abandoned, headers=AUTH)
                assert (await service.status(orphan))["state"] == "canceled"

                delivered = await submit({"action": "start", "name": "TV"})
                started = await ready(delivered)
                await client.delete("/v1/jobs/" + delivered, headers=AUTH)
                assert service.active["id"] == started["session"]  # Deleting delivered job does not revoke ownership.
                status = await ready(await submit({"action": "status", "session": started["session"]}))
                assert status["state"] == "pending" and len(calls) == 2
                canceled = await ready(await submit({"action": "cancel", "session": started["session"]}))
                assert canceled["state"] == "canceled" and not store.path.exists()
                for secret in ("private-access", "private-refresh", "private-client", "private-device"):
                    assert secret not in json.dumps([started, status, canceled])

                entered, release = asyncio.Event(), asyncio.Event()
                async def slow_start(request):
                    entered.set()
                    try:
                        await release.wait()
                    except asyncio.CancelledError:
                        pass  # An upstream implementation may still return after cancellation.
                    return dict(PENDING)
                service.execute = slow_start
                slow = await submit({"action": "start"})
                await entered.wait()
                assert (await client.get("/v1/jobs/" + slow, headers=AUTH)).json()["state"] == "pending"
                slow_task = app.state.jobs[slow][1]
                await client.delete("/v1/jobs/" + slow, headers=AUTH)
                with pytest.raises(asyncio.CancelledError):
                    await slow_task
                assert service.active is None and not profiles
    asyncio.run(run())


def test_cancel_auth_status_job_discards_late_token_result(tmp_path):
    async def run():
        service, store, now, calls, profiles = flow(tmp_path)
        started = await service.start(); now[0] = 5
        entered = asyncio.Event()
        async def late_poll(request):
            entered.set()
            try:
                await asyncio.Event().wait()
            except asyncio.CancelledError:
                return {"state": "authorized", "credentials": TOKENS}
        service.execute = late_poll
        app = create_app(token="test-device-auth-token", device_auth=service)
        async with app.router.lifespan_context(app):
            async with httpx.AsyncClient(transport=httpx.ASGITransport(app=app), base_url="http://test") as client:
                reply = await client.post("/v1/jobs", headers=AUTH, json={"action": "auth", "auth": {"action": "status", "session": started["session"]}})
                job = reply.json()["job"]; task = app.state.jobs[job][1]
                await entered.wait()
                await client.delete("/v1/jobs/" + job, headers=AUTH)
                await task
                assert not profiles and not store.path.exists()
                assert (await service.status(started["session"]))["state"] == "canceled"
    asyncio.run(run())


def flow(tmp_path, responses=None):
    now, calls, profiles = [0.0], [], []
    responses = list(responses or [{"state": "authorized", "credentials": TOKENS}])
    async def execute(request):
        calls.append(request)
        return dict(PENDING) if request["action"] == "start" else responses.pop(0)
    store = OAuthProfiles(tmp_path)
    service = DeviceAuth(execute, store, profiles.append, lambda: len(profiles) < 8, clock=lambda: now[0])
    return service, store, now, calls, profiles


def test_authenticated_api_signin_privacy_persistence_and_poll_interval(tmp_path):
    service, store, now, calls, profiles = flow(tmp_path)
    app = create_app(token="test-device-auth-token", device_auth=service)
    with TestClient(app) as client:
        assert client.post("/v1/auth/start", json={}).status_code == 401
        assert not calls
        assert "device_auth" in client.get("/v1/health").json()["features"]
        start = client.post("/v1/auth/start", headers=AUTH, json={"name": "Living room"})
        assert start.status_code == 200
        value = start.json(); identity = value["session"]
        assert value["state"] == "pending" and value["user_code"] == "ABCD-EFGH"
        assert value["poll_after"] == 5
        assert client.post("/v1/auth/start", headers=AUTH, json={}).status_code == 409
        for _ in range(3):
            assert client.post("/v1/auth/status", headers=AUTH, json={"session": identity}).json()["state"] == "pending"
        assert len(calls) == 1
        now[0] = 5
        done = client.post("/v1/auth/status", headers=AUTH, json={"session": identity})
        assert done.status_code == 200 and done.json()["state"] == "authorized"
        assert done.json()["profile"]["name"] == "Living room"
        assert profiles == store.profiles and len(profiles) == 1
        assert client.post("/v1/auth/status", headers=AUTH, json={"session": identity}).json() == done.json()
        assert len(calls) == 2
        serialized = json.dumps([value, done.json()])
        for secret in ("private-access", "private-refresh", "private-client", "private-device"):
            assert secret not in serialized
        assert client.post("/v1/auth/status", headers=AUTH, json={"session": "0" * 32}).status_code == 404
        assert client.post("/v1/auth/start", headers=AUTH, json={"name": "x", "credentials": TOKENS}).status_code == 422
    assert OAuthProfiles(tmp_path).profiles == profiles
    assert stat.S_IMODE(store.path.stat().st_mode) == 0o600
    assert stat.S_IMODE(tmp_path.stat().st_mode) == 0o700


def test_slowdown_terminal_expiry_and_no_replay(tmp_path):
    async def run():
        service, store, now, calls, profiles = flow(tmp_path, [{"state": "pending", "slow_down": True}, {"state": "denied"}])
        start = await service.start(); identity = start["session"]
        now[0] = 5
        assert (await service.status(identity))["poll_after"] == 10
        now[0] = 14; await service.status(identity); assert len(calls) == 2
        now[0] = 15; assert (await service.status(identity))["state"] == "denied"
        assert (await service.status(identity))["state"] == "denied" and len(calls) == 3
        assert not profiles and not store.path.exists()
        start = await service.start(); now[0] += 900
        assert (await service.status(start["session"]))["state"] == "expired"
        assert "device_code" not in service.sessions[start["session"]]
        assert len(calls) == 4
    asyncio.run(run())


def test_cancel_inflight_and_queued_poll_discards_late_credentials(tmp_path):
    async def run():
        now = [0]; entered = asyncio.Event(); canceled = asyncio.Event()
        async def execute(request):
            if request["action"] == "start": return dict(PENDING)
            entered.set()
            try: await asyncio.Event().wait()
            except asyncio.CancelledError: canceled.set()
            return {"state": "authorized", "credentials": TOKENS} # Deliberate late upstream success.
        store = OAuthProfiles(tmp_path); profiles = []
        service = DeviceAuth(execute, store, profiles.append, lambda: True, clock=lambda: now[0])
        identity = (await service.start())["session"]; now[0] = 5
        polling = asyncio.create_task(service.status(identity)); await entered.wait()
        assert (await service.status(identity))["state"] == "pending" # No overlapping poll.
        assert service.cancel(identity)["state"] == "canceled"
        assert (await polling)["state"] == "canceled" and canceled.is_set()
        assert not profiles and not store.path.exists()
        assert (await service.status(identity))["state"] == "canceled"
    asyncio.run(run())


@pytest.mark.parametrize("change", [
    {"verification_url": "https://evil.example/device"}, {"verification_url": "https://www.google.com/device?token=secret"},
    {"interval": True}, {"expires_in": 1801}, {"user_code": "secret\n"}, {"device_code": ""},
    {"client": {"client_id": "x", "client_secret": "y", "unexpected": "secret"}},
])
def test_invalid_device_response_is_fixed_public_error(tmp_path, change):
    async def execute(_request): return {**PENDING, **change}
    service = DeviceAuth(execute, OAuthProfiles(tmp_path), lambda p: None, lambda: True)
    with pytest.raises(ServiceError) as error:
        asyncio.run(service.start())
    assert error.value.code == "auth_failed" and "secret" not in error.value.message
    assert service.active is None


def test_profile_file_corruption_and_atomic_failure_preserve_existing_credentials(tmp_path, monkeypatch):
    store = OAuthProfiles(tmp_path); profile = store.append(TOKENS, "One")
    original = store.path.read_bytes()
    def fail_replace(*_args): raise OSError("private-client-secret must not escape")
    with monkeypatch.context() as context:
        context.setattr(os, "replace", fail_replace)
        with pytest.raises(OSError): store.append(TOKENS, "Two")
    assert store.path.read_bytes() == original and store.profiles == [profile]
    assert not list(tmp_path.glob(".oauth-*"))
    store.path.write_text('{"version":2,"profiles":[]}')
    with pytest.raises(ValueError): OAuthProfiles(tmp_path)
    assert store.path.read_text() == '{"version":2,"profiles":[]}'
    store.path.unlink(); store.path.symlink_to(tmp_path / "missing")
    with pytest.raises(ValueError): OAuthProfiles(tmp_path)


def test_failed_poll_never_redeems_code_twice_or_exposes_exception(tmp_path):
    async def run():
        service, store, now, calls, profiles = flow(tmp_path, [{"state": "authorized", "credentials": {**TOKENS, "expiry_date": "invalid"}}])
        identity = (await service.start())["session"]; now[0] = 5
        with pytest.raises(ServiceError) as error: await service.status(identity)
        assert error.value.code == "auth_failed" and "private" not in error.value.message
        assert (await service.status(identity))["state"] == "failed" and len(calls) == 2
        assert not profiles and not store.path.exists()
    asyncio.run(run())


def test_worker_refresh_persistence_and_dynamic_profile_restart(tmp_path):
    async def run():
        store = OAuthProfiles(tmp_path); profile = store.append(TOKENS, "One")
        worker = tmp_path / "worker.py"
        worker.write_text('''import json, sys
for line in sys.stdin:
    value = json.loads(line)
    if value['operation'] == 'init':
        profiles = value['profiles']; result = {'ready': True}; updates = []
    else:
        result = {'profiles': [{'id': p['id']} for p in profiles]}
        credentials = dict(profiles[0]['oauth']); credentials['access_token'] = 'new-private-access'
        updates = [{'profile': profiles[0]['id'], 'credentials': credentials}]
    print(json.dumps({'id': value['id'], 'result': result, 'credential_updates': updates}), flush=True)
''')
        service = LibraryService(sys.executable, worker=worker, oauth_store=store)
        first = await service.execute({"action": "profiles"})
        process = service.process
        assert "new-private-access" not in json.dumps(first)
        assert OAuthProfiles(tmp_path).profiles[0]["oauth"]["access_token"] == "new-private-access"
        service.add_oauth_profile(store.append(TOKENS, "Two"))
        second = await service.execute({"action": "profiles"})
        assert len(second["profiles"]) == 2 and process.returncode is not None
        assert service.process is not process
        await service.close()
    asyncio.run(run())


def test_two_stores_refuse_lost_update_and_do_not_remove_foreign_lock(tmp_path):
    one, two = OAuthProfiles(tmp_path), OAuthProfiles(tmp_path)
    one.append(TOKENS, "One")
    original = one.path.read_bytes()
    with pytest.raises(ServiceError) as error: two.append(TOKENS, "Two")
    assert error.value.code == "auth_storage_changed" and one.path.read_bytes() == original
    one.lock_path.write_text("another-owner")
    with pytest.raises(ServiceError) as error: one.update(one.profiles[0]["id"], TOKENS)
    assert error.value.code == "auth_storage_busy" and one.lock_path.read_text() == "another-owner"


def test_queued_poll_cancel_never_reaches_library_worker(tmp_path):
    async def run():
        # Hold the shipping LibraryService lock to represent an earlier request.
        worker = tmp_path / "never-called.py"; worker.write_text("raise RuntimeError('must not run')")
        library = LibraryService(sys.executable, worker=worker)
        store = OAuthProfiles(tmp_path)
        service = DeviceAuth(lambda request: library.execute(request, operation="device"), store,
                             lambda profile: None, lambda: True, clock=lambda: 10)
        session = {"id": "a" * 32, "state": "pending", "polling": False, "deadline": 100,
                   "next_poll": 0, "interval": 5, "client": TOKENS["client"],
                   "device_code": "private-device", "user_code": "A-B", "verification_url": "https://www.google.com/device"}
        service.sessions[session["id"]] = session; service.active = session
        await library.lock.acquire()
        task = asyncio.create_task(service.status(session["id"]))
        while not library.pending: await asyncio.sleep(0)
        service.cancel(session["id"])
        assert (await task)["state"] == "canceled"
        assert library.pending == 0 and library.process is None and not store.path.exists()
        library.lock.release(); await library.close()
    asyncio.run(run())


@pytest.mark.parametrize("identity", [None, 3, [], True, {}])
def test_corrupt_profile_identity_has_fixed_configuration_error(tmp_path, identity):
    path = tmp_path / "oauth-profiles.json"
    path.write_text(json.dumps({"version": 1, "profiles": [{"id": identity, "name": "One", "cookie": "", "oauth": TOKENS}]}))
    original = path.read_bytes()
    with pytest.raises(ValueError): OAuthProfiles(tmp_path)
    assert path.read_bytes() == original

@pytest.mark.parametrize("label,requested,expected", [
    ("My channel", "YouTube account", "My channel"),
    ("My channel", "Living room", "Living room"),
    ("bad\nname", "YouTube account", "YouTube account"),
    ("x" * 151, "YouTube account", "YouTube account"),
])
def test_authorized_display_name_preserves_custom_label_and_credentials(tmp_path, label, requested, expected):
    async def run():
        service, store, now, calls, profiles = flow(tmp_path, [{"state": "authorized", "credentials": TOKENS, "name": label}])
        started = await service.start(requested)
        now[0] = 5
        done = await service.status(started["session"])
        assert done["state"] == "authorized" and done["profile"]["name"] == expected
        assert store.profiles[0]["oauth"] == TOKENS
    asyncio.run(run())
