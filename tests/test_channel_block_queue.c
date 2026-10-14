#define main existing_provider_regression
#include "test_native.c"
#undef main
static void seed(void) {
    cJSON *root=cJSON_Parse("{\"version\":2,\"source\":\"http://127.0.0.1:8765\",\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\",\"records\":["
        "{\"id\":\"abcdefghijk\",\"profile\":\"guest\",\"queued\":true,\"queue_order\":1},"
        "{\"id\":\"bcdefghijkl\",\"profile\":\"guest\",\"queued\":true,\"queue_order\":2,\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"position\":17},"
        "{\"id\":\"cdefghijklm\",\"profile\":\"guest\",\"queued\":true,\"queue_order\":3}],"
        "\"blocked_channels\":[{\"profile\":\"guest\",\"channel\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"name\":\"Blocked fixture\"}]}");
    assert(root);write_store(root);cJSON_Delete(root);
}
static void preserved(void) {
    cJSON *root=read_store(),*row=cJSON_GetArrayItem(cJSON_GetObjectItem(root,"records"),1);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(row,"queued")) && !cJSON_IsTrue(cJSON_GetObjectItem(row,"played")));
    assert(cJSON_GetObjectItem(row,"position")->valuedouble==17);cJSON_Delete(root);
}
int main(void) {
    char dir[]="/tmp/tube-block-queue-XXXXXX";assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t *p=&evo_provider_youtube;evo_provider_item_t next;evo_stream_choice_t choice={0};
    seed();assert(!p->init());assert(tube_set_playback_mode(TUBE_MODE_PAUSE));
    p->stream_chosen("abcdefghijk",&choice);
    tube_next_offer_t offer;
    assert(tube_finish_drained_and_next(30) && tube_next_offer_hold() && tube_next_offer(&offer));
    assert(!strcmp(offer.item.id,"cdefghijklm") && tube_finish_drained_and_next(30));preserved();
    assert(tube_next_offer_input(1,0,0) && tube_next_offer_input(0,1,0));
    assert(!tube_next_offer(&offer) && !tube_take_next(&next));preserved();
    seed();assert(!p->init());p->stream_chosen("abcdefghijk",&choice);
    assert(tube_finish_drained_and_next(30) && tube_next_offer_hold() && tube_take_next(&next) && !strcmp(next.id,"cdefghijklm"));preserved();
    seed();assert(!p->init());p->stream_chosen("abcdefghijk",&choice);
    assert(tube_manual_navigation_begin(1) && tube_take_next(&next) && !strcmp(next.id,"cdefghijklm"));preserved();
    p->stream_chosen("cdefghijklm",&choice);
    assert(tube_manual_navigation_begin(-1) && tube_take_next(&next) && !strcmp(next.id,"abcdefghijk"));preserved();
    /* A selection made before a new block must not launch after it. */
    p->list_catalog("blocked-channels",0,items_cb,NULL);assert(last_count==1);
    char action[160];strcpy(action,last_items[0].id);p->list_catalog(action,0,items_cb,NULL);assert(last_ok);
    p->stream_chosen("abcdefghijk",&choice);assert(tube_manual_navigation_begin(1));
    p->list_catalog("account:menu:bcdefghijkl",0,items_cb,NULL);
    int found=0;for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].title,"Block channel")){strcpy(action,last_items[i].id);found=1;}
    assert(found);p->list_catalog(action,0,items_cb,NULL);assert(last_ok);
    assert(!tube_take_next(&next) && !tube_queue_retry_pending());preserved();
    assert(!pending_count);p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: blocked queue EOF/manual navigation, retained state and pending-selection rejection");
}
