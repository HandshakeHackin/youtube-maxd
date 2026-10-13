/* Search options, library search routing and fallback, result cards and suggestions. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *p;
static char row[160];
static void show(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(last_ok && !pending_count);}
static void drain(void) {while(pending_count)reply(0,0,500,"{}");}
static int find(const char *title) {
    for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].title,title)){snprintf(row,sizeof row,"%s",last_items[i].id);return i;}
    return -1;
}
/* A field of the nested library request that is waiting to be sent. */
static const char *field(const char *key) {
    static char value[512];
    cJSON *job=cJSON_Parse(pending[0].body),*library=cJSON_GetObjectItem(job,"library"),*item=cJSON_GetObjectItem(library,key);
    assert(job && library && !strcmp(cJSON_GetObjectItem(job,"action")->valuestring,"library"));
    if(cJSON_IsArray(item)) {
        value[0]=0;cJSON *entry;
        cJSON_ArrayForEach(entry,item){strcat(value,entry->valuestring);strcat(value,";");}
    } else snprintf(value,sizeof value,"%s",cJSON_IsString(item)?item->valuestring:"<absent>");
    cJSON_Delete(job);return value;
}
static int plain(const char *query) {
    char needle[128];snprintf(needle,sizeof needle,"\"query\":\"%s\"",query);
    return pending_count==1 && strstr(pending[0].body,"\"action\":\"search\"") && !strstr(pending[0].body,"library") && strstr(pending[0].body,needle);
}
static int saved_cards(const char *profile) {
    cJSON *store=read_store(),*entry;int value=-1;
    cJSON_ArrayForEach(entry,cJSON_GetObjectItem(store,"profile_settings"))
        if(!strcmp(cJSON_GetObjectItem(entry,"profile")->valuestring,profile))value=cJSON_IsTrue(cJSON_GetObjectItem(entry,"search_channels_playlists"));
    cJSON_Delete(store);return value;
}
static const char *none="{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":false}}";
static const char *mixed="{\"state\":\"done\",\"result\":{\"api_version\":1,\"kind\":\"catalog\",\"items\":["
    "{\"kind\":\"video\",\"id\":\"abcdefghijk\",\"title\":\"Video hit\",\"subtitle\":\"Blocked soon\",\"channel_id\":\"UCbbbbbbbbbbbbbbbbbbbbbb\",\"is_folder\":false,"
    "\"playback_context\":{\"group\":\"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\",\"index\":0}},"
    "{\"kind\":\"channel\",\"id\":\"channel:UCaaaaaaaaaaaaaaaaaaaaaa\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"title\":\"Channel hit\",\"subtitle\":\"12K subscribers\",\"overview\":\"About\",\"art_url\":\"https://yt3.example/a.jpg\",\"is_folder\":true},"
    "{\"kind\":\"channel\",\"id\":\"channel:UCbbbbbbbbbbbbbbbbbbbbbb\",\"channel_id\":\"UCbbbbbbbbbbbbbbbbbbbbbb\",\"title\":\"Blocked soon\",\"is_folder\":true},"
    "{\"kind\":\"channel\",\"id\":\"channel:not-a-channel\",\"title\":\"Bad\",\"is_folder\":true},"
    "{\"kind\":\"playlist\",\"id\":\"library:playlist:PLfixture01\",\"title\":\"Playlist hit\",\"subtitle\":\"7 videos\",\"is_folder\":true}],"
    "\"has_more\":true,\"cursor\":\"cursor-one\"}}";
