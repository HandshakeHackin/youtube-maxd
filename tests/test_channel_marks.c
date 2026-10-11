/* Subscribed channels: YouTube's new-content mark where the companion offers it, the order
 * with new content first, and "mark as watched" remembered for each profile on this PS5. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *p;
static char row[160];
static const char *third="{\"api_version\":1,\"features\":[\"search\",\"library\",\"channel_pages\",\"channel_new_content\"]}";
static const char *older="{\"api_version\":1,\"features\":[\"search\",\"library\",\"channel_pages\"]}";
static const char *basic="{\"api_version\":1,\"features\":[\"search\",\"library\",\"channel_new_content\"]}";
#define CH(letter,title,subtitle,mark) "{\"id\":\"channel:UC" letter letter letter letter letter letter letter letter letter letter letter letter letter letter letter letter letter letter letter letter letter letter "\",\"title\":\"" title "\",\"subtitle\":\"" subtitle "\",\"is_folder\":true" mark "}"
#define LIST(rows) "{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[" rows "],\"has_more\":false}}"
static const char *marked=LIST(CH("a","Beta","10 subscribers",",\"has_new\":false") "," CH("b","alpha","",",\"has_new\":true") ","
    CH("c","Gamma","5 subscribers",",\"has_new\":true") "," CH("d","Delta","",""));
static const char *cleared=LIST(CH("a","Beta","10 subscribers",",\"has_new\":false") "," CH("b","alpha","",",\"has_new\":false") ","
    CH("c","Gamma","5 subscribers",",\"has_new\":true") "," CH("d","Delta","",""));
static const char *plain=LIST(CH("a","Beta","10 subscribers","") "," CH("b","alpha","","") "," CH("c","Gamma","5 subscribers","") "," CH("d","Delta","",""));
static const char *B="UCbbbbbbbbbbbbbbbbbbbbbb",*C="UCcccccccccccccccccccccc";
static void show(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(last_ok && !pending_count);}
static void refused(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(!last_ok && !pending_count);}
static int find(const char *title) {
    for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].title,title)){snprintf(row,sizeof row,"%s",last_items[i].id);return i;}
    return -1;
}
static void pair(void) {assert(pending_count==1 && strstr(pending[0].url,"/v1/pair"));reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");}
static void health(const char *body) {
    assert(pending_count==1 && !strcmp(pending[0].method,"GET") && strstr(pending[0].url,"/v1/health"));reply(0,1,200,body);
}
static int direct(void) {return pending_count==1 && !strcmp(pending[0].method,"POST") && strstr(pending[0].url,"/v1/jobs")!=NULL;}
static const char *field(const char *key) {
    static char value[256];
    cJSON *job=cJSON_Parse(pending[0].body),*item=cJSON_GetObjectItem(cJSON_GetObjectItem(job,"library"),key);
    assert(job);snprintf(value,sizeof value,"%s",cJSON_IsString(item)?item->valuestring:cJSON_IsTrue(item)?"true":cJSON_IsFalse(item)?"false":"<absent>");
    cJSON_Delete(job);return value;
}
/* List the subscribed channels with this companion answer. */
static void channels(const char *body) {
    p->list_catalog("library:channels",0,items_cb,NULL);
    assert(direct() && !strcmp(field("action"),"channels"));poll_job(body);assert(last_ok && last_count==4);
}
/* The row of a channel title: 1 = shown as new. */
static int is_new(const char *title) {int i=find(title);assert(i>=0);return !strncmp(last_items[i].subtitle,"New",3);}
static int seen(const char *profile,const char *channel,const char *video) {
    cJSON *store=read_store(),*entry;int found=0;
    cJSON_ArrayForEach(entry,cJSON_GetObjectItem(store,"seen_channels"))
        if(!strcmp(cJSON_GetObjectItem(entry,"profile")->valuestring,profile) && !strcmp(cJSON_GetObjectItem(entry,"channel")->valuestring,channel) &&
           (!video || !strcmp(cJSON_GetObjectItem(entry,"video")->valuestring,video)))++found;
    cJSON_Delete(store);return found;
}
static int seen_total(void) {cJSON *store=read_store();int n=cJSON_GetArraySize(cJSON_GetObjectItem(store,"seen_channels"));cJSON_Delete(store);return n;}
static const char *uploads="{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"channel\":{\"id\":\"%s\",\"tabs\":[\"videos\"],\"sorts\":[],\"search\":false,\"tab\":\"videos\",\"sort\":\"\"},"
    "\"items\":[{\"id\":\"channel:%s\",\"title\":\"Self\",\"is_folder\":true},{\"id\":\"%s\",\"title\":\"Newest\"},{\"id\":\"bcdefghijkl\",\"title\":\"Older\"}],\"has_more\":false}}";
