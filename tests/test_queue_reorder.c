#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *provider = &evo_provider_youtube;
static int action(const char *video, const char *title, char out[112]) {
    char menu[48]; snprintf(menu,sizeof menu,"account:menu:%s",video);
    provider->list_catalog(menu,0,items_cb,NULL); assert(last_ok && !pending_count);
    for (int i = 0; i < last_count; ++i) if (!strcmp(last_items[i].title,title)) {
        snprintf(out,112,"%s",last_items[i].id); return 1;
    }
    out[0] = 0; return 0;
}
static void order(const char *a,const char *b,const char *c,const char *d) {
    provider->list_catalog("queue",0,items_cb,NULL); assert(last_ok);
    const char *ids[]={a,b,c,d}; int count=0;
    while (count < 4 && ids[count]) { assert(!strcmp(last_items[count].id,ids[count])); ++count; }
    assert(last_count == count);
}
int main(void) {
    char dir[]="/tmp/tube-queue-reorder-XXXXXX", up[112],down[112],stale[112],block[112];
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    cJSON *store=cJSON_Parse("{\"version\":2,\"source\":\"http://127.0.0.1:8765\",\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\",\"records\":["
        "{\"id\":\"abcdefghijk\",\"profile\":\"guest\",\"queued\":true,\"queue_order\":1,\"position\":7,\"favorite\":true},"
        "{\"id\":\"bcdefghijkl\",\"profile\":\"guest\",\"queued\":true,\"queue_order\":2},"
        "{\"id\":\"cdefghijklm\",\"profile\":\"guest\",\"queued\":true,\"queue_order\":3},"
        "{\"id\":\"defghijklmn\",\"profile\":\"guest\",\"queued\":true,\"queue_order\":4,\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\"},"
        "{\"id\":\"efghijklmno\",\"profile\":\"other\",\"queued\":true,\"queue_order\":5}]}");
    assert(store); write_store(store); cJSON_Delete(store); assert(!provider->init());
    order("abcdefghijk","bcdefghijkl","cdefghijklm","defghijklmn");
    assert(action("bcdefghijkl","Move up",up));
    provider->list_catalog(up,1,items_cb,NULL); assert(!last_ok);
    provider->list_catalog(up,0,items_cb,NULL); assert(last_ok && !strcmp(last_items[0].title,"Moved up in queue"));
    order("bcdefghijkl","abcdefghijk","cdefghijklm","defghijklmn");
    provider->list_catalog(up,0,items_cb,NULL); assert(!last_ok);
    assert(!action("bcdefghijkl","Move up",up));
    assert(action("bcdefghijkl","Move down",down));
    provider->list_catalog(down,0,items_cb,NULL); assert(last_ok);
    order("abcdefghijk","bcdefghijkl","cdefghijklm","defghijklmn");
    assert(action("bcdefghijkl","Move down",stale));
    assert(!tube_toggle_queue("cdefghijklm"));
    provider->list_catalog(stale,0,items_cb,NULL); assert(!last_ok); // Adjacency changed after the menu was read.
    assert(tube_toggle_queue("cdefghijklm"));
    order("abcdefghijk","bcdefghijkl","defghijklmn","cdefghijklm");
    assert(action("bcdefghijkl","Move down",down));
    provider->list_catalog(down,0,items_cb,NULL); assert(last_ok);
    order("abcdefghijk","defghijklmn","bcdefghijkl","cdefghijklm");
    assert(action("defghijklmn","Block channel",block));
    provider->list_catalog(block,0,items_cb,NULL); assert(last_ok);
    order("abcdefghijk","bcdefghijkl","cdefghijklm",NULL);
    assert(action("bcdefghijkl","Move up",up));
    provider->list_catalog(up,0,items_cb,NULL); assert(last_ok);
    order("bcdefghijkl","abcdefghijk","cdefghijklm",NULL);
    assert(action("bcdefghijkl","Move down",stale));
    provider->shutdown(); assert(!provider->init());
    order("bcdefghijkl","abcdefghijk","cdefghijklm",NULL);
    provider->list_catalog(stale,0,items_cb,NULL); assert(!last_ok);
    assert(action("bcdefghijkl","Move down",stale));
    provider->list_catalog("library:profile:other",0,items_cb,NULL); assert(last_ok);
    provider->list_catalog(stale,0,items_cb,NULL); assert(!last_ok);
    order("efghijklmno",NULL,NULL,NULL);
    provider->list_catalog("library:profile:guest",0,items_cb,NULL); assert(last_ok);
    provider->shutdown();
    store=read_store(); cJSON *rows=cJSON_GetObjectItem(store,"records");
    assert(cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetArrayItem(rows,0),"favorite")));
    assert(cJSON_GetObjectItem(cJSON_GetArrayItem(rows,0),"position")->valuedouble == 7);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetArrayItem(rows,3),"queued")));
    for (int i=0;i<4;++i) cJSON_ReplaceItemInObject(cJSON_GetArrayItem(rows,i),"queue_order",cJSON_CreateNumber(0));
    write_store(store); cJSON_Delete(store); assert(!provider->init());
    order("abcdefghijk","bcdefghijkl","cdefghijklm",NULL);
    assert(action("bcdefghijkl","Move up",up));
    provider->list_catalog(up,0,items_cb,NULL); assert(last_ok);
    order("bcdefghijkl","abcdefghijk","cdefghijklm",NULL);
    provider->shutdown(); unlink(evo_data_path("youtube.json")); assert(!rmdir(dir));
    puts("PASS: queue adjacency, one-use actions, boundaries, blocked/profile isolation, persistence and tied legacy ranks");
}
