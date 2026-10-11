#define main existing_provider_regression
#include "test_native.c"
#undef main
int main(void) {
    char dir[]="/tmp/tube-actions-XXXXXX",actions[3][160];
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t *p=&evo_provider_youtube;assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->search("fixture",0,items_cb,NULL);reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"First\"},{\"id\":\"bcdefghijkl\",\"title\":\"Second\"},{\"id\":\"cdefghijklm\",\"title\":\"Third\"}]}}");
    assert(tube_toggle_queue("abcdefghijk")==1 && tube_toggle_queue("bcdefghijkl")==1);
    p->list_catalog("section:music",0,items_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"cdefghijklm\",\"title\":\"Third\"}],\"has_more\":false}}");
    p->list_catalog("account:menu:cdefghijklm",0,items_cb,NULL);
    assert(last_count==5);
    for(int i=0;i<3;++i)snprintf(actions[i],sizeof actions[i],"%s",last_items[i+1].id);
    p->list_catalog(actions[0],0,items_cb,NULL);assert(last_ok && !pending_count);
    cJSON *store=read_store();
    assert(cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetArrayItem(cJSON_GetObjectItem(store,"records"),2),"queued_music")));
    cJSON_Delete(store);
    p->list_catalog("queue",0,items_cb,NULL);
    assert(last_count==3 && !strcmp(last_items[0].id,"cdefghijklm") && !strcmp(last_items[1].id,"abcdefghijk"));
    assert(tube_queue_play_next("bcdefghijkl")==1);
    p->list_catalog("queue",0,items_cb,NULL);
    assert(last_count==3 && !strcmp(last_items[0].id,"bcdefghijkl") && !strcmp(last_items[1].id,"cdefghijklm"));
    assert(tube_toggle_favorite("cdefghijklm")==1);
    p->list_catalog(actions[1],0,items_cb,NULL);assert(last_ok);
    p->list_catalog("history",0,items_cb,NULL);
    assert(last_count==1 && last_items[0].played && !last_items[0].resume_pos_sec);
    p->list_catalog(actions[2],0,items_cb,NULL);assert(last_ok);
    p->list_catalog("history",0,items_cb,NULL);assert(!last_count);
    p->list_catalog("favorites",0,items_cb,NULL);assert(last_count==1);
    p->list_catalog("queue",0,items_cb,NULL);assert(last_count==3);
    p->shutdown();assert(!p->init());
    p->list_catalog("queue",0,items_cb,NULL);assert(last_count==3 && !strcmp(last_items[0].id,"bcdefghijkl"));
    p->list_catalog(actions[1],0,items_cb,NULL);assert(!last_ok && !pending_count);
    p->list_catalog("account:menu:cdefghijklm",0,items_cb,NULL);
    snprintf(actions[1],sizeof actions[1],"%s",last_items[2].id);
    p->list_catalog("library:profile:other",0,items_cb,NULL);
    p->list_catalog(actions[1],0,items_cb,NULL);assert(!last_ok && !pending_count);
    p->list_catalog("history",0,items_cb,NULL);assert(!last_count);
    assert(tube_queue_play_next("bad")==-1 && tube_mark_watched("abcdefghijk")==-1);
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: local video actions, queue priority, history isolation, persistence and stale menu rejection");
}
