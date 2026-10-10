/* Real C request/response path for TV section shelves and continuations. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
int main(void) {
    char dir[]="/tmp/tube-topics-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t *p=&evo_provider_youtube; assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->list_catalog("section:music",0,items_cb,NULL);
    reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    assert(strstr(pending[0].body,"\"action\":\"topics\"") && strstr(pending[0].body,"\"section\":\"music\""));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"section:music:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\",\"title\":\"Curated music\",\"is_folder\":true},{\"id\":\"section:evil:bad\",\"is_folder\":true}],\"has_more\":true,\"cursor\":\"bbbbbbbb-cccc-dddd-eeee-ffffffffffff\"}}");
    assert(last_ok && last_count==1 && last_more && last_items[0].is_folder);
    char row[160]; snprintf(row,sizeof row,"%s",last_items[0].id);
    p->list_catalog("section:music",1,items_cb,NULL);
    assert(strstr(pending[0].body,"\"action\":\"continue\"") && strstr(pending[0].body,"bbbbbbbb-cccc-dddd-eeee-ffffffffffff"));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":false}}");
    p->list_catalog("settings:captions",0,items_cb,NULL);
    p->list_catalog("settings:speed:175",0,items_cb,NULL);
    p->list_catalog(row,0,items_cb,NULL);
    assert(strstr(pending[0].body,"\"action\":\"topic_open\"") && strstr(pending[0].body,"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"Song\"}],\"has_more\":false}}");
    assert(last_count==1 && !last_items[0].is_folder);
    p->resolve("abcdefghijk",resolve_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/test/a.mpd\"}]}}");
    p->stream_chosen("abcdefghijk",NULL); assert(tube_playback_speed_for("abcdefghijk")==100);
    assert(tube_toggle_queue("abcdefghijk")==1);
    cJSON *saved=read_store();cJSON *records=cJSON_GetObjectItem(saved,"records");
    assert(cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetArrayItem(records,0),"queued_music")));cJSON_Delete(saved);
    p->list_catalog("section:news",0,items_cb,NULL);
    assert(strstr(pending[0].body,"\"section\":\"news\""));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":false}}");
    p->list_catalog("section:news",1,items_cb,NULL); assert(!last_ok && !pending_count);
    const char *extra[]={"sports","live","movies"};
    for(unsigned i=0;i<sizeof extra/sizeof *extra;i++) {
        char parent[40],needle[64];snprintf(parent,sizeof parent,"section:%s",extra[i]);
        snprintf(needle,sizeof needle,"\"section\":\"%s\"",extra[i]);
        p->list_catalog(parent,0,items_cb,NULL);assert(strstr(pending[0].body,needle));
        poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":false}}");
        assert(last_ok && last_count==0);
    }
    p->list_catalog("section:invalid",0,items_cb,NULL); assert(!last_ok && !pending_count);
    p->shutdown(); puts("PASS: native TV section routing, row IDs, pagination, invalid sections and nested Music playback/queue origin");
}
