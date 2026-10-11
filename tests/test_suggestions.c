#define main existing_provider_regression
#include "test_native.c"
#undef main
int main(void) {
    char dir[]="/tmp/tube-suggestions-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t *p=&evo_provider_youtube;assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->list_catalog("library:suggestions:abcdefghijk",0,items_cb,NULL);
    reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    assert(pending_count==1 && strstr(pending[0].body,"\"action\":\"suggestions\"") && strstr(pending[0].body,"\"id\":\"abcdefghijk\""));
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"bcdefghijkl\",\"playback_context\":{\"group\":\"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\",\"index\":0}}],\"has_more\":true,\"cursor\":\"related-next\"}}");
    assert(last_ok && last_count==1 && last_more && last_items[0].playback_scope);
    assert(!strcmp(last_items[0].playback_playlist,"group:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"));
    p->list_catalog("library:suggestions:abcdefghijk",1,items_cb,NULL);
    assert(strstr(pending[0].body,"\"action\":\"continue\"") && strstr(pending[0].body,"related-next"));
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[],\"has_more\":false}}");
    assert(last_ok && !last_count && !last_more);
    p->list_catalog("account:menu:bcdefghijkl",0,items_cb,NULL);
    int found=0;for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].id,"library:suggestions:bcdefghijkl"))found=1;
    assert(found);
    p->list_catalog("library:suggestions:invalid",0,items_cb,NULL);assert(!last_ok && !pending_count);
    p->list_catalog("library:suggestions:bcdefghijkl",1,items_cb,NULL);assert(!last_ok && !pending_count);
    p->list_catalog("library:suggestions:abcdefghijk",0,items_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"library:filter:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\",\"title\":\"From this channel\",\"is_folder\":true}]}}");
    assert(last_ok && last_count==1 && last_items[0].is_folder);
    char filter[160];snprintf(filter,sizeof filter,"%s",last_items[0].id);
    p->list_catalog(filter,0,items_cb,NULL);
    assert(strstr(pending[0].body,"\"action\":\"suggestions_filter\"") && strstr(pending[0].body,"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"));
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[],\"has_more\":true,\"cursor\":\"filter-next\"}}");
    p->list_catalog(filter,1,items_cb,NULL);assert(strstr(pending[0].body,"filter-next") && strstr(pending[0].body,"\"action\":\"continue\""));
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[],\"has_more\":false}}");
    p->list_catalog("library:filter:invalid",0,items_cb,NULL);assert(!last_ok && !pending_count);
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: native related-video menu, scoped catalog requests, continuations and retained group identity");
}
