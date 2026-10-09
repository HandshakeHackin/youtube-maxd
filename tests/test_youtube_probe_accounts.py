"""Account probe exercises the shipping API and refuses anonymous substitution."""
import importlib.util
from pathlib import Path
import time
from types import SimpleNamespace

from fastapi.testclient import TestClient
import pytest

from prosperotube.device_auth import OAuthProfiles
from prosperotube.models import ServiceError


def module():
    path = Path(__file__).resolve().parents[1] / "scripts/probe_youtube.py"
    spec = importlib.util.spec_from_file_location("youtube_probe", path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def arguments(**kwargs):
    return SimpleNamespace(js_runtime="node", js_runtime_path="/explicit/node",
                           **{"profile": None, "library_node": None, "account_state": None, **kwargs})


def test_account_probe_routes_shipping_resolver_and_never_substitutes_guest(tmp_path):
    credentials = {"access_token": "probe-test-access", "refresh_token": "probe-test-refresh",
                   "expiry_date": "2099-01-01T00:00:00.000Z",
                   "client": {"client_id": "probe-test-client", "client_secret": "probe-test-secret"}}
    store = OAuthProfiles(tmp_path)
    profile = store.append(credentials, "Synthetic account")
    saved = store.path.read_bytes()
    app = module().probe_app(arguments(profile=profile["id"], library_node="/never-start-node",
                                       account_state=tmp_path))
    resolver = app.state.extractor.for_profile(profile["id"])
    assert resolver.library is app.state.library and resolver.profile == profile["id"]
    assert resolver.js_runtimes == {"node": {"path": "/explicit/node"}}
    calls = []

    async def preflight(request, *, operation="execute"):
        calls.append((operation, request))
        assert request == {"action": "credentials", "profile": profile["id"]}
        return {"access_token": credentials["access_token"], "expiry_date": credentials["expiry_date"]}

    async def guest(*args, **kwargs):
        raise ServiceError("guest_probe_only", "Synthetic Guest failure.", 403)

    app.state.library.execute = preflight
    app.state.extractor.guest.extract = guest
    identity = "abcdefghijk"
    url = "https://www.youtube.com/watch?v=" + identity
    resolver.cache[(url, False, 1, 24)] = (time.monotonic() + 30, {
        "id": identity, "title": "Synthetic account video", "duration": 8, "formats": [{
            "format_id": "account", "url": "https://r1.googlevideo.com/test", "ext": "mp4",
            "protocol": "https", "vcodec": "avc1.64001e", "acodec": "mp4a.40.2", "height": 360}]})
    with TestClient(app) as client:
        token = client.post("/v1/pair", json={"code": app.state.pairing_code}).json()["token"]
        headers = {"Authorization": "Bearer " + token}
        result = client.post("/v1/resolve", headers=headers, json={"id": identity, "profile": profile["id"]})
        assert result.status_code == 200 and result.json()["choices"]
        assert b"probe-test" not in result.content
        assert client.post("/v1/resolve", headers=headers,
                           json={"id": identity, "profile": "guest"}).status_code == 403
        missing = client.post("/v1/resolve", headers=headers,
                              json={"id": identity, "profile": "missing"})
        assert missing.status_code == 404 and missing.json()["error"]["code"] == "unknown_profile"
    assert len(calls) == 1 and app.state.library.process is None
    assert store.path.read_bytes() == saved


def test_probe_rejects_missing_invalid_or_corrupt_account_without_creating_state(tmp_path):
    probe = module()
    absent = tmp_path / "absent"
    for options in ({"profile": "account"}, {"library_node": "node"}, {"account_state": absent},
                    {"profile": "guest", "library_node": "node", "account_state": absent},
                    {"profile": "../account", "library_node": "node", "account_state": absent},
                    {"profile": "missing", "library_node": "node", "account_state": absent}):
        with pytest.raises(ValueError):
            probe.probe_app(arguments(**options))
    assert not absent.exists()
    store = OAuthProfiles(tmp_path / "empty")
    # Existing valid empty and corrupt files must also refuse account substitution.
    store.path.write_text('{"version":1,"profiles":[]}')
    with pytest.raises(ValueError):
        probe.probe_app(arguments(profile="missing", library_node="node", account_state=store.directory))
    corrupt = b"private-credential-corrupt-fixture"
    store.path.write_bytes(corrupt)
    with pytest.raises(ValueError) as error:
        probe.probe_app(arguments(profile="missing", library_node="node", account_state=store.directory))
    assert "private-credential" not in str(error.value) and store.path.read_bytes() == corrupt
    app = probe.probe_app(arguments())
    with TestClient(app):
        assert app.state.library is None
