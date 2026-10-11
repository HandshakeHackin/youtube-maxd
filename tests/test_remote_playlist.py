import asyncio
import httpx
import pytest
from prosperotube.models import ServiceError
from prosperotube.remote_lounge import Lounge, Receiver, PlayerState

VIDEO='00000000001'
PLAYLIST='PLsyntheticqueue'

def seed(lounge):
    receiver=Receiver('0'*32,'device','PS5',profile='account',aid=0)
    lounge.receivers[receiver.session]=receiver
    lounge.accept(receiver, [[[1,['setPlaylist',dict(videoId=VIDEO,listId=PLAYLIST,currentIndex=101,ctt='click')]]]])
    receiver.queue.clear();receiver.ack=receiver.sequence
    receiver.last_player=PlayerState(video_id=VIDEO,playlist=PLAYLIST,index=101,ctt='click',position_sec=3.5).model_dump()
    return receiver

def window():
    return dict(api_version=1,playlist=PLAYLIST,index=101,video_id=VIDEO,window_start=100,
                video_ids=['00000000000',VIDEO,'00000000002'])

def test_selected_profile_expansion_after_confirmation_and_current_position():
    async def run():
        entered=asyncio.Event();release=asyncio.Event();calls=[]
        async def load(request):
            calls.append(request);entered.set();await release.wait();return window()
        lounge=Lounge(httpx.AsyncClient(),playlist_loader=load);receiver=seed(lounge)
        try:
            receiver.ack=0;lounge.schedule_playlist(receiver,receiver.last_player);assert receiver.playlist_task is None
            receiver.ack=1;lounge.schedule_playlist(receiver,receiver.last_player);task=receiver.playlist_task
            await entered.wait();lounge.schedule_playlist(receiver,receiver.last_player);assert receiver.playlist_task is task
            assert calls==[dict(action='remote_playlist',profile='account',id=VIDEO,playlist=PLAYLIST,index=101)]
            receiver.last_player={**receiver.last_player,'position_sec':8.75}
            release.set();await task
            update=receiver.queue[0]
            assert update['action']=='updatePlaylist' and update['sequence']==2
            assert update['window_start']==100 and update['index']==101 and update['ctt']=='click'
            assert update['position_sec']==8.75 and update['video_ids']==window()['video_ids']
            assert receiver.playlist_anchor['expanded'] and not receiver.full_playlist
            receiver.queue.clear();receiver.ack=2;receiver.playlist_retry=0
            lounge.schedule_playlist(receiver,receiver.last_player)
            assert receiver.playlist_task is None # Same confirmed index never reloads the window.
        finally:
            await lounge.close();await lounge.http.aclose()
        assert not lounge.playlist_tasks
    asyncio.run(run())

@pytest.mark.parametrize('retirement',['replacement','stop','failed','player_change','close'])
def test_delayed_expansion_cannot_revive_retired_or_changed_queue(retirement):
    async def run():
        entered=asyncio.Event();release=asyncio.Event()
        async def load(request):
            entered.set()
            try:await release.wait()
            except asyncio.CancelledError:pass # Simulate an already-completed uncancellable upstream response.
            return window()
        lounge=Lounge(httpx.AsyncClient(),playlist_loader=load);receiver=seed(lounge)
        lounge.schedule_playlist(receiver,receiver.last_player);task=receiver.playlist_task;await entered.wait()
        if retirement=='replacement':lounge.accept(receiver, [[[2,['setPlaylist',dict(videoId=VIDEO,listId='PLanotherqueue',ctt='new')]]]])
        elif retirement=='stop':lounge.accept(receiver, [[[2,['stopVideo',{}]]]])
        elif retirement=='failed':lounge.fail(receiver,'remote_upstream')
        elif retirement=='player_change':receiver.last_player={**receiver.last_player,'playlist':''}
        else:await lounge.close()
        release.set();await task
        assert not any(c['action']=='updatePlaylist' for c in receiver.queue)
        await lounge.close();await lounge.http.aclose();assert not lounge.playlist_tasks
    asyncio.run(run())

@pytest.mark.parametrize('change',[{'window_start':True},{'video_ids':['invalid']},{'index':102},{'playlist':'PLanotherqueue'},{'video_ids':[VIDEO]*129},{'api_version':2}])
def test_invalid_expansion_preserves_phone_queue_and_backs_off(change):
    async def run():
        async def load(request):return {**window(),**change}
        lounge=Lounge(httpx.AsyncClient(),playlist_loader=load);receiver=seed(lounge);anchor=receiver.playlist_anchor
        lounge.schedule_playlist(receiver,receiver.last_player);await receiver.playlist_task
        assert receiver.playlist_anchor is anchor and not receiver.queue and not receiver.error
        lounge.schedule_playlist(receiver,receiver.last_player);assert receiver.playlist_task is None
        await lounge.close();await lounge.http.aclose()
    asyncio.run(run())

def test_unavailable_library_does_not_break_phone_receiver():
    async def run():
        async def load(request):raise ServiceError('unknown_profile','Unavailable',404)
        lounge=Lounge(httpx.AsyncClient(),playlist_loader=load);receiver=seed(lounge)
        lounge.schedule_playlist(receiver,receiver.last_player);await receiver.playlist_task
        assert not receiver.error and receiver.playlist_anchor and not receiver.queue
        await lounge.close();await lounge.http.aclose()
    asyncio.run(run())


