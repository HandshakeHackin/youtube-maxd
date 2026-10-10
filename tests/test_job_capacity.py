"""Rapid controller browsing must not consume the active-job limit."""
import asyncio
import time

from fastapi.testclient import TestClient

from prosperotube.app import create_app

AUTH = {"Authorization": "Bearer test-capacity-token"}


class Library:
    def __init__(self, slow=False):
        self.slow = slow
        self.closed = []
        self.serial = 0

    async def close(self):
        pass

    async def execute(self, request):
        if request["action"] == "chat_close":
            self.closed.append(request["cursor"])
            return {"api_version": 1, "success": True}
        if self.slow:
            await asyncio.sleep(3600)
        self.serial += 1
        if request["action"] == "live_chat":
            return {"api_version": 1, "kind": "live_chat", "cursor": f"chat-{self.serial}", "items": []}
        return {"api_version": 1, "kind": "catalog", "items": []}


def submit(client, action="home"):
    return client.post("/v1/jobs", headers=AUTH, json={"action": "library", "library": {"action": action}})


def test_completed_jobs_allow_continued_browsing_and_bounded_retry_retention():
    application = create_app(token="test-capacity-token", library=Library())
    with TestClient(application) as client:
        jobs = []
        for _ in range(140):
            response = submit(client)
            assert response.status_code == 200
            job = response.json()["job"]
            jobs.append(job)
            for _ in range(100):
                result = client.get("/v1/jobs/" + job, headers=AUTH).json()
                if result["state"] != "pending":
                    break
                time.sleep(.001)
            assert result["state"] == "done"
            assert len(application.state.jobs) <= 80
        assert client.get("/v1/jobs/" + jobs[-1], headers=AUTH).json() == result
        assert client.get("/v1/jobs/" + jobs[0], headers=AUTH).status_code == 410
        oldest_retained = jobs[-80]
        assert client.post("/v1/jobs", headers=AUTH, json={"action": "resolve"}).status_code == 400
        assert client.get("/v1/jobs/" + oldest_retained, headers=AUTH).status_code == 200


def test_sixteen_inflight_jobs_still_reject_more_and_cancel_releases_capacity():
    with TestClient(create_app(token="test-capacity-token", library=Library(slow=True))) as client:
        jobs = [submit(client).json()["job"] for _ in range(16)]
        response = submit(client)
        assert response.status_code == 503 and response.json()["error"]["code"] == "job_limit"
        assert client.get("/v1/jobs/" + jobs[0], headers=AUTH).json()["state"] == "pending"
        assert client.delete("/v1/jobs/" + jobs[0], headers=AUTH).status_code == 200
        assert submit(client).status_code == 200


def test_evicting_undelivered_chat_results_closes_the_abandoned_context():
    library = Library()
    application = create_app(token="test-capacity-token", library=library)
    with TestClient(application) as client:
        for _ in range(81):
            response = submit(client, "live_chat")
            assert response.status_code == 200
            task = application.state.jobs[response.json()["job"]][1]
            for _ in range(100):
                if task.done():
                    break
                time.sleep(.001)
            assert task.done()
        for _ in range(100):
            if library.closed:
                break
            time.sleep(.001)
        assert library.closed == ["chat-1"]
        assert len(application.state.jobs) == 80
