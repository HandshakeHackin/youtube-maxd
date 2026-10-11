#define main existing_provider_regression
#include "test_native.c"
#undef main
#include "tube_channel_name.h"
static void block_video(const evo_provider_t *p,const char *id) {
    char menu[80];snprintf(menu,sizeof menu,"account:menu:%s",id);p->list_catalog(menu,0,items_cb,NULL);
    char action[160]={0};for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].title,"Block channel"))strcpy(action,last_items[i].id);
    assert(*action);p->list_catalog(action,0,items_cb,NULL);assert(last_ok);
}
int main(void) {
    assert(tube_channel_name_equal("ÉCOLE","école"));
    assert(tube_channel_name_equal("Σ","ς") && tube_channel_name_equal("I","ı") && tube_channel_name_equal("İ","i"));
    assert(tube_channel_name_equal("КАНАЛ","канал") && tube_channel_name_equal("𐐀","𐐨"));
    assert(!tube_channel_name_equal("ß","SS") && !tube_channel_name_equal("é","é"));
    assert(!tube_channel_name_equal("\xf0\x90","\xf0\x90") && !tube_channel_name_equal("",""));
    char dir[]="/tmp/tube-name-block-XXXXXX";assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t *p=&evo_provider_youtube;assert(!p->init());assert(!p->set_source("http://127.0.0.1:8765#123456"));
    const char *page="{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\",\"subtitle\":\"ÉCOLE\"},{\"id\":\"bcdefghijkl\",\"subtitle\":\"école\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\"},{\"id\":\"cdefghijklm\",\"subtitle\":\"Other\"}]}}";
    p->search("names",0,items_cb,NULL);reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");poll_job(page);assert(last_count==3);
    assert(tube_toggle_queue("abcdefghijk")==1);block_video(p,"abcdefghijk");
    p->search("names",0,items_cb,NULL);poll_job(page);assert(last_count==1 && !strcmp(last_items[0].id,"cdefghijklm"));
    p->list_catalog("queue",0,items_cb,NULL);assert(!last_count);
    p->shutdown();assert(!p->init());p->list_catalog("blocked-channels",0,items_cb,NULL);assert(last_count==1 && !strcmp(last_items[0].title,"ÉCOLE"));
    block_video(p,"cdefghijklm");p->list_catalog("blocked-channels",0,items_cb,NULL);assert(last_count==2);
    char first[160],second[160];strcpy(first,last_items[0].id);strcpy(second,last_items[1].id);
    p->list_catalog(first,0,items_cb,NULL);assert(last_ok && last_count==1);
    p->list_catalog(second,0,items_cb,NULL);assert(!last_ok);
    p->list_catalog("queue",0,items_cb,NULL);assert(last_count==1);
    p->list_catalog("library:profile:other",0,items_cb,NULL);p->search("names",0,items_cb,NULL);poll_job(page);assert(last_count==3);
    assert(!pending_count);p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: Unicode simple-case channel names, name-only persistence, profile isolation and retired removal IDs");
}
