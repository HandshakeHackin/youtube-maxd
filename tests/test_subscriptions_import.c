/* Import subscriptions: an export file from the app's import folder is parsed by the
 * companion, its channel count is shown for one confirmation, and the channels become local
 * follows of the selected profile, bounded by the follow list. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
#include <sys/stat.h>
static const evo_provider_t *p;
static char row[160];
static const char *third="{\"api_version\":1,\"features\":[\"search\",\"library\",\"subscriptions_import\"]}";
static const char *older="{\"api_version\":1,\"features\":[\"search\",\"library\"]}";
static const char *newpipe="{\"subscriptions\":[{\"service_id\":0,\"url\":\"https://www.youtube.com/channel/UCaaaaaaaaaaaaaaaaaaaaaa\",\"name\":\"Alpha\"}]}";
static void show(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(last_ok && !pending_count);}
static void refused(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(!last_ok && !pending_count);}
static int find(const char *title) {
    for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].title,title)){snprintf(row,sizeof row,"%s",last_items[i].id);return i;}
    return -1;
}
static int listed(const char *id) {for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].id,id))return i;return -1;}
static void pair(void) {assert(pending_count==1 && strstr(pending[0].url,"/v1/pair"));reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");}
static void health(const char *body) {
    assert(pending_count==1 && !strcmp(pending[0].method,"GET") && strstr(pending[0].url,"/v1/health"));reply(0,1,200,body);
}
static void put(const char *name,const char *text,size_t size) {
    char path[640];snprintf(path,sizeof path,"%s/%s",evo_data_path("import"),name);
    FILE *file=fopen(path,"wb");assert(file && fwrite(text,1,size,file)==size && !fclose(file));
}
/* The import request for a file: the file itself is the body of one POST. */
static int posted(const char *text) {
    return pending_count==1 && !strcmp(pending[0].method,"POST") && strstr(pending[0].url,"/v1/subscriptions/import") && !strcmp(pending[0].body,text);
}
static int follows(const char *profile) {
    cJSON *store=read_store(),*entry;int count=0;
    cJSON_ArrayForEach(entry,cJSON_GetObjectItem(store,"followed_channels"))if(!strcmp(cJSON_GetObjectItem(entry,"profile")->valuestring,profile))++count;
    cJSON_Delete(store);return count;
}
/* Every page of the local Followed channels list. */
static int followed_rows(const char *channel) {
    int total=0,found=0,more=1;char id[64];snprintf(id,sizeof id,"channel:%s",channel);
    for(int page=0;more;++page) {
        p->list_catalog("subscriptions",page,items_cb,NULL);assert(last_ok && !pending_count && last_count<=24);
        if(listed(id)>=0)found=1;
        total+=last_count;more=last_more;
    }
    return channel[0]?found:total;
}
int main(void) {
    char dir[]="/tmp/tube-import-XXXXXX",path[640],apply[160],*large;
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    /* The folder is created for the user and named on the empty screen. */
    show("settings:backup");assert(find("Import subscriptions")>=0 && !strcmp(row,"settings:backup:import"));
    show("settings:backup:import");assert(last_count==1 && !strcmp(last_items[0].title,"No import file found") && strstr(last_items[0].overview,evo_data_path("import")));
    struct stat info;assert(!stat(evo_data_path("import"),&info) && S_ISDIR(info.st_mode));
    /* Listed: plain names of text files up to 1 MiB, sorted. Hidden, empty, oversized, directories and odd names are not. */
    put("subscriptions.json",newpipe,strlen(newpipe));put("another-export_2.JSON","[]",2);put(".hidden","{}",2);put("empty.json","",0);
    put("with space.json","{}",2);put("binary.json","{\0}",3);
    large=calloc(1,1024*1024+2);assert(large);memset(large,' ',1024*1024+1);put("large.json",large,1024*1024+1);
    large[1024*1024]=0;large[0]='[';large[1]=']';put("limit.json",large,1024*1024);free(large);
    snprintf(path,sizeof path,"%s/folder.json",evo_data_path("import"));assert(!mkdir(path,0700));
    show("settings:backup:import");assert(last_count==4);
    assert(!strcmp(last_items[0].title,"another-export_2.JSON") && !strcmp(last_items[1].title,"binary.json") && !strcmp(last_items[2].title,"limit.json"));
    assert(!strcmp(last_items[3].title,"subscriptions.json") && !strcmp(last_items[3].id,"settings:backup:import:file:subscriptions.json") && strstr(last_items[3].subtitle,"1 KiB"));
    assert(strstr(last_items[2].subtitle,"1024 KiB"));
    const char *bad[]={"settings:backup:import:file:","settings:backup:import:file:.hidden","settings:backup:import:file:../youtube.json","settings:backup:import:file:empty.json",
        "settings:backup:import:file:large.json","settings:backup:import:file:missing.json","settings:backup:import:file:with space.json","settings:backup:import:file:folder.json",
        "settings:backup:import:file:sub/file.json"};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;++i)refused(bad[i]);
    refused("settings:backup:import:file:binary.json"); /* not text: nothing is sent */
    p->list_catalog("settings:backup:import:file:subscriptions.json",1,items_cb,NULL);assert(!last_ok && !pending_count);
    /* A companion without the import is not sent the file. */
    p->list_catalog("settings:backup:import:file:subscriptions.json",0,items_cb,NULL);pair();health(older);
    assert(!last_ok && !pending_count && strstr(error_text,"does not offer"));
    assert(!p->set_source("http://127.0.0.1:8766#123456"));
    /* The companion parses the file; the count waits for one confirmation and nothing is followed yet. */
    p->list_catalog("settings:backup:import:file:subscriptions.json",0,items_cb,NULL);pair();health(third);assert(posted(newpipe));
    reply(0,1,200,"{\"api_version\":1,\"kind\":\"subscriptions_import\",\"format\":\"newpipe\",\"channels\":["
        "{\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"title\":\"Alpha\"},{\"channel_id\":\"UCbbbbbbbbbbbbbbbbbbbbbb\",\"title\":\"\"},"
        "{\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"title\":\"Alpha again\"},{\"channel_id\":\"not-a-channel\",\"title\":\"Bad\"},"
        "{\"channel_id\":\"UCcccccccccccccccccccccc\",\"title\":\"Gamma\",\"group\":\"News\"}],\"skipped\":2,\"truncated\":false}");
    assert(last_ok && !pending_count && last_count==2 && !strcmp(last_items[0].title,"3 channels found in subscriptions.json"));
    assert(strstr(last_items[0].subtitle,"newpipe") && strstr(last_items[0].subtitle,"3 entries"));
    assert(!strcmp(last_items[1].title,"Add 3 channels to Followed channels") && !strncmp(last_items[1].id,"settings:backup:import:apply:",29));
    snprintf(apply,sizeof apply,"%s",last_items[1].id);
    assert(!followed_rows("") && !follows("guest"));
    /* The confirmation adds the channels once; reloading it repeats nothing. */
    assert(tube_toggle_favorite("abcdefghijk")==1);
    p->list_catalog(apply,1,items_cb,NULL);assert(last_ok && !follows("guest")); /* a later page adds nothing */
    show(apply);assert(last_count==2 && !strcmp(last_items[0].title,"3 channels added to Followed channels") && strstr(last_items[0].subtitle,"0 already followed"));
    assert(!strcmp(last_items[1].id,"subscriptions") && follows("guest")==3);
    show(apply);assert(!strcmp(last_items[0].title,"3 channels added to Followed channels") && follows("guest")==3);
    assert(followed_rows("")==3 && followed_rows("UCaaaaaaaaaaaaaaaaaaaaaa") && followed_rows("UCcccccccccccccccccccccc"));
    p->list_catalog("subscriptions",0,items_cb,NULL);
    assert(!strcmp(last_items[0].title,"Alpha") && !strcmp(last_items[1].title,"UCbbbbbbbbbbbbbbbbbbbbbb") && !strcmp(last_items[2].title,"Gamma")); /* file order; an untitled channel shows its identity */
    /* Importing again skips what is followed, whether by import or through a video. */
    p->list_catalog("playlist:PLfixture01",0,items_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"One\",\"subtitle\":\"Delta\",\"channel_id\":\"UCdddddddddddddddddddddd\"}],\"has_more\":false}}");
    assert(tube_toggle_subscription("abcdefghijk")==1 && followed_rows("")==4 && follows("guest")==3);
    p->list_catalog("settings:backup:import:file:subscriptions.json",0,items_cb,NULL);assert(posted(newpipe));
    reply(0,1,200,"{\"api_version\":1,\"kind\":\"subscriptions_import\",\"format\":\"grayjay\",\"channels\":["
        "{\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"title\":\"Alpha\"},{\"channel_id\":\"UCdddddddddddddddddddddd\",\"title\":\"Delta\"},"
        "{\"channel_id\":\"UCeeeeeeeeeeeeeeeeeeeeee\",\"title\":\"Epsilon\"}],\"skipped\":0,\"truncated\":true}");
    assert(last_ok && strstr(last_items[0].subtitle,"grayjay") && strstr(last_items[0].subtitle,"0 entries") && strstr(last_items[0].subtitle,"more than"));
    refused(apply); /* the earlier confirmation is spent by the new file */
    show(last_items[1].id);assert(!strcmp(last_items[0].title,"1 channel added to Followed channels") && strstr(last_items[0].subtitle,"2 already followed"));
    assert(follows("guest")==4 && followed_rows("")==5);
    /* Unfollowing: L2 on a video of the channel, or the channel's own menu. */
    assert(tube_toggle_subscription("abcdefghijk")==0 && followed_rows("")==4 && !followed_rows("UCdddddddddddddddddddddd"));
    show("settings:pin:channel:UCeeeeeeeeeeeeeeeeeeeeee");assert(find("Unfollow channel")>=0 && !strcmp(row,"settings:pin:unfollow:channel:UCeeeeeeeeeeeeeeeeeeeeee"));
    show(row);assert(find("Unfollow channel")<0 && followed_rows("")==3 && follows("guest")==3);
    show(row);assert(followed_rows("")==3); /* repeating it changes nothing */
    show("settings:pin:channel:UCdddddddddddddddddddddd");assert(find("Unfollow channel")<0);
    /* A blocked channel is not listed; follows belong to the profile that imported them. */
    p->list_catalog("playlist:PLfixture01",0,items_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"bcdefghijkl\",\"title\":\"Two\",\"subtitle\":\"Gamma\",\"channel_id\":\"UCcccccccccccccccccccccc\"}],\"has_more\":false}}");
    assert(tube_toggle_subscription("bcdefghijkl")==0 && followed_rows("")==2); /* followed by import: L2 unfollows */
    assert(tube_toggle_subscription("bcdefghijkl")==1 && followed_rows("")==3 && follows("guest")==2);
    show("library:profile:alice");assert(!followed_rows(""));refused(apply);
    show("library:profile:guest");assert(followed_rows("")==3);
    /* Invalid answers and errors add nothing. */
    const char *answers[]={"{\"api_version\":1,\"kind\":\"catalog\",\"channels\":[]}","{\"api_version\":1,\"kind\":\"subscriptions_import\",\"channels\":\"none\"}",
        "{\"api_version\":2,\"kind\":\"subscriptions_import\",\"channels\":[]}","[]"};
    for(unsigned i=0;i<sizeof answers/sizeof *answers;++i) {
        p->list_catalog("settings:backup:import:file:subscriptions.json",0,items_cb,NULL);assert(posted(newpipe));
        reply(0,1,200,answers[i]);assert(!last_ok && !pending_count && strstr(error_text,"import"));
    }
    p->list_catalog("settings:backup:import:file:subscriptions.json",0,items_cb,NULL);
    reply(0,1,400,"{\"error\":{\"code\":\"invalid_import\",\"message\":\"The file is not a supported subscriptions export.\"}}");
    assert(!last_ok && strstr(error_text,"not a supported"));
    p->list_catalog("settings:backup:import:file:another-export_2.JSON",0,items_cb,NULL);assert(posted("[]"));
    reply(0,1,200,"{\"api_version\":1,\"kind\":\"subscriptions_import\",\"format\":\"grayjay\",\"channels\":[],\"skipped\":0,\"truncated\":false}");
    assert(last_ok && last_count==1 && !strcmp(last_items[0].title,"0 channels found in another-export_2.JSON"));
    /* The follow list is bounded: what does not fit is reported, and what was read beyond it never fits. */
    p->shutdown();
    cJSON *store=read_store(),*list=cJSON_GetObjectItem(store,"followed_channels");
    for(int i=0;i<509;++i) {
        char entry[160];snprintf(entry,sizeof entry,"{\"profile\":\"alice\",\"channel\":\"UC%022d\",\"title\":\"Channel %d\"}",i,i);
        cJSON_AddItemToArray(list,cJSON_Parse(entry));
    }
    cJSON_AddItemToArray(list,cJSON_Parse("{\"profile\":\"alice\",\"channel\":\"UC0000000000000000000001\",\"title\":\"Repeated\"}"));
    cJSON_AddItemToArray(list,cJSON_Parse("{\"profile\":\"bad profile\",\"channel\":\"UC0000000000000000000900\",\"title\":\"\"}"));
    cJSON_AddItemToArray(list,cJSON_Parse("{\"profile\":\"alice\",\"channel\":\"not-a-channel\",\"title\":\"\"}"));
    cJSON_AddItemToArray(list,cJSON_CreateNumber(3));
    write_store(store);cJSON_Delete(store);assert(!p->init());
    assert(followed_rows("")==3);show("library:profile:alice");assert(followed_rows("")==509 && followed_rows("UC0000000000000000000508"));
    show("library:profile:guest");
    p->list_catalog("settings:backup:import:file:subscriptions.json",0,items_cb,NULL);health(third);assert(posted(newpipe));
    reply(0,1,200,"{\"api_version\":1,\"kind\":\"subscriptions_import\",\"format\":\"pockettube\",\"channels\":["
        "{\"channel_id\":\"UCffffffffffffffffffffff\",\"title\":\"F\"},{\"channel_id\":\"UCgggggggggggggggggggggg\",\"title\":\"G\"},"
        "{\"channel_id\":\"UChhhhhhhhhhhhhhhhhhhhhh\",\"title\":\"H\"}],\"skipped\":0,\"truncated\":false}");
    assert(last_ok && !strcmp(last_items[1].title,"Add 3 channels to Followed channels"));
    show(last_items[1].id);assert(!strcmp(last_items[0].title,"1 channel added to Followed channels") && strstr(last_items[0].subtitle,"2 not added") && strstr(last_items[0].subtitle,"512"));
    assert(followed_rows("")==4 && follows("guest")==3 && follows("alice")==509);
    p->shutdown();
    puts("PASS: import folder listing, file sent to a companion that offers the import, confirmed one-shot follow, skipped and bounded channels, unfollow, profile isolation");
}
