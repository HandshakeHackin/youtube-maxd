/* The companion's feature list gates second-batch fields: content language and country,
 * hide switches, thumbnail source, the alternative SponsorBlock server; plus the
 * caption and audio language lists. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *p;
static evo_stream_choice_t choice;
static const char *A="abcdefghijk";
static const char *all="{\"api_version\":1,\"version\":\"0.1\",\"service\":\"ProsperoTube\",\"mode\":\"youtube\",\"features\":[\"search\",\"channels\",\"dash\",\"locale\","
    "\"thumbnail_source\",\"storyboards\",\"library\",\"content_filters\",\"channel_pages\"],\"sponsorblock\":true,\"sponsorblock_alt\":true}";
static const char *basic="{\"api_version\":1,\"features\":[\"search\",\"channels\",\"dash\",\"relay\"],\"sponsorblock\":true}";
static const char *none="{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":false}}";
static const char *more="{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"One\"}],\"has_more\":true,\"cursor\":\"next-page\"}}";
static void show(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(last_ok && !pending_count);}
static void refused(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(!last_ok && !pending_count);}
static int find(const char *id) {for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].id,id))return i;return -1;}
static void pair(void) {assert(pending_count==1 && strstr(pending[0].url,"/v1/pair"));reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");}
/* The request waits for the feature list; afterwards the job itself is pending. */
static void health(int status,const char *body) {
    assert(pending_count==1 && !strcmp(pending[0].method,"GET") && strstr(pending[0].url,"/v1/health") && !*pending[0].body);
    reply(0,1,status,body);assert(pending_count==1 && !strcmp(pending[0].method,"POST") && strstr(pending[0].url,"/v1/jobs"));
}
static int direct(void) {return pending_count==1 && !strcmp(pending[0].method,"POST") && strstr(pending[0].url,"/v1/jobs")!=NULL;}
/* A field of the pending job ("" = top level): text, a list joined by commas, or <absent>. */
static const char *field(const char *object,const char *key) {
    static char value[512];
    cJSON *job=cJSON_Parse(pending[0].body),*item=cJSON_GetObjectItem(*object?cJSON_GetObjectItem(job,object):job,key),*entry;
    assert(job);value[0]=0;
    if(cJSON_IsString(item))snprintf(value,sizeof value,"%s",item->valuestring);
    else if(cJSON_IsArray(item))cJSON_ArrayForEach(entry,item)snprintf(value+strlen(value),sizeof value-strlen(value),"%s%s",*value?",":"",entry->valuestring);
    else snprintf(value,sizeof value,"<absent>");
    cJSON_Delete(job);return value;
}
static int plain(const char *object) {
    return !strcmp(field(object,"hl"),"<absent>") && !strcmp(field(object,"gl"),"<absent>") &&
        !strcmp(field(object,"hide"),"<absent>") && !strcmp(field(object,"thumbnails"),"<absent>");
}
static void home(void) {p->list_catalog("library:home",0,items_cb,NULL);}
static cJSON *profile_row(cJSON *store,const char *profile) {
    cJSON *row;
    cJSON_ArrayForEach(row,cJSON_GetObjectItem(store,"profile_settings"))
        if(!strcmp(cJSON_GetObjectItem(row,"profile")->valuestring,profile))return row;
    return NULL;
}
static const char *saved(const char *profile,const char *key) {
    static char value[512];
    cJSON *store=read_store(),*item=cJSON_GetObjectItem(profile_row(store,profile),key),*entry;
    value[0]=0;
    if(cJSON_IsString(item))snprintf(value,sizeof value,"%s",item->valuestring);
    else if(cJSON_IsArray(item))cJSON_ArrayForEach(entry,item)snprintf(value+strlen(value),sizeof value-strlen(value),"%s%s",*value?",":"",entry->valuestring);
    else snprintf(value,sizeof value,"%s",cJSON_IsTrue(item)?"true":cJSON_IsFalse(item)?"false":"<absent>");
    cJSON_Delete(store);return value;
}
static void selected(int ok,const evo_stream_choice_t *choices,int count,void *ud) {(void)ud;assert(ok && count==1);choice=choices[0];}
static void resolved(void) {
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"duration_sec\":100,\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/cap/d0.mpd\","
        "\"container\":\"dash\",\"video_codec\":\"avc\",\"audio_codec\":\"aac\"}]}}");
}
/* Count the rows of a paged settings list and check that every row is named, not a bare code. */
static int rows(const char *parent,const char *prefix,int *marked) {
    int total=0,page=0;*marked=0;
    do {
        p->list_catalog(parent,page++,items_cb,NULL);assert(last_ok && !pending_count);
        for(int i=0;i<last_count;++i) {
            assert(!strncmp(last_items[i].id,prefix,strlen(prefix)) && strcmp(last_items[i].title,last_items[i].id+strlen(prefix)) && *last_items[i].title);
            if(!strcmp(last_items[i].subtitle,"Selected"))++*marked;
        }
        total+=last_count;
    } while(last_more);
    return total;
}
int main(void) {
    char dir[]="/tmp/tube-companion-features-XXXXXX";int marked;
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    SEPARATE_SETTINGS();
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    /* At the defaults nothing asks for the feature list and no request changes. */
    home();pair();assert(direct() && plain("library"));poll_job(none);
    assert(!p->search("cats",0,items_cb,NULL) && direct() && plain(""));poll_job(none);
    assert(!p->resolve(A,selected,NULL) && direct() && plain("playback"));resolved();
    /* Content language and country: the companion's lists with readable names and a Default entry. */
    assert(rows("settings:content-language","settings:content-language:",&marked)==50 && marked==1);
    assert(rows("settings:content-country","settings:content-country:",&marked)==90 && marked==1);
    show("settings:content-language");assert(!strcmp(last_items[0].id,"settings:content-language:default") && !strcmp(last_items[0].subtitle,"Selected"));
    show("settings:content-language:de");assert(find("settings:content-language:de")==8 && !strcmp(last_items[8].title,"German") && !strcmp(last_items[8].subtitle,"Selected"));
    assert(strcmp(last_items[0].subtitle,"Selected") && !strcmp(saved("guest","content_language"),"de"));
    show("settings:content-country:DE");assert(find("settings:content-country:DE")==14 && !strcmp(last_items[14].title,"Germany") && !strcmp(last_items[14].subtitle,"Selected"));
    p->list_catalog("settings:content-country:JO",3,items_cb,NULL);assert(last_ok && !strcmp(saved("guest","content_country"),"DE")); /* a later page changes nothing */
    assert(rows("settings:content-language","settings:content-language:",&marked)==50 && marked==1);
    assert(rows("settings:content-country","settings:content-country:",&marked)==90 && marked==1);
    const char *bad[]={"settings:content-language:xx","settings:content-language:EN","settings:content-language:","settings:content-language:de ",
        "settings:content-country:de","settings:content-country:XX","settings:content-country:","settings:content-country:DEU"};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;++i)refused(bad[i]);
    assert(!strcmp(saved("guest","content_language"),"de") && !strcmp(saved("guest","content_country"),"DE"));
    for(int page=0,found=0;!found;++page) {
        p->list_catalog("settings",page,items_cb,NULL);assert(last_ok && (last_more || find("settings:content-language")>=0));
        if(find("settings:content-language")<0)continue;
        found=1;assert(!strcmp(last_items[find("settings:content-language")].title,"Content language: German"));
        assert(!strcmp(last_items[find("settings:content-country")].title,"Content country: Germany"));
    }
    /* The first request that needs it reads the feature list once; every list, search, topic and resolve carries the locale. */
    home();health(200,all);assert(!strcmp(field("library","hl"),"de") && !strcmp(field("library","gl"),"DE"));
    assert(!strcmp(field("library","hide"),"<absent>") && !strcmp(field("library","thumbnails"),"<absent>"));poll_job(more);
    p->list_catalog("library:home",1,items_cb,NULL);assert(direct() && !strcmp(field("library","action"),"continue") && !strcmp(field("library","hl"),"de"));poll_job(none);
    assert(!p->search("cats",0,items_cb,NULL) && direct() && !strcmp(field("","hl"),"de") && !strcmp(field("","gl"),"DE") && !strcmp(field("","query"),"cats"));poll_job(none);
    p->list_catalog("section:news",0,items_cb,NULL);assert(direct() && !strcmp(field("library","action"),"topics") && !strcmp(field("library","hl"),"de") && !strcmp(field("library","gl"),"DE"));poll_job(none);
    p->list_catalog("playlist:PLabcdefghijk",0,items_cb,NULL);assert(direct() && !strcmp(field("","action"),"catalog") && !strcmp(field("","hl"),"de") && !strcmp(field("","gl"),"DE"));poll_job(none);
    p->list_catalog("library:subscriptions",0,items_cb,NULL);assert(direct() && !strcmp(field("library","gl"),"DE"));poll_job(none);
    p->list_catalog("description:abcdefghijk",0,items_cb,NULL);assert(direct() && !strcmp(field("library","action"),"description") && !strcmp(field("library","hl"),"de"));
    while(pending_count)reply(0,0,500,"{}");
    assert(!p->resolve(A,selected,NULL) && direct() && !strcmp(field("playback","hl"),"de") && !strcmp(field("playback","gl"),"<absent>") && !strcmp(field("","hl"),"<absent>"));resolved();
    /* A changed locale retires the pages opened with the earlier one. */
    home();poll_job(more);assert(last_more);
    show("settings:content-language:fr");
    p->list_catalog("library:home",1,items_cb,NULL);assert(!last_ok && !pending_count);
    home();assert(direct() && !strcmp(field("library","hl"),"fr"));poll_job(more);
    show("settings:content-country:default");p->list_catalog("library:home",1,items_cb,NULL);assert(!last_ok && !pending_count);
    home();assert(!strcmp(field("library","hl"),"fr") && !strcmp(field("library","gl"),"<absent>"));poll_job(none);
    /* The choice belongs to the profile and survives a restart; Default sends nothing. */
    show("library:profile:alice");home();assert(direct() && plain("library"));poll_job(none);
    show("settings:content-country:JP");
    show("library:profile:guest");home();assert(!strcmp(field("library","hl"),"fr") && !strcmp(field("library","gl"),"<absent>"));poll_job(none);
    p->shutdown();assert(!p->init());
    home();health(200,all);assert(!strcmp(field("library","hl"),"fr"));poll_job(none); /* the feature list is read again after a restart */
    show("library:profile:alice");home();assert(!strcmp(field("library","hl"),"<absent>") && !strcmp(field("library","gl"),"JP"));poll_job(none);
    show("settings:content-country:default");show("library:profile:guest");
    show("settings:content-language:default");home();assert(direct() && plain("library"));poll_job(none);
    assert(!*saved("guest","content_language") && !*saved("alice","content_country"));
    /* Corrupt values read as Default. */
    show("settings:content-language:ja");p->shutdown();
    cJSON *store=read_store(),*row=profile_row(store,"guest");
    assert(!strcmp(cJSON_GetObjectItem(row,"content_language")->valuestring,"ja"));
    cJSON_ReplaceItemInObject(row,"content_language",cJSON_CreateString("xx"));cJSON_ReplaceItemInObject(row,"content_country",cJSON_CreateNumber(5));
    cJSON_ReplaceItemInObject(profile_row(store,"alice"),"content_country",cJSON_CreateString("jp"));
    write_store(store);cJSON_Delete(store);assert(!p->init());
    home();assert(direct() && plain("library"));poll_job(none);
    show("library:profile:alice");home();assert(direct() && plain("library"));poll_job(none);
    show("library:profile:guest");
    /* An older companion: no feature list, or one without the feature, keeps requests as they were. */
    show("settings:content-language:it");
    home();health(404,"{\"detail\":\"Not Found\"}");assert(plain("library"));poll_job(none);
    home();health(200,"{\"features\":\"all\"}");assert(plain("library"));poll_job(none);
    home();health(200,basic);assert(plain("library"));poll_job(none);
    home();assert(direct() && plain("library"));poll_job(none); /* read once: it is known to lack the feature */
    /* Another source or a new pairing forgets what the earlier companion offered. */
    assert(!p->set_source("http://127.0.0.1:8766#123456"));
    home();pair();health(200,all);assert(!strcmp(field("library","hl"),"it"));poll_job(none);
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    home();pair();health(200,all);assert(!strcmp(field("library","hl"),"it"));poll_job(none);
    /* A refused field means the companion changed: the list is read again. */
    home();assert(direct());reply(0,1,422,"{\"detail\":[{\"type\":\"extra_forbidden\"}]}");assert(!last_ok && !pending_count);
    home();health(200,basic);assert(plain("library"));poll_job(none);
    home();assert(direct());reply(0,1,401,"{\"error\":{\"code\":\"unauthorized\",\"message\":\"Pair again.\"}}");assert(!last_ok);
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    home();pair();health(200,all);assert(!strcmp(field("library","hl"),"it"));poll_job(none);
    show("settings:content-language:default");
    /* Hide content: sixteen switches. Until one changes nothing is sent and Home keeps its own rule. */
    show("settings:hide");assert(last_count==17 && !strcmp(last_items[0].id,"settings:hide:shorts_all") && !strcmp(last_items[0].title,"Hide Shorts everywhere: off"));
    assert(!strcmp(last_items[3].title,"Hide watched from Home: on") && !strcmp(last_items[16].id,"settings:hide:reset") && strstr(last_items[16].subtitle,"Nothing was changed"));
    assert(!strcmp(saved("guest","hide"),"<absent>"));
    home();assert(direct() && plain("library"));poll_job(none);
    show("settings:hide:shorts_all");assert(!strcmp(last_items[0].title,"Hide Shorts everywhere: on") && !strcmp(last_items[3].title,"Hide watched from Home: on"));
    assert(!strcmp(saved("guest","hide"),"shorts_all,watched_home"));
    home();assert(direct() && !strcmp(field("library","hide"),"shorts_all,watched_home"));poll_job(more);
    p->list_catalog("library:home",1,items_cb,NULL);assert(!strcmp(field("library","action"),"continue") && !strcmp(field("library","hide"),"<absent>"));poll_job(none);
    assert(!p->search("cats",0,items_cb,NULL) && !strcmp(field("","hide"),"shorts_all,watched_home"));poll_job(none);
    p->list_catalog("section:trending",0,items_cb,NULL);assert(!strcmp(field("library","hide"),"shorts_all,watched_home"));poll_job(none);
    assert(!p->resolve(A,selected,NULL) && direct() && plain("playback") && plain(""));resolved();
    show("settings:hide:watched_home");assert(!strcmp(last_items[3].title,"Hide watched from Home: off") && !strcmp(saved("guest","hide"),"shorts_all"));
    show("settings:hide:shorts_all");assert(!*saved("guest","hide"));
    home();assert(!*field("library","hide"));poll_job(none); /* an empty selection is still the whole selection */
    for(int i=0;i<16;++i){char id[160];show("settings:hide");snprintf(id,sizeof id,"%s",last_items[i].id);show(id);assert(strstr(last_items[i].title,": on"));}
    assert(!strcmp(saved("guest","hide"),"shorts_all,mixes,watched_watch_later,watched_home,watched_subscriptions,watched_notifications,shorts_subscriptions,"
        "shorts_search,shorts_home,shorts_channel,shorts_history,shorts_trending,streams_subscriptions,upcoming_subscriptions,upcoming_home,upcoming_channel"));
    home();assert(strstr(field("library","hide"),"upcoming_channel") && strlen(pending[0].body)<2000);poll_job(more);
    show("settings:hide:reset");assert(!strcmp(last_items[3].title,"Hide watched from Home: on") && !strcmp(last_items[0].title,"Hide Shorts everywhere: off") && !strcmp(saved("guest","hide"),"<absent>"));
    p->list_catalog("library:home",1,items_cb,NULL);assert(!last_ok && !pending_count); /* retired with the changed selection */
    home();assert(plain("library"));poll_job(none);
    refused("settings:hide:bogus");refused("settings:hide:");refused("settings:hide:shorts_all ");
    /* An empty page with more to come is followed, a bounded number of times, only while a selection is sent. */
    show("settings:hide:mixes");
    item_calls=0;home();poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":true,\"cursor\":\"empty-1\"}}");
    assert(!item_calls && direct() && !strcmp(field("library","action"),"continue") && !strcmp(field("library","cursor"),"empty-1"));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"One\"}],\"has_more\":true,\"cursor\":\"after-one\"}}");
    assert(item_calls==1 && last_ok && last_count==1 && last_more && !pending_count);
    p->list_catalog("library:home",1,items_cb,NULL);assert(!strcmp(field("library","cursor"),"after-one"));poll_job(none);
    item_calls=0;home();
    for(int i=0;i<8;++i) {
        char body[256];snprintf(body,sizeof body,"{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":true,\"cursor\":\"empty-%d\"}}",i);
        poll_job(body);assert(!item_calls && direct());
    }
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":true,\"cursor\":\"empty-9\"}}");
    assert(item_calls==1 && last_ok && !last_count && last_more && !pending_count);
    show("settings:hide:reset");
    item_calls=0;home();poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":true,\"cursor\":\"empty-1\"}}");
    assert(item_calls==1 && last_ok && !last_count && last_more && !pending_count);
    /* Thumbnail source: default, start, middle or end, on lists and searches only. */
    show("settings:thumbnails");assert(last_count==4 && !strcmp(last_items[0].id,"settings:thumbnails:default") && !strcmp(last_items[0].subtitle,"Selected"));
    show("settings:thumbnails:middle");assert(!strcmp(last_items[2].subtitle,"Selected") && !strcmp(last_items[2].title,"Video middle") && !strcmp(saved("guest","thumbnail_source"),"middle"));
    home();assert(direct() && !strcmp(field("library","thumbnails"),"middle") && !strcmp(field("library","hide"),"<absent>"));poll_job(more);
    assert(!p->search("cats",0,items_cb,NULL) && !strcmp(field("","thumbnails"),"middle"));poll_job(none);
    assert(!p->resolve(A,selected,NULL) && plain("playback") && plain(""));resolved();
    refused("settings:thumbnails:sideways");refused("settings:thumbnails:");
    show("settings:thumbnails:end");home();assert(!strcmp(field("library","thumbnails"),"end"));poll_job(none);
    show("settings:thumbnails:start");home();assert(!strcmp(field("library","thumbnails"),"start"));poll_job(none);
    show("library:profile:alice");home();assert(plain("library"));poll_job(none);
    show("settings:hide:mixes");show("settings:thumbnails:end");
    p->list_catalog("playlist:pick:add:abcdefghijk",0,items_cb,NULL);
    assert(direct() && !strcmp(field("library","action"),"playlists") && !strcmp(field("library","thumbnails"),"end") && !strcmp(field("library","hide"),"<absent>"));poll_job(none);
    show("library:profile:guest");home();assert(!strcmp(field("library","thumbnails"),"start"));poll_job(none);
    show("settings:thumbnails:default");home();assert(plain("library"));poll_job(none);
    /* A companion without filters or thumbnail sources receives neither. */
    show("settings:hide:mixes");show("settings:thumbnails:end");show("settings:content-language:de");
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    home();pair();health(200,basic);assert(plain("library"));poll_job(none);
    show("settings:hide");assert(strstr(last_items[1].subtitle,"cannot filter"));
    item_calls=0;home();poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":true,\"cursor\":\"empty-1\"}}");
    assert(item_calls==1 && !pending_count);
    /* Corrupt selections: unknown names are dropped, a non-list is "nothing changed". */
    p->shutdown();store=read_store();
    assert(!strcmp(cJSON_GetObjectItem(profile_row(store,"guest"),"thumbnail_source")->valuestring,"end"));
    cJSON_ReplaceItemInObject(profile_row(store,"guest"),"hide",cJSON_Parse("[\"bogus\",\"mixes\",7,\"mixes\",\"upcoming_home\"]"));
    cJSON_ReplaceItemInObject(profile_row(store,"guest"),"thumbnail_source",cJSON_CreateString("sideways"));
    cJSON_ReplaceItemInObject(profile_row(store,"alice"),"hide",cJSON_CreateString("mixes"));
    cJSON_ReplaceItemInObject(profile_row(store,"alice"),"thumbnail_source",cJSON_CreateNumber(2));
    write_store(store);cJSON_Delete(store);assert(!p->init());
    home();health(200,all);assert(!strcmp(field("library","hide"),"mixes,upcoming_home") && !strcmp(field("library","thumbnails"),"<absent>") && !strcmp(field("library","hl"),"de"));poll_job(none);
    show("library:profile:alice");home();assert(direct() && plain("library"));poll_job(none);
    show("settings:hide");assert(!strcmp(last_items[3].title,"Hide watched from Home: on") && !strcmp(last_items[1].title,"Hide mixes: off"));
    show("library:profile:guest");show("settings:hide:reset");show("settings:content-language:default");
    /* SponsorBlock alternative server: asked for only when switched on and offered. */
    show("settings:sponsor");show("settings:sb");assert(last_count==17 && !strcmp(last_items[14].id,"settings:sb:alt") && !strcmp(last_items[14].title,"Use alternative server: off"));
    assert(!strcmp(last_items[15].title,"About SponsorBlock: https://sponsor.ajay.app") && !strcmp(last_items[16].title,"SponsorBlock status: https://status.sponsor.ajay.app"));
    show("settings:sb:about");assert(last_count==17);
    assert(!p->resolve(A,selected,NULL) && direct());resolved();
    assert(pending_count==1 && strstr(pending[0].url,"/v1/sponsor/abcdefghijk?categories=") && !strstr(pending[0].url,"alt="));reply(0,1,200,"{\"segments\":[]}");
    show("settings:sb:alt");assert(!strcmp(last_items[14].title,"Use alternative server: on") && !strcmp(saved("guest","sponsorblock_alt_server"),"true"));
    assert(!p->resolve(A,selected,NULL) && direct());resolved();
    assert(pending_count==1 && strstr(pending[0].url,"/v1/sponsor/abcdefghijk?categories=") && strstr(pending[0].url,"&alt=true"));reply(0,1,200,"{\"segments\":[]}");
    show("library:profile:alice");show("settings:sponsor");assert(!strcmp(saved("alice","sponsorblock_alt_server"),"false"));
    assert(!p->resolve(A,selected,NULL));resolved();assert(pending_count==1 && !strstr(pending[0].url,"alt="));reply(0,1,200,"{\"segments\":[]}");
    show("library:profile:guest");
    assert(!p->set_source("http://127.0.0.1:8765#123456")); /* a companion started without the server */
    assert(!p->resolve(A,selected,NULL));pair();health(200,"{\"api_version\":1,\"features\":[\"locale\"],\"sponsorblock_alt\":false}");resolved();
    assert(pending_count==1 && strstr(pending[0].url,"/v1/sponsor/") && !strstr(pending[0].url,"alt="));reply(0,1,200,"{\"segments\":[]}");
    show("settings:sb");assert(strstr(last_items[14].subtitle,"without an alternative"));
    assert(!p->set_source("http://127.0.0.1:8765#123456")); /* the feature may also be named in the list */
    assert(!p->resolve(A,selected,NULL));pair();health(200,"{\"api_version\":1,\"features\":[\"sponsorblock_alt\"]}");resolved();
    assert(pending_count==1 && strstr(pending[0].url,"&alt=true"));reply(0,1,200,"{\"segments\":[]}");
    show("settings:sb:alt");show("settings:sponsor");
    show("settings:dearrow");assert(last_count==5 && !strcmp(last_items[3].title,"About DeArrow: https://dearrow.ajay.app") && !strcmp(last_items[4].title,"DeArrow status: https://status.sponsor.ajay.app"));
    /* The feature list also says whether the library worker runs: a filtered search is not attempted without it. */
    show("settings:search:date");
    assert(!p->search("cats",0,items_cb,NULL) && direct() && !strcmp(field("","action"),"search") && !strcmp(field("","query"),"cats"));poll_job(none);
    assert(search_lead==2 && strstr(lead_items[0].subtitle,"library worker is off"));
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    home();pair();assert(direct());poll_job(none); /* nothing needs the list: it stays unread */
    assert(!p->search("cats",0,items_cb,NULL) && direct() && !strcmp(field("","action"),"library") && !strcmp(field("library","action"),"search"));
    while(pending_count)reply(0,0,500,"{}");
    show("settings:search:reset");
    /* Caption and audio languages: the same list with names, a reset entry and the selection mark. */
    assert(rows("settings:languages","settings:lang:",&marked)==49 && marked==1);
    assert(rows("settings:audio-languages","settings:audio:",&marked)==50 && marked==1);
    show("settings:languages");assert(!strcmp(last_items[0].id,"settings:lang:en") && !strcmp(last_items[0].title,"Default: English") && !strcmp(last_items[0].subtitle,"Selected"));
    show("settings:audio-languages");assert(!strcmp(last_items[0].id,"settings:audio:") && !strcmp(last_items[0].title,"Original audio") && !strcmp(last_items[0].subtitle,"Selected"));
    p->list_catalog("settings:lang:pt-BR",1,items_cb,NULL);assert(last_ok && !strcmp(saved("guest","language"),"en"));
    show("settings:lang:pt-BR");assert(!strcmp(saved("guest","language"),"pt-BR") && strcmp(last_items[0].subtitle,"Selected"));
    p->list_catalog("settings:languages",1,items_cb,NULL);assert(find("settings:lang:pt-BR")>=0 && !strcmp(last_items[find("settings:lang:pt-BR")].title,"Portuguese (Brazil)"));
    assert(!strcmp(last_items[find("settings:lang:pt-BR")].subtitle,"Selected"));
    show("settings:audio:ja");assert(!strcmp(saved("guest","audio_language"),"ja") && strcmp(last_items[0].subtitle,"Selected"));
    assert(rows("settings:audio-languages","settings:audio:",&marked)==50 && marked==1);
    assert(!p->resolve(A,selected,NULL) && !strcmp(field("playback","caption_language"),"pt-BR") && !strcmp(field("playback","audio_language"),"ja"));resolved();
    show("settings:audio:");assert(!*saved("guest","audio_language") && !strcmp(last_items[0].subtitle,"Selected"));
    show("settings:lang:en");assert(!strcmp(saved("guest","language"),"en") && !strcmp(last_items[0].subtitle,"Selected"));
    const char *wrong[]={"settings:lang:xx","settings:lang:","settings:lang:EN","settings:audio:xx","settings:audio:..","settings:lang:pt-BR "};
    for(unsigned i=0;i<sizeof wrong/sizeof *wrong;++i)refused(wrong[i]);
    assert(!strcmp(saved("guest","language"),"en") && !*saved("guest","audio_language"));
    /* A code saved by the earlier ten-entry list stays selected and listed until replaced. */
    show("settings:lang:pt");show("settings:audio:zh");
    assert(rows("settings:languages","settings:lang:",&marked)==50 && !marked); /* its own row carries the longer mark */
    show("settings:languages");assert(!strcmp(last_items[1].id,"settings:lang:pt") && strstr(last_items[1].subtitle,"earlier version"));
    show("settings:audio-languages");assert(!strcmp(last_items[1].id,"settings:audio:zh") && strstr(last_items[1].subtitle,"earlier version"));
    show("settings:lang:fr");assert(rows("settings:languages","settings:lang:",&marked)==49 && marked==1);
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: feature-gated content language, country, hide switches, empty filtered pages, thumbnail source, alternative SponsorBlock server, worker detection and language lists");
    return 0;
}
