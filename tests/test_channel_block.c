#define main existing_provider_regression
#include "test_native.c"
#undef main
static const char *page="{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"First\",\"subtitle\":\"Fixture channel\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\"},{\"id\":\"bcdefghijkl\",\"title\":\"Second\",\"channel_id\":\"UCbbbbbbbbbbbbbbbbbbbbbb\"}],\"has_more\":true}}";
int main(void) {
    char dir[]="/tmp/tube-block-XXXXXX",block[160],unblock[160];
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t *p=&evo_provider_youtube;assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->search("fixture",0,items_cb,NULL);reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");poll_job(page);
    assert(last_count==2 && last_more);
    assert(tube_toggle_favorite("abcdefghijk")==1 && tube_toggle_queue("abcdefghijk")==1);
    p->list_catalog("account:menu:abcdefghijk",0,items_cb,NULL);
    int found=0;for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].title,"Block channel")){strcpy(block,last_items[i].id);found=1;}
    assert(found);p->list_catalog(block,0,items_cb,NULL);assert(last_ok && !pending_count);
    p->list_catalog(block,0,items_cb,NULL);assert(last_ok); /* Repeated Block never becomes Unblock. */
    p->search("fixture",0,items_cb,NULL);poll_job(page);assert(last_count==1 && !strcmp(last_items[0].id,"bcdefghijkl") && last_more);
    p->list_catalog("favorites",0,items_cb,NULL);assert(!last_count);
    p->list_catalog("blocked-channels",0,items_cb,NULL);assert(last_count==1 && !strcmp(last_items[0].title,"Fixture channel"));strcpy(unblock,last_items[0].id);
    p->list_catalog("library:profile:other",0,items_cb,NULL);
    p->list_catalog(unblock,0,items_cb,NULL);assert(!last_ok);
    p->list_catalog("blocked-channels",0,items_cb,NULL);assert(last_ok && !last_count);
    p->search("fixture",0,items_cb,NULL);poll_job(page);assert(last_count==2);
    p->list_catalog("library:profile:guest",0,items_cb,NULL);
    p->shutdown();assert(!p->init());
    p->list_catalog("blocked-channels",0,items_cb,NULL);assert(last_count==1);strcpy(unblock,last_items[0].id);
    p->list_catalog(unblock,0,items_cb,NULL);assert(last_ok && !last_count);
    p->list_catalog("favorites",0,items_cb,NULL);assert(last_count==1);
    p->list_catalog("queue",0,items_cb,NULL);assert(last_count==1);
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: channel block persistence, catalog filtering, pagination, profile isolation and reversible local visibility");
}
