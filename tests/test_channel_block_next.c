#define main existing_provider_regression
#include "test_native.c"
#undef main
int main(void) {
    char dir[]="/tmp/tube-block-next-XXXXXX";assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    cJSON *root=cJSON_Parse("{\"version\":2,\"source\":\"http://127.0.0.1:8765\",\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\",\"records\":[],\"blocked_channels\":[{\"profile\":\"guest\",\"channel\":\"UCaaaaaaaaaaaaaaaaaaaaaa\"}]}");
    write_store(root);cJSON_Delete(root);const evo_provider_t *p=&evo_provider_youtube;assert(!p->init());
    assert(tube_set_playback_mode(TUBE_MODE_ALL));evo_stream_choice_t choice={0};evo_provider_item_t next;
    const char *channels[]={"UCaaaaaaaaaaaaaaaaaaaaaa","UCbbbbbbbbbbbbbbbbbbbbbb","","invalid"};
    for(int i=0;i<4;++i) {
        p->stream_chosen("abcdefghijk",&choice);assert(!tube_finish_drained_and_next(30));
        assert(pending_count==1 && strstr(pending[0].body,"\"require_channel\":true"));
        root=cJSON_CreateObject();cJSON_AddStringToObject(root,"state","done");cJSON *result=cJSON_AddObjectToObject(root,"result");
        cJSON_AddNumberToObject(result,"api_version",1);cJSON_AddStringToObject(result,"video_id","abcdefghijk");
        cJSON_AddStringToObject(result,"next_video_id","bcdefghijkl");
        if(*channels[i])cJSON_AddStringToObject(result,"next_channel_id",channels[i]);
        char *body=cJSON_PrintUnformatted(root);poll_job(body);free(body);cJSON_Delete(root);
        if(i==1) {
            assert(tube_finish_drained_and_next(30) && tube_take_next(&next));
            assert(!strcmp(next.id,"bcdefghijkl") && !strcmp(next.parent_id,channels[i]));
        } else {
            assert(!tube_finish_drained_and_next(30) && !tube_take_next(&next) && !tube_playlist_eof_pending());
            assert(tube_take_completion_action()==TUBE_COMPLETION_PAUSE);
        }
        assert(!pending_count);
    }
    p->search("late block",0,items_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"bcdefghijkl\",\"channel_id\":\"UCbbbbbbbbbbbbbbbbbbbbbb\"}]}}");
    p->stream_chosen("abcdefghijk",&choice);assert(!tube_finish_drained_and_next(30));
    poll_job("{\"state\":\"done\",\"result\":{\"api_version\":1,\"video_id\":\"abcdefghijk\",\"next_video_id\":\"bcdefghijkl\",\"next_channel_id\":\"UCbbbbbbbbbbbbbbbbbbbbbb\"}}");
    assert(tube_finish_drained_and_next(30));
    p->list_catalog("account:menu:bcdefghijkl",0,items_cb,NULL);
    char action[160]={0};for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].title,"Block channel"))strcpy(action,last_items[i].id);
    assert(*action);p->list_catalog(action,0,items_cb,NULL);assert(last_ok && !tube_take_next(&next));
    p->list_catalog("library:profile:other",0,items_cb,NULL);assert(tube_set_playback_mode(TUBE_MODE_ALL));
    p->stream_chosen("abcdefghijk",&choice);assert(!tube_finish_drained_and_next(30));
    assert(strstr(pending[0].body,"\"require_channel\":false"));
    poll_job("{\"state\":\"done\",\"result\":{\"api_version\":1,\"video_id\":\"abcdefghijk\",\"next_video_id\":\"bcdefghijkl\"}}");
    assert(tube_finish_drained_and_next(30) && tube_take_next(&next));
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: recommendation channel verification, blocked/missing metadata rejection and profile isolation");
}
