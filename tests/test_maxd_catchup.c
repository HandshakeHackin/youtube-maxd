/* Exercise real async subscription scanning and append-to-queue ownership. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *p;
static void choose(const char *id) {assert(!p->list_catalog(id,0,items_cb,NULL) && last_ok);}
static void drain(void) {while(pending_count)reply(0,0,500,"{}");}
static void health(void) {while(pending_count && strstr(pending[0].url,"/v1/health"))reply(0,1,200,"{\"api_version\":1,\"features\":[\"search\",\"library\"]}");}
static const char *feed="{\"state\":\"done\",\"result\":{\"items\":["
 "{\"id\":\"abcdefghij1\",\"channel_id\":\"UCabcdefghijklmnopqrstuv\",\"title\":\"First upload\",\"duration_sec\":900},"
 "{\"id\":\"abcdefghij2\",\"channel_id\":\"UCabcdefghijklmnopqrstuv\",\"title\":\"Second upload\",\"duration_sec\":600},"
 "{\"id\":\"abcdefghij3\",\"channel_id\":\"UCabcdefghijklmnopqrstuv\",\"title\":\"Short upload\",\"duration_sec\":30,\"is_short\":true},"
 "{\"id\":\"abcdefghij4\",\"channel_id\":\"UCzyxwvutsrqponmlkjihgfe\",\"title\":\"Other channel\",\"duration_sec\":900},"
 "{\"id\":\"abcdefghij5\",\"channel_id\":\"UCabcdefghijklmnopqrstuv\",\"title\":\"Already watched\",\"duration_sec\":600,\"watch_percent\":100}]}}";
static void enqueue(void) {
    health();assert(!p->list_catalog("settings:smart:1:enqueue",0,items_cb,NULL));
    assert(pending_count && strstr(pending[0].body,"subscriptions"));poll_job(feed);health();assert(last_ok);
}
int main(void) {
    char dir[]="/tmp/maxd-catchup-XXXXXX";assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->search("seed",0,items_cb,NULL);reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[]}}");health();
    choose("settings:catchup:new:guest");choose("settings:smart:1:channel:UCabcdefghijklmnopqrstuv");
    enqueue();assert(last_count==2 && strstr(last_items[0].title,"Added 2"));
    choose("queue");assert(last_count==2 && !strcmp(last_items[0].id,"abcdefghij1") && !strcmp(last_items[1].id,"abcdefghij2"));
    enqueue();assert(strstr(last_items[0].title,"Added 0"));choose("queue");assert(last_count==2);
    choose("settings:smart:1:shorts");for(int i=0;i<5;++i)choose("settings:smart:1:budget"); /* no limit */
    enqueue();assert(strstr(last_items[0].title,"Added 1"));choose("queue");assert(last_count==3 && !strcmp(last_items[2].id,"abcdefghij3"));
    /* A profile switch while the scan is pending cannot enqueue into either account. */
    assert(!p->list_catalog("settings:smart:1:enqueue",0,items_cb,NULL));assert(pending_count);
    choose("library:profile:alice");drain();choose("queue");assert(!last_count);
    choose("library:profile:guest");choose("queue");assert(last_count==3);
    p->shutdown();drain();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: actual subscription catch-up scanning, channel/Shorts/watched filters, budget, duplicate-free queue append and cancelled ownership");
}