int main(void) {
    char dir[]="/tmp/tube-channel-marks-XXXXXX",id[96],body[768];
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    SEPARATE_SETTINGS();
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    /* Guest has no subscribed channels and nothing to mark. */
    show("library:channels");assert(last_count==1 && !strcmp(last_items[0].title,"Choose an account profile first"));
    assert(!strcmp(tube_folder_menu("library:channels","Subscribed channels"),"channels:menu"));
    show("channels:menu");assert(last_count==1 && !strcmp(last_items[0].title,"Choose an account profile first"));
    show("channels:watched");assert(last_count==1 && !strcmp(last_items[0].title,"Choose an account profile first"));
    snprintf(id,sizeof id,"channel-watched:%s",B);show(id);assert(!strcmp(last_items[0].title,"Choose an account profile first"));
    show("library:profile:fixture");
    /* A companion without the marks is asked for the list as before. */
    p->list_catalog("library:channels",0,items_cb,NULL);pair();health(older);
    assert(direct() && !strcmp(field("action"),"channels") && !strcmp(field("new_content"),"<absent>"));poll_job(plain);
    assert(last_ok && last_count==4 && !is_new("alpha") && !is_new("Gamma") && !strcmp(last_items[0].subtitle,"10 subscribers"));
    show("channels:watched");assert(!strcmp(last_items[0].title,"No channel is marked as new"));
    /* One that offers them is asked, and marked channels say so. */
    assert(!p->set_source("http://127.0.0.1:8766#123456"));show("library:profile:fixture");
    p->list_catalog("library:channels",0,items_cb,NULL);pair();health(third);
    assert(direct() && !strcmp(field("action"),"channels") && !strcmp(field("new_content"),"true") && !strcmp(field("profile"),"fixture"));poll_job(marked);
    assert(last_ok && last_count==4 && !strcmp(last_items[0].title,"Beta") && !strcmp(last_items[0].subtitle,"10 subscribers"));
    assert(!strcmp(last_items[1].subtitle,"New") && !strcmp(last_items[2].subtitle,"New | 5 subscribers") && !*last_items[3].subtitle);
    /* The order: account, by name, or new content first; chosen from the list's own menu or YouTube preferences. */
    show("channels:menu");assert(last_count==3 && !strcmp(last_items[0].title,"Order: account order") && !strcmp(last_items[0].id,"settings:channel-sort"));
    assert(!strcmp(last_items[1].id,"channels:watched"));
    show("settings:channel-sort");assert(last_count==3 && !strcmp(last_items[0].subtitle,"Selected") && !strcmp(last_items[2].title,"New content first"));
    show("settings:channel-sort:2");assert(!strcmp(last_items[2].subtitle,"Selected") && strcmp(last_items[0].subtitle,"Selected"));
    channels(marked);
    assert(!strcmp(last_items[0].title,"alpha") && !strcmp(last_items[0].subtitle,"New") && !strcmp(last_items[1].title,"Gamma") && !strcmp(last_items[1].subtitle,"New | 5 subscribers"));
    assert(!strcmp(last_items[2].title,"Beta") && !strcmp(last_items[3].title,"Delta")); /* the rest keeps the account's order */
    show("channels:menu");assert(!strcmp(last_items[0].title,"Order: new content first"));
    show("settings:channel-sort:1");channels(marked);
    assert(!strcmp(last_items[0].title,"alpha") && !strcmp(last_items[1].title,"Beta") && !strcmp(last_items[2].title,"Delta") && !strcmp(last_items[3].title,"Gamma"));
    assert(is_new("alpha") && is_new("Gamma") && !is_new("Beta"));
    refused("settings:channel-sort:3");refused("settings:channel-sort:x");refused("settings:channel-sort:12");refused("settings:channel-sort:");
    show("settings:channel-sort:0");
    /* Mark one channel as watched: remembered at once, and its uploads are opened so that YouTube clears its own mark. */
    channels(marked);
    snprintf(id,sizeof id,"settings:pin:channel:%s",B);tube_folder_menu(id+13,"alpha");show(id);
    assert(find("Mark channel as watched")>=0);snprintf(id,sizeof id,"channel-watched:%s",B);assert(!strcmp(row,id));
    p->list_catalog(id,0,items_cb,NULL);
    assert(direct() && !strcmp(field("action"),"channel") && !strcmp(field("channel"),B) && !strcmp(field("tab"),"videos"));
    assert(seen("fixture",B,"")==1); /* before the companion answers */
    snprintf(body,sizeof body,uploads,B,B,"abcdefghijk");poll_job(body);
    assert(last_ok && last_count==1 && !strcmp(last_items[0].title,"Channel marked as watched") && seen("fixture",B,"abcdefghijk")==1);
    channels(marked);assert(!is_new("alpha") && is_new("Gamma"));
    /* YouTube's cleared mark retires the memory: the next upload marks the channel again. */
    channels(cleared);assert(!is_new("alpha") && is_new("Gamma") && !seen("fixture",B,NULL));
    channels(marked);assert(is_new("alpha") && is_new("Gamma"));
    /* A failed request leaves the channel marked on this PS5. */
    p->list_catalog(id,0,items_cb,NULL);assert(direct());reply(0,0,500,"{}");assert(!last_ok && seen("fixture",B,"")==1);
    channels(marked);assert(!is_new("alpha") && is_new("Gamma"));
    /* Mark all: the channels the last list showed as new. */
    show("channels:watched");assert(last_count==1 && !strcmp(last_items[0].title,"1 channel marked as watched") && seen("fixture",C,NULL)==1);
    show("channels:watched");assert(!strcmp(last_items[0].title,"No channel is marked as new"));
    channels(marked);assert(!is_new("alpha") && !is_new("Gamma") && seen_total()==2);
    show("settings:channel-sort:2");channels(marked);assert(!strcmp(last_items[0].title,"Beta") && !is_new("alpha")); /* nothing is new: the account's order */
    show("settings:channel-sort:0");
    /* Another profile keeps its own marks; opening a marked channel's uploads remembers the newest video. */
    show("library:profile:alice");channels(marked);assert(is_new("alpha") && is_new("Gamma"));
    snprintf(id,sizeof id,"channel:%s",C);p->list_catalog(id,0,items_cb,NULL);
    assert(direct() && !strcmp(field("action"),"channel") && !strcmp(field("channel"),C));
    snprintf(body,sizeof body,uploads,C,C,"cdefghijklm");poll_job(body);assert(last_ok && seen("alice",C,"cdefghijklm")==1);
    p->list_catalog("channel:UCaaaaaaaaaaaaaaaaaaaaaa",0,items_cb,NULL);
    snprintf(body,sizeof body,uploads,"UCaaaaaaaaaaaaaaaaaaaaaa","UCaaaaaaaaaaaaaaaaaaaaaa","cdefghijklm");poll_job(body);
    assert(last_ok && !seen("alice","UCaaaaaaaaaaaaaaaaaaaaaa",NULL)); /* it was not marked as new */
    channels(marked);assert(is_new("alpha") && !is_new("Gamma") && seen_total()==3);
    show("channels:watched");assert(!strcmp(last_items[0].title,"1 channel marked as watched") && seen("alice",B,NULL)==1 && seen("fixture",B,NULL)==1);
    /* Saved with the profile; malformed, repeated and Guest entries are dropped one by one. */
    p->shutdown();
    cJSON *store=read_store(),*marks=cJSON_GetObjectItem(store,"seen_channels");assert(cJSON_GetArraySize(marks)==4);
    cJSON_AddItemToArray(marks,cJSON_Parse("{\"profile\":\"alice\",\"channel\":\"UCbbbbbbbbbbbbbbbbbbbbbb\",\"video\":\"\",\"seen\":5}"));
    cJSON_AddItemToArray(marks,cJSON_Parse("{\"profile\":\"guest\",\"channel\":\"UCdddddddddddddddddddddd\",\"video\":\"\",\"seen\":5}"));
    cJSON_AddItemToArray(marks,cJSON_Parse("{\"profile\":\"alice\",\"channel\":\"not-a-channel\",\"video\":\"\",\"seen\":5}"));
    cJSON_AddItemToArray(marks,cJSON_Parse("{\"profile\":\"bad profile\",\"channel\":\"UCdddddddddddddddddddddd\",\"seen\":5}"));
    cJSON_AddItemToArray(marks,cJSON_Parse("{\"profile\":\"alice\",\"channel\":\"UCdddddddddddddddddddddd\",\"video\":\"bad id\",\"seen\":\"then\"}"));
    cJSON_AddItemToArray(marks,cJSON_CreateString("mark"));
    write_store(store);cJSON_Delete(store);assert(!p->init());
    p->list_catalog("library:channels",0,items_cb,NULL);health(third);poll_job(marked);
    assert(last_ok && !is_new("alpha") && !is_new("Gamma")); /* alice, as selected before the restart */
    p->shutdown();assert(seen_total()==5 && seen("alice","UCdddddddddddddddddddddd","")==1 && seen("alice",B,NULL)==1);assert(!p->init());
    /* Bounded: the entry seen longest ago leaves first. */
    p->shutdown();store=read_store();marks=cJSON_GetObjectItem(store,"seen_channels");
    while(cJSON_GetArraySize(marks))cJSON_DeleteItemFromArray(marks,0);
    for(int i=0;i<600;++i) {
        char entry[160];snprintf(entry,sizeof entry,"{\"profile\":\"fixture\",\"channel\":\"UC%022d\",\"video\":\"\",\"seen\":%d}",i,1000+i);
        cJSON_AddItemToArray(marks,cJSON_Parse(entry));
    }
    write_store(store);cJSON_Delete(store);assert(!p->init());show("library:profile:fixture");
    p->shutdown();assert(seen_total()==512 && seen("fixture","UC0000000000000000000000",NULL)==1 && seen("fixture","UC0000000000000000000511",NULL)==1);
    assert(!seen("fixture","UC0000000000000000000512",NULL));assert(!p->init());
    snprintf(id,sizeof id,"channel-watched:%s",B);p->list_catalog(id,0,items_cb,NULL);health(third);assert(direct());reply(0,0,500,"{}");
    p->shutdown();assert(seen_total()==512 && seen("fixture",B,NULL)==1 && !seen("fixture","UC0000000000000000000000",NULL) && seen("fixture","UC0000000000000000000001",NULL)==1);
    assert(!p->init());
    /* A companion known to have no channel pages: the mark is kept on this PS5 only. */
    assert(!p->set_source("http://127.0.0.1:8767#123456"));show("library:profile:fixture");
    p->list_catalog("library:channels",0,items_cb,NULL);pair();health(basic);assert(direct() && !strcmp(field("new_content"),"true"));poll_job(marked);
    assert(is_new("Gamma"));snprintf(id,sizeof id,"channel-watched:%s",C);
    show(id);assert(!strcmp(last_items[0].title,"Channel marked as watched") && strstr(last_items[0].subtitle,"this PS5"));
    channels(marked);assert(!is_new("Gamma"));
    refused("channel-watched:UCshort");refused("channel-watched:");p->list_catalog(id,1,items_cb,NULL);assert(!last_ok && !pending_count);
    /* A profile that leaves the companion takes its marks with it. */
    p->list_catalog("library:profiles",0,items_cb,NULL);assert(direct());
    poll_job("{\"state\":\"done\",\"result\":{\"profiles\":[{\"id\":\"guest\",\"name\":\"Guest\"}]}}");assert(last_ok);
    p->shutdown();assert(!seen_total());
    puts("PASS: new-content marks, channel order with new content first, mark one or all channels as watched per profile, retirement, bounds and feature gating");
}
