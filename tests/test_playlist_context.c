/* Playlist occurrences survive asynchronous resolution, without leaking into history. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static evo_stream_choice_t choice;
static void selected(int ok,const evo_stream_choice_t *choices,int count,void *ud) {
    (void)ud; assert(ok && count==1); choice=choices[0];
}
static void finish(const evo_provider_t *p) {
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"duration_sec\":100,\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/cap/d0.mpd\",\"container\":\"dash\",\"video_codec\":\"avc\",\"audio_codec\":\"aac\"}]}}");
    p->stream_chosen("abcdefghijk",&choice);
}
int main(void) {
    char dir[]="/tmp/tube-playlist-XXXXXX",list[101]; int index;
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t *p=&evo_provider_youtube; assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    assert(!p->list_catalog("library:playlist:PLabcdefghijk",0,items_cb,NULL));
    reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"items\":["
        "{\"id\":\"abcdefghijk\",\"playback_context\":{\"playlist\":\"PLabcdefghijk\",\"index\":3}},"
        "{\"id\":\"abcdefghijk\",\"playback_context\":{\"playlist\":\"PLabcdefghijk\",\"index\":9}},"
        "{\"id\":\"abcdefghijk\",\"playback_context\":{\"playlist\":\"PLabcdefghijk\",\"index\":9}},"
        "{\"id\":\"bcdefghijkl\",\"playback_context\":{\"playlist\":\"PLabcdefghijk\",\"index\":1.5}}]}}");
    assert(last_ok && last_count==3);
    unsigned scope=last_items[0].playback_scope; assert(scope);
    assert(last_items[0].playback_index==3 && last_items[1].playback_index==9);
    assert(!last_items[2].playback_scope);
    p->list_catalog("settings:captions",0,items_cb,NULL);
    assert(tube_select_playlist_context("abcdefghijk","PLabcdefghijk",9,scope));
    assert(!p->resolve("abcdefghijk",selected,NULL));
    assert(tube_select_playlist_context("abcdefghijk","PLabcdefghijk",3,scope));
    finish(p);
    assert(tube_playback_playlist_context(list,sizeof list,&index) && index==9);
    assert(!strcmp(list,"PLabcdefghijk"));
    assert(!tube_playback_playlist_context(list,2,&index) && !*list && index==-1);
    tube_cancel();
    assert(!p->resolve("abcdefghijk",selected,NULL)); finish(p);
    assert(!tube_playback_playlist_context(list,sizeof list,&index));
    assert(tube_select_playlist_context("abcdefghijk","PLabcdefghijk",3,scope));
    assert(!p->resolve("abcdefghijk",selected,NULL)); finish(p);
    assert(tube_playback_playlist_context(list,sizeof list,&index) && index==3);
    assert(tube_queue_recovery(12,0,1));
    evo_provider_item_t item; double position; int subtitles;
    assert(tube_take_recovery(&item,&position,&subtitles));
    assert(tube_select_playlist_context("abcdefghijk","",0,0));
    assert(!p->resolve("abcdefghijk",selected,NULL)); finish(p); tube_recovery_finish();
    assert(tube_playback_playlist_context(list,sizeof list,&index) && index==3);
    p->list_catalog("history",0,items_cb,NULL);
    for(int i=0;i<last_count;++i) assert(!last_items[i].playback_scope && !*last_items[i].playback_playlist);
    assert(!tube_select_playlist_context("abcdefghijk","PLabcdefghijk",-1,scope));
    assert(!tube_select_playlist_context("abcdefghijk","PLabcdefghijk",100001,scope));
    assert(!tube_select_playlist_context("abcdefghijk","bad",1,scope));
    assert(!p->list_catalog("channel:UCaaaaaaaaaaaaaaaaaaaaaa",0,items_cb,NULL));
    poll_job("{\"state\":\"done\",\"result\":{\"items\":["
        "{\"id\":\"abcdefghijk\",\"playback_context\":{\"channel\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"index\":3}},"
        "{\"id\":\"abcdefghijk\",\"playback_context\":{\"channel\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"index\":9}},"
        "{\"id\":\"bcdefghijkl\",\"playback_context\":{\"channel\":\"UCbbbbbbbbbbbbbbbbbbbbbb\",\"index\":0}},"
        "{\"id\":\"cdefghijklm\",\"playback_context\":{\"channel\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"playlist\":\"PLabcdefghijk\",\"index\":0}},"
        "{\"id\":\"defghijklmn\",\"playback_context\":{\"playlist\":\"channel:UCaaaaaaaaaaaaaaaaaaaaaa\",\"index\":0}}]}}");
    assert(last_ok && last_count==5);
    assert(last_items[0].playback_scope==scope && last_items[1].playback_index==9);
    assert(!strcmp(last_items[0].playback_playlist,"channel:UCaaaaaaaaaaaaaaaaaaaaaa"));
    for(int i=2;i<5;++i) assert(!last_items[i].playback_scope);
    assert(tube_select_playlist_context("abcdefghijk",last_items[1].playback_playlist,9,scope));
    assert(!p->resolve("abcdefghijk",selected,NULL)); finish(p);
    assert(tube_playback_playlist_context(list,sizeof list,&index) && index==9);
    assert(!strcmp(list,"channel:UCaaaaaaaaaaaaaaaaaaaaaa"));
    assert(tube_playlist_window_begin());
    assert(pending_count==1 && strstr(pending[0].body,"channel_window") && strstr(pending[0].body,"UCaaaaaaaaaaaaaaaaaaaaaa"));
    poll_job("{\"state\":\"done\",\"result\":{\"api_version\":1,\"playlist\":\"channel:UCaaaaaaaaaaaaaaaaaaaaaa\",\"video_id\":\"abcdefghijk\",\"index\":9,\"anchor_video_id\":\"abcdefghijk\",\"anchor_index\":9,\"window_start\":8,\"video_ids\":[\"bcdefghijkl\",\"abcdefghijk\",\"cdefghijklm\"],\"video_indices\":[8,9,10],\"playlist_size\":11}}");
    tube_playlist_window_t window;
    assert(tube_playlist_window_status(&window)==1 && window.current==1 && window.playlist_size==11);
    assert(tube_playlist_window_at(10));
    /* A target response cannot silently rebase the original channel anchor. */
    poll_job("{\"state\":\"done\",\"result\":{\"api_version\":1,\"playlist\":\"channel:UCaaaaaaaaaaaaaaaaaaaaaa\",\"video_id\":\"cdefghijklm\",\"index\":10,\"anchor_video_id\":\"abcdefghijk\",\"anchor_index\":8,\"window_start\":10,\"video_ids\":[\"cdefghijklm\"],\"video_indices\":[10]}}");
    assert(tube_playlist_window_status(NULL)==-1);
    assert(tube_queue_recovery(12,0,1));
    assert(tube_take_recovery(&item,&position,&subtitles));
    assert(tube_select_playlist_context("abcdefghijk","",0,0));
    assert(!p->resolve("abcdefghijk",selected,NULL)); finish(p); tube_recovery_finish();
    assert(tube_playback_playlist_context(list,sizeof list,&index) && index==9);
    assert(!strcmp(list,"channel:UCaaaaaaaaaaaaaaaaaaaaaa"));
    for(int mode=0;mode<3;++mode) {
        int reverse=mode==1;
        assert(tube_set_playback_mode(mode==2?TUBE_MODE_SHUFFLE:reverse?TUBE_MODE_REVERSE:TUBE_MODE_ALL));
        p->stream_chosen("abcdefghijk",&choice);
        assert(!tube_finish_drained_and_next(100) && tube_playlist_eof_pending());
        assert(pending_count==1 && strstr(pending[0].body,"channel_window"));
        poll_job("{\"state\":\"done\",\"result\":{\"api_version\":1,\"playlist\":\"channel:UCaaaaaaaaaaaaaaaaaaaaaa\",\"video_id\":\"abcdefghijk\",\"index\":9,\"anchor_video_id\":\"abcdefghijk\",\"anchor_index\":9,\"window_start\":8,\"video_ids\":[\"bcdefghijkl\",\"abcdefghijk\",\"cdefghijklm\"],\"video_indices\":[8,9,10],\"playlist_size\":120}}");
        assert(tube_finish_drained_and_next(100) && tube_take_next(&item));
        assert(item.playback_index==(reverse?8:10) && item.playback_scope==scope);
        assert(!strcmp(item.playback_playlist,"channel:UCaaaaaaaaaaaaaaaaaaaaaa"));
    }
    assert(tube_set_playback_mode(TUBE_MODE_LIST));p->stream_chosen("abcdefghijk",&choice);
    assert(!tube_finish_drained_and_next(100) && !pending_count);
    assert(tube_take_completion_action()==TUBE_COMPLETION_PAUSE);
    assert(tube_toggle_queue("cdefghijklm")==1);p->stream_chosen("abcdefghijk",&choice);
    assert(tube_finish_drained_and_next(100) && tube_take_next(&item));
    assert(!strcmp(item.id,"cdefghijklm") && !pending_count);
    assert(tube_toggle_queue("cdefghijklm")==0);
    assert(tube_set_playback_mode(TUBE_MODE_ALL));
    assert(tube_select_playlist_context("abcdefghijk","channel:UCaaaaaaaaaaaaaaaaaaaaaa",10,scope));
    assert(!p->resolve("abcdefghijk",selected,NULL)); finish(p);
    assert(!tube_finish_drained_and_next(100));
    poll_job("{\"state\":\"done\",\"result\":{\"api_version\":1,\"playlist\":\"channel:UCaaaaaaaaaaaaaaaaaaaaaa\",\"video_id\":\"abcdefghijk\",\"index\":10,\"anchor_video_id\":\"abcdefghijk\",\"anchor_index\":10,\"window_start\":10,\"video_ids\":[\"abcdefghijk\"],\"video_indices\":[10],\"playlist_size\":11}}");
    assert(!tube_finish_drained_and_next(100));
    assert(pending_count==1 && strstr(pending[0].body,"playback_next") && !strstr(pending[0].body,"\"playlist\""));
    poll_job("{\"state\":\"done\",\"result\":{\"api_version\":1,\"video_id\":\"abcdefghijk\",\"next_video_id\":null}}");
    assert(!tube_finish_drained_and_next(100) && !tube_take_next(&item));
    for(int stale=0;stale<2;++stale) {
        p->stream_chosen("abcdefghijk",&choice);
        assert(!tube_finish_drained_and_next(100));
        poll_job("{\"state\":\"done\",\"result\":{\"api_version\":1,\"playlist\":\"channel:UCaaaaaaaaaaaaaaaaaaaaaa\",\"video_id\":\"abcdefghijk\",\"index\":10,\"anchor_video_id\":\"abcdefghijk\",\"anchor_index\":10,\"window_start\":10,\"video_ids\":[\"abcdefghijk\"],\"video_indices\":[10],\"playlist_size\":13}}");
        assert(!tube_finish_drained_and_next(100));
        assert(pending_count==1 && strstr(pending[0].body,"\"target\":11"));
        char response[600];
        snprintf(response,sizeof response,"{\"state\":\"done\",\"result\":{\"api_version\":1,\"playlist\":\"channel:UCaaaaaaaaaaaaaaaaaaaaaa\",\"anchor_video_id\":\"abcdefghijk\",\"anchor_index\":%d,\"unavailable_only\":true,\"requested_index\":11,\"skipped_unavailable\":2,\"playlist_size\":13,\"exhausted\":true}}",stale?9:10);
        poll_job(response);
        assert(!tube_finish_drained_and_next(100));
        if(!stale) {
            assert(pending_count==1 && strstr(pending[0].body,"playback_next"));
            poll_job("{\"state\":\"done\",\"result\":{\"api_version\":1,\"video_id\":\"abcdefghijk\",\"next_video_id\":null}}");
        } else assert(!pending_count && tube_playlist_window_status(NULL)==-1);
        assert(!tube_take_next(&item));
    }
    p->list_catalog("history",0,items_cb,NULL);
    for(int i=0;i<last_count;++i) assert(!last_items[i].playback_scope);
    p->shutdown(); assert(!p->init());
    assert(!tube_select_playlist_context("abcdefghijk","PLabcdefghijk",3,scope));
    assert(!tube_playback_playlist_context(list,sizeof list,&index));
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: duplicate playlist occurrences, request snapshots, recovery, bounds and stale scope");
    return 0;
}