@pytest.mark.parametrize('successor', [False, True])
def test_ended_unavailable_anchor_gets_refill_or_explicit_terminal_window(successor):
    async def run():
        async def load(request):
            return dict(api_version=1, playlist=PLAYLIST, index=101, video_id=VIDEO,
                        window_start=101, video_ids=[VIDEO, '00000000002'] if successor else [VIDEO])
        lounge=Lounge(httpx.AsyncClient(),playlist_loader=load);receiver=seed(lounge)
        receiver.last_player.update(state=0,position_sec=0,has_previous=False,has_next=False)
        try:
            lounge.schedule_playlist(receiver,receiver.last_player);await receiver.playlist_task
            assert len(receiver.queue)==1
            update=receiver.queue[0]
            assert update['action']=='updatePlaylist' and update['index']==101
            assert update['video_ids']==([VIDEO,'00000000002'] if successor else [VIDEO])
            receiver.queue.clear();receiver.ack=2;receiver.playlist_retry=0
            lounge.schedule_playlist(receiver,receiver.last_player)
            assert receiver.playlist_task is None # Terminal confirmation cannot loop.
        finally:
            await lounge.close();await lounge.http.aclose()
    asyncio.run(run())


def test_ended_occurrence_refills_large_supplied_playlist_without_playing_flags():
    ids=[f'v{i:010d}' for i in range(500)]
    receiver=Receiver('0'*32,'device','PS5',profile='account',aid=0)
    receiver.full_playlist=dict(ids=ids,base=0,playlist=PLAYLIST,ctt='click',window_start=0,window_end=127)
    player=PlayerState(video_id=ids[127],playlist=PLAYLIST,ctt='click',index=127,state=0).model_dump()
    Lounge.refresh_playlist_window(receiver,player)
    assert len(receiver.queue)==1
    update=receiver.queue[0]
    assert update['index']==127 and update['window_start']==95 and update['video_ids']==ids[95:223]
    Lounge.refresh_playlist_window(receiver,player)
    assert len(receiver.queue)==1


def test_finished_occurrence_rechecks_once_after_playing_boundary_probe():
    async def run():
        calls=[]
        async def load(request):
            calls.append(request)
            return dict(api_version=1,playlist=PLAYLIST,index=101,video_id=VIDEO,
                        window_start=101,video_ids=[VIDEO])
        lounge=Lounge(httpx.AsyncClient(),playlist_loader=load);receiver=seed(lounge)
        receiver.last_player.update(state=1)
        try:
            lounge.schedule_playlist(receiver,receiver.last_player);await receiver.playlist_task
            assert not receiver.queue and len(calls)==1
            receiver.playlist_retry=0
            lounge.schedule_playlist(receiver,receiver.last_player);assert receiver.playlist_task is None
            receiver.last_player.update(state=0,position_sec=300,duration_sec=300)
            lounge.schedule_playlist(receiver,receiver.last_player);await receiver.playlist_task
            assert len(calls)==2 and len(receiver.queue)==1
            assert receiver.queue[0]['position_sec']==300
            receiver.queue.clear();receiver.ack=receiver.sequence;receiver.playlist_retry=0
            lounge.schedule_playlist(receiver,receiver.last_player);assert receiver.playlist_task is None
        finally:
            await lounge.close();await lounge.http.aclose()
    asyncio.run(run())

def test_radio_refill_preserves_previous_items_while_accepting_new_future():
    async def run():
        radio='RDsyntheticqueue'
        async def load(request):return {**window(), 'playlist':radio,'window_start':101,'video_ids':[VIDEO,'00000000002'],'radio_rebased':True}
        lounge=Lounge(httpx.AsyncClient(),playlist_loader=load);receiver=seed(lounge)
        receiver.playlist_anchor.update(playlist=radio,window_start=100,video_ids=['00000000000',VIDEO,'00000000003'])
        receiver.last_player={**receiver.last_player,'playlist':radio}
        lounge.schedule_playlist(receiver,receiver.last_player);await receiver.playlist_task
        update=receiver.queue[0]
        assert update['window_start']==100 and update['index']==101
        assert update['video_ids']==['00000000000',VIDEO,'00000000002']
        await lounge.close();await lounge.http.aclose()
    asyncio.run(run())

def test_sparse_expansion_preserves_occurrences_and_refills_from_sparse_anchor():
    async def run():
        calls=[]
        async def load(request):
            calls.append(request)
            return {**window(), 'video_indices':[100,101,105]}
        lounge=Lounge(httpx.AsyncClient(),playlist_loader=load);receiver=seed(lounge)
        lounge.schedule_playlist(receiver,receiver.last_player);await receiver.playlist_task
        update=receiver.queue[0]
        assert update['video_indices']==[100,101,105] and update['index']==101
        receiver.queue.clear();receiver.ack=2;receiver.playlist_retry=0
        receiver.last_player.update(index=105,video_id='00000000002')
        lounge.schedule_playlist(receiver,receiver.last_player)
        assert receiver.playlist_task is not None
        await receiver.playlist_task
        assert calls[-1]['index']==105
        await lounge.close();await lounge.http.aclose()
    asyncio.run(run())

@pytest.mark.parametrize('indices',[None,[100,101],[100,101,101],[100,True,105],[100,102,105],[99,101,105],[100,101,100001]])
def test_invalid_sparse_indices_preserve_existing_queue(indices):
    async def run():
        async def load(request):return {**window(), 'video_indices':indices}
        lounge=Lounge(httpx.AsyncClient(),playlist_loader=load);receiver=seed(lounge);anchor=receiver.playlist_anchor
        lounge.schedule_playlist(receiver,receiver.last_player);await receiver.playlist_task
        assert receiver.playlist_anchor is anchor and not receiver.queue and not receiver.error
        await lounge.close();await lounge.http.aclose()
    asyncio.run(run())
