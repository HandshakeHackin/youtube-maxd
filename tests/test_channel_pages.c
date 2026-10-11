/* Channel pages: sections, sort orders and search inside a channel when the companion
 * offers them; the plain channel list otherwise. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
#define UC "UCaaaaaaaaaaaaaaaaaaaaaa"
#define CHANNEL "channel:" UC
static const evo_provider_t *p;
static const char *all="{\"api_version\":1,\"features\":[\"search\",\"library\",\"locale\",\"thumbnail_source\",\"content_filters\",\"channel_pages\"]}";
static const char *basic="{\"api_version\":1,\"features\":[\"search\",\"library\"]}";
static const char *none="{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":false}}";
static void show(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(last_ok && !pending_count);}
static void refused(const char *id,int page) {p->list_catalog(id,page,items_cb,NULL);assert(!last_ok && !pending_count);}
static int find(const char *title) {for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].title,title))return i;return -1;}
static void pair(void) {assert(pending_count==1 && strstr(pending[0].url,"/v1/pair"));reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");}
static void health(int status,const char *body) {
    assert(pending_count==1 && !strcmp(pending[0].method,"GET") && strstr(pending[0].url,"/v1/health"));
    reply(0,1,status,body);assert(pending_count==1 && strstr(pending[0].url,"/v1/jobs"));
}
static int direct(void) {return pending_count==1 && !strcmp(pending[0].method,"POST") && strstr(pending[0].url,"/v1/jobs")!=NULL;}
static const char *field(const char *object,const char *key) {
    static char value[512];
    cJSON *job=cJSON_Parse(pending[0].body),*item=cJSON_GetObjectItem(*object?cJSON_GetObjectItem(job,object):job,key);
    assert(job);
    if(cJSON_IsString(item))snprintf(value,sizeof value,"%s",item->valuestring);
    else if(cJSON_IsNumber(item))snprintf(value,sizeof value,"%d",item->valueint);
    else snprintf(value,sizeof value,"<absent>");
    cJSON_Delete(job);return value;
}
/* A page of the channel's own videos with the facts of the opened section. */
static void videos(const char *tab,const char *sort,const char *sorts,const char *tabs,int search,const char *cursor) {
    char body[2048];
    snprintf(body,sizeof body,"{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":["
        "{\"id\":\"abcdefghijk\",\"title\":\"First\",\"channel_id\":\"" UC "\",\"playback_context\":{\"channel\":\"" UC "\",\"index\":0}},"
        "{\"id\":\"bcdefghijkl\",\"title\":\"Second\",\"channel_id\":\"" UC "\",\"playback_context\":{\"channel\":\"" UC "\",\"index\":1}},"
        "{\"id\":\"cdefghijklm\",\"title\":\"Third\",\"channel_id\":\"" UC "\",\"playback_context\":{\"channel\":\"" UC "\",\"index\":2}}],"
        "\"has_more\":%s,\"cursor\":\"%s\",\"channel\":{\"id\":\"" UC "\",\"title\":\"Channel A\",\"tabs\":[%s],\"search\":%s,\"tab\":\"%s\",\"sort\":\"%s\",\"sorts\":[%s]}}}",
        *cursor?"true":"false",cursor,tabs,search?"true":"false",tab,sort,sorts);
    poll_job(body);
}
static const char *tabs4="\"home\",\"videos\",\"shorts\",\"playlists\"",*sorts3="\"newest\",\"popular\",\"oldest\"";
static char parent[80];
static int submit(const char *text) {
    tube_playlist_prompt_t prompt;
    if(!tube_take_playlist_prompt(&prompt))return -2;
    assert(!strcmp(prompt.label,"Search in this channel") && !*prompt.initial);
    return tube_submit_playlist_name(prompt.ticket,text,parent,sizeof parent);
}
int main(void) {
    char dir[]="/tmp/tube-channel-pages-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    /* The channel folder reads the feature list, then opens the companion's channel page on Videos. */
    p->list_catalog(CHANNEL,0,items_cb,NULL);pair();health(200,all);
    assert(!strcmp(field("","action"),"library") && !strcmp(field("library","action"),"channel") && !strcmp(field("library","channel"),UC));
    assert(!strcmp(field("library","profile"),"guest") && !strcmp(field("library","tab"),"<absent>") && !strcmp(field("library","channel_sort"),"<absent>") && !strcmp(field("","parent"),"<absent>"));
    videos("videos","newest",sorts3,tabs4,1,"channel-next");
    assert(last_ok && last_count==4 && last_more && last_items[0].is_folder && !strcmp(last_items[0].id,"channel-menu:" UC) && !strcmp(last_items[0].title,"Channel sections"));
    assert(!strcmp(last_items[1].id,"abcdefghijk") && !strcmp(last_items[1].playback_playlist,CHANNEL) && last_items[3].playback_index==2 && last_items[1].playback_scope);
    p->list_catalog(CHANNEL,1,items_cb,NULL);
    assert(direct() && !strcmp(field("library","action"),"continue") && !strcmp(field("library","cursor"),"channel-next") && !strcmp(field("library","channel"),"<absent>"));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"defghijklmn\",\"playback_context\":{\"channel\":\"" UC "\",\"index\":3}}],\"has_more\":false}}");
    assert(last_ok && last_count==1 && !last_more && !last_items[0].is_folder && last_items[0].playback_index==3);
    refused(CHANNEL,2);
    /* The sections menu lists what this channel offers: sections, orders of the open one, search. */
    show("channel-menu:" UC);
    assert(last_count==8 && find("Home")==0 && find("Videos")==1 && find("Shorts")==2 && find("Playlists")==3 && find("Live")<0);
    assert(!strcmp(last_items[0].id,"channel-tab:" UC ":home") && !strcmp(last_items[1].subtitle,"Section last opened"));
    assert(!strcmp(last_items[4].title,"Videos: Newest first") && !strcmp(last_items[4].subtitle,"Selected") && !strcmp(last_items[4].id,"channel-tab:" UC ":videos:newest"));
    assert(!strcmp(last_items[5].id,"channel-tab:" UC ":videos:popular") && !strcmp(last_items[6].title,"Videos: Oldest first") && strcmp(last_items[5].subtitle,"Selected"));
    assert(!strcmp(last_items[7].title,"Search in channel") && !strcmp(last_items[7].id,"channel-search:" UC));
    /* Another order of Videos: retained-group rows, the applied order marked. */
    p->list_catalog("channel-tab:" UC ":videos:popular",0,items_cb,NULL);
    assert(direct() && !strcmp(field("library","action"),"channel") && !strcmp(field("library","tab"),"videos") && !strcmp(field("library","channel_sort"),"popular"));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"abcdefghijk\",\"playback_context\":{\"group\":\"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\",\"index\":0}}],"
        "\"has_more\":true,\"cursor\":\"popular-next\",\"channel\":{\"id\":\"" UC "\",\"tabs\":[\"home\",\"videos\",\"shorts\",\"playlists\"],\"search\":true,\"tab\":\"videos\",\"sort\":\"popular\",\"sorts\":[\"newest\",\"popular\",\"oldest\"]}}}");
    assert(last_ok && last_count==1 && last_more && !last_items[0].is_folder && !strcmp(last_items[0].playback_playlist,"group:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"));
    p->list_catalog("channel-tab:" UC ":videos:popular",1,items_cb,NULL);assert(!strcmp(field("library","action"),"continue") && !strcmp(field("library","cursor"),"popular-next"));poll_job(none);
    refused(CHANNEL,1); /* the channel's own list was replaced by this section */
    show("channel-menu:" UC);assert(last_count==8 && !strcmp(last_items[5].subtitle,"Selected") && strcmp(last_items[4].subtitle,"Selected"));
    /* Shorts, Playlists and Home: the rows the companion lists, no order where none is offered. */
    p->list_catalog("channel-tab:" UC ":shorts",0,items_cb,NULL);
    assert(direct() && !strcmp(field("library","tab"),"shorts") && !strcmp(field("library","channel_sort"),"<absent>"));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"abcdefghijk\",\"is_short\":true,\"playback_context\":{\"group\":\"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\",\"index\":4}}],"
        "\"has_more\":false,\"channel\":{\"id\":\"" UC "\",\"tabs\":[\"home\",\"videos\",\"shorts\",\"live\",\"playlists\",\"bogus\"],\"search\":false,\"tab\":\"shorts\",\"sort\":\"\",\"sorts\":[]}}}");
    assert(last_ok && last_count==1 && last_items[0].playback_index==4);
    show("channel-menu:" UC);assert(last_count==5 && find("Live")==3 && find("Search in channel")<0 && !strcmp(last_items[2].subtitle,"Section last opened"));
    refused("channel-search:" UC,0);
    p->list_catalog("channel-tab:" UC ":playlists",0,items_cb,NULL);assert(!strcmp(field("library","tab"),"playlists"));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"library:playlist:PLfixture01\",\"title\":\"List one\",\"is_folder\":true},{\"id\":\"bad id\"}],\"has_more\":false}}");
    assert(last_ok && last_count==1 && last_items[0].is_folder && !strcmp(last_items[0].id,"library:playlist:PLfixture01"));
    show("channel-menu:" UC);assert(last_count==5); /* a page without facts keeps the earlier ones */
    p->list_catalog("channel-tab:" UC ":home",0,items_cb,NULL);assert(!strcmp(field("library","tab"),"home"));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"library:filter:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\",\"title\":\"Popular uploads\",\"subtitle\":\"Channel section\",\"is_folder\":true},"
        "{\"id\":\"abcdefghijk\",\"title\":\"Loose card\"}],\"has_more\":false,\"channel\":{\"id\":\"UCbbbbbbbbbbbbbbbbbbbbbb\",\"tabs\":[\"home\"],\"search\":true,\"tab\":\"home\"}}}");
    assert(last_ok && last_count==2 && last_items[0].is_folder && !last_items[1].is_folder);
    show("channel-menu:" UC);assert(last_count==5); /* facts of another channel are not taken */
    p->list_catalog("library:filter:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",0,items_cb,NULL);assert(direct() && !strcmp(field("library","action"),"suggestions_filter"));poll_job(none);
    /* Malformed section ids ask nothing. */
    const char *bad[]={"channel-tab:" UC ":bogus","channel-tab:" UC ":videos:sideways","channel-tab:" UC ":","channel-tab:" UC,"channel-tab:bad:videos",
        "channel-tab:UCaaaaaaaaaaaaaaaaaaaaa!:videos","channel-tab:" UC ":videos:","channel-menu:bad","channel-menu:UCbbbbbbbbbbbbbbbbbbbbbb","channel-search:bad","channel-search:run:1","channel-search:run:"};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;++i)refused(bad[i],0);
    /* A section the channel lacks fails as the companion reports it. */
    p->list_catalog("channel-tab:" UC ":live",0,items_cb,NULL);
    poll_job("{\"state\":\"failed\",\"error\":{\"code\":\"channel_tab_unavailable\",\"message\":\"This channel has no such section.\"}}");
    assert(!last_ok && strstr(error_text,"no such section"));
    /* Search in channel: the text prompt, then a companion list continued by cursor. */
    p->list_catalog(CHANNEL,0,items_cb,NULL);assert(direct());videos("videos","newest",sorts3,tabs4,1,"");
    assert(last_count==4 && !last_more);
    show("channel-search:" UC);assert(last_count==1 && !strcmp(last_items[0].title,"Enter text to search in this channel"));
    assert(submit("   ")==-1 && !strcmp(tube_prompt_error(),"Enter 1 to 150 characters to search this channel."));
    show("channel-search:" UC);assert(submit("  cats and dogs ")==0 && !*tube_prompt_error() && !strncmp(parent,"channel-search:run:",19));
    assert(!strcmp(tube_prompt_title(parent),"Search in channel") && !strcmp(tube_prompt_title("playlist:review:3"),"Confirm playlist change") && !strcmp(tube_prompt_title("settings:pin:renamed"),"Pinned section"));
    assert(submit("again")==-2); /* one text per prompt */
    p->list_catalog(parent,0,items_cb,NULL);
    assert(direct() && !strcmp(field("library","action"),"channel_search") && !strcmp(field("library","channel"),UC) && !strcmp(field("library","query"),"cats and dogs"));
    assert(!strcmp(field("library","tab"),"<absent>") && !strcmp(field("library","channel_sort"),"<absent>"));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"abcdefghijk\",\"playback_context\":{\"group\":\"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\",\"index\":0}},"
        "{\"id\":\"library:playlist:PLfixture01\",\"is_folder\":true}],\"has_more\":true,\"cursor\":\"found-next\",\"channel\":{\"id\":\"" UC "\",\"tabs\":[\"videos\"],\"search\":true,\"tab\":\"search\",\"sort\":\"\",\"sorts\":[]}}}");
    assert(last_ok && last_count==2 && last_more && !last_items[0].is_folder && last_items[1].is_folder);
    p->list_catalog(parent,1,items_cb,NULL);assert(!strcmp(field("library","action"),"continue") && !strcmp(field("library","cursor"),"found-next"));poll_job(none);
    show("channel-menu:" UC);assert(last_count==8); /* a search result does not replace the channel's facts */
    char first[80];snprintf(first,sizeof first,"%s",parent);
    show("channel-search:" UC);assert(submit("second")==0 && strcmp(first,parent));
    refused(first,0); /* only the last search can be listed */
    p->list_catalog(parent,0,items_cb,NULL);assert(!strcmp(field("library","query"),"second"));poll_job(none);
    /* The locale and the hide switches of the profile apply to channel pages too. */
    show("settings:content-language:de");show("settings:hide:shorts_all");
    refused("channel-menu:" UC,0);refused(parent,0); /* retired with the changed choice */
    p->list_catalog(CHANNEL,0,items_cb,NULL);
    assert(direct() && !strcmp(field("library","action"),"channel") && !strcmp(field("library","hl"),"de") && strstr(pending[0].body,"\"hide\":[\"shorts_all\",\"watched_home\"]"));
    videos("videos","newest",sorts3,tabs4,1,"");
    show("settings:content-language:default");show("settings:hide:reset");
    /* Facts and searches belong to the profile that opened the channel. */
    p->list_catalog(CHANNEL,0,items_cb,NULL);videos("videos","newest",sorts3,tabs4,1,"");
    show("channel-search:" UC);assert(submit("mine")==0);
    show("library:profile:alice");
    refused("channel-menu:" UC,0);refused("channel-search:" UC,0);refused(parent,0);
    p->list_catalog(CHANNEL,0,items_cb,NULL);assert(direct() && !strcmp(field("library","profile"),"alice"));videos("videos","newest","","\"videos\"",0,"");
    show("channel-menu:" UC);assert(last_count==1 && find("Videos")==0); /* one section, no order, no search */
    show("library:profile:guest");
    /* Without channel pages the channel stays the plain catalog list, paged by number. */
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->list_catalog(CHANNEL,0,items_cb,NULL);pair();health(200,basic);
    assert(!strcmp(field("","action"),"catalog") && !strcmp(field("","parent"),CHANNEL) && !strcmp(field("","page"),"0") && !strcmp(field("","library"),"<absent>"));
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\",\"playback_context\":{\"channel\":\"" UC "\",\"index\":0}}],\"has_more\":true}}");
    assert(last_ok && last_count==1 && last_more && !last_items[0].is_folder && !strcmp(last_items[0].playback_playlist,CHANNEL));
    p->list_catalog(CHANNEL,1,items_cb,NULL);assert(direct() && !strcmp(field("","action"),"catalog") && !strcmp(field("","page"),"1"));poll_job(none);
    p->list_catalog(CHANNEL,0,items_cb,NULL);assert(direct() && !strcmp(field("","action"),"catalog"));poll_job(none); /* the list was read once */
    refused("channel-menu:" UC,0);
    p->list_catalog("channel-tab:" UC ":shorts",0,items_cb,NULL);assert(!last_ok && !pending_count && strstr(error_text,"does not offer"));
    /* A companion that cannot be asked keeps the plain list and is asked again next time. */
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->list_catalog(CHANNEL,0,items_cb,NULL);pair();
    assert(pending_count==1 && strstr(pending[0].url,"/v1/health"));reply(0,0,0,"");
    assert(direct() && !strcmp(field("","action"),"catalog") && !strcmp(field("","parent"),CHANNEL));poll_job(none);
    p->list_catalog(CHANNEL,0,items_cb,NULL);health(200,all);assert(!strcmp(field("library","action"),"channel"));videos("videos","newest",sorts3,tabs4,1,"");
    assert(last_count==4);
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: channel sections, sort orders, search in channel, profile and locale scope, and the plain channel list without the feature");
    return 0;
}
