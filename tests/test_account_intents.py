"""One-attempt playlist-library and watch-history changes across worker restarts; no account use."""
import asyncio
from pathlib import Path
import sys
import time

import pytest

from prosperotube.library import LibraryService
from prosperotube.models import ServiceError

SAVE = dict(action='playlist_save', profile='signed', playlist='PLabcdefghijk', session='intent-1')


def worker(tmp_path):
    path = tmp_path / 'account-worker.py'
    path.write_text('''import json, sys, time
from pathlib import Path
calls = Path(__file__).with_name("account-calls.jsonl")
states = {"history_pause": "paused", "history_resume": "active", "history_clear": "cleared"}
for line in sys.stdin:
    value = json.loads(line)
    if value["operation"] == "init":
        print(json.dumps({"id": value["id"], "result": {"ready": True}}), flush=True); continue
    request = value["request"]
    with calls.open("a") as f: f.write(json.dumps(request) + "\\n")
    session, action = request.get("session"), request["action"]
    if session == "hang": time.sleep(30)
    if session == "missing":
        print(json.dumps({"id": value["id"], "error": {"code": "history_control_unavailable", "message": "Not offered.", "status": 404}}), flush=True); continue
    if action.startswith("playlist_"):
        result = {"api_version": 1, "success": True, "playlist_id": "PLzyxwvutsrqp" if session.startswith("wrong") else request["playlist"],
                  "saved": action == "playlist_save", "private": "never-forwarded"}
    else:
        result = {"api_version": 1, "success": True, "history": "bogus" if session.startswith("wrong") else states[action], "private": "never-forwarded"}
    print(json.dumps({"id": value["id"], "result": result}), flush=True)
''', encoding='utf-8')
    return path, tmp_path / 'account-calls.jsonl'


def service_for(tmp_path):
    path, calls = worker(tmp_path)
    service = LibraryService(sys.executable, worker=path, timeout=2)
    service.profiles = [{'id': 'signed', 'cookie': 'synthetic-only'}, {'id': 'other', 'cookie': 'synthetic-only'}]
    return service, calls


def test_playlist_and_history_confirmations_survive_worker_restart_and_are_never_repeated(tmp_path):
    async def run():
        service, calls = service_for(tmp_path)
        saved = dict(api_version=1, success=True, playlist_id='PLabcdefghijk', saved=True)
        assert await service.execute(dict(SAVE)) == saved
        await service._stop()
        assert await service.execute(dict(SAVE)) == saved and service.process is None
        assert len(calls.read_text().splitlines()) == 1
        for patch in (dict(playlist='PLzyxwvutsrqp'), dict(action='playlist_unsave'), dict(action='history_clear')):
            with pytest.raises(ServiceError) as error:
                await service.execute({**SAVE, **patch})
            assert error.value.status == 409 and error.value.code in ('playlist_unconfirmed', 'history_unconfirmed')
        assert await service.execute({**SAVE, 'action': 'playlist_unsave', 'session': 'intent-2'}) == {**saved, 'saved': False}
        assert (await service.execute({**SAVE, 'profile': 'other'}))['saved'] is True
        for action, state in (('history_pause', 'paused'), ('history_resume', 'active'), ('history_clear', 'cleared')):
            request = dict(action=action, profile='signed', session='history-' + action)
            assert await service.execute(request) == dict(api_version=1, success=True, history=state)
            assert await service.execute(request) == dict(api_version=1, success=True, history=state)
        assert len(calls.read_text().splitlines()) == 6
        # A wrong confirmation, a timeout and a lost worker all leave the intent unrepeatable.
        for request, code in ((dict(SAVE, session='wrong'), 'playlist_unconfirmed'),
                              (dict(action='history_pause', profile='signed', session='wrong-history'), 'history_unconfirmed')):
            with pytest.raises(ServiceError) as error:
                await service.execute(request)
            assert error.value.code == code and error.value.status == 502
            with pytest.raises(ServiceError) as error:
                await service.execute(request)
            assert error.value.code == code and error.value.status == 409
        service.timeout = .2
        with pytest.raises(ServiceError) as error:
            await service.execute(dict(action='history_clear', profile='signed', session='hang'))
        assert error.value.code == 'library_timeout' and service.process is None
        service.timeout = 2
        with pytest.raises(ServiceError) as error:
            await service.execute(dict(action='history_clear', profile='signed', session='hang'))
        assert error.value.code == 'history_unconfirmed' and error.value.status == 409
        assert len(calls.read_text().splitlines()) == 9
        # A control the worker did not find sent nothing, so the same confirmation stays usable.
        for _ in range(2):
            with pytest.raises(ServiceError) as error:
                await service.execute(dict(action='history_resume', profile='signed', session='missing'))
            assert error.value.code == 'history_control_unavailable' and error.value.status == 404
        assert len(calls.read_text().splitlines()) == 11 and ('signed', 'missing') not in service.account_intents
        await service.close()
    asyncio.run(run())


