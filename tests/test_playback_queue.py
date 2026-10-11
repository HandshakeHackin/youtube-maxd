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
                    video_id=request["id"] or "abcdefghijk", window_start=40,
                    video_ids=["abcdefghijk"] * 3, video_indices=[40, 42, 47], playlist_size=48, radio_rebased=False)
    async def close(self): pass

@pytest.mark.parametrize("context", [{},{"playlist":"RDabcdefghijk","index":9}])
def test_next_video_direct_and_job_transport_preserves_profile_and_terminal_result(context):
    class NextLibrary(Library):
        async def execute(self, request):
            self.calls.append(request)
            return dict(api_version=1,video_id=request["id"],next_video_id=None)
    library=NextLibrary()
    request=dict(action="playback_next",profile="account",id="abcdefghijk",**context)
    with TestClient(create_app(library=library,token="playback-window-fixture")) as client:
        direct=client.post("/v1/library",json=request,headers=AUTH)
        assert direct.status_code==200 and direct.json()["next_video_id"] is None
        job=client.post("/v1/jobs",json=dict(action="library",library=request),headers=AUTH).json()["job"]
        for _ in range(100):
            result=client.get("/v1/jobs/"+job,headers=AUTH).json()
            if result["state"]!="pending": break
            time.sleep(.001)
        assert result["state"]=="done" and result["result"]==direct.json()
        for patch in [dict(id="bad"),dict(playlist="PLabcdefghijk"),dict(index=0),dict(direction=1)]:
            bare=dict(action="playback_next",profile="account",id="abcdefghijk")
            assert client.post("/v1/library",json={**bare,**patch},headers=AUTH).status_code==422
        assert len(library.calls)==2 and all(call["profile"]=="account" for call in library.calls)
        assert all(all(call[key]==value for key,value in context.items()) for call in library.calls)

@pytest.mark.parametrize("action,direction", [("playback_queue",0),("playback_at",0),("playback_at",-1),("playback_at",1)])
def test_direct_and_job_windows_keep_profile_sparse_indices_and_repeats(action,direction):
    request = {**REQUEST, "action": action, "direction":direction,"id": "" if action == "playback_at" else REQUEST["id"]}
    library = Library()
    with TestClient(create_app(library=library, token="playback-window-fixture")) as client:
        assert client.post("/v1/library", json=request).status_code == 401
        assert not library.calls
        direct = client.post("/v1/library", json=request, headers=AUTH)
        assert direct.status_code == 200
        response = client.post("/v1/jobs", json=dict(action="library", library=request), headers=AUTH)
        assert response.status_code == 200
        for _ in range(100):
            result = client.get("/v1/jobs/" + response.json()["job"], headers=AUTH).json()
            if result["state"] != "pending": break
            time.sleep(.001)
        assert result["state"] == "done" and result["result"] == direct.json()
        assert direct.json()["video_indices"] == [40, 42, 47]
        assert direct.json()["playlist_size"] == 48
        assert len(library.calls) == 2
        assert all(all(call[key] == value for key, value in request.items()) for call in library.calls)

@pytest.mark.parametrize("patch", [dict(index=None), dict(index=True), dict(index=-1),
    dict(index=100001), dict(index=1.5), dict(index="42"), dict(id="bad"),
    dict(playlist="bad"), dict(playlist="PLabcdefghij/"), dict(profile="bad/profile"),
    dict(direction=True),dict(direction=None),dict(direction=2),dict(direction="1")])
@pytest.mark.parametrize("action", ["playback_queue", "playback_at"])
def test_invalid_occurrence_never_reaches_worker(patch, action):
    library = Library()
    with TestClient(create_app(library=library, token="playback-window-fixture")) as client:
        request = {**REQUEST, "action": action, "id": "" if action == "playback_at" else REQUEST["id"], **patch}
        assert client.post("/v1/library", json=request, headers=AUTH).status_code == 422
        assert client.post("/v1/jobs", json=dict(action="library", library=request), headers=AUTH).status_code == 422
        assert not library.calls
