/* Not interested / Don't recommend channel: Home references, one submission, confirmation and local removal. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *p;
static char apply[160];
static const char *field(const char *key) {
    static char value[128];
    cJSON *job=cJSON_Parse(pending[0].body),*item=cJSON_GetObjectItem(cJSON_GetObjectItem(job,"library"),key);
    snprintf(value,sizeof value,"%s",cJSON_IsString(item)?item->valuestring:"<absent>");
    cJSON_Delete(job);return value;
}
static const char *page="{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":["
    "{\"id\":\"abcdefghijk\",\"title\":\"Card one\",\"subtitle\":\"Channel A\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"feedback_cursor\":\"feedback-ref-1\",\"feedback_not_interested\":true,\"feedback_channel\":true},"
    "{\"id\":\"bcdefghijkl\",\"title\":\"Card two\",\"subtitle\":\"Channel A\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"feedback_cursor\":\"feedback-ref-2\",\"feedback_not_interested\":true},"
    "{\"id\":\"cdefghijklm\",\"title\":\"Card three\",\"subtitle\":\"Channel B\",\"channel_id\":\"UCbbbbbbbbbbbbbbbbbbbbbb\"},"
    "{\"id\":\"defghijklmn\",\"title\":\"Card four\",\"subtitle\":\"Channel C\",\"channel_id\":\"UCcccccccccccccccccccccc\",\"feedback_cursor\":\"bad cursor!\",\"feedback_not_interested\":true}],\"has_more\":false}}";
static void home(const char *parent) {p->list_catalog(parent,0,items_cb,NULL);assert(pending_count==1);poll_job(page);assert(last_ok);}
/* Bit 1: Not interested offered, bit 2: Don't recommend channel offered. */
static int offers(const char *id) {
    char parent[64];int found=0,rows=0,more=1;snprintf(parent,sizeof parent,"account:menu:%s",id);
    for(int page=0;more;++page) { /* with both choices the menu runs onto a second page */
        p->list_catalog(parent,page,items_cb,NULL);assert(last_ok && !pending_count);
        for(int i=0;i<last_count;++i) {
            if(!strcmp(last_items[i].title,"Not interested")){found|=1;assert(!strncmp(last_items[i].id,"account:confirm:not_interested:",31));}
            if(!strcmp(last_items[i].title,"Don't recommend channel")){found|=2;assert(!strncmp(last_items[i].id,"account:confirm:dont_recommend_channel:",39));}
            if(!strcmp(last_items[i].title,"Refresh section"))found|=4;
        }
        rows+=last_count;more=last_more;
    }
    assert(found&4 && rows<=48);
    return found&3;
}
static int confirm(const char *action,const char *id) {
    char parent[96];snprintf(parent,sizeof parent,"account:confirm:%s:%s",action,id);
    p->list_catalog(parent,0,items_cb,NULL);assert(!pending_count);
    if(last_ok){assert(last_count==1 && strstr(last_items[0].subtitle,"fixture"));snprintf(apply,sizeof apply,"%s",last_items[0].id);}
    return last_ok;
}
static void submit(const char *action,const char *id,const char *cursor) {
    assert(confirm(action,id));p->list_catalog(apply,0,items_cb,NULL);
    assert(pending_count==1 && !strcmp(field("action"),"feedback") && !strcmp(field("id"),id) && !strcmp(field("cursor"),cursor));
    assert(!strcmp(field("feedback"),action) && !strcmp(field("profile"),"fixture"));
}
int main(void) {
    char dir[]="/tmp/tube-feedback-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->list_catalog("library:profile:fixture",0,items_cb,NULL);
    p->list_catalog("library:home",0,items_cb,NULL);
    reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");poll_job(page);assert(last_ok && last_count==4);
    /* The menu offers exactly the choices the card carries; references stay in memory. */
    assert(offers("abcdefghijk")==3 && offers("bcdefghijkl")==1 && !offers("cdefghijklm") && !offers("defghijklmn"));
    assert(tube_toggle_favorite("abcdefghijk")==1);
    cJSON *store=read_store();char *saved=cJSON_PrintUnformatted(store);assert(!strstr(saved,"feedback"));free(saved);cJSON_Delete(store);
    assert(!confirm("dont_recommend_channel","bcdefghijkl") && !confirm("not_interested","cdefghijklm") && !confirm("not_interested","defghijklmn"));
    /* A reply naming another video or choice, or no success, confirms nothing; the reference is spent. */
    submit("not_interested","abcdefghijk","feedback-ref-1");
    p->list_catalog(apply,0,items_cb,NULL);assert(pending_count==1); /* a second press submits nothing */
    poll_job("{\"state\":\"done\",\"result\":{\"api_version\":1,\"success\":true,\"video_id\":\"bcdefghijkl\",\"feedback\":\"not_interested\"}}");
    assert(!last_ok && strstr(error_text,"feedback"));
    p->list_catalog(apply,0,items_cb,NULL);assert(!pending_count);
    assert(!confirm("not_interested","abcdefghijk") && !confirm("dont_recommend_channel","abcdefghijk") && !offers("abcdefghijk"));
    home("library:home");assert(last_count==4 && offers("abcdefghijk")==3);
    submit("dont_recommend_channel","abcdefghijk","feedback-ref-1");
    poll_job("{\"state\":\"done\",\"result\":{\"api_version\":1,\"success\":true,\"video_id\":\"abcdefghijk\",\"feedback\":\"not_interested\"}}");assert(!last_ok);
    home("library:home");submit("not_interested","abcdefghijk","feedback-ref-1");
    poll_job("{\"state\":\"done\",\"result\":{\"api_version\":1,\"success\":false,\"video_id\":\"abcdefghijk\",\"feedback\":\"not_interested\"}}");assert(!last_ok);
    home("library:home");submit("not_interested","abcdefghijk","feedback-ref-1");
    poll_job("{\"state\":\"failed\",\"error\":{\"code\":\"feedback_unconfirmed\",\"message\":\"YouTube did not confirm the feedback.\"}}");
    assert(!last_ok && strstr(error_text,"did not confirm"));
    home("library:home");assert(last_count==4); /* nothing was removed */
    /* Listing the video anywhere else retires its Home reference. */
    p->list_catalog("library:subscriptions",0,items_cb,NULL);poll_job(page);assert(last_count==4 && !offers("abcdefghijk") && !offers("bcdefghijkl"));
    assert(!confirm("not_interested","abcdefghijk"));
    /* A confirmed Not interested removes that card from Home lists only. */
    home("library:filter:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");assert(offers("bcdefghijkl")==1);
    submit("not_interested","bcdefghijkl","feedback-ref-2");
    poll_job("{\"state\":\"done\",\"result\":{\"api_version\":1,\"success\":true,\"video_id\":\"bcdefghijkl\",\"feedback\":\"not_interested\"}}");
    assert(last_ok && last_count==1 && !strcmp(last_items[0].title,"YouTube account updated"));
    p->list_catalog(apply,0,items_cb,NULL);assert(last_ok && !pending_count);
    home("library:home");assert(last_count==3 && !strcmp(last_items[0].id,"abcdefghijk") && !strcmp(last_items[1].id,"cdefghijklm"));
    assert(!confirm("not_interested","bcdefghijkl")); /* the hidden card gets no new reference */
    p->list_catalog("library:subscriptions",0,items_cb,NULL);poll_job(page);assert(last_count==4);
    /* Don't recommend channel also removes the channel's other Home cards. */
    home("library:home");submit("dont_recommend_channel","abcdefghijk","feedback-ref-1");
    poll_job("{\"state\":\"done\",\"result\":{\"api_version\":1,\"success\":true,\"video_id\":\"abcdefghijk\",\"feedback\":\"dont_recommend_channel\"}}");assert(last_ok);
    home("library:home");assert(last_count==2 && !strcmp(last_items[0].id,"cdefghijklm") && !strcmp(last_items[1].id,"defghijklmn"));
    home("library:filter:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");assert(last_count==2);
    /* Another profile keeps its own Home and cannot use this profile's confirmation. */
    home("library:home");
    p->list_catalog("library:profile:other",0,items_cb,NULL);
    home("library:home");assert(last_count==4 && offers("abcdefghijk")==3);
    p->list_catalog("account:confirm:not_interested:abcdefghijk",0,items_cb,NULL);assert(last_ok);snprintf(apply,sizeof apply,"%s",last_items[0].id);
    p->list_catalog("library:profile:fixture",0,items_cb,NULL);
    p->list_catalog(apply,0,items_cb,NULL);assert(!last_ok && !pending_count);
    assert(!offers("cdefghijklm") && !confirm("not_interested","abcdefghijk"));
    home("library:home");assert(last_count==4); /* a new browsing scope: removals lasted for the earlier visit */
    /* Guest never sees the choices, whatever a page carries. */
    p->list_catalog("library:profile:guest",0,items_cb,NULL);home("library:home");assert(last_count==4 && !offers("abcdefghijk"));
    p->list_catalog("account:confirm:not_interested:abcdefghijk",0,items_cb,NULL);assert(!last_ok && !pending_count);
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: Home feedback references, menu choices, one submission, confirmation checks, local removal, list and profile retirement");
}
