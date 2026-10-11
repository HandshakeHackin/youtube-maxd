/* Comments of the third batch: the count and the viewer's own state on each row, a new
 * comment and a reply typed in the text prompt, reviewed, and posted by one confirmation. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *p;
static char apply[160];
static const char *A="abcdefghijk";
static const char *third="{\"api_version\":1,\"features\":[\"search\",\"library\",\"channel_pages\",\"comment_post\"]}";
static const char *older="{\"api_version\":1,\"features\":[\"search\",\"library\",\"channel_pages\"]}";
static void show(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(last_ok && !pending_count);}
static void refused(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(!last_ok && !pending_count);}
static int find(const char *title) {for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].title,title))return i;return -1;}
static void pair(void) {assert(pending_count==1 && strstr(pending[0].url,"/v1/pair"));reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");}
static void health(const char *body) {
    assert(pending_count==1 && !strcmp(pending[0].method,"GET") && strstr(pending[0].url,"/v1/health"));reply(0,1,200,body);
}
static int direct(void) {return pending_count==1 && !strcmp(pending[0].method,"POST") && strstr(pending[0].url,"/v1/jobs")!=NULL;}
static const char *field(const char *key) {
    static char value[1024];
    cJSON *job=cJSON_Parse(pending[0].body),*item=cJSON_GetObjectItem(cJSON_GetObjectItem(job,"library"),key);
    assert(job);snprintf(value,sizeof value,"%s",cJSON_IsString(item)?item->valuestring:"<absent>");
    cJSON_Delete(job);return value;
}
static void done(const char *result) {
    char body[1024];snprintf(body,sizeof body,"{\"state\":\"done\",\"result\":%s}",result);poll_job(body);
}
static const char *page="{\"state\":\"done\",\"result\":{\"kind\":\"comments\",\"comment_count\":\"1,234 Comments\",\"can_post\":true,\"items\":["
    "{\"id\":\"c1\",\"author\":\"Ann\",\"content\":\"First!\",\"published\":\"1 day ago\",\"likes\":\"5\",\"replies\":\"2\",\"pinned\":true,\"owner\":true,"
    "\"liked\":true,\"hearted\":true,\"member\":true,\"rating_cursor\":\"rate-1\",\"rating_like\":true,\"respond_cursor\":\"respond-1\",\"reply_cursor\":\"replies-1\"},"
    "{\"id\":\"c2\",\"author\":\"Bob\",\"content\":\"Second\",\"published\":\"2 days ago\",\"likes\":\"0\",\"replies\":\"0\",\"disliked\":true,"
    "\"rating_cursor\":\"rate-2\",\"rating_remove\":true,\"respond_cursor\":\"bad cursor!\"},"
    "{\"id\":\"c3\",\"author\":\"Cy\",\"content\":\"Third\",\"published\":\"3 days ago\",\"likes\":\"1\",\"replies\":\"0\",\"respond_cursor\":\"respond-3\"}],\"has_more\":false}}";
static const char *plain="{\"state\":\"done\",\"result\":{\"kind\":\"comments\",\"items\":["
    "{\"id\":\"c1\",\"author\":\"Ann\",\"content\":\"First!\",\"published\":\"1 day ago\",\"likes\":\"5\",\"replies\":\"2\"}],\"has_more\":false}}";
static void comments(const char *body) {p->list_catalog("discussion:top:abcdefghijk",0,items_cb,NULL);assert(direct());poll_job(body);assert(last_ok);}
/* The prompt armed by a screen, answered with this text: the folder that opens, or "" when refused. */
static const char *typed(const char *text) {
    static char parent[80];tube_playlist_prompt_t prompt;
    parent[0]=0;assert(tube_take_playlist_prompt(&prompt) && !tube_take_playlist_prompt(&prompt) && !*prompt.initial);
    if(tube_submit_playlist_name(prompt.ticket,text,parent,sizeof parent))parent[0]=0;
    return parent;
}
static void review(const char *parent,const char *title,const char *text) {
    show(parent);assert(last_count==1 && !strncmp(last_items[0].id,"account:apply:",14) && !strcmp(last_items[0].title,title));
    assert(strstr(last_items[0].overview,text) && strstr(last_items[0].overview,"fixture") && strstr(last_items[0].overview,A));
    assert(!strcmp(tube_comment_text(last_items[0].id),last_items[0].overview));
    snprintf(apply,sizeof apply,"%s",last_items[0].id);
}
static void press(void) {p->list_catalog(apply,0,items_cb,NULL);}
int main(void) {
    char dir[]="/tmp/tube-comment-post-XXXXXX",parent[80],session[80],long_text[700];
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    /* Guest reads the count; nothing offers writing, whatever the page says. */
    p->list_catalog("discussion:top:abcdefghijk",0,items_cb,NULL);pair();assert(direct());poll_job(page);
    assert(last_ok && last_count==4 && !strcmp(last_items[0].title,"1,234 Comments") && !strcmp(last_items[0].id,"account:notice:comments"));
    assert(!strcmp(last_items[1].title,"Ann") && !*tube_comment_rating(last_items[1].id));
    refused("comment:post:abcdefghijk");refused("comment:reply:respond-1");
    comments(plain);assert(last_count==1 && !strcmp(last_items[0].title,"Ann")); /* an older companion: the list as it was */
    show("library:profile:fixture");
    comments(plain);assert(last_count==1);refused("comment:post:abcdefghijk"); /* the page did not say the profile may post */
    /* The first page leads with the count and the way to write; rows show the viewer's own state. */
    comments(page);
    assert(last_count==4 && !strcmp(last_items[0].title,"Write a comment") && !strcmp(last_items[0].id,"comment:post:abcdefghijk"));
    assert(!strcmp(last_items[0].subtitle,"1,234 Comments"));
    assert(strstr(last_items[1].subtitle,"Pinned") && strstr(last_items[1].subtitle,"Creator") && strstr(last_items[1].subtitle,"Member"));
    assert(strstr(last_items[1].subtitle,"Creator's heart") && strstr(last_items[1].subtitle,"You like it") && !strstr(last_items[1].subtitle,"You dislike it"));
    assert(strstr(last_items[2].subtitle,"You dislike it") && !strstr(last_items[2].subtitle,"Member") && !strstr(last_items[3].subtitle,"You"));
    assert(!strcmp(tube_comment_text(last_items[1].id),"First!") && !strcmp(tube_comment_rating(last_items[1].id),"rate-1"));
    assert(!strcmp(tube_comment_rating(last_items[2].id),"rate-2") && !strcmp(tube_comment_text(last_items[3].id),"Third"));
    /* A new comment: prompt, review, one submission. */
    show("comment:post:abcdefghijk");assert(last_count==1 && !strcmp(last_items[0].title,"Enter your comment"));
    assert(!*typed("   ") && strstr(tube_prompt_error(),"comment"));
    show("comment:post:abcdefghijk");
    snprintf(parent,sizeof parent,"%s",typed("  Nice video \"indeed\"  "));assert(!strncmp(parent,"comment:review:",15) && !strcmp(tube_prompt_title(parent),"Confirm comment"));
    review(parent,"Post this comment","Nice video \"indeed\"");
    press();health(third);
    assert(direct() && !strcmp(field("action"),"comment_post") && !strcmp(field("id"),A) && !strcmp(field("text"),"Nice video \"indeed\""));
    assert(!strcmp(field("profile"),"fixture") && !strcmp(field("title"),"<absent>") && !strcmp(field("cursor"),"<absent>"));
    snprintf(session,sizeof session,"%s",field("session"));assert(strlen(session)>8 && strlen(session)<=64);
    press();assert(last_ok && pending_count==1 && !strcmp(last_items[0].title,"Account change pending")); /* a second press submits nothing */
    snprintf(long_text,sizeof long_text,"{\"api_version\":1,\"success\":true,\"id\":\"abcdefghijk\",\"session\":\"%s\",\"reply\":false}",session);done(long_text);
    assert(last_ok && last_count==1 && !strcmp(last_items[0].title,"Comment posted") && !pending_count);
    press();assert(last_ok && !pending_count);
    refused(parent); /* the reviewed text is spent */
    /* A reply names the comment's reference; a comment without a usable one offers none. */
    show("comment:menu:rate-1");assert(find("Like comment")==0 && find("Reply to comment")==1 && !strcmp(last_items[1].id,"comment:reply:respond-1"));
    show("comment:menu:rate-2");assert(find("Reply to comment")<0);
    show("comment:reply:respond-1");assert(!strcmp(last_items[0].title,"Enter your reply"));
    snprintf(parent,sizeof parent,"%s",typed("Thanks"));review(parent,"Post this reply","Thanks");
    press();assert(direct() && !strcmp(field("action"),"comment_reply") && !strcmp(field("id"),A) && !strcmp(field("cursor"),"respond-1") && !strcmp(field("text"),"Thanks"));
    assert(strcmp(field("session"),session)); /* one confirmation, one attempt */
    snprintf(session,sizeof session,"%s",field("session"));
    snprintf(long_text,sizeof long_text,"{\"api_version\":1,\"success\":true,\"id\":\"abcdefghijk\",\"session\":\"%s\",\"reply\":true}",session);done(long_text);
    assert(last_ok && !strcmp(last_items[0].title,"Comment posted"));
    show("comment:reply:respond-3");snprintf(parent,sizeof parent,"%s",typed("Agreed"));review(parent,"Post this reply","Agreed");
    /* A confirmation for another video, attempt or kind is not accepted, and nothing is sent again. */
    const char *wrong[]={"{\"api_version\":1,\"success\":true,\"id\":\"bcdefghijkl\",\"session\":\"%s\",\"reply\":true}",
        "{\"api_version\":1,\"success\":true,\"id\":\"abcdefghijk\",\"session\":\"%s-x\",\"reply\":true}",
        "{\"api_version\":1,\"success\":true,\"id\":\"abcdefghijk\",\"session\":\"%s\",\"reply\":false}",
        "{\"api_version\":1,\"success\":true,\"id\":\"abcdefghijk\",\"session\":\"%s\"}",
        "{\"api_version\":1,\"success\":false,\"id\":\"abcdefghijk\",\"session\":\"%s\",\"reply\":true}"};
    for(unsigned i=0;i<sizeof wrong/sizeof *wrong;++i) {
        show("comment:reply:respond-3");snprintf(parent,sizeof parent,"%s",typed("Agreed"));review(parent,"Post this reply","Agreed");
        press();assert(direct());snprintf(session,sizeof session,"%s",field("session"));
        snprintf(long_text,sizeof long_text,wrong[i],session);done(long_text);assert(!last_ok && !pending_count);
        press();assert(last_ok && !pending_count && !strcmp(last_items[0].title,"Account change not confirmed"));
    }
    show("comment:reply:respond-3");snprintf(parent,sizeof parent,"%s",typed("Agreed"));review(parent,"Post this reply","Agreed");press();
    poll_job("{\"state\":\"failed\",\"error\":{\"code\":\"comment_unconfirmed\",\"message\":\"YouTube did not confirm the comment.\"}}");
    assert(!last_ok && strstr(error_text,"did not confirm"));press();assert(!pending_count);
    /* Text rules of the prompt: 1 to 150 characters, no control characters. */
    show("comment:post:abcdefghijk");memset(long_text,'a',151);long_text[151]=0;assert(!*typed(long_text));
    show("comment:post:abcdefghijk");assert(!*typed("line\nbreak"));
    show("comment:post:abcdefghijk");long_text[150]=0;snprintf(parent,sizeof parent,"%s",typed(long_text));review(parent,"Post this comment",long_text);
    press();assert(direct() && strlen(field("text"))==150);reply(0,0,500,"{}");assert(!last_ok);
    /* Stale and foreign references. */
    refused("comment:post:bcdefghijkl");refused("comment:reply:respond-2");refused("comment:reply:bad cursor!");refused("comment:review:0");
    refused("comment:review:999999");refused("comment:post:");p->list_catalog("comment:post:abcdefghijk",1,items_cb,NULL);assert(!last_ok);
    show("comment:post:abcdefghijk");snprintf(parent,sizeof parent,"%s",typed("Mine"));
    show("library:profile:alice");refused(parent);refused("comment:post:abcdefghijk");refused("comment:reply:respond-1");
    show("library:profile:fixture");refused(parent);refused("comment:post:abcdefghijk"); /* the discussion is read again first */
    /* Replies carry the thread's video; their page has no lead row. */
    comments(page);
    p->list_catalog("discussion:replies:replies-1",0,items_cb,NULL);assert(direct() && !strcmp(field("action"),"replies"));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"comments\",\"comment_count\":\"9\",\"can_post\":true,\"items\":["
        "{\"id\":\"r1\",\"author\":\"Dee\",\"content\":\"Reply\",\"published\":\"now\",\"likes\":\"0\",\"replies\":\"0\",\"rating_cursor\":\"rate-9\",\"rating_like\":true,\"respond_cursor\":\"respond-9\"}],\"has_more\":false}}");
    assert(last_ok && last_count==1 && !strcmp(last_items[0].title,"Dee"));
    show("comment:menu:rate-9");assert(find("Reply to comment")==1);
    show("comment:reply:respond-9");snprintf(parent,sizeof parent,"%s",typed("To Dee"));review(parent,"Post this reply","To Dee");
    press();assert(direct() && !strcmp(field("id"),A) && !strcmp(field("cursor"),"respond-9"));reply(0,0,500,"{}");
    /* A companion without comment posting is never sent the action. */
    assert(!p->set_source("http://127.0.0.1:8766#123456"));show("library:profile:fixture");
    p->list_catalog("discussion:top:abcdefghijk",0,items_cb,NULL);pair();assert(direct());poll_job(page);
    show("comment:post:abcdefghijk");snprintf(parent,sizeof parent,"%s",typed("Hello"));review(parent,"Post this comment","Hello");
    press();health(older);assert(!last_ok && !pending_count && strstr(error_text,"does not offer"));
    press();assert(last_ok && !pending_count && !strcmp(last_items[0].title,"Account change not confirmed"));
    p->shutdown();
    puts("PASS: comment count and state flags, posting a comment and a reply through prompt, review and one confirmed submission, stale references and feature gating");
}
