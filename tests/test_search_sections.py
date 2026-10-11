"""Search options, sections, description and feedback transport; no network or account use."""
import asyncio
from pathlib import Path
import sys
import time

import pytest
from fastapi.testclient import TestClient

from prosperotube.app import create_app
from prosperotube.demo import create_demo_app
from prosperotube.fixture_library import VIDEOS
from prosperotube.library import LibraryService
from prosperotube.models import ServiceError

TOKEN = 'search-sections-fixture'
AUTH = {'Authorization': 'Bearer ' + TOKEN}
SEARCH = dict(action='search', profile='signed', query='cats', upload_date='week', duration='long',
              type='channel', features=['live', '4k'], order='view_count')
FEEDBACK = dict(action='feedback', profile='signed', id='abcdefghijk',
                cursor='12345678-1234-1234-1234-123456789abc', feedback='not_interested')


class Library:
    def __init__(self): self.calls = []
    async def execute(self, request):
        self.calls.append(request)
        return dict(api_version=1, kind='catalog', items=[], cursor='', has_more=False)
    async def close(self): pass


@pytest.mark.parametrize('operation', [SEARCH, dict(action='search', query='plain'),
    dict(action='search_suggestions', query='ca'), dict(action='search_suggestions'),
    dict(action='description', id='abcdefghijk'), dict(action='channels', profile='signed'),
    dict(action='notifications', profile='signed'), FEEDBACK,
    dict(action='topics', section='trending'), dict(action='topics', section='shorts'),
    dict(action='topics', section='my_videos', profile='signed')])
def test_direct_and_job_requests_reach_the_library_unchanged(operation):
    library = Library()
    with TestClient(create_app(library=library, token=TOKEN)) as client:
        assert client.post('/v1/library', json=operation).status_code == 401
        assert not library.calls
        direct = client.post('/v1/library', json=operation, headers=AUTH)
        assert direct.status_code == 200
        job = client.post('/v1/jobs', json=dict(action='library', library=operation), headers=AUTH)
        assert job.status_code == 200
        for _ in range(100):
            result = client.get('/v1/jobs/' + job.json()['job'], headers=AUTH).json()
            if result['state'] != 'pending': break
            time.sleep(.001)
        assert result['state'] == 'done' and result['result'] == direct.json()
        assert len(library.calls) == 2
        assert all(all(call[key] == value for key, value in operation.items()) for call in library.calls)
        assert library.calls[0]['features'] == operation.get('features', [])


@pytest.mark.parametrize('request_body', [
    SEARCH | dict(upload_date='hour'), SEARCH | dict(order='rating'), SEARCH | dict(order='upload_date'),
    SEARCH | dict(duration='tiny'), SEARCH | dict(type='artist'), SEARCH | dict(features=['live', 'live']),
    SEARCH | dict(features=['bogus']), SEARCH | dict(features='live'), SEARCH | dict(features=['hd'] * 12),
    SEARCH | dict(query=''), SEARCH | dict(query='   '), SEARCH | dict(query='x' * 201), SEARCH | dict(query='a\nb'),
    SEARCH | dict(sort_by='rating'), dict(action='home', query='cats'), dict(action='home', upload_date='week'),
    dict(action='search_suggestions', query='ca', type='video'), dict(action='search_suggestions', query='a\tb'),
    dict(action='description'), dict(action='description', id='bad'), dict(action='topics', section='popular'),
    FEEDBACK | dict(feedback=''), FEEDBACK | dict(feedback='report'), FEEDBACK | dict(id='bad'),
    FEEDBACK | dict(cursor=''), FEEDBACK | dict(cursor='bad/reference'), FEEDBACK | dict(profile='bad/profile'),
    dict(action='home', feedback='not_interested')])
def test_invalid_requests_do_not_reach_the_library(request_body):
    library = Library()
    with TestClient(create_app(library=library, token=TOKEN)) as client:
        for path, body in [('/v1/library', request_body), ('/v1/jobs', dict(action='library', library=request_body))]:
            assert client.post(path, json=body, headers=AUTH).status_code == 422
        assert not library.calls


def feedback_worker(tmp_path):
    path = tmp_path / 'feedback-worker.py'
    path.write_text('''import json, sys, time
from pathlib import Path
calls = Path(__file__).with_name("feedback-calls.jsonl")
for line in sys.stdin:
    value = json.loads(line)
    if value["operation"] == "init": result = {"ready": True}
    else:
        request = value["request"]
        with calls.open("a") as f: f.write(json.dumps(request) + "\\n")
        if request.get("cursor") == "hang": time.sleep(30)
        result = {"api_version": 1, "success": True, "video_id": "wrongvideo1" if request.get("cursor") == "wrong" else request.get("id"),
                  "feedback": request.get("feedback"), "private": "never-forwarded"}
    print(json.dumps({"id": value["id"], "result": result}), flush=True)
''', encoding='utf-8')
    return path, tmp_path / 'feedback-calls.jsonl'