def test_invalid_guest_and_unknown_account_changes_fail_before_starting_the_worker(tmp_path):
    async def run():
        service, calls = service_for(tmp_path)
        for patch, code in [(dict(profile='guest'), 'account_required'), (dict(profile='missing'), 'unknown_profile'),
                            (dict(session=''), 'invalid_request'), (dict(session='bad session'), 'invalid_request'),
                            (dict(session=7), 'invalid_request'), (dict(playlist='WL'), 'invalid_request'),
                            (dict(playlist='LL'), 'invalid_request'), (dict(playlist='short'), 'invalid_request'),
                            (dict(playlist=None), 'invalid_request'), (dict(profile='bad/profile'), 'invalid_request')]:
            with pytest.raises(ServiceError) as error:
                await service.execute({**SAVE, **patch})
            assert error.value.code == code
        with pytest.raises(ServiceError) as error:
            await service.execute(dict(action='history_pause', profile='guest', session='x'))
        assert error.value.code == 'account_required'
        assert service.process is None and not calls.exists()
        for index in range(256):
            service.account_intents[('signed', str(index))] = dict(identity=('history_pause', ''), result=None, deadline=time.monotonic() + 60)
        with pytest.raises(ServiceError) as error:
            await service.execute(dict(SAVE))
        assert error.value.code == 'account_busy' and error.value.status == 429
        service.account_intents[('signed', 'old')] = dict(identity=('history_pause', ''), result=None, deadline=time.monotonic() - 1)
        service.profiles[0]['oauth'] = {}
        service.oauth_store = type('Store', (), {'remove': lambda self, identity: True})()
        await service.execute(dict(action='profile_remove', profile='signed'))
        assert not service.account_intents
        await service.close()
    asyncio.run(run())


def test_hide_feedback_uses_the_existing_one_attempt_gate(tmp_path):
    async def run():
        service, calls = service_for(tmp_path)
        path = tmp_path / 'account-worker.py'
        path.write_text(path.read_text().replace('if action.startswith("playlist_"):',
            'if action == "feedback":\n        result = {"api_version": 1, "success": True, "video_id": request["id"], "feedback": request["feedback"]}\n    elif action.startswith("playlist_"):'))
        request = dict(action='feedback', profile='signed', id='abcdefghijk', cursor='12345678-1234-1234-1234-123456789abc', feedback='hide')
        confirmed = dict(api_version=1, success=True, video_id='abcdefghijk', feedback='hide')
        assert await service.execute(dict(request)) == confirmed
        await service._stop()
        assert await service.execute(dict(request)) == confirmed and len(calls.read_text().splitlines()) == 1
        with pytest.raises(ServiceError) as error:
            await service.execute({**request, 'feedback': 'unhide'})
        assert error.value.code == 'invalid_feedback'
        await service.close()
    asyncio.run(run())


def test_real_worker_rejects_new_requests_offline_before_any_upstream_call():
    node = Path.home() / '.cache/prosperotube/node/node-v24.10.0-linux-x64/bin/node'
    bundle = Path(__file__).resolve().parents[1] / 'companion/innertube/worker.bundle.mjs'
    if not node.is_file() or not bundle.is_file():
        pytest.skip('optional library runtime not installed')
    async def run():
        service = LibraryService(node)
        channel = 'UC' + 'c' * 22
        for request, code in [(dict(action='home', hl='xx'), 'invalid_locale'), (dict(action='home', gl='us'), 'invalid_locale'),
                              (dict(action='home', hide=['shorts']), 'invalid_filter'),
                              (dict(action='channel', channel='bad'), 'invalid_identity'),
                              (dict(action='channel_search', channel=channel, query=''), 'invalid_search'),
                              (dict(action='playlist_save', playlist='PLabcdefghijk', session='x'), 'account_required'),
                              (dict(action='history_clear', session='x'), 'account_required')]:
            with pytest.raises(ServiceError) as error:
                await service.execute(request)
            assert error.value.code == code
        with pytest.raises(ServiceError) as error:
            await service.execute(dict(action='catalog', profile='guest', kind='search', query='cats', hl='xx'), operation='playback')
        assert error.value.code == 'invalid_locale'
        await service.close()
    asyncio.run(run())
