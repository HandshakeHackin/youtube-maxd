/* Actual native request/job parser; no network or console. */
#define main provider_regression_main
#include "test_native.c"
#undef main
static evo_stream_choice_t choice;
static void selected(int ok,const evo_stream_choice_t *values,int count,void *ud) {
    (void)ud; assert(ok && count==1); choice=values[0];
}
static cJSON *window(void) {
    return cJSON_Parse("{\"api_version\":1,\"playlist\":\"PLabcdefghijk\",\"video_id\":\"abcdefghijk\",\"index\":9,\"window_start\":3,\"video_ids\":[\"abcdefghijk\",\"abcdefghijk\",\"bcdefghijkl\"],\"video_indices\":[3,9,20]}");
}
static void result(cJSON *value) {
    cJSON *root=cJSON_CreateObject();cJSON_AddStringToObject(root,"state","done");
    cJSON_AddItemToObject(root,"result",value);char *text=cJSON_PrintUnformatted(root);
    if (pending_count && !strcmp(pending[0].method,"GET")) reply(0,1,200,text);
    else poll_job(text);
    free(text);cJSON_Delete(root);
}
int main(void) {
    char dir[]="/tmp/tube-window-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t *p=&evo_provider_youtube;assert(!p->init());
    assert(!tube_playlist_window_begin() && tube_playlist_window_status(NULL)==-1);
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->list_catalog("library:playlist:PLabcdefghijk",0,items_cb,NULL);
    reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\",\"playback_context\":{\"playlist\":\"PLabcdefghijk\",\"index\":9}}]}}");
    assert(last_count==1 && last_items[0].playback_scope);
    unsigned scope=last_items[0].playback_scope;
    p->list_catalog("settings:captions",0,items_cb,NULL);
    assert(tube_select_playlist_context("abcdefghijk","PLabcdefghijk",9,scope));
    assert(!p->resolve("abcdefghijk",selected,NULL));
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/cap/d0.mpd\",\"container\":\"dash\",\"video_codec\":\"avc\",\"audio_codec\":\"aac\"}]}}");
    p->stream_chosen("abcdefghijk",&choice);
    assert(tube_playlist_window_begin());assert(!tube_playlist_window_begin());
    assert(tube_playlist_window_status(NULL)==0);
    assert(strstr(pending[0].body,"\"action\":\"playback_queue\"") && strstr(pending[0].body,"\"index\":9"));
    assert(strstr(pending[0].body,"\"profile\":\"guest\""));
    result(window());tube_playlist_window_t out;
    assert(tube_playlist_window_status(&out)==1 && out.count==3 && out.current==1);
    assert(out.indices[0]==3 && out.indices[2]==20 && !strcmp(out.ids[0],out.ids[1]));
    assert(out.scope==scope && out.selection==tube_phone_selection());
    for(int kind=0;kind<10;++kind) {
        cJSON *sized=window();
        const double sizes[]={21,20,0,-1,100001,100002,21.5,21,21,21};
        cJSON_AddNumberToObject(sized,"playlist_size",sizes[kind]);
        if(kind==7)cJSON_AddTrueToObject(sized,"is_infinite");
        if(kind==8)cJSON_ReplaceItemInObject(sized,"playlist_size",cJSON_CreateString("21"));
        if(kind==9)cJSON_AddStringToObject(sized,"is_infinite","false");
        assert(tube_playlist_window_begin());result(sized);
        assert(tube_playlist_window_status(&out)==1);
        assert(out.playlist_size==(kind==0?21:kind==4?100001:0));
    }
    assert(!tube_playlist_window_at(-1) && !tube_playlist_window_at(100001) && !pending_count);
    assert(tube_playlist_window_at(20));
    assert(strstr(pending[0].body,"\"action\":\"playback_at\"") && !strstr(pending[0].body,"\"id\":"));
    cJSON *target=window();cJSON_ReplaceItemInObject(target,"index",cJSON_CreateNumber(20));
    cJSON_ReplaceItemInObject(target,"video_id",cJSON_CreateString("bcdefghijkl"));result(target);
    assert(tube_playlist_window_status(&out)==1 && out.current==2 && !strcmp(out.ids[out.current],"bcdefghijkl"));
    char active[101];int active_index;
    assert(tube_playback_playlist_context(active,sizeof active,&active_index) && active_index==9);
    assert(tube_playlist_window_at(20));result(window());assert(tube_playlist_window_status(NULL)==-1);
    assert(tube_playlist_window_at(0));
    target=window();cJSON_ReplaceItemInObject(target,"index",cJSON_CreateNumber(0));
    cJSON_ReplaceItemInObject(target,"window_start",cJSON_CreateNumber(0));
    cJSON_ReplaceItemInArray(cJSON_GetObjectItem(target,"video_indices"),0,cJSON_CreateNumber(0));result(target);
    assert(tube_playlist_window_status(&out)==1 && out.current==0 && out.indices[0]==0);
    for(int bad=0;bad<12;++bad) {
        cJSON *value=window();
        switch(bad) {
        case 0:cJSON_ReplaceItemInObject(value,"playlist",cJSON_CreateString("PLdifferent11"));break;
        case 1:cJSON_ReplaceItemInObject(value,"index",cJSON_CreateNumber(8));break;
        case 2:cJSON_ReplaceItemInObject(value,"window_start",cJSON_CreateNumber(0));break;
        case 3:cJSON_ReplaceItemInArray(cJSON_GetObjectItem(value,"video_indices"),1,cJSON_CreateNumber(3));break;
        case 4:cJSON_ReplaceItemInArray(cJSON_GetObjectItem(value,"video_indices"),1,cJSON_CreateNumber(9.5));break;
        case 5:cJSON_ReplaceItemInArray(cJSON_GetObjectItem(value,"video_indices"),1,cJSON_CreateTrue());break;
        case 6:cJSON_ReplaceItemInArray(cJSON_GetObjectItem(value,"video_ids"),1,cJSON_CreateString("bcdefghijkl"));break;
        case 7:cJSON_DeleteItemFromArray(cJSON_GetObjectItem(value,"video_indices"),2);break;
        case 8:cJSON_ReplaceItemInObject(value,"video_ids",cJSON_CreateArray());break;
        case 9:cJSON_ReplaceItemInObject(value,"video_indices",cJSON_CreateNull());break;
        case 10:cJSON_ReplaceItemInArray(cJSON_GetObjectItem(value,"video_ids"),1,cJSON_CreateString("invalid"));break;
        case 11:
            cJSON_DeleteItemFromObject(value,"video_indices");
            for(int i=3;i<129;++i)cJSON_AddItemToArray(cJSON_GetObjectItem(value,"video_ids"),cJSON_CreateString("abcdefghijk"));
            break;
        }
        assert(tube_playlist_window_begin()); result(value);
        assert(tube_playlist_window_status(NULL)==-1 && !pending_count);
    }
    cJSON *dense=window();cJSON_DeleteItemFromObject(dense,"video_indices");
    cJSON_ReplaceItemInObject(dense,"window_start",cJSON_CreateNumber(8));
    assert(tube_playlist_window_begin());result(dense);
    assert(tube_playlist_window_status(&out)==1 && out.current==1 && out.indices[2]==10);
    tube_cancel();assert(tube_playlist_window_status(NULL)==-1);
    cJSON *full=window();cJSON_DeleteItemFromObject(full,"video_indices");
    cJSON_ReplaceItemInObject(full,"window_start",cJSON_CreateNumber(0));
    for(int i=3;i<128;++i)cJSON_AddItemToArray(cJSON_GetObjectItem(full,"video_ids"),cJSON_CreateString("abcdefghijk"));
    assert(tube_playlist_window_begin());result(full);
    assert(tube_playlist_window_status(&out)==1 && out.count==128 && out.current==9 && out.indices[127]==127);
    assert(tube_playlist_window_begin());
    reply(0,1,200,"{\"job\":\"old_window\",\"state\":\"pending\"}");
    usleep(300000);tube_poll();assert(pending_count==1);
    tube_playlist_window_cancel();assert(tube_playlist_window_begin());
    reply(1,1,200,"{\"job\":\"new_window\",\"state\":\"pending\"}");
    usleep(300000);tube_poll();assert(pending_count==2);
    cJSON *root=cJSON_CreateObject();cJSON_AddStringToObject(root,"state","done");
    cJSON_AddItemToObject(root,"result",window());char *text=cJSON_PrintUnformatted(root);
    reply(1,1,200,text);assert(tube_playlist_window_status(&out)==1 && out.current==1);
    reply(0,1,200,text);assert(tube_playlist_window_status(&out)==1 && out.current==1);
    free(text);cJSON_Delete(root); // retired completion cannot overwrite the newer window
    assert(tube_playlist_window_begin());tube_playlist_window_cancel();
    reply(0,1,401,"{\"error\":{\"message\":\"retired credentials\"}}");
    assert(!strstr(error_text,"retired credentials"));
    assert(tube_playlist_window_status(NULL)==-1);
    assert(tube_playlist_window_begin() && pending_count==1 && strstr(pending[0].url,"/v1/jobs"));
    p->stream_chosen("abcdefghijk",&choice);
    reply(0,1,200,"{\"job\":\"retired_selection\",\"state\":\"pending\"}");
    assert(tube_playlist_window_status(NULL)==-1); // same ID, distinct selection
    assert(tube_playlist_window_begin());result(window());
    p->list_catalog("settings:autoplay",0,items_cb,NULL);
    assert(tube_autoplay_enabled());
    assert(!tube_finish_drained_and_next(100) && tube_playlist_eof_pending());
    assert(!tube_finish_drained_and_next(100) && pending_count==1);
    result(window());assert(tube_finish_drained_and_next(100));
    evo_provider_item_t next;
    assert(tube_playlist_eof_pending() && tube_take_next(&next));
    assert(!strcmp(next.id,"bcdefghijkl") && next.playback_index==20 && next.playback_scope==scope);
    assert(!next.resume_pos_sec && !strcmp(next.playback_playlist,"PLabcdefghijk"));
    assert(!tube_take_next(&next) && !tube_playlist_eof_pending());
    assert(tube_set_playback_mode(TUBE_MODE_REVERSE));
    assert(tube_toggle_queue("bcdefghijkl"));
    p->stream_chosen("abcdefghijk",&choice);
    assert(!tube_finish_drained_and_next(100));result(window());
    assert(tube_finish_drained_and_next(100) && tube_take_next(&next));
    assert(!strcmp(next.id,"abcdefghijk") && next.playback_index==3);
    assert(!tube_toggle_queue("bcdefghijkl")); // reverse playlist precedes explicit queue
    p->stream_chosen("abcdefghijk",&choice);
    assert(!tube_finish_drained_and_next(100));
    cJSON *reverse_edge=window();
    cJSON_DeleteItemFromArray(cJSON_GetObjectItem(reverse_edge,"video_ids"),0);
    cJSON_DeleteItemFromArray(cJSON_GetObjectItem(reverse_edge,"video_indices"),0);
    cJSON_ReplaceItemInObject(reverse_edge,"window_start",cJSON_CreateNumber(9));result(reverse_edge);
    assert(!tube_finish_drained_and_next(100) && strstr(pending[0].body,"\"index\":8"));
    result(cJSON_Parse("{\"api_version\":1,\"playlist\":\"PLabcdefghijk\",\"video_id\":\"bcdefghijkl\",\"index\":8,\"window_start\":8,\"video_ids\":[\"bcdefghijkl\"]}"));
    assert(tube_finish_drained_and_next(100) && tube_take_next(&next) && next.playback_index==8);
    for(int pass=0;pass<2;++pass) {
        assert(tube_select_playlist_context("abcdefghijk","PLabcdefghijk",pass?9:0,scope));
        assert(!p->resolve("abcdefghijk",selected,NULL));
        poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/cap/d0.mpd\",\"container\":\"dash\",\"video_codec\":\"avc\",\"audio_codec\":\"aac\"}]}}");
        p->stream_chosen("abcdefghijk",&choice);
        if(!pass) {
            assert(tube_toggle_queue("bcdefghijkl"));
            assert(!tube_finish_drained_and_next(100) && !pending_count && !tube_take_next(&next));
            assert(!tube_toggle_queue("bcdefghijkl"));
        }
    }
    for(int fallback=0;fallback<3;++fallback) {
        assert(tube_set_playback_mode(TUBE_MODE_SHUFFLE));p->stream_chosen("abcdefghijk",&choice);
        assert(!tube_finish_drained_and_next(100));
        cJSON *shuffle=window();cJSON_AddNumberToObject(shuffle,"playlist_size",fallback==2?30:120);result(shuffle);
        if(fallback==2) {
            assert(tube_finish_drained_and_next(100) && tube_take_next(&next) && next.playback_index==20);
            continue;
        }
        assert(!tube_finish_drained_and_next(100) && pending_count==1);
        cJSON *request=cJSON_Parse(pending[0].body);
        int random_index=cJSON_GetObjectItem(cJSON_GetObjectItem(request,"library"),"index")->valueint;
        cJSON_Delete(request);assert(random_index>=0 && random_index<120 && random_index!=9);
        if(fallback) poll_job("{\"state\":\"failed\",\"error\":{\"code\":\"remote_playlist_unavailable\",\"message\":\"Fixture unavailable target\"}}");
        else {
            cJSON *target_row=cJSON_Parse("{\"api_version\":1,\"playlist\":\"PLabcdefghijk\",\"video_id\":\"bcdefghijkl\",\"video_ids\":[\"bcdefghijkl\"]}");
            cJSON_AddNumberToObject(target_row,"index",random_index);cJSON_AddNumberToObject(target_row,"window_start",random_index);result(target_row);
        }
        assert(tube_finish_drained_and_next(100) && tube_take_next(&next));
        assert(next.playback_index==(fallback?20:random_index) && next.playback_scope==scope && !pending_count);
    }
    assert(tube_set_playback_mode(TUBE_MODE_ALL));
    p->stream_chosen("abcdefghijk",&choice);
    assert(!tube_finish_drained_and_next(100));
    cJSON *last=window();
    cJSON_DeleteItemFromArray(cJSON_GetObjectItem(last,"video_ids"),2);
    cJSON_DeleteItemFromArray(cJSON_GetObjectItem(last,"video_indices"),2);
    cJSON_AddNumberToObject(last,"playlist_size",10);result(last);
    assert(!tube_finish_drained_and_next(100) && !tube_playlist_eof_pending() && !pending_count);
    assert(!tube_take_next(&next));
    for(int action=0;action<4;++action) {
        p->stream_chosen("abcdefghijk",&choice);
        assert(!tube_finish_drained_and_next(100));
        cJSON *tail=window();
        cJSON_DeleteItemFromArray(cJSON_GetObjectItem(tail,"video_ids"),2);
        cJSON_DeleteItemFromArray(cJSON_GetObjectItem(tail,"video_indices"),2);
        if(!action)cJSON_AddNumberToObject(tail,"playlist_size",100);
        result(tail);assert(!tube_finish_drained_and_next(100) && tube_playlist_eof_pending());
        assert(pending_count==1 && strstr(pending[0].body,"\"action\":\"playback_at\"") && strstr(pending[0].body,"\"index\":10"));
        assert(!tube_finish_drained_and_next(100) && pending_count==1);
        cJSON *following=window();
        cJSON_ReplaceItemInObject(following,"index",cJSON_CreateNumber(action==3 ? 9 : 10));
        cJSON_ReplaceItemInObject(following,"video_id",cJSON_CreateString("bcdefghijkl"));
        cJSON_ReplaceItemInArray(cJSON_GetObjectItem(following,"video_indices"),2,cJSON_CreateNumber(10));
        if(action==1 || action==2) {
            reply(0,1,200,"{\"job\":\"retiring_edge\",\"state\":\"pending\"}");
            usleep(300000);tube_poll();assert(pending_count==1);
        }
        if(action==1)tube_playlist_eof_cancel();
        if(action==2)p->stream_chosen("abcdefghijk",&choice);
        result(following);
        if(!action) {
            assert(tube_finish_drained_and_next(100) && tube_take_next(&next));
            assert(!strcmp(next.id,"bcdefghijkl") && next.playback_index==10 && next.playback_scope==scope);
        } else if(action!=2) {
            assert(!tube_finish_drained_and_next(100) && !tube_take_next(&next) && !tube_playlist_eof_pending());
        } else {
            assert(tube_playlist_window_status(NULL)==-1 && !tube_take_next(&next) && !tube_playlist_eof_pending());
            tube_playlist_eof_cancel();
        }
        assert(!pending_count);
    }
    p->stream_chosen("abcdefghijk",&choice);
    assert(!tube_finish_drained_and_next(100));tube_playlist_eof_cancel();
    reply(0,1,200,"{\"job\":\"retired_end\",\"state\":\"pending\"}");
    assert(!tube_finish_drained_and_next(100) && !tube_take_next(&next));
    p->stream_chosen("abcdefghijk",&choice);
    assert(tube_toggle_queue("abcdefghijk"));
    assert(tube_toggle_queue("bcdefghijkl"));
    assert(tube_finish_drained_and_next(100) && !pending_count);
    assert(tube_take_next(&next) && !strcmp(next.id,"bcdefghijkl") && !next.playback_scope);
    assert(!tube_toggle_queue("bcdefghijkl"));
    p->stream_chosen("abcdefghijk",&choice);
    assert(!tube_finish_drained_and_next(100) && tube_playlist_eof_pending());
    tube_playlist_eof_cancel();
    reply(0,1,200,"{\"job\":\"retired_mode\",\"state\":\"pending\"}");
    assert(tube_set_playback_mode(TUBE_MODE_PAUSE));
    assert(!tube_queue_respects_playback_mode());
    assert(tube_toggle_queue("bcdefghijkl"));
    p->stream_chosen("abcdefghijk",&choice);
    assert(tube_finish_drained_and_next(100) && !pending_count);
    assert(tube_queue_retry_pending() && tube_take_next(&next));
    assert(!strcmp(next.id,"bcdefghijkl") && !next.playback_scope);
    p->list_catalog("settings:queue-respects-mode",0,items_cb,NULL);assert(last_ok);
    assert(tube_queue_respects_playback_mode());
    p->stream_chosen("abcdefghijk",&choice);
    assert(!tube_finish_drained_and_next(100) && !pending_count && !tube_take_next(&next));
    assert(tube_set_playback_mode(TUBE_MODE_CLOSE));
    p->stream_chosen("abcdefghijk",&choice);
    assert(tube_finish_drained_and_next(100) && !pending_count && !tube_take_next(&next));
    assert(!tube_finish_drained_and_next(100)); // close is consumed once
    p->list_catalog("settings:queue-respects-mode",0,items_cb,NULL);assert(last_ok);
    p->stream_chosen("abcdefghijk",&choice);
    assert(tube_finish_drained_and_next(100) && tube_take_next(&next));
    assert(!strcmp(next.id,"bcdefghijkl"));
    assert(!tube_toggle_queue("bcdefghijkl"));
    p->stream_chosen("abcdefghijk",&choice);
    assert(tube_finish_drained_and_next(100) && !pending_count && !tube_take_next(&next));
    assert(tube_set_playback_mode(TUBE_MODE_ALL));
    p->stream_chosen("abcdefghijk",&choice);
    assert(!tube_finish_drained_and_next(100) && tube_playlist_eof_pending());
    p->list_catalog("library:profile:other",0,items_cb,NULL);
    reply(0,1,200,"{\"job\":\"retired_profile\",\"state\":\"pending\"}");
    assert(!tube_playlist_eof_pending() && !tube_take_next(&next) && !pending_count);
    assert(tube_playlist_window_status(NULL)==-1 && !tube_playlist_window_begin());
    p->list_catalog("library:profile:guest",0,items_cb,NULL);
    assert(tube_set_playback_mode(TUBE_MODE_ONE));
    p->stream_chosen("abcdefghijk",&choice);
    assert(tube_toggle_queue("abcdefghijk"));
    assert(!tube_finish_drained_and_next(100) && !pending_count && !tube_take_next(&next));
    assert(tube_take_completion_action()==TUBE_COMPLETION_REPEAT);
    assert(!tube_take_completion_action() && !tube_finish_drained_and_next(100));
    tube_completion_playback_state(0,1); // demux seek has not retired yet
    assert(!tube_finish_drained_and_next(100) && !tube_take_completion_action());
    tube_completion_playback_state(0,0);
    assert(!tube_finish_drained_and_next(100));
    assert(tube_take_completion_action()==TUBE_COMPLETION_REPEAT);
    assert(!tube_toggle_queue("abcdefghijk")); // repeating preserves the current queue entry
    assert(tube_set_playback_mode(TUBE_MODE_PAUSE));p->stream_chosen("abcdefghijk",&choice);
    assert(!tube_finish_drained_and_next(100));assert(tube_take_completion_action()==TUBE_COMPLETION_PAUSE);
    assert(!tube_finish_drained_and_next(100) && !tube_take_completion_action());
    tube_completion_playback_state(0,0); // manual replay rearms completion
    assert(!tube_finish_drained_and_next(100));assert(tube_take_completion_action()==TUBE_COMPLETION_PAUSE);
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: native playlist windows, sparse repeats, malformed replies, cancellation and selection/profile retirement");
    return 0;
}
