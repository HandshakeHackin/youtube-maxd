"""Live-chat API validation and asynchronous jobs use the shipping service boundary."""
import time

from fastapi.testclient import TestClient

from prosperotube.app import create_app


def test_replay_clock_is_bounded_numeric_and_forwarded_without_account_mutation():
    calls = []
    class Library:
        async def close(self): pass
        async def execute(self, request):
            calls.append(request)
            return {'api_version': 1, 'kind': 'live_chat', 'is_replay': True,
                    'position_ms': int(request['chat_position_sec'] * 1000), 'items': []}
    with TestClient(create_app(token='test-token', library=Library())) as client:
        headers = {'Authorization': 'Bearer test-token'}
        for value in (-1, 1000001, '5', True):
            response = client.post('/v1/library', headers=headers, json={
                'action': 'live_chat', 'id': 'abcdefghijk', 'chat_position_sec': value})
            assert response.status_code == 422 and not calls
        for value in (0, 1.25, 1000000):
            response = client.post('/v1/library', headers=headers, json={
                'action': 'live_chat', 'id': 'abcdefghijk', 'chat_position_sec': value})
            assert response.status_code == 200
            assert response.json()['position_ms'] == int(value * 1000)
            assert calls[-1]['chat_position_sec'] == value


def test_live_chat_api_auth_modes_jobs_and_close_are_read_operations():
    calls = []
    class Library:
        async def close(self): pass
        async def execute(self, request):
            calls.append(request)
            if request['action'] == 'chat_close':
                return {'api_version': 1, 'success': True}
            return {'api_version': 1, 'kind': 'live_chat', 'id': request.get('id'),
                    'items': [], 'cursor': 'opaque-cursor', 'ended': False,
                    'poll_after_ms': 2000, 'is_replay': False, 'has_more': False}
    with TestClient(create_app(token='test-token', library=Library())) as client:
        headers = {'Authorization': 'Bearer test-token'}
        assert client.post('/v1/library', json={'action': 'live_chat', 'id': 'abcdefghijk'}).status_code == 401
        assert not calls
        invalid = client.post('/v1/library', headers=headers, json={'action': 'live_chat', 'id': 'abcdefghijk', 'chat_mode': 'bad'})
        assert invalid.status_code == 422 and not calls
        started = client.post('/v1/jobs', headers=headers, json={'action': 'library', 'library': {
            'action': 'live_chat', 'id': 'abcdefghijk', 'chat_mode': 'live', 'chat_history': True}})
        assert started.status_code == 200
        deadline = time.monotonic() + 5
        while True:
            result = client.get('/v1/jobs/' + started.json()['job'], headers=headers).json()
            if result['state'] != 'pending': break
            assert time.monotonic() < deadline
            time.sleep(.01)
        assert result['state'] == 'done' and result['result']['kind'] == 'live_chat'
        assert calls[0]['chat_mode'] == 'live' and calls[0]['chat_history'] is True
        assert client.post('/v1/library', headers=headers, json={'action': 'chat_poll', 'cursor': 'opaque-cursor'}).status_code == 200
        assert client.post('/v1/library', headers=headers, json={'action': 'chat_close', 'cursor': 'opaque-cursor'}).json()['success']
        assert [call['action'] for call in calls] == ['live_chat', 'chat_poll', 'chat_close']


def test_abandoned_chat_jobs_release_the_context_but_delivered_results_transfer_ownership():
    closed = []
    class Library:
        async def close(self): pass
        async def execute(self, request):
            if request['action'] == 'chat_close':
                closed.append((request['profile'], request['cursor']))
                return {'api_version': 1, 'success': True}
            return {'api_version': 1, 'kind': 'live_chat', 'cursor': 'owned-cursor'}
    app = create_app(token='test-token', library=Library())
    headers = {'Authorization': 'Bearer test-token'}
    with TestClient(app) as client:
        def start():
            job = client.post('/v1/jobs', headers=headers, json={'action': 'library', 'library': {
                'action': 'live_chat', 'id': 'abcdefghijk', 'profile': 'one'}}).json()['job']
            deadline = time.monotonic() + 5
            while not app.state.jobs[job][1].done():
                assert time.monotonic() < deadline
                time.sleep(.01)
            return job
        job = start()
        assert client.delete('/v1/jobs/' + job, headers=headers).status_code == 200
        deadline = time.monotonic() + 5
        while not closed:
            assert time.monotonic() < deadline
            time.sleep(.01)
        assert closed == [('one', 'owned-cursor')]
        job = start()
        assert client.get('/v1/jobs/' + job, headers=headers).json()['state'] == 'done'
        assert client.delete('/v1/jobs/' + job, headers=headers).status_code == 200
        assert len(closed) == 1
        assert client.post('/v1/library', headers=headers, json={'action': 'chat_close', 'profile': 'one',
                                                              'cursor': 'owned-cursor'}).status_code == 200
        assert len(closed) == 2


