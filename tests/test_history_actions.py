"""History removal transport keeps the retained account-page reference intact."""
import time
import pytest
from fastapi.testclient import TestClient
from prosperotube.app import create_app

AUTH={'Authorization':'Bearer history-action-fixture'}
REQUEST=dict(action='history_remove',profile='signed',id='abcdefghijk',cursor='retained-history-reference')

class Library:
    def __init__(self):self.calls=[]
    async def execute(self,request):
        self.calls.append(request)
        return dict(api_version=1,success=True,video_id=request['id'])
    async def close(self):pass

def test_direct_and_job_removal_preserve_account_video_and_reference():
    library=Library()
    with TestClient(create_app(library=library,token='history-action-fixture')) as client:
        assert client.post('/v1/library',json=REQUEST).status_code==401
        assert not library.calls
        direct=client.post('/v1/library',json=REQUEST,headers=AUTH)
        assert direct.status_code==200
        job=client.post('/v1/jobs',json=dict(action='library',library=REQUEST),headers=AUTH)
        assert job.status_code==200
        for _ in range(100):
            result=client.get('/v1/jobs/'+job.json()['job'],headers=AUTH).json()
            if result['state']!='pending':break
            time.sleep(.001)
        assert result['state']=='done' and result['result']==direct.json()
        assert len(library.calls)==2
        assert all(all(call[key]==value for key,value in REQUEST.items()) for call in library.calls)

@pytest.mark.parametrize('patch',[dict(id='bad'),dict(id=None),dict(cursor=''),dict(cursor='bad/reference'),
    dict(cursor='a'*65),dict(profile='bad/profile')])
def test_invalid_history_removal_does_not_reach_worker(patch):
    library=Library()
    with TestClient(create_app(library=library,token='history-action-fixture')) as client:
        for path,body in [('/v1/library',{**REQUEST,**patch}),('/v1/jobs',dict(action='library',library={**REQUEST,**patch}))]:
            assert client.post(path,json=body,headers=AUTH).status_code==422
        assert not library.calls
