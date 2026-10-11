/* Open description: request, reader text, header rows, failures and scope retirement. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *p;
static char text_row[160];
static const char *field(const char *key) {
    static char value[128];
    cJSON *job=cJSON_Parse(pending[0].body),*item=cJSON_GetObjectItem(cJSON_GetObjectItem(job,"library"),key);
    snprintf(value,sizeof value,"%s",cJSON_IsString(item)?item->valuestring:"<absent>");
    cJSON_Delete(job);return value;
}
/* One description reply; text is repeated to the requested length. */
static void answer(const char *id,const char *title,const char *channel,const char *channel_id,const char *published,const char *views,size_t length,int truncated) {
    char *body=malloc(length+1024);assert(body);
    int used=snprintf(body,1024,"{\"state\":\"done\",\"result\":{\"api_version\":1,\"kind\":\"description\",\"id\":\"%s\",\"title\":\"%s\",\"channel\":\"%s\","
        "\"channel_id\":\"%s\",\"published\":\"%s\",\"views\":\"%s\",\"truncated\":%s,\"description\":\"",id,title,channel,channel_id,published,views,truncated?"true":"false");
    for(size_t i=0;i<length;++i)body[used++]=i%64==63?' ':(char)('a'+i%26);
    strcpy(body+used,"\"}}");poll_job(body);free(body);
}
static void ask(const char *id) {
    char parent[64];snprintf(parent,sizeof parent,"description:%s",id);
    p->list_catalog(parent,0,items_cb,NULL);
    assert(pending_count==1 && !strcmp(field("action"),"description") && !strcmp(field("id"),id));
}
int main(void) {
    char dir[]="/tmp/tube-description-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->list_catalog("playlist:PLfixture01",0,items_cb,NULL);
    reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"First\"}],\"has_more\":false}}");
    /* The video menu offers the reader; the request names the video and the profile. */
    p->list_catalog("account:menu:abcdefghijk",0,items_cb,NULL);
    int offered=-1;for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].title,"Open description"))offered=i;
    assert(offered>=0 && !strcmp(last_items[offered].id,"description:abcdefghijk") && last_items[offered].is_folder);
    ask("abcdefghijk");assert(!strcmp(field("profile"),"guest") && !strcmp(field("query"),"<absent>"));
    answer("abcdefghijk","Video title","Fixture channel","UCaaaaaaaaaaaaaaaaaaaaaa","2026-01-02","1234567",1000,0);
    /* The first row carries the whole text; the header rows follow. */
    assert(last_ok && last_count==4 && !last_more && !strcmp(last_items[0].title,"Video title") && last_items[0].is_folder);
    assert(!strncmp(last_items[0].id,"description:text:",17) && !strcmp(last_items[0].subtitle,"Description; L2 / R2 scroll"));
    snprintf(text_row,sizeof text_row,"%s",last_items[0].id);
    assert(strlen(tube_comment_text(text_row))==1000 && strlen(last_items[0].overview)==319 && !strncmp(tube_comment_text(text_row),"abcdefghij",10));
    assert(!strcmp(last_items[1].id,"channel:UCaaaaaaaaaaaaaaaaaaaaaa") && !strcmp(last_items[1].title,"Fixture channel") && last_items[1].is_folder);
    assert(!strcmp(last_items[2].title,"2026-01-02") && !strcmp(last_items[2].subtitle,"Published"));
    assert(!strcmp(last_items[3].title,"1,234,567 views") && !strcmp(last_items[3].subtitle,"Views"));
    for(int i=0;i<4;++i)for(int j=i+1;j<4;++j)assert(strcmp(last_items[i].id,last_items[j].id));
    assert(tube_toggle_favorite("abcdefghijk")==1); /* a save while the text is loaded never stores it */
    cJSON *store=read_store();char *saved=cJSON_PrintUnformatted(store);assert(!strstr(saved,"lmnopqrstuvwxyz"));free(saved);cJSON_Delete(store);
    p->list_catalog(text_row,0,items_cb,NULL);assert(last_ok && last_count==1 && !pending_count && !strcmp(last_items[0].id,"account:notice"));
    p->list_catalog("account:notice:views",0,items_cb,NULL);assert(last_ok && last_count==1 && !pending_count);
    /* The longest text the companion sends is kept whole; a longer one is refused. */
    ask("abcdefghijk");answer("abcdefghijk","Long","","","","",60000,1);
    assert(last_ok && last_count==1 && !strcmp(last_items[0].subtitle,"Description, shortened by the companion; L2 / R2 scroll"));
    assert(strlen(tube_comment_text(last_items[0].id))==60000 && !*tube_comment_text(text_row)); /* one text at a time */
    snprintf(text_row,sizeof text_row,"%s",last_items[0].id);
    ask("abcdefghijk");answer("abcdefghijk","Too long","","","","",60004,0);assert(!last_ok && strstr(error_text,"description"));
    /* Missing metadata leaves no header row; an empty text says so. */
    ask("abcdefghijk");answer("abcdefghijk","","Unknown identity","not-a-channel","","1",0,0);
    assert(last_ok && last_count==3 && !strcmp(last_items[0].title,"Description") && !strcmp(tube_comment_text(last_items[0].id),"This video has no description."));
    assert(!strcmp(last_items[1].id,"account:notice:channel") && !strcmp(last_items[1].title,"Unknown identity") && !strcmp(last_items[2].title,"1 view"));
    ask("abcdefghijk");answer("abcdefghijk","Counts","","","","1000",5,0);assert(last_count==2 && !strcmp(last_items[1].title,"1,000 views"));
    ask("abcdefghijk");answer("abcdefghijk","Counts","","","","999",5,0);assert(!strcmp(last_items[1].title,"999 views"));
    snprintf(text_row,sizeof text_row,"%s",last_items[0].id);
    /* A reply for another video, another kind or malformed fields is refused. */
    ask("abcdefghijk");answer("bcdefghijkl","Other","","","","",5,0);assert(!last_ok && strstr(error_text,"description"));
    ask("abcdefghijk");answer("abcdefghijk","Views","","","","12a",5,0);assert(!last_ok);
    ask("abcdefghijk");poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"id\":\"abcdefghijk\",\"description\":\"text\"}}");assert(!last_ok);
    ask("abcdefghijk");poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"description\",\"id\":\"abcdefghijk\",\"description\":7}}");assert(!last_ok);
    ask("abcdefghijk");poll_job("{\"state\":\"failed\",\"error\":{\"code\":\"upstream_format\",\"message\":\"The description could not be read.\"}}");
    assert(!last_ok && !pending_count && strstr(error_text,"could not be read"));
    assert(!strcmp(tube_comment_text(text_row),"abcde")); /* a failed reload keeps the text on screen */
    const char *bad[]={"description:","description:short","description:abcdefghijk1","description:abcdefghij!"};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;++i){p->list_catalog(bad[i],0,items_cb,NULL);assert(!last_ok && !pending_count);}
    p->list_catalog("description:abcdefghijk",1,items_cb,NULL);assert(!last_ok && !pending_count);
    /* Pending: nothing is shown until the companion answers; leaving cancels it. */
    ask("abcdefghijk");item_calls=0;p->list_catalog("history",0,items_cb,NULL);assert(item_calls==1 && last_ok);
    reply(0,1,200,"{\"job\":\"late\",\"state\":\"pending\"}");assert(item_calls==2 && !last_ok && !pending_count);
    assert(!strcmp(tube_comment_text(text_row),"abcde"));
    /* The text belongs to its profile and source. */
    p->list_catalog("library:profile:alice",0,items_cb,NULL);assert(!*tube_comment_text(text_row));
    ask("abcdefghijk");assert(!strcmp(field("profile"),"alice"));answer("abcdefghijk","Alice","","","","",5,0);
    snprintf(text_row,sizeof text_row,"%s",last_items[0].id);assert(!strcmp(tube_comment_text(text_row),"abcde"));
    assert(!p->set_source("http://127.0.0.1:8766#123456"));assert(!*tube_comment_text(text_row));
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: description request, reader text and header rows, bounds, malformed replies, pending and failed states, profile and source retirement");
}