def test_feedback_confirmation_and_uncertainty_survive_worker_restart(tmp_path):
    async def run():
        worker, calls = feedback_worker(tmp_path)
        service = LibraryService(sys.executable, worker=worker, timeout=2)
        service.profiles = [{'id': 'signed', 'cookie': 'synthetic-only'}, {'id': 'other', 'cookie': 'synthetic-only'}]
        request = {k: v for k, v in FEEDBACK.items()}
        confirmed = dict(api_version=1, success=True, video_id='abcdefghijk', feedback='not_interested')
        assert await service.execute(request) == confirmed
        await service._stop()
        assert await service.execute(request) == confirmed and service.process is None
        assert len(calls.read_text().splitlines()) == 1
        with pytest.raises(ServiceError) as error:
            await service.execute({**request, 'id': '12345678901'})
        assert error.value.code == 'feedback_unconfirmed'
        # The other choice and another profile are separate one-time intents.
        assert (await service.execute({**request, 'feedback': 'dont_recommend_channel'}))['feedback'] == 'dont_recommend_channel'
        assert (await service.execute({**request, 'profile': 'other'}))['success']
        assert len(calls.read_text().splitlines()) == 3
        with pytest.raises(ServiceError) as error:
            await service.execute({**request, 'cursor': 'wrong'})
        assert error.value.code == 'feedback_unconfirmed' and error.value.status == 502
        service.timeout = .2
        with pytest.raises(ServiceError) as error:
            await service.execute({**request, 'cursor': 'hang'})
        assert error.value.code == 'library_timeout' and service.process is None
        service.timeout = 2
        for cursor in ('wrong', 'hang'):
            with pytest.raises(ServiceError) as error:
                await service.execute({**request, 'cursor': cursor})
            assert error.value.code == 'feedback_unconfirmed' and error.value.status == 409
        assert len(calls.read_text().splitlines()) == 5 and service.process is None
        await service.close()
    asyncio.run(run())


def test_feedback_guests_invalid_and_unknown_profiles_fail_before_starting_worker(tmp_path):
    async def run():
        worker, calls = feedback_worker(tmp_path)
        service = LibraryService(sys.executable, worker=worker, timeout=2)
        service.profiles = [{'id': 'signed', 'cookie': 'synthetic-only'}]
        for patch, code in [(dict(profile='guest'), 'account_required'), (dict(profile='missing'), 'unknown_profile'),
                            (dict(feedback='report'), 'invalid_feedback'), (dict(id='bad'), 'invalid_feedback'),
                            (dict(cursor='bad/reference'), 'invalid_feedback'), (dict(cursor=7), 'invalid_feedback')]:
            with pytest.raises(ServiceError) as error:
                await service.execute({**FEEDBACK, **patch})
            assert error.value.code == code
        assert service.process is None and not calls.exists()
        for index in range(256):
            service.feedback_states[('signed', str(index), 'not_interested')] = dict(id='abcdefghijk', result=None, deadline=time.monotonic() + 60)
        with pytest.raises(ServiceError) as error:
            await service.execute(dict(FEEDBACK))
        assert error.value.code == 'feedback_busy'
        service.profiles[0]['oauth'] = {}
        service.oauth_store = type('Store', (), {'remove': lambda self, identity: True})()
        await service.execute(dict(action='profile_remove', profile='signed'))
        assert not service.feedback_states
        await service.close()
    asyncio.run(run())


def test_fixture_sections_suggestions_and_description(tmp_path):
    with TestClient(create_demo_app(tmp_path, token=TOKEN)) as client:
        def call(action, **kw):
            return client.post('/v1/library', headers=AUTH, json={'action': action, **kw})
        for section in ('trending', 'shorts'):
            row = call('topics', section=section).json()['items'][0]
            assert row['is_folder'] and row['id'].startswith(f'section:{section}:')
            assert len(call('topic_open', section=section, id=row['id'].split(':')[2]).json()['items']) == 3
        assert call('topics', section='my_videos').status_code == 401
        assert call('topics', section='my_videos', profile='fixture_one').json()['items'][0]['is_folder']
        suggestions = call('search_suggestions', query=' cat ').json()
        assert suggestions['kind'] == 'search_suggestions' and suggestions['query'] == 'cat'
        assert 1 <= len(suggestions['suggestions']) <= 15 and all(len(s) <= 200 for s in suggestions['suggestions'])
        description = call('description', id=VIDEOS[0]).json()
        assert description['kind'] == 'description' and description['id'] == VIDEOS[0]
        assert 320 < len(description['description']) <= 20000 and description['truncated'] is False
        assert call('description', id='abcdefghijk').status_code == 404
        assert call('feedback', id=VIDEOS[0], cursor='reference', feedback='not_interested', profile='fixture_one').status_code == 400


def test_real_worker_rejects_unsupported_search_and_guest_account_sections_offline():
    node = Path.home() / '.cache/prosperotube/node/node-v24.10.0-linux-x64/bin/node'
    dependency = Path(__file__).resolve().parents[1] / 'companion/innertube/worker.bundle.mjs'
    if not node.is_file() or not dependency.is_file():
        pytest.skip('optional library runtime not installed')
    async def run():
        service = LibraryService(node)
        for request, code in [(dict(action='search', query='cats', upload_date='hour'), 'invalid_search'),
                              (dict(action='search', query='cats', order='rating'), 'invalid_search'),
                              (dict(action='search', query=''), 'invalid_search'),
                              (dict(action='search_suggestions', query='x' * 201), 'invalid_search'),
                              (dict(action='description', id='bad'), 'invalid_identity'),
                              (dict(action='channels'), 'account_required'),
                              (dict(action='notifications'), 'account_required')]:
            with pytest.raises(ServiceError) as error:
                await service.execute(request)
            assert error.value.code == code
        await service.close()
    asyncio.run(run())
