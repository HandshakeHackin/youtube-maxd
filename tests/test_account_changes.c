/* Account changes of the second batch: save or remove another channel's playlist, pause,
 * resume and clear the account watch history, and hide a Subscriptions card. One
 * confirmation submits one job. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *p;
static char apply[160],session[2][80];
static const char *all="{\"api_version\":1,\"features\":[\"search\",\"library\",\"locale\",\"content_filters\",\"channel_pages\"]}";
static const char *basic="{\"api_version\":1,\"features\":[\"search\",\"library\"]}";
static const char *none="{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":false}}";
static void show(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(last_ok && !pending_count);}
static void refused(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(!last_ok && !pending_count);}
static int find(const char *title) {for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].title,title))return i;return -1;}
static void pair(void) {assert(pending_count==1 && strstr(pending[0].url,"/v1/pair"));reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");}
static void health(const char *body) {
    assert(pending_count==1 && !strcmp(pending[0].method,"GET") && strstr(pending[0].url,"/v1/health"));reply(0,1,200,body);
}
static int direct(void) {return pending_count==1 && !strcmp(pending[0].method,"POST") && strstr(pending[0].url,"/v1/jobs")!=NULL;}
static const char *field(const char *key) {
    static char value[256];
    cJSON *job=cJSON_Parse(pending[0].body),*item=cJSON_GetObjectItem(cJSON_GetObjectItem(job,"library"),key);
    assert(job);snprintf(value,sizeof value,"%s",cJSON_IsString(item)?item->valuestring:"<absent>");
    cJSON_Delete(job);return value;
}
static int session_ok(void) {
    const char *value=field("session");
    return *value && strlen(value)<=64 && strspn(value,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")==strlen(value);
}
/* Open a confirmation screen and remember its one apply row. */
static void confirm(const char *id,const char *title) {
    show(id);assert(last_count==1 && !strncmp(last_items[0].id,"account:apply:",14) && !strcmp(last_items[0].title,title));
    assert(strstr(last_items[0].subtitle,"fixture"));snprintf(apply,sizeof apply,"%s",last_items[0].id);
}
static void press(void) {p->list_catalog(apply,0,items_cb,NULL);}
static void done(const char *result) {
    char body[512];snprintf(body,sizeof body,"{\"state\":\"done\",\"result\":%s}",result);poll_job(body);
}
static int menu_offers(const char *video,const char *title) {
    char parent[64];int found=0,more=1;snprintf(parent,sizeof parent,"account:menu:%s",video);
    for(int page=0;more;++page) {
        p->list_catalog(parent,page,items_cb,NULL);assert(last_ok && !pending_count);
        if(find(title)>=0)found=1;
        more=last_more;
    }
    return found;
}
static const char *cards="{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":["
    "{\"id\":\"abcdefghijk\",\"title\":\"Card one\",\"subtitle\":\"Channel A\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"feedback_cursor\":\"hide-ref-1\",\"feedback_hide\":true},"
    "{\"id\":\"bcdefghijkl\",\"title\":\"Card two\",\"subtitle\":\"Channel A\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"feedback_cursor\":\"hide-ref-2\"},"
    "{\"id\":\"cdefghijklm\",\"title\":\"Card three\",\"subtitle\":\"Channel B\",\"channel_id\":\"UCbbbbbbbbbbbbbbbbbbbbbb\",\"feedback_cursor\":\"bad cursor!\",\"feedback_hide\":true}],\"has_more\":false}}";
static void list(const char *parent) {p->list_catalog(parent,0,items_cb,NULL);assert(direct());poll_job(cards);assert(last_ok);}
int main(void) {
    char dir[]="/tmp/tube-account-changes-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    /* Guest sees neither the playlist rows nor the history controls. */
    assert(!strcmp(tube_folder_menu("playlist:PLfixture0001","Public list"),"settings:pin:playlist:PLfixture0001"));
    show("settings:pin:playlist:PLfixture0001");assert(find("Save playlist to your library")<0 && find("Pin to main menu")==0);
    show("playlist:confirm:save:PLfixture0001");assert(last_count==1 && !strcmp(last_items[0].title,"Choose an account profile first"));
    assert(!strcmp(tube_folder_menu("library:history","YouTube history"),"account:history"));
    show("account:history");assert(last_count==1 && !strcmp(last_items[0].title,"Choose an account profile first"));
    show("account:history:clear");assert(last_count==1 && !strcmp(last_items[0].title,"Choose an account profile first"));
    show("library:profile:fixture");
    /* Save another channel's playlist: from the folder's menu, after one confirmation. */
    tube_folder_menu("playlist:PLfixture0001","Public list");show("settings:pin:playlist:PLfixture0001");
    assert(find("Save playlist to your library")==1 && !strcmp(last_items[1].id,"playlist:confirm:save:PLfixture0001"));
    assert(find("Remove playlist from your library")==2 && !strcmp(last_items[2].id,"playlist:confirm:unsave:PLfixture0001") && find("Refresh section")==3);
    confirm("playlist:confirm:save:PLfixture0001","Save this playlist to your library");
    press();pair();health(all);
    assert(direct() && !strcmp(field("action"),"playlist_save") && !strcmp(field("playlist"),"PLfixture0001") && !strcmp(field("profile"),"fixture") && session_ok());
    snprintf(session[0],sizeof session[0],"%s",field("session"));
    press();assert(last_ok && pending_count==1 && !strcmp(last_items[0].title,"Account change pending")); /* a second press submits nothing */
    done("{\"api_version\":1,\"success\":true,\"playlist_id\":\"PLfixture0001\",\"saved\":true}");
    assert(last_ok && last_count==1 && !strcmp(last_items[0].title,"YouTube account updated") && !pending_count);
    press();assert(last_ok && !pending_count && !strcmp(last_items[0].title,"YouTube account updated"));
    /* Remove it again: from the folder's menu or from the account's own playlist menu. */
    show("playlist:menu:PLfixture0001");assert(find("Remove saved playlist from library")==3 && !strcmp(last_items[3].id,"playlist:confirm:unsave:PLfixture0001"));
    confirm("playlist:confirm:unsave:PLfixture0001","Remove this playlist from your library");
    press();assert(direct() && !strcmp(field("action"),"playlist_unsave") && !strcmp(field("playlist"),"PLfixture0001") && session_ok());
    snprintf(session[1],sizeof session[1],"%s",field("session"));assert(strcmp(session[0],session[1])); /* one confirmation, one attempt */
    done("{\"api_version\":1,\"success\":true,\"playlist_id\":\"PLfixture0001\",\"saved\":false}");assert(last_ok);
    /* A confirmation for another playlist, the opposite state or an unknown result is not accepted. */
    confirm("playlist:confirm:save:PLfixture0001","Save this playlist to your library");press();
    done("{\"api_version\":1,\"success\":true,\"playlist_id\":\"PLfixture0002\",\"saved\":true}");assert(!last_ok && strstr(error_text,"playlist change"));
    press();assert(last_ok && !pending_count && !strcmp(last_items[0].title,"Account change not confirmed"));
    confirm("playlist:confirm:save:PLfixture0001","Save this playlist to your library");press();
    done("{\"api_version\":1,\"success\":true,\"playlist_id\":\"PLfixture0001\",\"saved\":false}");assert(!last_ok);
    confirm("playlist:confirm:unsave:PLfixture0001","Remove this playlist from your library");press();
    done("{\"api_version\":1,\"success\":true,\"playlist_id\":\"PLfixture0001\"}");assert(!last_ok);
    confirm("playlist:confirm:save:PLfixture0001","Save this playlist to your library");press();
    poll_job("{\"state\":\"failed\",\"error\":{\"code\":\"playlist_unconfirmed\",\"message\":\"YouTube did not confirm the playlist change.\"}}");
    assert(!last_ok && strstr(error_text,"did not confirm"));
    press();assert(last_ok && !pending_count && !strcmp(last_items[0].title,"Account change not confirmed"));
    refused("playlist:confirm:save:WL");refused("playlist:confirm:unsave:LL");refused("playlist:confirm:save:short");refused("playlist:confirm:save:");
    refused("playlist:confirm:save:PL bad id");
    /* Pause, resume and clear the account watch history; the History row opens the controls. */
    show("account:history");assert(last_count==3 && find("Pause watch history")==0 && find("Resume watch history")==1 && find("Clear watch history")==2);
    assert(!strcmp(last_items[2].id,"account:history:clear"));
    p->list_catalog("library:history",0,items_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":false,\"history_paused\":false}}");
    show("account:history");assert(last_count==2 && find("Pause watch history")==0 && find("Resume watch history")<0);
    confirm("account:history:pause","Pause watch history");assert(strstr(last_items[0].overview,"stops recording") && strstr(last_items[0].overview,"fixture"));
    assert(!strcmp(tube_comment_text(apply),last_items[0].overview));
    press();assert(direct() && !strcmp(field("action"),"history_pause") && !strcmp(field("profile"),"fixture") && session_ok() && !*field("id") && !*field("playlist"));
    press();assert(pending_count==1);
    done("{\"api_version\":1,\"success\":true,\"history\":\"paused\"}");assert(last_ok && !strcmp(last_items[0].title,"YouTube account updated"));
    press();assert(!pending_count);
    show("account:history");assert(last_count==2 && find("Resume watch history")==0 && find("Pause watch history")<0);
    confirm("account:history:resume","Resume watch history");press();assert(!strcmp(field("action"),"history_resume"));
    done("{\"api_version\":1,\"success\":true,\"history\":\"paused\"}");assert(!last_ok && strstr(error_text,"history change"));
    show("account:history");assert(find("Resume watch history")==0); /* an unconfirmed change leaves the known state */
    confirm("account:history:resume","Resume watch history");press();
    done("{\"api_version\":1,\"success\":true,\"history\":\"active\"}");assert(last_ok);
    show("account:history");assert(find("Pause watch history")==0 && find("Resume watch history")<0);
    p->list_catalog("library:history",0,items_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":false,\"history_paused\":true}}");
    show("account:history");assert(find("Resume watch history")==0 && find("Pause watch history")<0);
    p->list_catalog("library:history",0,items_cb,NULL);poll_job(none); /* YouTube offers neither control: the state is unknown */
    show("account:history");assert(last_count==3);
    assert(tube_mark_watched("abcdefghijk")==-1); /* no local record yet */
    confirm("account:history:clear","Clear watch history");assert(strstr(last_items[0].overview,"whole watch history"));
    press();assert(direct() && !strcmp(field("action"),"history_clear") && session_ok());
    done("{\"api_version\":1,\"success\":true,\"history\":\"cleared\"}");assert(last_ok);
    confirm("account:history:clear","Clear watch history");press();
    poll_job("{\"state\":\"failed\",\"error\":{\"code\":\"history_control_unavailable\",\"message\":\"This account's History page does not offer that control.\"}}");
    assert(!last_ok && strstr(error_text,"does not offer that control"));
    refused("account:history:bogus");refused("account:history:");p->list_catalog("account:history",1,items_cb,NULL);assert(!last_ok && !pending_count);
    /* The known state belongs to the profile and the browsing scope. */
    p->list_catalog("library:history",0,items_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":false,\"history_paused\":true}}");
    show("account:history");assert(last_count==2);
    show("library:profile:other");show("account:history");assert(last_count==3);
    show("library:profile:fixture");show("account:history");assert(last_count==3);
    /* Hide a Subscriptions card: offered only with a valid reference, submitted once, removed at once. */
    list("library:subscriptions");assert(last_count==3);
    assert(menu_offers("abcdefghijk","Hide from this list") && !menu_offers("bcdefghijkl","Hide from this list") && !menu_offers("cdefghijklm","Hide from this list"));
    assert(!menu_offers("abcdefghijk","Not interested"));
    refused("account:confirm:hide:bcdefghijkl");refused("account:confirm:hide:cdefghijklm");
    confirm("account:confirm:hide:abcdefghijk","Hide from this list");
    press();assert(direct() && !strcmp(field("action"),"feedback") && !strcmp(field("feedback"),"hide") && !strcmp(field("cursor"),"hide-ref-1") && !strcmp(field("id"),"abcdefghijk"));
    assert(!strcmp(field("session"),"<absent>"));
    done("{\"api_version\":1,\"success\":true,\"video_id\":\"abcdefghijk\",\"feedback\":\"not_interested\"}");assert(!last_ok && strstr(error_text,"feedback"));
    list("library:subscriptions");assert(last_count==3); /* nothing was removed */
    confirm("account:confirm:hide:abcdefghijk","Hide from this list");press();
    done("{\"api_version\":1,\"success\":true,\"video_id\":\"abcdefghijk\",\"feedback\":\"hide\"}");assert(last_ok && !strcmp(last_items[0].title,"YouTube account updated"));
    press();assert(!pending_count);
    list("library:subscriptions");assert(last_count==2 && !strcmp(last_items[0].id,"bcdefghijkl") && !strcmp(last_items[1].id,"cdefghijklm"));
    assert(!menu_offers("abcdefghijk","Hide from this list")); /* the hidden card gets no new reference */
    list("library:home");assert(last_count==2); /* Home rows may carry the same reference */
    list("library:watch_later");assert(last_count==3); /* other lists keep the video */
    list("library:subscriptions");list("library:watch_later");
    assert(!menu_offers("bcdefghijkl","Hide from this list"));refused("account:confirm:hide:abcdefghijk");
    /* Another profile and a restart forget the removal: YouTube's own list decides. */
    show("library:profile:other");list("library:subscriptions");assert(last_count==3 && menu_offers("abcdefghijk","Hide from this list"));
    show("library:profile:fixture");
    /* A companion whose worker predates these actions is not sent them. */
    assert(!p->set_source("http://127.0.0.1:8765#123456"));show("library:profile:fixture");
    confirm("playlist:confirm:save:PLfixture0001","Save this playlist to your library");
    press();pair();health(basic);assert(!last_ok && !pending_count && strstr(error_text,"does not offer"));
    confirm("account:history:pause","Pause watch history");press();assert(!last_ok && !pending_count && strstr(error_text,"does not offer"));
    /* Nor is a companion that could not be asked what it offers. */
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    confirm("account:history:clear","Clear watch history");press();pair();
    assert(pending_count==1 && strstr(pending[0].url,"/v1/health"));reply(0,0,0,"");
    assert(!last_ok && !pending_count && strstr(error_text,"did not report"));
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: playlist save and removal, account history pause, resume and clear, hidden Subscriptions cards, single submission and older companions");
    return 0;
}
