#define main existing_provider_regression
#include "test_native.c"
#undef main
static const char *page="{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"History fixture\",\"history_remove_cursor\":\"private-history-reference\"}],\"has_more\":false}}";
static void confirmation(const evo_provider_t *p,char *apply) {
    p->list_catalog("account:menu:abcdefghijk",0,items_cb,NULL);
    int found=0;for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].id,"account:confirm:history_remove:abcdefghijk"))found=1;
    assert(found && !pending_count);
    p->list_catalog("account:confirm:history_remove:abcdefghijk",0,items_cb,NULL);
    assert(last_ok && last_count==1 && strstr(last_items[0].subtitle,"fixture"));
    assert(strlen(last_items[0].id)<80);
    memcpy(apply,last_items[0].id,strlen(last_items[0].id)+1);
}
int main(void) {
    char dir[]="/tmp/tube-history-remove-XXXXXX",apply[80];
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t *p=&evo_provider_youtube;assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->list_catalog("library:profile:fixture",0,items_cb,NULL);
    p->list_catalog("library:history",0,items_cb,NULL);
    reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");poll_job(page);
    assert(tube_mark_watched("abcdefghijk")==1 && tube_toggle_favorite("abcdefghijk")==1 && tube_toggle_queue("abcdefghijk")==1);
    cJSON *store=read_store();char *saved=cJSON_PrintUnformatted(store);
    assert(!strstr(saved,"private-history-reference"));free(saved);cJSON_Delete(store);
    confirmation(p,apply);
    p->list_catalog("history",0,items_cb,NULL);assert(last_count==1 && last_items[0].played && !pending_count);
    confirmation(p,apply);p->list_catalog(apply,0,items_cb,NULL);
    assert(pending_count==1 && strstr(pending[0].body,"private-history-reference") && strstr(pending[0].body,"history_remove"));
    poll_job("{\"state\":\"done\",\"result\":{\"api_version\":1,\"success\":true,\"video_id\":\"12345678901\"}}");
    assert(!last_ok);
    p->list_catalog("history",0,items_cb,NULL);assert(last_count==1 && last_items[0].played);
    p->list_catalog(apply,0,items_cb,NULL);assert(!pending_count);
    p->list_catalog("account:confirm:history_remove:abcdefghijk",0,items_cb,NULL);assert(!last_ok && !pending_count);
    p->list_catalog("library:history",0,items_cb,NULL);poll_job(page);
    confirmation(p,apply);p->list_catalog(apply,0,items_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"api_version\":1,\"success\":true,\"video_id\":\"abcdefghijk\"}}");assert(last_ok);
    p->list_catalog("history",0,items_cb,NULL);assert(!last_count);
    p->list_catalog("favorites",0,items_cb,NULL);assert(last_count==1);
    p->list_catalog("queue",0,items_cb,NULL);assert(last_count==1);
    p->list_catalog("library:history",0,items_cb,NULL);poll_job(page);confirmation(p,apply);
    p->list_catalog("library:profile:other",0,items_cb,NULL);
    p->list_catalog(apply,0,items_cb,NULL);assert(!last_ok && !pending_count);
    p->list_catalog("library:profile:fixture",0,items_cb,NULL);
    p->list_catalog("account:confirm:history_remove:abcdefghijk",0,items_cb,NULL);assert(!last_ok && !pending_count);
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: account history confirmation, one submission, identity checks, transient references and profile retirement");
}
