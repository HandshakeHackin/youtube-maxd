"""Live-chat API validation and asynchronous jobs use the shipping service boundary."""
import time

from fastapi.testclient import TestClient

from prosperotube.app import create_app


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
            'action': 'live_chat', 'id': 'abcdefghijk', 'chat_mode': 'live'}})
        assert started.status_code == 200
        deadline = time.monotonic() + 5
        while True:
            result = client.get('/v1/jobs/' + started.json()['job'], headers=headers).json()
            if result['state'] != 'pending': break
            assert time.monotonic() < deadline
            time.sleep(.01)
        assert result['state'] == 'done' and result['result']['kind'] == 'live_chat'
        assert calls[0]['chat_mode'] == 'live'
        assert client.post('/v1/library', headers=headers, json={'action': 'chat_poll', 'cursor': 'opaque-cursor'}).status_code == 200
        assert client.post('/v1/library', headers=headers, json={'action': 'chat_close', 'cursor': 'opaque-cursor'}).json()['success']
        assert [call['action'] for call in calls] == ['live_chat', 'chat_poll', 'chat_close']
