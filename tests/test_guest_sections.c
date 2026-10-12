/* Guest Home without recommendations, sections YouTube withholds (also as the startup section) and an empty last page. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *p;
static char row[160];
static void show(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(last_ok && !pending_count);}
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
static const char *videos="{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"Real title\",\"subtitle\":\"Real channel\","
    "\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\"}],\"has_more\":true,\"cursor\":\"next-page\"}}";
static const char *none="{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":false}}";
static const char *withheld="{\"state\":\"failed\",\"error\":{\"code\":\"catalog_unavailable\",\"message\":\"YouTube does not offer this section here.\"}}";
static const char *broken="{\"state\":\"failed\",\"error\":{\"code\":\"library_upstream\",\"message\":\"YouTube could not be read. Retry.\"}}";
int main(void) {
    char dir[]="/tmp/tube-guest-sections-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    SEPARATE_SETTINGS();
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    /* A new installation starts in the main menu, which leads with Discover: no list is asked of YouTube. */
    assert(!*tube_boot_section() && !tube_select_account_on_boot() && !strcmp(tube_boot_section_title(),"Main menu"));
    show("");assert(last_count>3 && !strcmp(last_items[0].id,"discover"));
    /* Guest Home without rows explains itself and leads on. */
    p->list_catalog("library:home",0,items_cb,NULL);
    reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    assert(!strcmp(field("action"),"home") && !strcmp(field("profile"),"guest"));
    poll_job(none);assert(last_ok && last_count==4 && !last_more && !pending_count);
    assert(!strcmp(last_items[0].id,"account:notice") && strstr(last_items[0].subtitle,"only to signed-in accounts"));
    assert(!strcmp(last_items[1].id,"account:notice") && !strcmp(last_items[1].title,"Search"));
    assert(!strcmp(last_items[2].id,"discover") && !strcmp(last_items[3].id,"library") && !strcmp(last_items[3].title,"Accounts & library"));
    for(int i=0;i<4;++i)assert(last_items[i].is_folder);
    show(last_items[2].id);assert(find("Trending")>=0);show("library");assert(find("Sign in to YouTube")>=0 && !strcmp(row,"auth:start"));
    /* Recommendations, when YouTube sends them, are listed as before. */
    p->list_catalog("library:home",0,items_cb,NULL);poll_job(videos);
    assert(last_ok && last_count==1 && last_more && !strcmp(last_items[0].id,"abcdefghijk"));
    /* An account profile keeps its own empty Home. */
    show("library:profile:fixture");
    p->list_catalog("library:home",0,items_cb,NULL);assert(!strcmp(field("profile"),"fixture"));
    poll_job(none);assert(last_ok && !last_count && !last_more && !pending_count);
    show("library:profile:guest");
    /* A section YouTube withholds from the profile is one explanatory row, not a failure. */
    error_text[0]=0;p->list_catalog("section:trending",0,items_cb,NULL);assert(!strcmp(field("action"),"topics"));
    poll_job(withheld);assert(last_ok && last_count==1 && !last_more && !pending_count && !*error_text);
    assert(!strcmp(last_items[0].id,"account:notice") && !strcmp(last_items[0].title,"This section is not available from YouTube for this profile"));
    assert(!strcmp(last_items[0].subtitle,"YouTube does not offer this section here."));
    p->list_catalog("section:shorts",0,items_cb,NULL);poll_job(withheld);assert(last_ok && last_count==1);
    /* Any other failure stays a failure with its message. */
    p->list_catalog("section:trending",0,items_cb,NULL);poll_job(broken);assert(!last_ok && !pending_count && strstr(error_text,"could not be read"));
    /* A shelf that announces more and then sends an empty page ends there. */
    p->list_catalog("section:news",0,items_cb,NULL);poll_job(videos);assert(last_ok && last_count==1 && last_more);
    error_text[0]=0;item_calls=0;p->list_catalog("section:news",1,items_cb,NULL);
    assert(!strcmp(field("action"),"continue") && !strcmp(field("cursor"),"next-page"));
    poll_job(none);assert(item_calls==1 && last_ok && !last_count && !last_more && !pending_count && !*error_text);
    /* As the startup section, a withheld section fails its first list: the browser falls back to the main menu. */
    show("settings:boot-section");assert(find("Trending")>=0);show(row);assert(!strcmp(tube_boot_section(),"section:trending"));
    p->shutdown();assert(!p->init());
    show("");assert(!strcmp(tube_boot_section(),"section:trending"));
    p->list_catalog(tube_boot_section(),0,items_cb,NULL);poll_job(withheld);
    assert(!last_ok && !pending_count && strstr(error_text,"does not offer this section"));
    show("");assert(!strcmp(last_items[0].id,"discover"));
    /* Opened by hand afterwards it is the explanatory row again. */
    p->list_catalog("section:trending",0,items_cb,NULL);poll_job(withheld);assert(last_ok && last_count==1 && !strcmp(last_items[0].id,"account:notice"));
    /* A startup section that loads is unaffected, and Home as the startup section of Guest is never an empty list. */
    p->shutdown();assert(!p->init());
    p->list_catalog(tube_boot_section(),0,items_cb,NULL);poll_job(videos);assert(last_ok && last_count==1);
    show("settings:boot-section");assert(find("Home")>=0);show(row);assert(!strcmp(tube_boot_section(),"library:home"));
    p->shutdown();assert(!p->init());
    p->list_catalog(tube_boot_section(),0,items_cb,NULL);poll_job(none);assert(last_ok && last_count==4 && !strcmp(last_items[2].id,"discover"));
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: Guest Home notice rows, withheld sections, startup fallback and an empty last page");
}
