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
        return {'items':[{'id':ID,'title':'Result','playback_context':{'group':KEY,'index':i}} for i in ([24] if continuation else range(24))],
                'has_more':not continuation,'cursor':'' if continuation else 'next-page'}

def test_guest_search_group_occurrences_and_continuation():
    async def run():
        guest,library=Guest(),Library()
        resolver=ProfileExtractors(guest,[],library).for_profile('guest')
        first=await page_for(resolver,query='sample')
        second=await page_for(resolver,query='sample',page=1)
        assert len(first.items)==24 and second.items[0].playback_context.index==24
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
