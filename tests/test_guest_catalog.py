"""Guest catalog routing keeps group identity and public video extraction separate."""
import asyncio
import pytest
from prosperotube.catalog import page_for
from prosperotube.extractor import Extractor, ProfileExtractors
from prosperotube.models import ServiceError

ID='abcdefghijk'
KEY='12345678-1234-1234-1234-123456789abc'
class Guest(Extractor):
    def __init__(self):
        super().__init__()
        self.calls=[]
    async def extract(self,target,**kwargs):
        self.calls.append((target,kwargs))
        return {'id':ID,'title':'Guest video'}
class Library:
    def __init__(self):self.calls=[]
    async def execute(self,request,operation='execute'):
        self.calls.append((request,operation))
        assert request['profile']=='guest' and request['action']=='catalog' and operation=='playback'
        continuation=bool(request.get('cursor'))
        return {'items':[{'id':ID,'title':'Result','watch_percent':65,'playback_context':{'group':KEY,'index':i}} for i in ([24] if continuation else range(24))],
                'has_more':not continuation,'cursor':'' if continuation else 'next-page'}

def test_guest_search_group_occurrences_and_continuation():
    async def run():
        guest,library=Guest(),Library()
        resolver=ProfileExtractors(guest,[],library).for_profile('guest')
        first=await page_for(resolver,query='sample')
        second=await page_for(resolver,query='sample',page=1)
        assert len(first.items)==24 and second.items[0].playback_context.index==24
        assert first.items[0].watch_percent==second.items[0].watch_percent==65
        assert first.items[0].playback_context.group==second.items[0].playback_context.group==KEY
        assert len(library.calls)==2 and not guest.calls
        assert (await page_for(resolver,query='sample',page=1))==second
        assert len(library.calls)==2
        result=await resolver.extract('https://www.youtube.com/watch?v='+ID,fresh=True)
        assert result['id']==ID and guest.calls[-1][1]=={'fresh':True}
        assert len(library.calls)==2
        with pytest.raises(ServiceError):await resolver._private({'action':'credentials'})
        assert len(library.calls)==2
    asyncio.run(run())

def test_without_library_preserves_original_guest_extractor():
    guest=Guest()
    assert ProfileExtractors(guest,[]).for_profile('guest') is guest

def test_guest_catalog_failure_does_not_silently_replace_source_group():
    class Failed(Library):
        async def execute(self,*args,**kwargs):raise ServiceError('upstream_failure','Unavailable',502)
    async def run():
        guest=Guest();resolver=ProfileExtractors(guest,[],Failed()).for_profile('guest')
        with pytest.raises(ServiceError):await page_for(resolver,query='sample')
        assert not guest.calls
    asyncio.run(run())


def test_cookie_catalogs_remain_profile_owned_and_invalidation_revokes_video(monkeypatch):
    calls=[]
    async def video_extract(self,target,**kwargs):
        self.ensure_active()
        calls.append((self.cookie_header,target,kwargs))
        return {'id':ID,'title':'Account video'}
    monkeypatch.setattr(Extractor,'extract',video_extract)
    class Accounts:
        def __init__(self):self.calls=[]
        async def execute(self,request,operation='execute'):
            self.calls.append((request,operation))
            assert request['profile'] in {'one','two'} and operation=='playback'
            assert request['action']=='catalog' and 'cookie' not in request
            return {'items':[{'id':ID,'playback_context':{'group':KEY,'index':0}}],'has_more':False,'cursor':''}
    async def run():
        library=Accounts();profiles=ProfileExtractors(Extractor(),[
            {'id':'one','cookie':'SESSION=one'},{'id':'two','cookie':'SESSION=two'}],library)
        first=profiles.for_profile('one');second=profiles.for_profile('two')
        for resolver in (first,second):
            result=await page_for(resolver,query='sample')
            assert result.items[0].playback_context.group==KEY
            await resolver.extract('https://www.youtube.com/watch?v='+ID,fresh=True)
        assert [entry[0]['profile'] for entry in library.calls]==['one','two']
        assert [entry[0] for entry in calls]==['SESSION=one','SESSION=two']
        underlying=first.video;profiles.invalidate_profile('one')
        assert first.revoked and underlying.revoked and not second.revoked
        with pytest.raises(ServiceError):await first.extract('https://www.youtube.com/watch?v='+ID)
        with pytest.raises(ServiceError):await underlying.extract('https://www.youtube.com/watch?v='+ID)
        assert len(calls)==2
    asyncio.run(run())
