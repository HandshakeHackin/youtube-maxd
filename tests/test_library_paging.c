/* Library lists page independently: a screen that lists two lists at once (Afterglow
 * Subscriptions: the feed and the channel strip; Home: its shelves) can still read the next
 * page of each, while a Classic list pages as before and the lists kept aside stay bounded. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *p;
static const char *features="{\"api_version\":1,\"features\":[\"search\",\"library\"]}";
#define PAGE(id,more,cursor) "{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"" id "\",\"title\":\"Video " id "\"}],\"has_more\":" more ",\"cursor\":\"" cursor "\"}}"
#define CHANNELS "{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"channel:UCaaaaaaaaaaaaaaaaaaaaaa\",\"title\":\"A channel\",\"is_folder\":true}],\"has_more\":true,\"cursor\":\"channelsNext\"}}"
static int direct(void) {return pending_count==1 && !strcmp(pending[0].method,"POST") && strstr(pending[0].url,"/v1/jobs")!=NULL;}
static const char *field(const char *key) {
    static char value[256];
    cJSON *job=cJSON_Parse(pending[0].body),*item=cJSON_GetObjectItem(cJSON_GetObjectItem(job,"library"),key);
    assert(job);snprintf(value,sizeof value,"%s",cJSON_IsString(item)?item->valuestring:"<absent>");
    cJSON_Delete(job);return value;
}
/* A list that asks for the feature list first (the channel list does) is answered it. */
static void settle(void) {while(pending_count==1 && strstr(pending[0].url,"/v1/health"))reply(0,1,200,features);}
/* The first page of a list, answered. */
static void first(const char *parent,const char *answer) {p->list_catalog(parent,0,items_cb,NULL);settle();assert(direct());poll_job(answer);assert(last_ok);}
/* A later page asks the service to continue with this cursor. */
static void next(const char *parent,int page,const char *cursor,const char *answer) {
    p->list_catalog(parent,page,items_cb,NULL);settle();
    assert(direct() && !strcmp(field("action"),"continue") && !strcmp(field("cursor"),cursor) && !strcmp(field("profile"),"fixture"));
    poll_job(answer);assert(last_ok);
}
static void refused(const char *parent,int page) {p->list_catalog(parent,page,items_cb,NULL);assert(!last_ok && !pending_count);}
int main(void) {
    char dir[]="/tmp/tube-library-paging-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->list_catalog("library:profile:fixture",0,items_cb,NULL);assert(last_ok);
    p->list_catalog("library:subscriptions",0,items_cb,NULL);
    /* Pairing and the feature list come first, in whatever order the provider asks. */
    for(int i=0;i<4 && pending_count==1 && !direct();++i) {
        if(strstr(pending[0].url,"/v1/pair"))reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
        else if(strstr(pending[0].url,"/v1/health"))reply(0,1,200,features);
        else {fprintf(stderr,"unexpected %s %s\n",pending[0].method,pending[0].url);abort();}
    }
    assert(direct() && !strcmp(field("action"),"subscriptions"));poll_job(PAGE("abcdefghij1","true","subsPage1"));assert(last_ok);
    /* Afterglow Subscriptions: the channel strip is listed after the feed's first page... */
    first("library:channels",CHANNELS);
    /* ...and the feed still reads on, page after page, and so does the strip. */
    next("library:subscriptions",1,"subsPage1",PAGE("abcdefghij2","true","subsPage2"));
    next("library:channels",1,"channelsNext",PAGE("channel:UCbbbbbbbbbbbbbbbbbbbbbb","false",""));
    next("library:subscriptions",2,"subsPage2",PAGE("abcdefghij3","false",""));
    refused("library:subscriptions",3); /* the feed has ended */
    refused("library:channels",2);
    /* Home with its shelves of other lists between its pages. */
    first("library:home",PAGE("bcdefghijk1","true","homePage1"));
    first("library:watch_later",PAGE("cdefghijkl1","true","laterPage1"));
    first("library:history",PAGE("defghijklm1","true","historyPage1"));
    next("library:home",1,"homePage1",PAGE("bcdefghijk2","false",""));
    next("library:watch_later",1,"laterPage1",PAGE("cdefghijkl2","false",""));
    /* Classic: one list paged on its own, as before; listing it again starts it afresh. */
    first("library:playlists",PAGE("PLfixture0001","true","playlistsPage1"));
    next("library:playlists",1,"playlistsPage1",PAGE("PLfixture0002","true","playlistsPage2"));
    first("library:playlists",PAGE("PLfixture0001","true","playlistsAgain"));
    next("library:playlists",1,"playlistsAgain",PAGE("PLfixture0002","false",""));
    /* Bounded: five other lists later, the first is forgotten and asks to be reloaded. */
    first("library:subscriptions",PAGE("abcdefghij1","true","subsAgain"));
    first("library:channels",CHANNELS);
    first("library:home",PAGE("bcdefghijk1","true","homeAgain"));
    first("library:watch_later",PAGE("cdefghijkl1","true","laterAgain"));
    first("library:history",PAGE("defghijklm1","true","historyThird"));
    first("library:playlists",PAGE("PLfixture0001","true","playlistsThird"));
    refused("library:subscriptions",1);
    next("library:home",1,"homeAgain",PAGE("bcdefghijk2","false",""));
    /* Another profile forgets every list kept aside. */
    p->list_catalog("library:profile:alice",0,items_cb,NULL);assert(last_ok);
    refused("library:watch_later",1);refused("library:playlists",1);
    p->shutdown();
    puts("PASS: library lists page independently, bounded, forgotten with the profile");
    return 0;
}