int main(void) {
    char dir[]="/tmp/tube-search-options-XXXXXX",again[160],suggestion[160];
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    SEPARATE_SETTINGS();
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    /* Default options keep the plain video search; a typed result leads with its two folders. */
    assert(!p->search("cats",0,items_cb,NULL));
    reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");assert(plain("cats"));
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"Hit\"}],\"has_more\":true}}");
    assert(last_ok && last_count==1 && last_more && search_lead==2);
    assert(!strcmp(lead_items[0].title,"Search options") && !strcmp(lead_items[0].subtitle,"No filters; videos only") && lead_items[0].is_folder);
    assert(!strcmp(lead_items[1].id,"search-suggest") && !strcmp(lead_items[1].title,"Suggestions"));
    assert(!p->search("cats",1,items_cb,NULL));assert(plain("cats") && strstr(pending[0].body,"\"page\":1"));
    poll_job(none);assert(last_ok && !search_lead);
    p->list_catalog("search:science",0,items_cb,NULL);assert(pending_count==1 && !strstr(pending[0].body,"library"));
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"Hit\"}],\"has_more\":false}}");
    assert(last_count==1 && !search_lead); /* a Discover topic is not a typed search */
    /* The options page cycles each filter and rejects unknown rows. */
    p->list_catalog("settings",1,items_cb,NULL);assert(last_ok && find("Search options")>=0 && !strcmp(row,"settings:search"));
    show("settings:search");assert(last_count==21 && find("Search again: cats")==0 && find("Suggestions")==1 && find("Play the first result: cats")==19);
    snprintf(again,sizeof again,"%s",last_items[0].id);
    assert(find("Upload date: Any time")==2 && find("Duration: Any duration")==3 && find("Type: All types")==4 && find("Sort by: Relevance")==5);
    assert(find("Live: any")==6 && find("Channels and playlists in results: off")==17 && find("Reset search filters")==18 && !strcmp(last_items[18].subtitle,"No filter is set"));
    show("settings:search:date");assert(find("Upload date: Today")==2);
    show("settings:search:date");assert(find("Upload date: This week")==2);
    show("settings:search:duration");assert(find("Duration: Under 3 minutes")==3);
    show("settings:search:feature:live");show("settings:search:feature:4k");assert(find("Live: required")==6 && find("4K: required")==7 && find("HDR: any")==8);
    show("settings:search:order");assert(find("Sort by: View count")==5 && !strcmp(last_items[18].subtitle,"Filters are set"));
    const char *bad[]={"settings:search:bogus","settings:search:feature:bogus","settings:search:feature:","settings:search:"};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;++i){p->list_catalog(bad[i],0,items_cb,NULL);assert(!last_ok && !pending_count);}
    p->list_catalog("settings:search:order",1,items_cb,NULL);assert(last_ok); /* a later page never repeats the action */
    show("settings:search");assert(find("Sort by: View count")==5);
    /* Non-default options use the library search with every option spelled out. */
    assert(!p->list_catalog(again,0,items_cb,NULL) && pending_count==1);
    assert(!strcmp(field("action"),"search") && !strcmp(field("query"),"cats") && !strcmp(field("profile"),"guest"));
    assert(!strcmp(field("upload_date"),"week") && !strcmp(field("duration"),"short") && !strcmp(field("type"),"all"));
    assert(!strcmp(field("order"),"view_count") && !strcmp(field("features"),"live;4k;") && !strcmp(field("cursor"),"<absent>"));
    assert(!strstr(pending[0].body,"\"page\""));
    poll_job(mixed);
    assert(last_ok && last_more && search_lead==2 && !strcmp(lead_items[0].subtitle,"This week, Under 3 minutes, Live, 4K, View count"));
    /* Channel and playlist cards are folders; a malformed channel id is dropped. */
    assert(last_count==4 && !last_items[0].is_folder && last_items[0].playback_scope && !strncmp(last_items[0].playback_playlist,"group:",6));
    assert(!strcmp(last_items[1].id,"channel:UCaaaaaaaaaaaaaaaaaaaaaa") && last_items[1].is_folder && !strcmp(last_items[1].title,"Channel hit"));
    assert(!strcmp(last_items[1].subtitle,"12K subscribers") && !strcmp(last_items[1].art_url,"https://yt3.example/a.jpg"));
    assert(!strcmp(last_items[3].id,"library:playlist:PLfixture01") && last_items[3].is_folder && !strcmp(last_items[3].title,"Playlist hit"));
    /* Later pages continue the retained feed: no query, no options, even after a change. */
    show("settings:search:type");
    assert(!p->list_catalog(again,1,items_cb,NULL) && pending_count==1);
    assert(!strcmp(field("action"),"continue") && !strcmp(field("cursor"),"cursor-one") && !strcmp(field("query"),"<absent>") && !strcmp(field("type"),"<absent>"));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"bcdefghijkl\",\"title\":\"Second page\"}],\"has_more\":false}}");
    assert(last_ok && last_count==1 && !search_lead && !last_more);
    p->list_catalog(again,2,items_cb,NULL);assert(!last_ok && !pending_count);
    /* A card opens as the existing channel folder; a blocked channel's card is hidden. */
    p->list_catalog("channel:UCaaaaaaaaaaaaaaaaaaaaaa",0,items_cb,NULL);NO_FEATURES();
    assert(pending_count==1 && strstr(pending[0].body,"\"parent\":\"channel:UCaaaaaaaaaaaaaaaaaaaaaa\""));drain();
    p->list_catalog(again,1,items_cb,NULL);assert(pending_count==1 && !strcmp(field("action"),"continue"));drain();
    p->list_catalog("library:home",0,items_cb,NULL);drain();
    p->list_catalog(again,1,items_cb,NULL);assert(!last_ok && !pending_count && strstr(error_text,"Search again")); /* another library screen took the cursors */
    show("account:menu:abcdefghijk");assert(find("Block channel")>=0);show(row);
    assert(!p->list_catalog(again,0,items_cb,NULL) && !strcmp(field("type"),"video"));poll_job(mixed);
    assert(last_count==2 && !strcmp(last_items[0].id,"channel:UCaaaaaaaaaaaaaaaaaaaaaa") && !strcmp(last_items[1].id,"library:playlist:PLfixture01"));
    /* A companion without the library worker: the same request falls back to the
     * plain search, later searches skip the attempt, a changed option asks again. */
    assert(!p->search("dogs",0,items_cb,NULL) && !strcmp(field("query"),"dogs"));
    poll_job("{\"state\":\"failed\",\"error\":{\"code\":\"library_unavailable\",\"message\":\"Enable the library worker on the companion first.\"}}");
    assert(plain("dogs") && strstr(pending[0].body,"\"page\":0") && !strcmp(pending[0].method,"POST"));
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"cdefghijklm\",\"title\":\"Plain\"}],\"has_more\":true}}");
    assert(last_ok && last_count==1 && search_lead==2 && strstr(lead_items[0].subtitle,"library worker is off"));
    assert(!p->search("dogs",1,items_cb,NULL));assert(plain("dogs") && strstr(pending[0].body,"\"page\":1"));drain();
    assert(!p->search("birds",0,items_cb,NULL));assert(plain("birds"));drain();
    show("settings:search");assert(strstr(last_items[18].subtitle,"library worker is off"));
    show("settings:search:order");assert(!strcmp(last_items[18].subtitle,"Filters are set"));
    assert(!p->search("birds",0,items_cb,NULL) && !strcmp(field("order"),"relevance") && !strcmp(field("type"),"video"));
    poll_job("{\"state\":\"failed\",\"error\":{\"code\":\"invalid_search\",\"message\":\"Unsupported search option.\"}}");
    assert(!last_ok && !pending_count && strstr(error_text,"Unsupported")); /* only a missing worker falls back */
    assert(!p->search("birds",0,items_cb,NULL) && !strcmp(field("action"),"search"));
    poll_job("{\"state\":\"failed\",\"error\":{\"code\":\"unknown_operation\",\"message\":\"This library operation is unavailable.\"}}");
    assert(plain("birds"));drain(); /* the offline fixture library */
    /* Reset returns to the plain search; mixed cards are a saved choice of the profile. */
    show("settings:search:reset");assert(find("Upload date: Any time")==2 && find("Type: All types")==4 && find("Live: any")==6);
    assert(!p->search("birds",0,items_cb,NULL));assert(plain("birds"));drain();
    assert(saved_cards("guest")==0);
    show("settings:search:cards");assert(find("Channels and playlists in results: on")==17 && saved_cards("guest")==1);
    assert(!p->search("birds",0,items_cb,NULL) && !strcmp(field("type"),"all") && !strcmp(field("features"),"") && !strcmp(field("upload_date"),"any"));
    poll_job(none);assert(search_lead==2 && !strcmp(lead_items[0].subtitle,"No filters; channels and playlists included"));
    show("settings:search:date");
    /* Another profile starts without filters or cards and cannot run this profile's rows. */
    show("library:profile:alice");
    p->list_catalog(again,0,items_cb,NULL);assert(!last_ok && !pending_count);
    show("settings:search");assert(last_count==18 && find("Upload date: Any time")==0 && find("Channels and playlists in results: off")==15);
    assert(!p->search("alice query",0,items_cb,NULL));assert(plain("alice query"));drain();
    show("settings:search:duration");assert(!p->search("alice query",0,items_cb,NULL) && !strcmp(field("duration"),"short") && !strcmp(field("profile"),"alice"));drain();
    show("library:profile:guest");
    assert(!p->search("guest query",0,items_cb,NULL) && !strcmp(field("duration"),"any") && !strcmp(field("upload_date"),"any"));drain(); /* filters never cross profiles */
    /* Suggestions complete the last typed query; a row runs that search. */
    p->list_catalog("search-suggest",0,items_cb,NULL);
    assert(pending_count==1 && !strcmp(field("action"),"search_suggestions") && !strcmp(field("query"),"guest query"));
    poll_job("{\"state\":\"done\",\"result\":{\"api_version\":1,\"kind\":\"search_suggestions\",\"query\":\"guest query\","
        "\"suggestions\":[\"guest query one\",\"Guest Query One\",\"\",7,\"bad\\u0001text\",\"   \",\"guest query two\"]}}");
    assert(last_ok && last_count==2 && !last_more && !strcmp(last_items[0].title,"guest query one") && !strcmp(last_items[1].title,"guest query two"));
    assert(last_items[1].is_folder && !strncmp(last_items[1].id,"search-suggest:run:",19));
    snprintf(suggestion,sizeof suggestion,"%s",last_items[1].id);
    p->list_catalog("search-suggest",1,items_cb,NULL);assert(!last_ok && !pending_count);
    assert(!p->list_catalog(suggestion,0,items_cb,NULL) && !strcmp(field("action"),"search") && !strcmp(field("query"),"guest query two"));
    poll_job(mixed);assert(last_ok && search_lead==2 && last_count==2);
    assert(!p->list_catalog(suggestion,1,items_cb,NULL) && !strcmp(field("action"),"continue") && !strcmp(field("cursor"),"cursor-one"));drain();
    show("settings:search");assert(find("Search again: guest query two")==0);
    show("search-history");assert(!strcmp(last_items[2].title,"guest query two"));
    const char *stale[]={"search-suggest:run:0:0","search-suggest:run:","search-suggest:run:1:99","search-again:0","search-again:"};
    for(unsigned i=0;i<sizeof stale/sizeof *stale;++i){p->list_catalog(stale[i],0,items_cb,NULL);assert(!last_ok && !pending_count);}
    p->list_catalog("search-suggest",0,items_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"suggestions\":[\"wrong kind\"]}}");assert(!last_ok && strstr(error_text,"suggestion"));
    p->list_catalog("search-suggest",0,items_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"search_suggestions\",\"suggestions\":[]}}");
    assert(last_ok && last_count==1 && !strcmp(last_items[0].title,"No suggestions"));
    p->list_catalog(suggestion,0,items_cb,NULL);assert(!last_ok && !pending_count); /* a newer list retired the row */
    /* A profile or source change retires suggestion rows and the remembered query. */
    p->list_catalog("search-suggest",0,items_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"search_suggestions\",\"suggestions\":[\"kept\"]}}");
    snprintf(suggestion,sizeof suggestion,"%s",last_items[0].id);
    show("library:profile:alice");p->list_catalog(suggestion,0,items_cb,NULL);assert(!last_ok && !pending_count);
    p->list_catalog("search-suggest",0,items_cb,NULL);assert(!strcmp(field("query"),"") && !strcmp(field("profile"),"alice"));drain();
    show("settings:search");assert(last_count==18);
    /* Reload keeps the saved cards choice only; a corrupt value falls back to off. */
    p->shutdown();assert(!p->init());show("library:profile:guest");
    assert(!p->search("after reload",0,items_cb,NULL) && !strcmp(field("type"),"all") && !strcmp(field("upload_date"),"any") && !strcmp(field("profile"),"guest"));drain();
    show("library:profile:alice");assert(!p->search("after reload",0,items_cb,NULL));assert(plain("after reload"));drain();
    p->shutdown();cJSON *store=read_store(),*entry;
    cJSON_ArrayForEach(entry,cJSON_GetObjectItem(store,"profile_settings"))
        cJSON_ReplaceItemInObject(entry,"search_channels_playlists",cJSON_CreateString("true"));
    write_store(store);cJSON_Delete(store);assert(!p->init());
    show("library:profile:guest");assert(!p->search("corrupt",0,items_cb,NULL));assert(plain("corrupt"));drain();
    assert(!p->set_source("http://127.0.0.1:8766#123456"));
    show("settings:search");assert(last_count==18 && find("Upload date: Any time")==0);
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: search options, library search and continuation, worker fallback, channel and playlist cards, suggestions, profile isolation and corrupt values");
}
