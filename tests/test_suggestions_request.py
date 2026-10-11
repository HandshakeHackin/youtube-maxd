import pytest
from pydantic import ValidationError
from prosperotube.app import LibraryRequest
from prosperotube.app import create_app
from fastapi.testclient import TestClient
import time


def test_suggestions_request_requires_a_video():
    assert LibraryRequest(action="suggestions", id="abcdefghijk").model_dump()["id"] == "abcdefghijk"
    for identity in ("", "invalid", "https://youtube.com/watch?v=abcdefghijk"):
        with pytest.raises(ValidationError):
            LibraryRequest(action="suggestions", id=identity)


def test_suggestions_job_preserves_profile_and_group_context():
    class Library:
        async def execute(self, request):
            assert request["action"] == "suggestions"
            assert request["id"] == "abcdefghijk" and request["profile"] == "account"
            return {"items": [{"id": "bcdefghijkl", "playback_context": {
                "group": "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee", "index": 3}}], "has_more": False}

        async def close(self):
            pass

    auth = {"Authorization": "Bearer suggestions-test-token"}
    with TestClient(create_app(token="suggestions-test-token", library=Library())) as client:
        created = client.post("/v1/jobs", headers=auth, json={"action": "library", "library": {
            "action": "suggestions", "id": "abcdefghijk", "profile": "account"}})
        assert created.status_code == 200
        for _ in range(50):
            result = client.get("/v1/jobs/" + created.json()["job"], headers=auth).json()
            if result["state"] != "pending":
                break
            time.sleep(.01)
        assert result["state"] == "done"
        assert result["result"]["items"][0]["playback_context"]["index"] == 3