def test_offline_fixture_chat_rolls_stable_message_ids_and_scopes_capabilities(tmp_path):
    from prosperotube.demo import FixtureExtractor
    from prosperotube.fixture_library import FixtureLibrary, VIDEOS
    from prosperotube.models import ServiceError
    import asyncio
    import pytest
    clock = [100.0]
    library = FixtureLibrary(FixtureExtractor(), clock=lambda: clock[0])
    async def run():
        first = await library.execute({'action': 'live_chat', 'profile': 'guest', 'id': VIDEOS[2]})
        assert len(first['items']) == 24 and first['chat_mode'] == 'top'
        assert first['items'][1]['amount'] == '$5.00' and '<b>' in first['items'][0]['content']
        early = await library.execute({'action': 'chat_poll', 'cursor': first['cursor']})
        assert early['cursor'] == first['cursor'] and early['items'] == first['items']
        with pytest.raises(ServiceError):
            await library.execute({'action': 'chat_poll', 'profile': 'fixture_one', 'cursor': first['cursor']})
        with pytest.raises(ServiceError):
            await library.execute({'action': 'continue', 'cursor': first['cursor']})
        clock[0] += 2
        second = await library.execute({'action': 'chat_poll', 'cursor': first['cursor']})
        assert second['items'][0]['id'] == first['items'][1]['id']
        assert second['items'][-1]['id'] != first['items'][-1]['id']
        assert await library.execute({'action': 'chat_poll', 'cursor': first['cursor']}) == second
        assert (await library.execute({'action': 'chat_close', 'cursor': first['cursor']}))['success']
        with pytest.raises(ServiceError):
            await library.execute({'action': 'chat_poll', 'cursor': second['cursor']})
        live = await library.execute({'action': 'live_chat', 'profile': 'fixture_one', 'id': VIDEOS[2], 'chat_mode': 'live'})
        assert live['chat_mode'] == 'live'
        await library.execute({'action': 'profile_remove', 'profile': 'fixture_one'})
        assert not library.chats
        await library.close()
    asyncio.run(run())


def test_fixture_chat_history_retains_and_bounds_previous_messages():
    import asyncio
    from prosperotube.demo import FixtureExtractor
    from prosperotube.fixture_library import FixtureLibrary, VIDEOS

    async def run():
        clock = [100.0]
        library = FixtureLibrary(FixtureExtractor(), clock=lambda: clock[0])
        try:
            snapshot = await library.execute(dict(action='live_chat', id=VIDEOS[2], chat_history=True))
            first_id = snapshot['items'][0]['id']
            for index in range(240):
                clock[0] += 2
                snapshot = await library.execute(dict(action='chat_poll', cursor=snapshot['cursor']))
                assert len(snapshot['items']) == min(25 + index, 256)
                if index < 232:
                    assert snapshot['items'][0]['id'] == first_id
            assert snapshot['items'][0]['id'] != first_id
            assert len({row['id'] for row in snapshot['items']}) == 256
        finally:
            await library.close()
    asyncio.run(run())

def test_replay_page_boundary_rejects_invalid_indices_and_forwards_owned_read():
    calls = []
    class Library:
        async def close(self): pass
        async def execute(self, request):
            calls.append(request)
            return {'api_version': 1, 'kind': 'live_chat', 'items': [],
                    'cursor': 'next-page', 'chat_page': request['chat_page'], 'has_more': True}
    with TestClient(create_app(token='page-token', library=Library())) as client:
        headers = {'Authorization': 'Bearer page-token'}
        for page in (-1, 1000001, True, '1', 1.5):
            response = client.post('/v1/library', headers=headers, json={
                'action': 'chat_poll', 'cursor': 'owned-page', 'chat_page': page})
            assert response.status_code == 422 and not calls
        response = client.post('/v1/library', headers=headers, json={
            'action': 'chat_poll', 'cursor': 'owned-page', 'profile': 'one',
            'chat_position_sec': 60, 'chat_page': 1})
        assert response.status_code == 200 and response.json()['chat_page'] == 1
        assert calls[0]['cursor'] == 'owned-page' and calls[0]['profile'] == 'one'
        assert calls[0]['chat_page'] == 1 and calls[0]['chat_position_sec'] == 60
