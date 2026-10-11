/* Trending, Shorts, My videos, Subscribed channels (both orders), Notifications and their startup choices. */
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
static const char *field(const char *key) {
    static char value[128];
    cJSON *job=cJSON_Parse(pending[0].body),*item=cJSON_GetObjectItem(cJSON_GetObjectItem(job,"library"),key);
    snprintf(value,sizeof value,"%s",cJSON_IsString(item)?item->valuestring:"<absent>");
    cJSON_Delete(job);return value;
}
static int saved_sort(const char *profile) {
    cJSON *store=read_store(),*entry;int value=-1;
    cJSON_ArrayForEach(entry,cJSON_GetObjectItem(store,"profile_settings"))
        if(!strcmp(cJSON_GetObjectItem(entry,"profile")->valuestring,profile))value=cJSON_GetObjectItem(entry,"channel_sort")->valueint;
    cJSON_Delete(store);return value;
}
/* A page of count channels named by the given letters, in the order sent. */
static void channels(const char *names,int more) {
    char body[8192];int used=snprintf(body,sizeof body,"{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[");
    for(int i=0;names[i];++i)used+=snprintf(body+used,sizeof body-(size_t)used,
        "%s{\"kind\":\"channel\",\"id\":\"channel:UC%c%021d\",\"channel_id\":\"UC%c%021d\",\"title\":\"%c channel\",\"subtitle\":\"%d subscribers\",\"art_url\":\"https://yt3.example/%c.jpg\",\"is_folder\":true}",
        i?",":"",names[i],0,names[i],0,names[i],i+1,names[i]);
    snprintf(body+used,sizeof body-(size_t)used,"],\"has_more\":%s,\"cursor\":\"%s\"}}",more?"true":"false",more==1?"next-page":more?"bad cursor!":"");
    poll_job(body);
}
static const char *videos="{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"Real title\",\"subtitle\":\"Real channel\","
    "\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\"},{\"id\":\"section:trending:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\",\"title\":\"Shelf\",\"is_folder\":true}],\"has_more\":true,\"cursor\":\"next-page\"}}";
