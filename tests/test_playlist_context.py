"""Playlist occurrences retain original positions through guest and OAuth catalog APIs."""
import asyncio
import pytest
from prosperotube.catalog import page_for, playlist_context
from prosperotube.extractor import Extractor
from prosperotube.oauth_extractor import OAuthExtractor

ID='abcdefghijk'
PLAYLIST='PLabcdefghijk'

@pytest.mark.parametrize('index',[1,5,100001])
def test_source_indices_are_converted_once(index):
    value=playlist_context({'playlist_id':PLAYLIST,'playlist_index':index},PLAYLIST)
    assert value.model_dump()=={'playlist':PLAYLIST,'index':index-1}

@pytest.mark.parametrize('index',[None,True,0,-1,1.5,100002,'5'])
def test_unknown_or_invalid_source_positions_are_not_guessed(index):
    assert playlist_context({'playlist_index':index},PLAYLIST) is None

@pytest.mark.parametrize('supplied',[{'playlist':'PLdifferent11','index':0},{'playlist':PLAYLIST,'index':True},
                                     {'playlist':PLAYLIST,'index':-1},'invalid'])
def test_malformed_worker_context_never_falls_back_to_another_position(supplied):
    assert playlist_context({'playback_context':supplied,'playlist_index':5},PLAYLIST) is None

def test_guest_playlist_duplicates_and_holes_survive_while_search_still_deduplicates():
    class Source:
        async def extract(self,*args,**kwargs):
            return {'entries':[{'id':ID,'title':'Repeated','playlist_id':PLAYLIST,'playlist_index':i} for i in [1,5,9]]}
    async def run():
        page=await page_for(Source(),parent='playlist:'+PLAYLIST)
        assert [row.id for row in page.items]==[ID]*3
        assert [row.playback_context.index for row in page.items]==[0,4,8]
        page=await page_for(Source(),query='search')
        assert len(page.items)==1 and page.items[0].playback_context is None
    asyncio.run(run())

def test_oauth_worker_context_survives_adapter_and_explicit_pagination():
    class Library:
        async def execute(self,request,*,operation):
            assert operation=='playback' and request['profile']=='signed'
            assert request['action']=='catalog'
            offset=30 if request.get('cursor') else 0
            return {'items':[{'id':ID,'title':'Repeated','playback_context':{'playlist':PLAYLIST,'index':i}}
                             for i in [offset,offset+4]],'has_more':offset==0,'cursor':'next' if offset==0 else ''}
    async def run():
        extractor=OAuthExtractor('signed',Library(),Extractor())
        for page,indices in [(0,[0,4]),(1,[30,34]),(0,[0,4])]:
            result=await page_for(extractor,parent='playlist:'+PLAYLIST,page=page)
            assert [row.id for row in result.items]==[ID,ID]
            assert [row.playback_context.index for row in result.items]==indices
            assert result.has_more==(page==0)
    asyncio.run(run())

def test_catalog_http_and_job_responses_keep_occurrence_context():
    import time
    from fastapi.testclient import TestClient
    from prosperotube.app import create_app
    class Source:
        async def extract(self,*args,**kwargs):
            return {'entries':[{'id':ID,'title':'Repeated','playlist_index':i} for i in [1,7]]}
    headers={'Authorization':'Bearer fixture-context-token'}
    with TestClient(create_app(extractor=Source(),token='fixture-context-token')) as client:
        direct=client.get('/v1/catalog',params={'parent':'playlist:'+PLAYLIST},headers=headers)
        assert direct.status_code==200
        job=client.post('/v1/jobs',json={'action':'catalog','parent':'playlist:'+PLAYLIST},headers=headers)
        assert job.status_code==200
        for _ in range(100):
            result=client.get('/v1/jobs/'+job.json()['job'],headers=headers).json()
            if result['state']!='pending':break
            time.sleep(.001)
        assert result['state']=='done'
        for response in [direct.json(),result['result']]:
            assert [row['id'] for row in response['items']]==[ID,ID]
            assert [row['playback_context'] for row in response['items']]==[
                {'playlist':PLAYLIST,'index':0},{'playlist':PLAYLIST,'index':6}]
