#define main existing_provider_regression
#include "test_native.c"
#undef main
static evo_stream_choice_t choice;
static void selected(int ok,const evo_stream_choice_t *values,int count,void *ud) {(void)ud;assert(ok && count==1);choice=values[0];}
static void window(int start,int count,int malformed,int selected_index) {
    const char *ids[]={"abcdefghijk","bcdefghijkl","cdefghijklm","defghijklmn"};
    cJSON *root=cJSON_CreateObject(),*value=cJSON_AddObjectToObject(root,"result");
    cJSON_AddStringToObject(root,"state","done");cJSON_AddNumberToObject(value,"api_version",1);
    cJSON_AddStringToObject(value,"playlist","PLabcdefghijk");cJSON_AddStringToObject(value,"video_id",ids[selected_index]);
    cJSON_AddNumberToObject(value,"index",selected_index);cJSON_AddNumberToObject(value,"window_start",start);cJSON_AddNumberToObject(value,"playlist_size",4);
    cJSON *videos=cJSON_AddArrayToObject(value,"video_ids"),*channels=cJSON_AddArrayToObject(value,"video_channels");
    for(int i=start;i<start+count;++i){cJSON_AddItemToArray(videos,cJSON_CreateString(ids[i]));
        if(!malformed)cJSON_AddItemToArray(channels,cJSON_CreateString(i==1 || i==2?"UCaaaaaaaaaaaaaaaaaaaaaa":""));}
    char *body=cJSON_PrintUnformatted(root);poll_job(body);free(body);cJSON_Delete(root);
}
int main(void) {
    char dir[]="/tmp/tube-block-window-XXXXXX";assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    cJSON *root=cJSON_Parse("{\"version\":2,\"source\":\"http://127.0.0.1:8765\",\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\",\"captions\":false,\"records\":[],\"blocked_channels\":[{\"profile\":\"guest\",\"channel\":\"UCaaaaaaaaaaaaaaaaaaaaaa\"}]}");
    write_store(root);cJSON_Delete(root);const evo_provider_t *p=&evo_provider_youtube;assert(!p->init());
    p->list_catalog("playlist:PLabcdefghijk",0,items_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\",\"playback_context\":{\"playlist\":\"PLabcdefghijk\",\"index\":0}}]}}");
    assert(tube_select_playlist_context("abcdefghijk","PLabcdefghijk",0,last_items[0].playback_scope));
    p->resolve("abcdefghijk",selected,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/cap/d0.mpd\",\"container\":\"dash\",\"video_codec\":\"avc\",\"audio_codec\":\"aac\"}]}}");
    evo_provider_item_t next;
    for(int split=0;split<2;++split) {
        p->stream_chosen("abcdefghijk",&choice);assert(tube_manual_navigation_begin(1));
        window(0,split?2:4,0,0);
        if(split){assert(tube_manual_navigation_status()==0 && pending_count==1 && strstr(pending[0].body,"\"index\":2"));window(2,2,0,2);}
        assert(tube_manual_navigation_status()==1 && tube_take_next(&next));
        assert(!strcmp(next.id,"defghijklmn") && next.playback_index==3 && !pending_count);
    }
    p->stream_chosen("abcdefghijk",&choice);assert(tube_playlist_window_begin());window(0,4,1,0);
    assert(tube_playlist_window_status(NULL)==-1 && !tube_take_next(&next));
    p->list_catalog("playlist:PLabcdefghijk",0,items_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"defghijklmn\",\"playback_context\":{\"playlist\":\"PLabcdefghijk\",\"index\":3}}]}}");
    assert(tube_select_playlist_context("defghijklmn","PLabcdefghijk",3,last_items[0].playback_scope));
    p->resolve("defghijklmn",selected,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"defghijklmn\",\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/cap/d0.mpd\",\"container\":\"dash\",\"video_codec\":\"avc\",\"audio_codec\":\"aac\"}]}}");
    p->stream_chosen("defghijklmn",&choice);assert(tube_manual_navigation_begin(-1));window(0,4,0,3);
    assert(tube_manual_navigation_status()==1 && tube_take_next(&next) && next.playback_index==0 && !strcmp(next.id,"abcdefghijk"));
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: blocked playlist occurrences, cross-window progress and malformed channel metadata rejection");
}