static const char *none="{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":false}}";
int main(void) {
    char dir[]="/tmp/tube-library-sections-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    SEPARATE_SETTINGS();
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    /* Discover offers Trending and Shorts through the curated-section contract. */
    show("discover");assert(find("Trending")>=0 && !strcmp(row,"section:trending") && find("Shorts")>=0 && !strcmp(row,"section:shorts"));
    p->list_catalog("section:trending",0,items_cb,NULL);
    reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    assert(!strcmp(field("action"),"topics") && !strcmp(field("section"),"trending") && !strcmp(field("profile"),"guest"));
    poll_job(videos);assert(last_ok && last_count==2 && last_more && !last_items[0].is_folder && last_items[1].is_folder);
    snprintf(row,sizeof row,"%s",last_items[1].id);
    p->list_catalog("section:trending",1,items_cb,NULL);assert(!strcmp(field("action"),"continue") && !strcmp(field("cursor"),"next-page"));poll_job(none);
    p->list_catalog(row,0,items_cb,NULL);
    assert(!strcmp(field("action"),"topic_open") && !strcmp(field("section"),"trending") && !strcmp(field("id"),"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"));drain();
    p->list_catalog("section:shorts",0,items_cb,NULL);assert(!strcmp(field("action"),"topics") && !strcmp(field("section"),"shorts"));
    poll_job(videos);assert(last_ok && last_count==2 && !strcmp(last_items[0].id,"abcdefghijk"));
    /* Account lists ask Guest to choose a profile without contacting the companion. */
    const char *account[]={"section:my_videos","library:channels","library:notifications"};
    for(unsigned i=0;i<3;++i) {
        p->list_catalog(account[i],0,items_cb,NULL);
        assert(last_ok && last_count==1 && !pending_count && !strcmp(last_items[0].title,"Choose an account profile first"));
    }
    show("library:profile:fixture");
    assert(find("Subscribed channels")>=0 && !strcmp(row,"library:channels") && find("Notifications")>=0 && !strcmp(row,"library:notifications"));
    assert(find("My videos")>=0 && !strcmp(row,"section:my_videos"));
    p->list_catalog("section:my_videos",0,items_cb,NULL);
    assert(!strcmp(field("action"),"topics") && !strcmp(field("section"),"my_videos") && !strcmp(field("profile"),"fixture"));
    poll_job(videos);assert(last_ok && last_count==2 && !strcmp(last_items[0].title,"Real title"));
    /* Subscribed channels, account order: channel folders, one companion page at a time. */
    p->list_catalog("library:channels",0,items_cb,NULL);assert(!strcmp(field("action"),"channels") && !strcmp(field("profile"),"fixture"));
    channels("zAbz",1);
    assert(last_ok && last_count==3 && last_more && last_items[0].is_folder && !strcmp(last_items[0].title,"z channel"));
    assert(!strcmp(last_items[0].id,"channel:UCz000000000000000000000") && !strcmp(last_items[0].subtitle,"1 subscribers"));
    assert(!strcmp(last_items[1].title,"A channel") && !strcmp(last_items[2].title,"b channel") && !strcmp(last_items[2].art_url,"https://yt3.example/b.jpg"));
    p->list_catalog("library:channels",1,items_cb,NULL);assert(!strcmp(field("action"),"continue") && !strcmp(field("cursor"),"next-page"));
    channels("C",0);assert(last_ok && last_count==1 && !last_more);
    p->list_catalog("library:channels",2,items_cb,NULL);assert(!last_ok && !pending_count);
    /* By name: the whole list is read first, then sorted and paged from memory. */
    assert(saved_sort("fixture")==0);
    p->list_catalog("settings",1,items_cb,NULL);assert(find("Subscribed channels: account order")>=0 && !strcmp(row,"settings:channel-sort"));
    show("settings:channel-sort");assert(saved_sort("fixture")==1 && saved_sort("guest")==0);
    show("library");assert(find("Subscribed channels")>=0 && !strcmp(last_items[find("Subscribed channels")].subtitle,"Channels you subscribe to, by name"));
    item_calls=0;p->list_catalog("library:channels",0,items_cb,NULL);assert(!strcmp(field("action"),"channels"));
    channels("zAbz",1);assert(!item_calls && pending_count==1 && !strcmp(field("action"),"continue") && !strcmp(field("cursor"),"next-page"));
    channels("CbdefghijklmnopqrstuvwxyBDEFGH",1);assert(!item_calls && !strcmp(field("action"),"continue"));
    channels("e",0);
    assert(item_calls==1 && last_ok && last_count==24 && last_more && !pending_count);
    assert(!strcmp(last_items[0].title,"A channel") && !strcmp(last_items[1].title,"B channel") && !strcmp(last_items[2].title,"b channel"));
    assert(!strcmp(last_items[3].title,"C channel") && !strcmp(last_items[4].title,"D channel") && !strcmp(last_items[5].title,"d channel"));
    assert(last_items[0].is_folder && !strcmp(last_items[0].id,"channel:UCA000000000000000000000") && !strcmp(last_items[0].art_url,"https://yt3.example/A.jpg"));
    p->list_catalog("library:channels",1,items_cb,NULL);
    assert(item_calls==2 && last_ok && !pending_count && last_count==8 && !last_more && !strcmp(last_items[7].title,"z channel")); /* 32 distinct channels */
    p->list_catalog("library:channels",2,items_cb,NULL);assert(last_ok && !last_count && !pending_count);
    /* A broken continuation fails the list instead of showing part of it. */
    p->list_catalog("library:channels",0,items_cb,NULL);channels("ab",2);assert(!last_ok && !pending_count && strstr(error_text,"pagination"));
    p->list_catalog("library:channels",1,items_cb,NULL);assert(!last_ok && !pending_count);
    /* The sorted list and the setting belong to the profile. */
    p->list_catalog("library:channels",0,items_cb,NULL);channels("ba",0);assert(last_count==2 && !strcmp(last_items[0].title,"a channel"));
    show("library:profile:other");assert(saved_sort("other")==0);
    p->list_catalog("library:channels",0,items_cb,NULL);channels("ba",0);assert(last_count==2 && !strcmp(last_items[0].title,"b channel"));
    show("library:profile:fixture");
    p->list_catalog("library:channels",1,items_cb,NULL);assert(!last_ok && !pending_count); /* the earlier visit's list is retired */
    /* Notifications: the row shows the message and its time; the saved video keeps its own names. */
    p->list_catalog("library:notifications",0,items_cb,NULL);assert(!strcmp(field("action"),"notifications") && !strcmp(field("profile"),"fixture"));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":["
        "{\"kind\":\"video\",\"id\":\"abcdefghijk\",\"title\":\"Real channel uploaded: Real title\",\"subtitle\":\"2 hours ago\",\"unread\":true,\"playback_context\":{\"group\":\"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\",\"index\":0}},"
        "{\"kind\":\"video\",\"id\":\"bcdefghijkl\",\"title\":\"Someone is live\",\"subtitle\":\"\",\"unread\":true,\"playback_context\":{\"group\":\"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\",\"index\":1}},"
        "{\"kind\":\"video\",\"id\":\"abcdefghijk\",\"title\":\"Second notice\",\"subtitle\":\"1 day ago\",\"unread\":false,\"playback_context\":{\"group\":\"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\",\"index\":2}}],"
        "\"has_more\":true,\"cursor\":\"next-page\"}}");
    assert(last_ok && last_count==3 && last_more && !last_items[0].is_folder && last_items[0].playback_scope);
    assert(!strcmp(last_items[0].title,"Real channel uploaded: Real title") && !strcmp(last_items[0].subtitle,"Unread | 2 hours ago"));
    assert(!strcmp(last_items[1].subtitle,"Unread") && !strcmp(last_items[2].title,"Second notice") && !strcmp(last_items[2].subtitle,"1 day ago"));
    p->list_catalog("library:notifications",1,items_cb,NULL);assert(!strcmp(field("action"),"continue") && !strcmp(field("cursor"),"next-page"));drain();
    assert(tube_mark_watched("abcdefghijk")==1 && tube_mark_watched("bcdefghijkl")==1);
    p->list_catalog("history",0,items_cb,NULL);assert(last_count==2);
    for(int i=0;i<2;++i) {
        if(!strcmp(last_items[i].id,"abcdefghijk"))assert(!strcmp(last_items[i].title,"Real title") && !strcmp(last_items[i].subtitle,"Real channel") && !strcmp(last_items[i].parent_id,"UCaaaaaaaaaaaaaaaaaaaaaa"));
        else assert(!strcmp(last_items[i].title,"Someone is live") && !*last_items[i].subtitle);
    }
    /* The new sections can be the startup section of a profile. */
    show("settings:boot-section");assert(last_count==22);
    assert(find("Trending")==17 && find("Shorts")==18 && find("Subscribed channels")==19 && find("Notifications")==20 && find("My videos")==21);
    show("settings:boot-section:19");assert(!strcmp(tube_boot_section(),"library:channels") && !strcmp(tube_boot_section_title(),"Subscribed channels"));
    p->shutdown();assert(!p->init());
    assert(!strcmp(tube_boot_section(),"library:channels") && saved_sort("fixture")==1);
    show("library:profile:guest");assert(!*tube_boot_section() && !strcmp(tube_boot_section_title(),"Main menu"));
    /* A corrupt order falls back to the account's order. */
    p->shutdown();cJSON *store=read_store(),*entry;
    cJSON_ArrayForEach(entry,cJSON_GetObjectItem(store,"profile_settings"))cJSON_ReplaceItemInObject(entry,"channel_sort",cJSON_CreateNumber(7));
    write_store(store);cJSON_Delete(store);assert(!p->init());show("library:profile:fixture");
    p->list_catalog("library:channels",0,items_cb,NULL);channels("ba",0);assert(last_count==2 && !strcmp(last_items[0].title,"b channel"));
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: Trending, Shorts and My videos sections, subscribed channels in account and name order, notifications, guest notices and startup choices");
}
