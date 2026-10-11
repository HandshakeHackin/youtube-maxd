"""History removal transport keeps the retained account-page reference intact."""
import time
import pytest
from fastapi.testclient import TestClient
from prosperotube.app import create_app

AUTH={'Authorization':'Bearer history-action-fixture'}
@pytest.mark.parametrize('patch',[dict(require_channel='true'),dict(require_channel=1),dict(action='history')])
def test_channel_verification_request_schema(patch):
    from pydantic import ValidationError
    from prosperotube.app import LibraryRequest
    with pytest.raises(ValidationError):
        LibraryRequest.model_validate(dict(action='playback_next',id='abcdefghijk',require_channel=True)|patch)

REQUEST=dict(action='history_remove',profile='signed',id='abcdefghijk',cursor='retained-history-reference')

class Library:
    def __init__(self):self.calls=[]
    async def execute(self,request):
        self.calls.append(request)
        return dict(api_version=1,success=True,video_id=request['id'])
    async def close(self):pass

@pytest.mark.parametrize('operation',[REQUEST,dict(action='mark_watched',profile='signed',id='abcdefghijk',session='confirm-one'),
    dict(action='playback_next',profile='signed',id='abcdefghijk',require_channel=True),
    dict(action='playback_queue',profile='signed',id='abcdefghijk',playlist='PLabcdefghijk',index=0,require_channel=True),
    dict(action='playback_at',profile='signed',id='',playlist='PLabcdefghijk',index=1,require_channel=True)])
def test_direct_and_job_removal_preserve_account_video_and_reference(operation):
    library=Library()
    with TestClient(create_app(library=library,token='history-action-fixture')) as client:
        assert client.post('/v1/library',json=operation).status_code==401
        assert not library.calls
        direct=client.post('/v1/library',json=operation,headers=AUTH)
        assert direct.status_code==200
        job=client.post('/v1/jobs',json=dict(action='library',library=operation),headers=AUTH)
        assert job.status_code==200
        for _ in range(100):
            result=client.get('/v1/jobs/'+job.json()['job'],headers=AUTH).json()
            if result['state']!='pending':break
            time.sleep(.001)
        assert result['state']=='done' and result['result']==direct.json()
        assert len(library.calls)==2
        assert all(all(call[key]==value for key,value in operation.items()) for call in library.calls)

@pytest.mark.parametrize('patch',[dict(id='bad'),dict(session=''),dict(session='bad/session'),dict(session='a'*65)])
def test_invalid_mark_watched_does_not_reach_worker(patch):
    library=Library()
    payload=dict(action='mark_watched',profile='signed',id='abcdefghijk',session='confirm-one')|patch
    with TestClient(create_app(library=library,token='history-action-fixture')) as client:
        for path,body in [('/v1/library',payload),('/v1/jobs',dict(action='library',library=payload))]:
            assert client.post(path,json=body,headers=AUTH).status_code==422
        assert not library.calls

@pytest.mark.parametrize('patch',[dict(id='bad'),dict(id=None),dict(cursor=''),dict(cursor='bad/reference'),
    dict(cursor='a'*65),dict(profile='bad/profile')])
def test_invalid_history_removal_does_not_reach_worker(patch):
    library=Library()
    with TestClient(create_app(library=library,token='history-action-fixture')) as client:
        for path,body in [('/v1/library',{**REQUEST,**patch}),('/v1/jobs',dict(action='library',library={**REQUEST,**patch}))]:
            assert client.post(path,json=body,headers=AUTH).status_code==422
        assert not library.calls

@pytest.mark.parametrize('action',['playback_next','playback_queue','playback_at','channel_window'])
def test_name_verification_flags(action):
    from pydantic import ValidationError
    from prosperotube.app import LibraryRequest
    request=dict(action=action,id='abcdefghijk',require_channel=True,require_name=True)
    if action in {'playback_queue','playback_at'}:
        request.update(playlist='PLabcdefghijk',index=0)
    if action=='playback_at':
        request['id']=''
    if action=='channel_window':
        request.update(channel='UC'+'a'*22,index=0)
    assert LibraryRequest.model_validate(request).require_name
    for patch in [dict(require_channel=False),dict(require_name=1),dict(require_name='true')]:
        with pytest.raises(ValidationError):
            LibraryRequest.model_validate(request|patch)


def test_retained_group_navigation_transport():
    test_direct_and_job_removal_preserve_account_video_and_reference(dict(
        action='group_window',id='abcdefghijk',group='12345678-1234-1234-1234-123456789abc',
        index=0,target=1,direction=1,require_channel=True,require_name=True))
