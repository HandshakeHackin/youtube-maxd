"""Comment rating request retirement and API capability ownership, offline only."""
import asyncio
import json
import sys
from pathlib import Path

from fastapi.testclient import TestClient
import pytest

from prosperotube.demo import create_demo_app
from prosperotube.library import LibraryService
from prosperotube.models import ServiceError


def service(tmp_path, mode, timeout=3):
    config = tmp_path / "profiles.json"
    config.write_text(json.dumps([{"id": "one", "name": "One", "cookie_file": "cookie"}]))
    (tmp_path / "cookie").write_text("SAPISID=synthetic-comment-cookie")
    worker = tmp_path / "worker.py"
    worker.write_text('''import json,sys,time
from pathlib import Path
for line in sys.stdin:
    value=json.loads(line)
    if value['operation']=='init': result={'ready':True}
    else:
        request=value['request']
        if request['action']=='comment_rate':
            with Path(__file__).with_suffix('.calls').open('a') as f: f.write('attempt\\n')
            if MODE=='hang': time.sleep(30)
            result={'success':True,'rating':'dislike' if MODE=='wrong' else request['rating']}
        else: result={'api_version':1,'items':[]}
    print(json.dumps({'id':value['id'],'result':result}),flush=True)
'''.replace("MODE", repr(mode)))
    return LibraryService(sys.executable, profile_config=config, worker=worker, timeout=timeout), worker.with_suffix(".calls")


def test_comment_rating_confirms_once_and_survives_worker_restart(tmp_path):
    async def run():
        library, calls = service(tmp_path, "success")
        request = {"action": "comment_rate", "profile": "one", "cursor": "displayed-reference", "rating": "like"}
        try:
            first = await library.execute(request)
            await library._stop()
            assert await library.execute(request) == first and library.process is None
            with pytest.raises(ServiceError) as error:
                await library.execute({**request, "rating": "dislike"})
            assert error.value.code == "comment_unconfirmed"
            assert calls.read_text().splitlines() == ["attempt"]
        finally:
            await library.close()
    asyncio.run(run())


@pytest.mark.parametrize("failure", ["wrong", "timeout", "cancel"])
def test_uncertain_comment_rating_cannot_repeat_after_worker_loss(tmp_path, failure):
    async def run():
        library, calls = service(tmp_path, "wrong" if failure == "wrong" else "hang", timeout=1)
        request = {"action": "comment_rate", "profile": "one", "cursor": "displayed-reference", "rating": "like"}
        try:
            if failure == "cancel":
                task = asyncio.create_task(library.execute(request))
                async with asyncio.timeout(3):
                    while not calls.exists(): await asyncio.sleep(.01)
                task.cancel()
                with pytest.raises(asyncio.CancelledError): await task
            else:
                with pytest.raises(ServiceError): await library.execute(request)
            await library._stop()
            with pytest.raises(ServiceError) as error: await library.execute(request)
            assert error.value.code == "comment_unconfirmed" and library.process is None
            assert calls.read_text().splitlines() == ["attempt"]
        finally:
            await library.close()
    asyncio.run(run())


def test_fixture_comment_votes_through_direct_and_job_api_are_scoped_and_expire(tmp_path):
    app = create_demo_app(tmp_path, token="comment-rating-test-token")
    auth = {"Authorization": "Bearer comment-rating-test-token"}
    with TestClient(app) as client:
        def call(action, **kwargs):
            return client.post("/v1/library", headers=auth,
                               json={"action": action, "profile": "fixture_one", **kwargs})
        page = call("comments", id="DEMOAVC0001").json()
        item = page["items"][0]
        request = {"action": "comment_rate", "profile": "fixture_one", "cursor": item["rating_cursor"], "rating": "like"}
        assert item["rating_like"] and not item["rating_remove"]
        assert call("comment_rate", profile="guest", cursor=item["rating_cursor"], rating="like").status_code == 401
        assert call("comment_rate", profile="fixture_two", cursor=item["rating_cursor"], rating="like").status_code == 409
        assert call("continue", cursor=item["rating_cursor"]).status_code == 409
        assert client.post("/v1/library", json=request).status_code == 401
        job = client.post("/v1/jobs", headers=auth, json={"action": "library", "library": request}).json()["job"]
        result = None
        for _ in range(100):
            result = client.get("/v1/jobs/" + job, headers=auth).json()
            if result["state"] != "pending": break
        assert result["state"] == "done" and result["result"]["rating"] == "like"
        assert client.post("/v1/library", headers=auth, json=request).json() == result["result"]
        assert call("comment_rate", cursor=item["rating_cursor"], rating="dislike").status_code == 409
        fresh = call("comments", id="DEMOAVC0001").json()["items"][0]
        assert not fresh["rating_like"] and fresh["rating_remove"]
        assert call("comment_rate", cursor=fresh["rating_cursor"], rating="none").json()["success"]
        peer = call("comments", profile="fixture_two", id="DEMOAVC0001").json()["items"][0]
        assert peer["rating_like"] and not peer["rating_remove"]
        replies = call("replies", cursor=item["reply_cursor"]).json()["items"]
        assert replies[0]["rating_cursor"] and call("comment_rate", cursor=replies[0]["rating_cursor"], rating="dislike").json()["success"]
        expired = call("comments", id="DEMOAVC0001").json()["items"][0]["rating_cursor"]
        app.state.library.references[expired]["deadline"] = 0
        assert call("comment_rate", cursor=expired, rating="like").status_code == 409
        assert call("comment_rate", cursor="unknown", rating="arbitrary").status_code == 422
        assert not call("comments", profile="guest", id="DEMOAVC0001").json()["items"][0]["rating_cursor"]
