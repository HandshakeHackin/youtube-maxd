/* Explicit video navigation uses the shipping provider without EOF side effects. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static evo_stream_choice_t choice;
static void selected(int ok,const evo_stream_choice_t *choices,int count,void *ud) {
    (void)ud;assert(ok && count==1);choice=choices[0];
}
static const char *window="{\"state\":\"done\",\"result\":{\"api_version\":1,\"playlist\":\"PLabcdefghijk\",\"video_id\":\"abcdefghijk\",\"index\":1,\"window_start\":0,\"video_ids\":[\"bcdefghijkl\",\"abcdefghijk\",\"cdefghijklm\"],\"video_indices\":[0,1,2],\"playlist_size\":3}}";
int main(void) {
    char dir[]="/tmp/tube-manual-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t *p=&evo_provider_youtube;assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    assert(!p->list_catalog("playlist:PLabcdefghijk",0,items_cb,NULL));
    reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\",\"playback_context\":{\"playlist\":\"PLabcdefghijk\",\"index\":1}},{\"id\":\"cdefghijklm\"}]}}");
    unsigned scope=last_items[0].playback_scope;
    assert(tube_select_playlist_context("abcdefghijk","PLabcdefghijk",1,scope));
    assert(!p->resolve("abcdefghijk",selected,NULL));
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"duration_sec\":100,\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/cap/d0.mpd\",\"container\":\"dash\",\"video_codec\":\"avc\",\"audio_codec\":\"aac\"}]}}");
    p->stream_chosen("abcdefghijk",&choice);
    p->report_progress("abcdefghijk",37,100,EVO_PROVIDER_PLAY_STOP);
    evo_provider_item_t next;
    for(int mode=0;mode<=6;++mode) for(int direction=-1;direction<=1;direction+=2) {
        assert(tube_set_playback_mode(mode));p->stream_chosen("abcdefghijk",&choice);
        assert(tube_manual_navigation_begin(direction));
        assert(tube_manual_navigation_pending() && tube_manual_navigation_status()==0);
        assert(!tube_finish_drained_and_next(37));
        poll_job(window);
        assert(tube_manual_navigation_status()==1 && tube_take_next(&next));
        assert(next.playback_index==(direction<0?0:2) && next.playback_scope==scope);
        assert(!tube_manual_navigation_pending() && tube_playback_mode()==mode);
        assert(!tube_take_completion_action());
        cJSON *store=read_store(),*row=cJSON_GetArrayItem(cJSON_GetObjectItem(store,"records"),0);
        assert(!cJSON_IsTrue(cJSON_GetObjectItem(row,"played")));
        assert(cJSON_GetObjectItem(row,"position")->valuedouble==37);cJSON_Delete(store);
    }
    p->stream_chosen("abcdefghijk",&choice);
    assert(tube_manual_navigation_begin(1));tube_playlist_eof_cancel();
    reply(0,1,401,"{}");assert(!tube_manual_navigation_pending() && !tube_take_next(&next));
    assert(tube_set_playback_mode(TUBE_MODE_PAUSE));
    assert(tube_toggle_queue("abcdefghijk")==1 && tube_toggle_queue("cdefghijklm")==1);
    assert(tube_manual_navigation_begin(1) && !pending_count);
    assert(tube_manual_navigation_status()==1 && tube_take_next(&next));
    assert(!strcmp(next.id,"cdefghijklm"));
    cJSON *store=read_store(),*row=cJSON_GetArrayItem(cJSON_GetObjectItem(store,"records"),0);
    assert(!cJSON_IsTrue(cJSON_GetObjectItem(row,"queued")) && !cJSON_IsTrue(cJSON_GetObjectItem(row,"played")));
    assert(cJSON_GetObjectItem(row,"position")->valuedouble==37);cJSON_Delete(store);
    assert(tube_manual_navigation_begin(1));tube_cancel();
    assert(!tube_manual_navigation_pending() && !tube_take_next(&next));
    assert(!p->resolve("cdefghijklm",selected,NULL));
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"cdefghijklm\",\"duration_sec\":100,\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/cap/d0.mpd\",\"container\":\"dash\",\"video_codec\":\"avc\",\"audio_codec\":\"aac\"}]}}");
    p->stream_chosen("cdefghijklm",&choice);
    assert(tube_manual_navigation_begin(-1) && !pending_count && tube_take_next(&next));
    assert(!strcmp(next.id,"abcdefghijk") && next.resume_pos_sec==37);
    assert(tube_toggle_queue("abcdefghijk")==1 && tube_toggle_queue("abcdefghijk")==0);
    store=read_store();row=cJSON_GetArrayItem(cJSON_GetObjectItem(store,"records"),0);
    assert(cJSON_GetObjectItem(row,"queue_order")->valuedouble==0);cJSON_Delete(store);
    p->list_catalog("settings:clear-queue",0,items_cb,NULL);
    assert(!tube_manual_navigation_begin(-1) && !pending_count);
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: manual video direction across seven modes, resume preservation, queue priority and cancellation");
}
