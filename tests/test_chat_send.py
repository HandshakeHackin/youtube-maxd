"""Chat send guards survive worker loss; no real message or account is used."""
import asyncio
import json
import sys

import pytest
from fastapi.testclient import TestClient

from prosperotube.app import create_app
from prosperotube.library import LibraryService
from prosperotube.models import ServiceError


def service(tmp_path, mode):
    worker = tmp_path / 'worker.py'
    worker.write_text('''import json,sys,time
from pathlib import Path
for line in sys.stdin:
    value=json.loads(line)
    if value['operation']=='init': result={'ready':True}
    else:
        request=value['request']
        with Path(__file__).with_suffix('.calls').open('a') as f: f.write('attempt\\n')
        if MODE=='hang': time.sleep(30)
        result={'api_version':1,'success':True,'id':request['id'],'session':request['session'],
                'message_id':'' if MODE=='wrong' else 'confirmed-message','private_extra':'must-not-cross'}
    print(json.dumps({'id':value['id'],'result':result}),flush=True)
'''.replace('MODE', repr(mode)))
    library = LibraryService(sys.executable, worker=worker, timeout=1)
    library.profiles = [{'id':'one','cookie':'synthetic-only'}]
    return library, worker.with_suffix('.calls')


REQUEST = dict(action='chat_send', profile='one', id='abcdefghijk', cursor='owned-chat',
               session='reviewed-intent', text='Hello 日本語 😀')


@pytest.mark.parametrize('total', [1, 30])
def test_fixture_sent_messages_keep_native_snapshot_limit(total):
    from prosperotube.demo import FixtureExtractor
    from prosperotube.fixture_library import FixtureLibrary, VIDEOS

    async def run():
        clock = [100.0]
        library = FixtureLibrary(FixtureExtractor(), clock=lambda: clock[0])
        try:
            opened = await library.execute(dict(action='live_chat', profile='fixture_one', id=VIDEOS[2]))
            sent = []
            for index in range(total):
                request = dict(action='chat_send', profile='fixture_one', id=VIDEOS[2],
                               cursor=opened['cursor'], session=f'intent-{index}', text=f'Hello 日本語 {index}')
                sent.append((await library.execute(request))['message_id'])
                assert (await library.execute(request))['message_id'] == sent[-1]
            poll = dict(action='chat_poll', profile='fixture_one', cursor=opened['cursor'])
            early = await library.execute(poll)
            assert len(early['items']) == 24
            assert [row['id'] for row in early['items']][-min(total, 24):] == sent[-24:]
            clock[0] += 2
            updated = await library.execute(poll)
            assert len(updated['items']) == 24
            assert [row['id'] for row in updated['items']][-min(total, 24):] == sent[-24:]
            assert await library.execute(poll) == updated
            assert len({row['id'] for row in updated['items']}) == 24
            assert len(opened['items']) == 24 and all(row['id'] not in sent for row in opened['items'])
        finally:
            await library.close()
    asyncio.run(run())


def test_confirmed_message_cached_without_text_or_worker_restart(tmp_path):
    async def run():
        library, calls = service(tmp_path, 'success')
        try:
            result = await library.execute(REQUEST)
            assert result == dict(api_version=1,success=True,id=REQUEST['id'],session=REQUEST['session'],message_id='confirmed-message')
            await library._stop()
            assert await library.execute(REQUEST) == result and library.process is None
            for field, value in [('text','Changed'),('cursor','other-chat'),('id','different11')]:
                with pytest.raises(ServiceError) as error: await library.execute({**REQUEST,field:value})
                assert error.value.code == 'chat_unconfirmed'
            assert calls.read_text().splitlines() == ['attempt']
            assert REQUEST['text'] not in json.dumps(list(library.chat_sends.values()), ensure_ascii=False)
        finally: await library.close()
    asyncio.run(run())


@pytest.mark.parametrize('failure', ['wrong','timeout','cancel'])
def test_unconfirmed_chat_send_is_not_replayed_after_worker_loss(tmp_path, failure):
    async def run():
        library, calls = service(tmp_path, 'wrong' if failure == 'wrong' else 'hang')
        try:
            if failure == 'cancel':
                task = asyncio.create_task(library.execute(REQUEST))
                async with asyncio.timeout(3):
                    while not calls.exists(): await asyncio.sleep(.01)
                task.cancel()
                with pytest.raises(asyncio.CancelledError): await task
            else:
                with pytest.raises(ServiceError): await library.execute(REQUEST)
            await library._stop()
            with pytest.raises(ServiceError) as error: await library.execute(REQUEST)
            assert error.value.code == 'chat_unconfirmed' and library.process is None
            assert calls.read_text().splitlines() == ['attempt']
        finally: await library.close()
    asyncio.run(run())


def test_shipping_direct_and_job_chat_send_confirmation_and_invalid_text(tmp_path):
    library, calls = service(tmp_path, 'success')
    app = create_app(token='chat-test-token',library=library)
    headers = {'Authorization':'Bearer chat-test-token'}
    with TestClient(app) as client:
        assert client.post('/v1/library',json=REQUEST).status_code == 401
        for text in ['', ' ', 'bad\nline', 'x'*201]:
            assert client.post('/v1/library',headers=headers,json={**REQUEST,'text':text}).status_code in (400,422)
        assert client.post('/v1/library',headers=headers,json={**REQUEST,'profile':'guest'}).status_code == 401
        assert not calls.exists()
        first = client.post('/v1/library',headers=headers,json=REQUEST)
        assert first.status_code == 200 and first.json()['message_id'] == 'confirmed-message'
        job = client.post('/v1/jobs',headers=headers,json={'action':'library','library':REQUEST}).json()['job']
        for _ in range(100):
            result = client.get('/v1/jobs/'+job,headers=headers).json()
            if result['state'] != 'pending': break
        assert result['state'] == 'done' and result['result'] == first.json()
        assert calls.read_text().splitlines() == ['attempt']
