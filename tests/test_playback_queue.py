"""Authenticated direct/job transport for occurrence-owned playback windows."""
import time
import pytest
from fastapi.testclient import TestClient
from prosperotube.app import create_app

REQUEST = dict(action="playback_queue", profile="account", id="abcdefghijk",
               playlist="PLabcdefghijk", index=42)
AUTH = {"Authorization": "Bearer playback-window-fixture"}

class Library:
    def __init__(self): self.calls = []
    async def execute(self, request):
        self.calls.append(request)
        return dict(api_version=1, playlist=request["playlist"], index=request["index"],
                    video_id=request["id"], window_start=40,
                    video_ids=["abcdefghijk"] * 3, video_indices=[40, 42, 47], radio_rebased=False)
    async def close(self): pass

def test_direct_and_job_windows_keep_profile_sparse_indices_and_repeats():
    library = Library()
    with TestClient(create_app(library=library, token="playback-window-fixture")) as client:
        assert client.post("/v1/library", json=REQUEST).status_code == 401
        assert not library.calls
        direct = client.post("/v1/library", json=REQUEST, headers=AUTH)
        assert direct.status_code == 200
        response = client.post("/v1/jobs", json=dict(action="library", library=REQUEST), headers=AUTH)
        assert response.status_code == 200
        for _ in range(100):
            result = client.get("/v1/jobs/" + response.json()["job"], headers=AUTH).json()
            if result["state"] != "pending": break
            time.sleep(.001)
        assert result["state"] == "done" and result["result"] == direct.json()
        assert direct.json()["video_indices"] == [40, 42, 47]
        assert len(library.calls) == 2
        assert all(all(call[key] == value for key, value in REQUEST.items()) for call in library.calls)

@pytest.mark.parametrize("patch", [dict(index=None), dict(index=True), dict(index=-1),
    dict(index=100001), dict(index=1.5), dict(index="42"), dict(id="bad"),
    dict(playlist="bad"), dict(playlist="PLabcdefghij/"), dict(profile="bad/profile")])
def test_invalid_occurrence_never_reaches_worker(patch):
    library = Library()
    with TestClient(create_app(library=library, token="playback-window-fixture")) as client:
        request = {**REQUEST, **patch}
        assert client.post("/v1/library", json=request, headers=AUTH).status_code == 422
        assert client.post("/v1/jobs", json=dict(action="library", library=request), headers=AUTH).status_code == 422
        assert not library.calls
