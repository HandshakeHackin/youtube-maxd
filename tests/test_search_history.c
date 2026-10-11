/* Per-profile recent searches: order, bounds, actions, the disable switch and corrupt rows. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *p;
static void show(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(last_ok);}
static void drain(void) {while(pending_count)reply(0,0,500,"{}");}
static void query(const char *text) {assert(!p->search(text,0,items_cb,NULL));drain();}
/* Stored queries of one profile, most recent first. */
static int stored(const char *profile,int index,char *out,size_t capacity) {
    cJSON *store=read_store(),*entry;int count=0;
    cJSON_ArrayForEach(entry,cJSON_GetObjectItem(store,"search_history"))
        if(!strcmp(cJSON_GetObjectItem(entry,"profile")->valuestring,profile)) {
            if(count==index && out)snprintf(out,capacity,"%s",cJSON_GetObjectItem(entry,"query")->valuestring);
            ++count;
        }
    cJSON_Delete(store);return count;
}
static int root_row(void) {
    show("");
    for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].id,"search-history"))return i;
    return -1;
}
int main(void) {
    char dir[]="/tmp/tube-search-history-XXXXXX",text[840],first[160],foreign[160],clear[160];
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    SEPARATE_SETTINGS();
    assert(root_row()<0);
    show("search-history");assert(last_count==1 && !strcmp(last_items[0].title,"No recent searches"));
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    assert(!p->search("First query",0,items_cb,NULL));
    reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[],\"has_more\":false}}");assert(last_ok);
    assert(root_row()==1 && stored("guest",0,text,sizeof text)==1 && !strcmp(text,"First query"));
    /* Most recent first; a repeat differing only by letter case moves to the front. */
    query("second");assert(!p->search("second",1,items_cb,NULL));drain(); /* later pages are not new searches */
    query("FIRST QUERY");
    assert(p->search("  ",0,items_cb,NULL)<0 && p->search("bad\x01query",0,items_cb,NULL)<0);
    assert(stored("guest",0,text,sizeof text)==2 && !strcmp(text,"FIRST QUERY"));
    assert(stored("guest",1,text,sizeof text)==2 && !strcmp(text,"second"));
    show("search-history");
    assert(last_count==4 && !strcmp(last_items[0].id,"search-history:remove") && !strncmp(last_items[1].id,"search-history:clear:",21));
    assert(!strcmp(last_items[2].title,"FIRST QUERY") && !strcmp(last_items[3].title,"second") && last_items[3].is_folder);
    /* A row searches again and returns to the front; its id survives for paging. */
    snprintf(first,sizeof first,"%s",last_items[3].id);assert(!strncmp(first,"search-history:run:",19));
    assert(!p->list_catalog(first,0,items_cb,NULL));
    assert(pending_count==1 && strstr(pending[0].body,"\"action\":\"search\"") && strstr(pending[0].body,"\"query\":\"second\""));
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"Hit\"}],\"has_more\":true}}");
    assert(last_ok && last_count==1 && last_more);
    assert(!p->list_catalog(first,1,items_cb,NULL));
    assert(pending_count==1 && strstr(pending[0].body,"\"query\":\"second\"") && strstr(pending[0].body,"\"page\":1"));drain();
    assert(stored("guest",0,text,sizeof text)==2 && !strcmp(text,"second"));
    p->list_catalog("search-history:run:999999",0,items_cb,NULL);assert(!last_ok && !pending_count);
    p->list_catalog("search-history:run:",0,items_cb,NULL);assert(!last_ok);
    p->list_catalog("search-history:run:1x",0,items_cb,NULL);assert(!last_ok);
    p->list_catalog("search-history:other",0,items_cb,NULL);assert(!last_ok);
    /* The longest valid query (200 four-byte characters) is kept whole. */
    char longest[801]="";for(int i=0;i<200;++i)strcat(longest,"\xf0\x9f\x8e\xb5");
    query(longest);assert(stored("guest",0,text,sizeof text)==3 && !strcmp(text,longest));
    show("search-history");assert(strlen(last_items[2].id)<64 && strlen(last_items[2].title)==188);
    /* Fifty distinct queries per profile; the oldest leave first. */
    for(int i=0;i<55;++i){char q[32];snprintf(q,sizeof q,"bounded %d",i);query(q);}
    assert(stored("guest",0,text,sizeof text)==50 && !strcmp(text,"bounded 54"));
    assert(stored("guest",49,text,sizeof text)==50 && !strcmp(text,"bounded 5"));
    show("search-history");assert(last_count==24 && last_more);
    p->list_catalog("search-history",1,items_cb,NULL);assert(last_ok && last_count==24 && last_more);
    p->list_catalog("search-history",2,items_cb,NULL);assert(last_ok && last_count==4 && !last_more && !strcmp(last_items[3].title,"bounded 5"));
    /* Remove one: the action applies once and a repeated id is rejected. */
    show("search-history:remove");
    assert(last_count==24 && last_more && !strcmp(last_items[0].title,"bounded 54") && !strncmp(last_items[0].id,"search-history:forget:",22));
    snprintf(first,sizeof first,"%s",last_items[0].id);snprintf(foreign,sizeof foreign,"%s",last_items[1].id);
    show(first);assert(last_count==24 && !strcmp(last_items[0].title,"bounded 53"));
    assert(stored("guest",0,text,sizeof text)==49 && !strcmp(text,"bounded 53"));
    p->list_catalog(first,1,items_cb,NULL);assert(last_ok && last_count==24 && stored("guest",0,NULL,0)==49);
    p->list_catalog(first,0,items_cb,NULL);assert(!last_ok && stored("guest",0,NULL,0)==49);
    /* Another profile starts empty and cannot use this profile's rows. */
    show("search-history");snprintf(clear,sizeof clear,"%s",last_items[1].id);
    show("library:profile:alice");assert(root_row()<0);
    p->list_catalog(foreign,0,items_cb,NULL);assert(!last_ok);
    p->list_catalog(clear,0,items_cb,NULL);assert(!last_ok);
    assert(stored("guest",0,NULL,0)==49 && !stored("alice",0,NULL,0));
    query("Bounded 53");query("alice only");
    assert(stored("alice",0,text,sizeof text)==2 && !strcmp(text,"alice only") && stored("guest",0,NULL,0)==49);
    show("search-history");assert(last_count==4);snprintf(clear,sizeof clear,"%s",last_items[1].id);
    show("library:profile:guest");p->list_catalog(clear,0,items_cb,NULL);assert(!last_ok && stored("alice",0,NULL,0)==2);
    show("library:profile:alice");p->list_catalog(clear,0,items_cb,NULL);assert(!last_ok); /* retired with the earlier visit */
    show("search-history");show(last_items[1].id);
    assert(last_count==1 && !strcmp(last_items[0].title,"No recent searches") && !stored("alice",0,NULL,0) && root_row()<0);
    assert(stored("guest",0,NULL,0)==49);
    /* The switch is per profile: off hides the list and records nothing. */
    show("library:profile:guest");assert(root_row()==1);
    show("settings:search-history");assert(root_row()<0);
    show("search-history");assert(last_count==1 && !strcmp(last_items[0].title,"Search history is off"));
    query("not recorded");assert(stored("guest",0,text,sizeof text)==49 && !strcmp(text,"bounded 53"));
    show("library:profile:alice");query("alice still records");assert(stored("alice",0,NULL,0)==1);
    p->shutdown();assert(!p->init());
    show("library:profile:guest");assert(root_row()<0);query("still off");assert(stored("guest",0,NULL,0)==49);
    show("settings:search-history");assert(root_row()==1 && stored("guest",0,text,sizeof text)==49 && !strcmp(text,"bounded 53"));
    cJSON *store=read_store(),*entry;
    cJSON_ArrayForEach(entry,cJSON_GetObjectItem(store,"profile_settings"))
        assert(cJSON_IsTrue(cJSON_GetObjectItem(entry,"search_history")));
    /* Corrupt, duplicate and excess rows are dropped one by one; the rest load. */
    p->shutdown();cJSON *rows=cJSON_Parse("[7,\"text\",{\"profile\":\"bad/profile\",\"query\":\"x\"},{\"profile\":\"guest\",\"query\":\"\"},"
        "{\"profile\":\"guest\",\"query\":\"bad\\u0001control\"},{\"profile\":\"guest\",\"query\":9},{\"profile\":\"guest\"},"
        "{\"profile\":\"guest\",\"query\":\"Kept\"},{\"profile\":\"guest\",\"query\":\"kept\"},{\"profile\":\"alice\",\"query\":\"KEPT\"}]");
    for(int i=0;i<60;++i) {
        char q[32];snprintf(q,sizeof q,"loaded %d",i);cJSON *row=cJSON_CreateObject();
        cJSON_AddStringToObject(row,"profile","guest");cJSON_AddStringToObject(row,"query",q);cJSON_AddItemToArray(rows,row);
    }
    cJSON_ReplaceItemInObject(store,"search_history",rows);
    cJSON_ArrayForEach(entry,cJSON_GetObjectItem(store,"profile_settings"))
        cJSON_ReplaceItemInObject(entry,"search_history",cJSON_CreateString("false"));
    write_store(store);cJSON_Delete(store);assert(!p->init());
    assert(root_row()==1);query("after load"); /* a corrupt switch value falls back to on */
    assert(stored("guest",0,text,sizeof text)==50 && !strcmp(text,"after load"));
    assert(stored("guest",1,text,sizeof text)==50 && !strcmp(text,"Kept"));
    assert(stored("guest",49,text,sizeof text)==50 && !strcmp(text,"loaded 47"));
    assert(stored("alice",0,text,sizeof text)==1 && !strcmp(text,"KEPT"));
    /* The shared bound of 256 entries holds across profiles and still accepts new searches. */
    p->shutdown();store=read_store();rows=cJSON_CreateArray();
    for(int i=0;i<300;++i) {
        char owner[32],q[32];snprintf(owner,sizeof owner,"profile%d",i/50);snprintf(q,sizeof q,"shared %d",i);
        cJSON *row=cJSON_CreateObject();
        cJSON_AddStringToObject(row,"profile",owner);cJSON_AddStringToObject(row,"query",q);cJSON_AddItemToArray(rows,row);
    }
    cJSON_ReplaceItemInObject(store,"search_history",rows);write_store(store);cJSON_Delete(store);assert(!p->init());
    show("library:profile:profile5");query("newest");
    store=read_store();assert(cJSON_GetArraySize(cJSON_GetObjectItem(store,"search_history"))==256);cJSON_Delete(store);
    assert(stored("profile5",0,text,sizeof text)==6 && !strcmp(text,"newest"));
    assert(stored("profile0",0,NULL,0)==50 && stored("profile4",0,NULL,0)==50);
    /* A store written before this feature, or with a non-list value, loads empty. */
    p->shutdown();store=read_store();cJSON_ReplaceItemInObject(store,"search_history",cJSON_CreateString("junk"));
    write_store(store);cJSON_Delete(store);assert(!p->init());assert(root_row()<0);
    p->shutdown();store=read_store();cJSON_DeleteItemFromObject(store,"search_history");
    write_store(store);cJSON_Delete(store);assert(!p->init());assert(root_row()<0);
    query("fresh");assert(root_row()==1);
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: search history order, case-insensitive dedupe, bounds, rerun, remove, clear, profile isolation, disable switch and corrupt rows");
}
