/* Per-profile section visibility and order, pinned channels and playlists, their menus and corrupt data. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *p;
static void show(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(last_ok && !pending_count);}
static int at(const char *id) {
    for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].id,id))return i;
    return -1;
}
static int titled(const char *title) {
    for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].title,title))return i;
    return -1;
}
static void refused(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(!last_ok && !pending_count);}
/* The visible ids of a list, joined, to compare whole orders at once. */
static const char *order(const char *parent) {
    static char text[1024];text[0]=0;show(parent);
    for(int i=0;i<last_count;++i){strcat(text,last_items[i].id);strcat(text," ");}
    return text;
}
static cJSON *profile_row(cJSON *store,const char *profile) {
    cJSON *entry;
    cJSON_ArrayForEach(entry,cJSON_GetObjectItem(store,"profile_settings"))
        if(!strcmp(cJSON_GetObjectItem(entry,"profile")->valuestring,profile))return entry;
    return NULL;
}
static int pins(const char *profile) {
    cJSON *store=read_store(),*entry;int count=0;
    cJSON_ArrayForEach(entry,cJSON_GetObjectItem(store,"pinned_sections"))if(!strcmp(cJSON_GetObjectItem(entry,"profile")->valuestring,profile))++count;
    cJSON_Delete(store);return count;
}
#define ROOT_TAIL "settings app:settings app:exit "
#define CHANNEL_A "channel:UCaaaaaaaaaaaaaaaaaaaaaa"
int main(void) {
    char dir[]="/tmp/tube-section-order-XXXXXX",parent[80],id[160];tube_playlist_prompt_t prompt;
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    assert(!strcmp(order(""),"discover history favorites queue subscriptions " ROOT_TAIL));
    /* The Touchpad menu of a section; fixed rows and unknown ids have none. */
    assert(!strcmp(tube_folder_menu("history","Continue watching & history"),"settings:section:2") && !strcmp(tube_folder_menu("section:shorts","Shorts"),"settings:section:20"));
    assert(!*tube_folder_menu("settings","YouTube preferences") && !*tube_folder_menu("app:exit","Close") && !*tube_folder_menu(NULL,NULL) && !*tube_folder_menu("abcdefghijk","Video"));
    show("settings:section:2");
    assert(last_count==6 && !strcmp(last_items[0].title,"Continue watching & history: shown") && !strcmp(last_items[1].id,"settings:section:2:up"));
    assert(!strcmp(last_items[2].id,"settings:section:2:down") && !strcmp(last_items[3].title,"Hide section") && !strcmp(last_items[4].id,"settings:sections"));
    assert(!strcmp(last_items[5].id,"settings:refresh") && !strcmp(last_items[5].title,"Refresh section"));
    show("settings:section:0");assert(titled("Move up")<0 && titled("Move down")>=0);   /* first of the main menu */
    show("settings:section:6");assert(titled("Move up")>=0 && titled("Move down")<0);   /* last of the main menu */
    /* One press moves a row past rows that are not on screen (no recent searches yet). */
    show("settings:section:2:up");assert(!strcmp(last_items[0].title,"Continue watching & history: shown") && titled("Move up")<0);
    assert(!strcmp(order(""),"history discover favorites queue subscriptions " ROOT_TAIL));
    show("settings:section:2:up");assert(!strcmp(order(""),"history discover favorites queue subscriptions " ROOT_TAIL)); /* already first */
    show("settings:section:4:down");assert(!strcmp(order(""),"history discover favorites subscriptions queue " ROOT_TAIL));
    p->list_catalog("settings:section:4:down",1,items_cb,NULL);assert(last_ok); /* a later page never repeats the action */
    assert(!strcmp(order(""),"history discover favorites subscriptions queue " ROOT_TAIL));
    /* Hiding removes the row only; the section still opens and can be shown again. */
    show("settings:section:3:toggle");assert(!strcmp(last_items[0].title,"Favorites: hidden") && titled("Show section")>=0);
    assert(!strcmp(order(""),"history discover subscriptions queue " ROOT_TAIL));
    show("favorites");
    show("settings:sections");assert(last_count==21 && !strcmp(last_items[0].title,"Main menu: Continue watching & history") && !strcmp(last_items[0].id,"settings:section:2"));
    assert(titled("Main menu: Favorites")>=0 && !strncmp(last_items[titled("Main menu: Favorites")].subtitle,"Hidden",6));
    assert(titled("Main menu: Recent searches")>=0 && titled("Discover: Trending")==19 && titled("Discover: Shorts")==20);
    p->list_catalog("settings",1,items_cb,NULL);assert(last_ok && at("settings:sections")>=0);
    /* Discover has its own order; its rows never mix with the main menu. */
    assert(!strncmp(order("discover"),"library:home library search:science ",36) && last_count==14);
    show("settings:section:19:up");show("settings:section:19:up");show("settings:section:7:toggle");show("settings:section:8:down");
    assert(!strncmp(order("discover"),"search:science library search:technology ",41) && last_count==13);
    assert(at("section:trending")==9 && at("section:live")==10 && at("section:movies")==11 && at("library:home")<0);
    assert(!strcmp(order(""),"history discover subscriptions queue " ROOT_TAIL));
    const char *bad[]={"settings:section:21","settings:section:-1","settings:section:","settings:section:x","settings:section:2:sideways","settings:section:2:","settings:section:99999999999999999999"};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;++i)refused(bad[i]);
    /* Conditional rows keep their place: Recent searches appears where the order puts it. */
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    assert(!p->search("query",0,items_cb,NULL));while(pending_count)reply(0,0,500,"{}");
    assert(!strcmp(order(""),"history discover search-history subscriptions queue " ROOT_TAIL));
    show("settings:section:1:up");assert(!strcmp(order(""),"history search-history discover subscriptions queue " ROOT_TAIL));
    /* The order is saved for the profile; another profile starts with the built-in one. */
    cJSON *store=read_store(),*sections=cJSON_GetObjectItem(profile_row(store,"guest"),"sections");
    assert(cJSON_GetArraySize(sections)==21 && !strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(sections,0),"id")->valuestring,"history"));
    assert(cJSON_IsFalse(cJSON_GetObjectItem(cJSON_GetArrayItem(sections,0),"hidden")));cJSON_Delete(store);
    show("library:profile:alice");
    assert(!strcmp(order(""),"discover history favorites queue subscriptions " ROOT_TAIL));
    assert(!strncmp(order("discover"),"library:home library search:science ",36) && last_count==14);
    show("settings:section:4:toggle");assert(!strcmp(order(""),"discover history favorites subscriptions " ROOT_TAIL));
    p->shutdown();assert(!p->init());
    assert(!strcmp(order(""),"discover history favorites subscriptions " ROOT_TAIL));
    show("library:profile:guest");assert(!strcmp(order(""),"history search-history discover subscriptions queue " ROOT_TAIL));
    assert(!strncmp(order("discover"),"search:science library search:technology ",41) && last_count==13);
    /* Pin a channel from its menu: the title comes from the folder the menu was opened on. */
    assert(!*tube_folder_menu("channel:bad","Bad") && !*tube_folder_menu("playlist:x","Short") && !strcmp(tube_folder_menu(CHANNEL_A,"Channel A"),"settings:pin:" CHANNEL_A));
    show("settings:pin:" CHANNEL_A);assert(last_count==2 && !strcmp(last_items[0].title,"Pin to main menu") && !strcmp(last_items[0].id,"settings:pin:add:" CHANNEL_A));
    assert(!strcmp(last_items[1].id,"settings:refresh"));
    show("settings:pin:add:" CHANNEL_A);
    assert(last_count==4 && !strcmp(last_items[0].title,"Pinned: Channel A") && !strcmp(last_items[1].title,"Unpin from main menu") && !strcmp(last_items[2].title,"Rename pinned section"));
    show("settings:pin:add:" CHANNEL_A);assert(pins("guest")==1); /* never pinned twice */
    assert(!strcmp(order(""),"history search-history discover subscriptions queue " CHANNEL_A " " ROOT_TAIL));
    assert(!strcmp(last_items[5].title,"Channel A") && last_items[5].is_folder && !strncmp(last_items[5].subtitle,"Pinned channel",14));
    assert(!strcmp(tube_folder_menu(CHANNEL_A,"Channel A"),"settings:pin:" CHANNEL_A)); /* the pinned row opens the same menu */
    /* Playlists: a public one from its menu, an account one through its existing playlist menu. */
    assert(!strcmp(tube_folder_menu("playlist:PLfixture01","Public list"),"settings:pin:playlist:PLfixture01"));
    show("settings:pin:add:playlist:PLfixture01");
    assert(!*tube_folder_menu("library:playlist:PLfixture02","Account list"));
    show("playlist:menu:PLfixture02");assert(titled("Pin to main menu")>=0 && !strcmp(last_items[titled("Pin to main menu")].id,"settings:pin:library:playlist:PLfixture02"));
    show("settings:pin:add:library:playlist:PLfixture02");assert(!strcmp(last_items[0].title,"Pinned: Account list"));
    show("playlist:menu:PLfixture02");assert(titled("Pinned section actions")>=0);
    show("settings:pin:add:channel:UCbbbbbbbbbbbbbbbbbbbbbb");assert(!strcmp(last_items[0].title,"Pinned: UCbbbbbbbbbbbbbbbbbbbbbb")); /* no folder was noted */
    assert(!strcmp(order(""),"history search-history discover subscriptions queue " CHANNEL_A " playlist:PLfixture01 library:playlist:PLfixture02 channel:UCbbbbbbbbbbbbbbbbbbbbbb " ROOT_TAIL));
    assert(!strncmp(last_items[6].subtitle,"Pinned playlist",15) && !strcmp(last_items[7].title,"Account list"));
    /* Pins move among themselves. */
    show("settings:pin:playlist:PLfixture01");assert(last_count==6 && titled("Move up")>=0 && titled("Move down")>=0);
    show("settings:pin:up:playlist:PLfixture01");assert(titled("Move up")<0);
    show("settings:pin:down:" CHANNEL_A);show("settings:pin:down:" CHANNEL_A);show("settings:pin:down:" CHANNEL_A);
    assert(!strcmp(order(""),"history search-history discover subscriptions queue playlist:PLfixture01 library:playlist:PLfixture02 channel:UCbbbbbbbbbbbbbbbbbbbbbb " CHANNEL_A " " ROOT_TAIL));
    show("settings:sections");assert(last_count==24 && last_more && !strcmp(last_items[21].title,"Pinned: Public list") && !strcmp(last_items[21].id,"settings:pin:playlist:PLfixture01"));
    p->list_catalog("settings:sections",1,items_cb,NULL);assert(last_ok && last_count==1 && !strcmp(last_items[0].title,"Pinned: Channel A"));
    /* Rename through the keyboard prompt: applied at once, bounded like a playlist name. */
    assert(!tube_take_playlist_prompt(&prompt));
    show("settings:pin:rename:" CHANNEL_A);assert(last_count==2 && !strcmp(last_items[0].title,"Enter a section name"));
    assert(tube_take_playlist_prompt(&prompt) && !strcmp(prompt.label,"Pinned section name") && !strcmp(prompt.initial,"Channel A") && !tube_take_playlist_prompt(&prompt));
    assert(tube_submit_playlist_name(prompt.ticket+1,"Other",parent,sizeof parent)<0 && tube_submit_playlist_name(prompt.ticket,"   ",parent,sizeof parent)<0);
    assert(tube_submit_playlist_name(prompt.ticket,"bad\x01name",parent,sizeof parent)<0);
    assert(!tube_submit_playlist_name(prompt.ticket,"  Favourite caf\xc3\xa9  ",parent,sizeof parent) && !strcmp(parent,"settings:pin:renamed"));
    assert(tube_submit_playlist_name(prompt.ticket,"Again",parent,sizeof parent)<0);
    show("settings:pin:renamed");assert(!strcmp(last_items[0].title,"Pinned: Favourite caf\xc3\xa9") && !strcmp(last_items[0].id,"settings:pin:" CHANNEL_A));
    show("");assert(!strcmp(last_items[8].title,"Favourite caf\xc3\xa9") && !strcmp(last_items[8].id,CHANNEL_A));
    refused("settings:pin:rename:channel:UCcccccccccccccccccccccc"); /* not pinned */
    /* Unpin; repeating it changes nothing. */
    show("settings:pin:remove:playlist:PLfixture01");assert(last_count==2 && !strcmp(last_items[0].title,"Pin to main menu"));
    show("settings:pin:remove:playlist:PLfixture01");assert(pins("guest")==3 && at("settings:refresh")==1);
    const char *invalid[]={"settings:pin:","settings:pin:add:","settings:pin:add:abcdefghijk","settings:pin:add:channel:bad","settings:pin:add:section:music","settings:pin:sideways:" CHANNEL_A,"settings:pin:add:library:home"};
    for(unsigned i=0;i<sizeof invalid/sizeof *invalid;++i)refused(invalid[i]);
    assert(pins("guest")==3);
    /* Pins belong to the profile; each profile holds at most 24. */
    show("library:profile:alice");assert(at(CHANNEL_A)<0 && !strcmp(order(""),"discover history favorites subscriptions " ROOT_TAIL));
    refused("settings:pin:renamed");
    show("settings:pin:add:" CHANNEL_A);assert(!strcmp(last_items[0].title,"Pinned: UCaaaaaaaaaaaaaaaaaaaaaa")); /* guest's noted title is not reused */
    for(int i=1;i<24;++i){snprintf(id,sizeof id,"settings:pin:add:channel:UC%022d",i);show(id);}
    assert(pins("alice")==24);
    snprintf(id,sizeof id,"settings:pin:add:channel:UC%022d",24);p->list_catalog(id,0,items_cb,NULL);
    assert(!last_ok && strstr(error_text,"full") && pins("alice")==24 && pins("guest")==3);
    show("");assert(last_count==24 && last_more);p->list_catalog("",1,items_cb,NULL);assert(last_ok && last_count==7 && !strcmp(last_items[6].id,"app:exit"));
    show("settings:pin:remove:" CHANNEL_A);show(id);assert(pins("alice")==24 && !strncmp(last_items[0].title,"Pinned: ",8));
    /* Reload keeps pins, titles and order. */
    p->shutdown();assert(!p->init());assert(pins("alice")==24 && pins("guest")==3);
    show("library:profile:guest");
    assert(!strcmp(order(""),"history search-history discover subscriptions queue library:playlist:PLfixture02 channel:UCbbbbbbbbbbbbbbbbbbbbbb " CHANNEL_A " " ROOT_TAIL));
    assert(!strcmp(last_items[7].title,"Favourite caf\xc3\xa9"));
    /* Corrupt data: bad rows drop one by one, unnamed sections follow in the built-in order. */
    p->shutdown();store=read_store();
    cJSON_ReplaceItemInObject(profile_row(store,"guest"),"sections",cJSON_Parse("[{\"id\":\"queue\"},{\"id\":\"queue\",\"hidden\":true},{\"id\":\"nope\"},7,\"history\","
        "{\"id\":\"history\",\"hidden\":\"yes\"},{\"id\":\"favorites\",\"hidden\":true},{\"hidden\":true},{\"id\":\"section:shorts\"}]"));
    cJSON_ReplaceItemInObject(profile_row(store,"alice"),"sections",cJSON_CreateString("junk"));
    cJSON *rows=cJSON_Parse("[7,\"text\",{\"profile\":\"bad/profile\",\"id\":\"" CHANNEL_A "\",\"title\":\"x\"},{\"profile\":\"guest\",\"id\":\"abcdefghijk\",\"title\":\"video\"},"
        "{\"profile\":\"guest\",\"id\":\"channel:nope\"},{\"profile\":\"guest\",\"id\":\"section:music\",\"title\":\"section\"},{\"profile\":\"guest\"},"
        "{\"profile\":\"guest\",\"id\":\"" CHANNEL_A "\",\"title\":\"Kept\"},{\"profile\":\"guest\",\"id\":\"" CHANNEL_A "\",\"title\":\"Duplicate\"},"
        "{\"profile\":\"guest\",\"id\":\"playlist:PLfixture01\"}]");
    for(int i=0;i<30;++i) {
        cJSON *row=cJSON_CreateObject();snprintf(id,sizeof id,"channel:UC%022d",i);
        cJSON_AddStringToObject(row,"profile","alice");cJSON_AddStringToObject(row,"id",id);cJSON_AddStringToObject(row,"title","Loaded");cJSON_AddItemToArray(rows,row);
    }
    cJSON_ReplaceItemInObject(store,"pinned_sections",rows);write_store(store);cJSON_Delete(store);assert(!p->init());
    assert(!strcmp(order(""),"queue history discover search-history subscriptions " CHANNEL_A " playlist:PLfixture01 " ROOT_TAIL));
    assert(!strcmp(last_items[5].title,"Kept") && !strcmp(last_items[6].title,"PLfixture01"));
    assert(!strncmp(order("discover"),"section:shorts library:home library ",36) && last_count==14);
    show("settings:section:3");assert(!strcmp(last_items[0].title,"Favorites: hidden"));
    show("library:profile:alice");show("");assert(last_count==24 && at("discover")==0 && at("queue")==3);
    show("settings:sections");p->list_catalog("settings:sections",1,items_cb,NULL);assert(last_count==21); /* 21 sections and 24 pins */
    /* A store without these fields loads the built-in menu and no pins. */
    p->shutdown();store=read_store();cJSON_DeleteItemFromObject(store,"pinned_sections");
    cJSON *entry;cJSON_ArrayForEach(entry,cJSON_GetObjectItem(store,"profile_settings"))cJSON_DeleteItemFromObject(entry,"sections");
    write_store(store);cJSON_Delete(store);assert(!p->init());
    assert(!strcmp(order(""),"discover history favorites queue subscriptions " ROOT_TAIL) && !pins("alice"));
    show("library:profile:guest");assert(!strcmp(order(""),"discover search-history history favorites queue subscriptions " ROOT_TAIL));
    /* A source change returns to Guest and retires a noted folder. */
    assert(!*tube_folder_menu("library:playlist:PLfixture03","Noted list"));
    assert(!p->set_source("http://127.0.0.1:8766#123456"));
    show("settings:pin:add:library:playlist:PLfixture03");assert(!strcmp(last_items[0].title,"Pinned: PLfixture03"));
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: section visibility and order, pinned channels and playlists, rename, bounds, profile isolation, reload and corrupt data");
}
