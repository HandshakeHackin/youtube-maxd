/* Third-batch companion contracts, each sent only to a companion that lists the feature: the
 * Kids section, stream reminders, playlist order, notification levels, video details with
 * like and dislike counts, the first search result and the most recent playlist. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *p;
static char row[160],apply[160];
static const char *A="abcdefghijk",*B="bcdefghijkl",*UC="UCaaaaaaaaaaaaaaaaaaaaaa";
static const char *third="{\"api_version\":1,\"features\":[\"search\",\"library\",\"channel_pages\",\"translated_captions\",\"video_details\","
    "\"subscriptions_import\",\"kids\",\"stream_reminder\",\"playlist_order\",\"subscribe_notifications\",\"comment_post\",\"search_first\","
    "\"channel_new_content\",\"dislikes\"]}";
static const char *older="{\"api_version\":1,\"features\":[\"search\",\"library\",\"channel_pages\"]}";
static const char *none="{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":false}}";
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
static int direct(void) {return pending_count==1 && !strcmp(pending[0].method,"POST") && strstr(pending[0].url,"/v1/jobs")!=NULL;}
/* A field of the pending library job: text, true/false, or <absent>. */
static const char *field(const char *key) {
    static char value[256];
    cJSON *job=cJSON_Parse(pending[0].body),*item=cJSON_GetObjectItem(cJSON_GetObjectItem(job,"library"),key);
    assert(job);snprintf(value,sizeof value,"%s",cJSON_IsString(item)?item->valuestring:cJSON_IsTrue(item)?"true":cJSON_IsFalse(item)?"false":"<absent>");
    cJSON_Delete(job);return value;
}
static int session_ok(void) {
    const char *value=field("session");
    return *value && strcmp(value,"<absent>") && strlen(value)<=64 && strspn(value,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")==strlen(value);
}
static void done(const char *result) {
    char body[1536];snprintf(body,sizeof body,"{\"state\":\"done\",\"result\":%s}",result);poll_job(body);
}
/* A paged menu offers a row with this title; its id is left in `row`. */
static int offered(const char *parent,const char *title) {
    int more=1;
    for(int page=0;more;++page) {
        p->list_catalog(parent,page,items_cb,NULL);assert(last_ok && !pending_count);
        if(find(title)>=0)return 1;
        more=last_more;
    }
    return 0;
}
static int menu_offers(const char *video,const char *title) {
    char parent[64];snprintf(parent,sizeof parent,"account:menu:%s",video);return offered(parent,title);
}
/* Open a review screen and remember its one apply row. */
static void confirm(const char *id,const char *title) {
    show(id);assert(last_count==1 && !strncmp(last_items[0].id,"account:apply:",14) && !strcmp(last_items[0].title,title));
    assert(strstr(last_items[0].subtitle,"fixture") && !strcmp(tube_comment_text(last_items[0].id),last_items[0].overview));
    snprintf(apply,sizeof apply,"%s",last_items[0].id);
}
static void press(void) {p->list_catalog(apply,0,items_cb,NULL);}
static const char *videos="{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":["
    "{\"id\":\"abcdefghijk\",\"title\":\"Premiere\",\"subtitle\":\"Channel A\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"is_upcoming\":true},"
    "{\"id\":\"bcdefghijkl\",\"title\":\"Episode\",\"subtitle\":\"Channel A\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\"}],\"has_more\":false}}";
static const char *details="{\"api_version\":1,\"kind\":\"video_details\",\"id\":\"abcdefghijk\",\"title\":\"Premiere\","
    "\"channel\":{\"id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"name\":\"Channel A\",\"subscribers\":\"1.2M subscribers\",\"art_url\":\"https://yt3.ggpht.com/a\"},"
    "\"published\":\"2026-10-01\",\"views\":\"1234567\",\"like_count\":\"4321\",\"description\":\"An excerpt\",\"description_truncated\":true,"
    "\"duration_sec\":125,\"badges\":[\"live\",\"4k\",\"other\"],\"has_related\":true}";
static cJSON *profile_row(cJSON *store,const char *profile) {
    cJSON *entry;
    cJSON_ArrayForEach(entry,cJSON_GetObjectItem(store,"profile_settings"))
        if(!strcmp(cJSON_GetObjectItem(entry,"profile")->valuestring,profile))return entry;
    return NULL;
}
int main(void) {
    char dir[]="/tmp/tube-third-XXXXXX",id[160];evo_provider_item_t next;
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    SEPARATE_SETTINGS();
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    /* Kids is a Discover section the profile may hide, move or start in. */
    show("discover");assert(listed("section:kids")>=0 && !strcmp(last_items[listed("section:kids")].title,"Kids"));
    snprintf(id,sizeof id,"%s",tube_folder_menu("section:kids","Kids"));assert(!strncmp(id,"settings:section:",17));
    snprintf(row,sizeof row,"%.100s:toggle",id);show(row);show("discover");assert(listed("section:kids")<0);
    snprintf(row,sizeof row,"%.100s:toggle",id);show(row);show("discover");assert(listed("section:kids")>=0);
    show("settings:boot-section");assert(find("Kids")>=0);show(row);assert(!strcmp(tube_boot_section(),"section:kids") && !strcmp(tube_boot_section_title(),"Kids"));
    show("settings:boot-section:0");assert(!*tube_boot_section());
    /* A companion that does not list a feature is not sent its action or field. */
    p->list_catalog("section:kids",0,items_cb,NULL);pair();health(older);
    assert(!last_ok && !pending_count && strstr(error_text,"does not offer"));
    p->list_catalog("details:abcdefghijk",0,items_cb,NULL);assert(!last_ok && !pending_count && strstr(error_text,"does not offer"));
    p->list_catalog("reminder:abcdefghijk",0,items_cb,NULL);assert(!last_ok && !pending_count);
    show("library:profile:fixture");
    p->list_catalog("playlist-order:PLfixture0001",0,items_cb,NULL);assert(!last_ok && !pending_count);
    confirm("reminder:confirm:set:abcdefghijk","Set stream reminder");press();assert(!last_ok && !pending_count && strstr(error_text,"does not offer"));
    press();assert(last_ok && !pending_count && !strcmp(last_items[0].title,"Account change not confirmed"));
    confirm("notify:confirm:level:all:UCaaaaaaaaaaaaaaaaaaaaaa","Notifications: All notifications");press();assert(!last_ok && !pending_count);
    show("library:profile:guest");
    /* Another companion lists them all. */
    assert(!p->set_source("http://127.0.0.1:8766#123456"));
    p->list_catalog("section:kids",0,items_cb,NULL);pair();health(third);
    assert(direct() && !strcmp(field("action"),"topics") && !strcmp(field("section"),"kids") && !strcmp(field("profile"),"guest"));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"section:kids:01234567-89ab-cdef-0123-456789abcdef\","
        "\"title\":\"Shows\",\"subtitle\":\"YouTube Kids\",\"art_url\":\"\",\"is_folder\":true}],\"has_more\":false}}");
    assert(last_ok && last_count==1 && last_items[0].is_folder && !strcmp(last_items[0].title,"Shows"));
    show("library:profile:fixture");
    p->list_catalog("section:kids:01234567-89ab-cdef-0123-456789abcdef",0,items_cb,NULL);
    assert(direct() && !strcmp(field("action"),"topic_open") && !strcmp(field("section"),"kids") && !strcmp(field("id"),"01234567-89ab-cdef-0123-456789abcdef"));
    poll_job(videos);assert(last_ok && last_count==2 && !last_items[0].is_folder);
    refused("section:kids:not a reference");
    /* Stream reminder: only an upcoming video's menu offers it; the state comes from YouTube. */
    assert(!menu_offers(B,"Stream reminder"));
    assert(menu_offers(A,"Stream reminder") && !strcmp(row,"reminder:abcdefghijk"));
    p->list_catalog(row,0,items_cb,NULL);
    assert(direct() && !strcmp(field("action"),"stream_reminder") && !strcmp(field("id"),A) && !strcmp(field("reminder"),"status") && !strcmp(field("session"),"<absent>"));
    done("{\"api_version\":1,\"kind\":\"stream_reminder\",\"id\":\"abcdefghijk\",\"upcoming\":true,\"available\":true,\"reminder_set\":false}");
    assert(last_ok && last_count==2 && !strcmp(last_items[0].title,"Reminder: not set") && !strcmp(last_items[1].title,"Set stream reminder"));
    assert(!strcmp(last_items[1].id,"reminder:confirm:set:abcdefghijk"));
    confirm("reminder:confirm:set:abcdefghijk","Set stream reminder");assert(strstr(last_items[0].overview,A));
    press();assert(direct() && !strcmp(field("action"),"stream_reminder") && !strcmp(field("reminder"),"set") && !strcmp(field("id"),A) && session_ok() && !strcmp(field("profile"),"fixture"));
    press();assert(last_ok && pending_count==1 && !strcmp(last_items[0].title,"Account change pending")); /* a second press submits nothing */
    done("{\"api_version\":1,\"success\":true,\"id\":\"abcdefghijk\",\"reminder_set\":true}");
    assert(last_ok && !strcmp(last_items[0].title,"YouTube account updated") && !pending_count);
    press();assert(last_ok && !pending_count);
    p->list_catalog("reminder:abcdefghijk",0,items_cb,NULL);
    done("{\"api_version\":1,\"kind\":\"stream_reminder\",\"id\":\"abcdefghijk\",\"upcoming\":true,\"available\":true,\"reminder_set\":true}");
    assert(last_count==2 && !strcmp(last_items[0].title,"Reminder: set") && !strcmp(last_items[1].id,"reminder:confirm:unset:abcdefghijk"));
    confirm("reminder:confirm:unset:abcdefghijk","Remove stream reminder");press();assert(!strcmp(field("reminder"),"unset"));
    done("{\"api_version\":1,\"success\":true,\"id\":\"abcdefghijk\",\"reminder_set\":true}");assert(!last_ok && strstr(error_text,"reminder")); /* the opposite state */
    confirm("reminder:confirm:unset:abcdefghijk","Remove stream reminder");press();
    done("{\"api_version\":1,\"success\":true,\"id\":\"bcdefghijkl\",\"reminder_set\":false}");assert(!last_ok); /* another video */
    confirm("reminder:confirm:unset:abcdefghijk","Remove stream reminder");press();
    done("{\"api_version\":1,\"success\":true,\"id\":\"abcdefghijk\",\"reminder_set\":false}");assert(last_ok);
    p->list_catalog("reminder:abcdefghijk",0,items_cb,NULL);
    done("{\"api_version\":1,\"kind\":\"stream_reminder\",\"id\":\"abcdefghijk\",\"upcoming\":false,\"available\":false,\"reminder_set\":false}");
    assert(last_ok && last_count==1 && !strcmp(last_items[0].title,"This video is not an upcoming stream"));
    p->list_catalog("reminder:abcdefghijk",0,items_cb,NULL);
    done("{\"api_version\":1,\"kind\":\"stream_reminder\",\"id\":\"abcdefghijk\",\"upcoming\":true,\"available\":false,\"reminder_set\":false}");
    assert(last_ok && last_count==1 && !strcmp(last_items[0].title,"YouTube offers no reminder for this stream"));
    p->list_catalog("reminder:abcdefghijk",0,items_cb,NULL);
    done("{\"api_version\":1,\"kind\":\"stream_reminder\",\"id\":\"bcdefghijkl\",\"upcoming\":true,\"available\":true,\"reminder_set\":false}");assert(!last_ok);
    p->list_catalog("reminder:abcdefghijk",0,items_cb,NULL);
    done("{\"api_version\":1,\"kind\":\"stream_reminder\",\"id\":\"abcdefghijk\",\"upcoming\":true,\"available\":\"yes\",\"reminder_set\":false}");assert(!last_ok);
    refused("reminder:short");refused("reminder:confirm:set:short");refused("reminder:confirm:toggle:abcdefghijk");refused("reminder:confirm:set");
    p->list_catalog("reminder:abcdefghijk",1,items_cb,NULL);assert(!last_ok && !pending_count);
    /* Playlist order: the orders YouTube offers for an own playlist, changed after one confirmation. */
    show("playlist:menu:PLfixture0001");assert(find("Playlist order")>=0 && !strcmp(row,"playlist-order:PLfixture0001"));
    p->list_catalog(row,0,items_cb,NULL);
    assert(direct() && !strcmp(field("action"),"playlist_order") && !strcmp(field("playlist"),"PLfixture0001") && !strcmp(field("playlist_order"),"<absent>"));
    assert(!strcmp(field("session"),"<absent>") && !strcmp(field("id"),"<absent>"));
    done("{\"api_version\":1,\"kind\":\"playlist_order\",\"playlist_id\":\"PLfixture0001\",\"editable\":true,"
        "\"orders\":[\"added_newest\",\"popular\",\"manual\",\"popular\"],\"order\":\"popular\"}");
    assert(last_ok && last_count==2 && !strcmp(last_items[0].title,"Date added, newest first") && !strcmp(last_items[1].title,"Most popular"));
    assert(!strcmp(last_items[1].subtitle,"Selected") && strcmp(last_items[0].subtitle,"Selected"));
    assert(!strcmp(last_items[0].id,"playlist-order:confirm:added_newest:PLfixture0001"));
    confirm(last_items[0].id,"Sort playlist: Date added, newest first");assert(strstr(last_items[0].overview,"PLfixture0001"));
    press();assert(direct() && !strcmp(field("action"),"playlist_order") && !strcmp(field("playlist_order"),"added_newest") && !strcmp(field("playlist"),"PLfixture0001"));
    assert(session_ok() && !*field("id"));
    done("{\"api_version\":1,\"success\":true,\"playlist_id\":\"PLfixture0001\",\"order\":\"added_newest\"}");assert(last_ok && !strcmp(last_items[0].title,"YouTube account updated"));
    confirm("playlist-order:confirm:published_oldest:PLfixture0001","Sort playlist: Date published, oldest first");press();
    done("{\"api_version\":1,\"success\":true,\"playlist_id\":\"PLfixture0001\",\"order\":\"added_newest\"}");assert(!last_ok && strstr(error_text,"playlist order"));
    confirm("playlist-order:confirm:published_oldest:PLfixture0001","Sort playlist: Date published, oldest first");press();
    done("{\"api_version\":1,\"success\":true,\"playlist_id\":\"PLfixture0002\",\"order\":\"published_oldest\"}");assert(!last_ok);
    p->list_catalog("playlist-order:PLfixture0001",0,items_cb,NULL);
    done("{\"api_version\":1,\"kind\":\"playlist_order\",\"playlist_id\":\"PLfixture0001\",\"editable\":false,\"orders\":[],\"order\":\"\"}");
    assert(last_ok && last_count==1 && !strcmp(last_items[0].title,"This playlist cannot be sorted"));
    p->list_catalog("playlist-order:PLfixture0001",0,items_cb,NULL);
    done("{\"api_version\":1,\"kind\":\"playlist_order\",\"playlist_id\":\"PLfixture0002\",\"editable\":true,\"orders\":[\"popular\"],\"order\":\"\"}");assert(!last_ok);
    refused("playlist-order:confirm:manual:PLfixture0001");refused("playlist-order:confirm:popular:x");refused("playlist-order:x");
    /* Notification level: with a new subscription, or for a channel already subscribed to. */
    assert(menu_offers(A,"Subscription notifications") && !strcmp(row,"notify:UCaaaaaaaaaaaaaaaaaaaaaa"));
    tube_folder_menu("channel:UCaaaaaaaaaaaaaaaaaaaaaa","Channel A");show("settings:pin:channel:UCaaaaaaaaaaaaaaaaaaaaaa");
    assert(find("Subscription notifications")>=0 && !strcmp(row,"notify:UCaaaaaaaaaaaaaaaaaaaaaa"));
    show(row);assert(last_count==6 && !strcmp(last_items[0].title,"Subscribe: All notifications") && !strcmp(last_items[5].title,"Notifications: No notifications"));
    assert(!strcmp(last_items[1].id,"notify:confirm:subscribe:personalized:UCaaaaaaaaaaaaaaaaaaaaaa") && !strcmp(last_items[5].id,"notify:confirm:level:none:UCaaaaaaaaaaaaaaaaaaaaaa"));
    confirm(last_items[1].id,"Subscribe: Personalized notifications");
    press();assert(direct() && !strcmp(field("action"),"subscribe") && !strcmp(field("id"),UC) && !strcmp(field("notifications"),"personalized"));
    done("{\"api_version\":1,\"success\":true,\"playlist_id\":\"\",\"notifications\":\"personalized\"}");assert(last_ok && !strcmp(last_items[0].title,"YouTube account updated"));
    confirm("notify:confirm:subscribe:all:UCaaaaaaaaaaaaaaaaaaaaaa","Subscribe: All notifications");press();
    done("{\"api_version\":1,\"success\":true,\"playlist_id\":\"\",\"notifications\":\"\"}"); /* subscribed, level not applied: never subscribe again */
    assert(last_ok && !strcmp(last_items[0].title,"Subscribed; notification level not confirmed"));
    press();assert(last_ok && !pending_count);
    confirm("notify:confirm:level:none:UCaaaaaaaaaaaaaaaaaaaaaa","Notifications: No notifications");
    press();assert(direct() && !strcmp(field("action"),"subscribe_notifications") && !strcmp(field("id"),UC) && !strcmp(field("notifications"),"none"));
    assert(!strcmp(field("session"),"<absent>"));
    done("{\"api_version\":1,\"success\":true,\"playlist_id\":\"\",\"notifications\":\"none\"}");assert(last_ok && !strcmp(last_items[0].title,"YouTube account updated"));
    confirm("notify:confirm:level:none:UCaaaaaaaaaaaaaaaaaaaaaa","Notifications: No notifications");press();
    done("{\"api_version\":1,\"success\":true,\"playlist_id\":\"\",\"notifications\":\"all\"}");assert(!last_ok && strstr(error_text,"notification level"));
    /* The plain Subscribe action is sent as before. */
    assert(menu_offers(A,"Subscribe on YouTube"));show(row);snprintf(apply,sizeof apply,"%s",last_items[0].id);
    press();assert(direct() && !strcmp(field("action"),"subscribe") && !strcmp(field("notifications"),"<absent>") && !strcmp(field("session"),"<absent>"));
    done("{\"success\":true}");assert(last_ok);
    refused("notify:UCshort");refused("notify:confirm:subscribe:loud:UCaaaaaaaaaaaaaaaaaaaaaa");refused("notify:confirm:level:all:UCshort");
    refused("notify:confirm:other:all:UCaaaaaaaaaaaaaaaaaaaaaa");refused("notify:confirm:level:all");
    /* Most recent playlist: remembered from a confirmed add or remove, offered in every video menu. */
    assert(!menu_offers(B,"Add to Mine"));
    p->list_catalog("playlist:pick:add:abcdefghijk",0,items_cb,NULL);assert(direct());
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"library:playlist:PLmine000001\",\"title\":\"Mine\",\"is_folder\":true},"
        "{\"id\":\"library:playlist:PLother00001\",\"title\":\"Other\",\"is_folder\":true}],\"has_more\":false}}");
    assert(last_ok && last_count==2 && !strcmp(last_items[0].id,"playlist:confirm:add:abcdefghijk:PLmine000001"));
    show(last_items[0].id);snprintf(apply,sizeof apply,"%s",last_items[0].id);press();assert(direct() && !strcmp(field("action"),"playlist_add"));
    poll_job("{\"state\":\"failed\",\"error\":{\"code\":\"operation_failed\",\"message\":\"YouTube refused.\"}}");
    assert(!last_ok && !menu_offers(B,"Add to Mine")); /* an unconfirmed change is not remembered */
    show("playlist:confirm:add:abcdefghijk:PLmine000001");snprintf(apply,sizeof apply,"%s",last_items[0].id);press();done("{\"success\":true}");assert(last_ok);
    assert(menu_offers(B,"Add to Mine") && !strcmp(row,"playlist:confirm:add:bcdefghijkl:PLmine000001"));
    assert(menu_offers(B,"Remove from Mine") && !strcmp(row,"playlist:confirm:remove:bcdefghijkl:PLmine000001"));
    show(row);snprintf(apply,sizeof apply,"%s",last_items[0].id);press();assert(!strcmp(field("action"),"playlist_remove") && !strcmp(field("playlist"),"PLmine000001") && !strcmp(field("id"),B));
    done("{\"success\":true}");assert(last_ok && menu_offers(A,"Add to Mine"));
    show("playlist:confirm:remove:abcdefghijk:PLother00001");snprintf(apply,sizeof apply,"%s",last_items[0].id);press();done("{\"success\":true}");
    assert(menu_offers(A,"Add to Other") && !menu_offers(A,"Add to Mine")); /* one shortcut: the last playlist used */
    show("library:profile:guest");assert(!menu_offers(A,"Add to Other"));
    show("library:profile:alice");assert(!menu_offers(A,"Add to Other") && menu_offers(A,"Add to a YouTube playlist"));
    p->shutdown();assert(!p->init());
    assert(!menu_offers(A,"Add to Other"));show("library:profile:fixture");assert(menu_offers(A,"Add to Other") && menu_offers(A,"Remove from Other"));
    p->shutdown();
    cJSON *store=read_store(),*recent=cJSON_GetObjectItem(store,"recent_playlists");
    assert(cJSON_GetArraySize(recent)==1 && !strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(recent,0),"profile")->valuestring,"fixture"));
    cJSON_ReplaceItemInObject(cJSON_GetArrayItem(recent,0),"playlist",cJSON_CreateString("not a playlist"));
    cJSON_AddItemToArray(recent,cJSON_Parse("{\"profile\":\"bad profile!\",\"playlist\":\"PLmine000001\",\"title\":\"Mine\"}"));
    cJSON_AddItemToArray(recent,cJSON_Parse("{\"profile\":\"guest\",\"playlist\":\"PLmine000001\",\"title\":\"Mine\"}"));
    cJSON_AddItemToArray(recent,cJSON_CreateNumber(7));
    write_store(store);cJSON_Delete(store);assert(!p->init());
    assert(!menu_offers(A,"Add to Other") && !menu_offers(A,"Add to Mine") && menu_offers(A,"Add to a YouTube playlist"));
    p->shutdown();store=read_store();assert(!cJSON_GetArraySize(cJSON_GetObjectItem(store,"recent_playlists")));cJSON_Delete(store);assert(!p->init());
    /* Video details: one answer shown as rows; the feature list is read again after the restart. */
    assert(menu_offers(A,"Video details") && !strcmp(row,"details:abcdefghijk"));
    p->list_catalog(row,0,items_cb,NULL);health(third);
    assert(direct() && !strcmp(field("action"),"video_details") && !strcmp(field("id"),A) && !strcmp(field("profile"),"fixture"));
    done(details);assert(last_ok && !pending_count && last_count==7);
    assert(!strcmp(last_items[0].title,"Premiere") && !strcmp(last_items[0].subtitle,"Live | 4K | 2:05") && !strcmp(last_items[0].overview,"An excerpt"));
    assert(!strcmp(last_items[1].id,"channel:UCaaaaaaaaaaaaaaaaaaaaaa") && !strcmp(last_items[1].title,"Channel A") && !strcmp(last_items[1].subtitle,"1.2M subscribers"));
    assert(!strcmp(last_items[1].art_url,"https://yt3.ggpht.com/a") && !strcmp(last_items[2].title,"2026-10-01"));
    assert(!strcmp(last_items[3].title,"1,234,567 views") && !strcmp(last_items[4].title,"4,321 likes"));
    assert(!strcmp(last_items[5].id,"description:abcdefghijk") && !strcmp(last_items[6].id,"library:suggestions:abcdefghijk"));
    p->list_catalog("details:abcdefghijk",0,items_cb,NULL);
    done("{\"api_version\":1,\"kind\":\"video_details\",\"id\":\"abcdefghijk\",\"title\":\"\",\"channel\":{\"id\":\"\",\"name\":\"Somebody\",\"subscribers\":\"1200\",\"art_url\":\"\"},"
        "\"published\":\"\",\"views\":\"1\",\"like_count\":\"\",\"description\":\"\",\"description_truncated\":false,\"duration_sec\":3725,\"badges\":[],\"has_related\":false}");
    assert(last_ok && last_count==4 && !strcmp(last_items[0].title,A) && !strcmp(last_items[0].subtitle,"1:02:05"));
    assert(!strcmp(last_items[1].id,"account:notice:channel") && !strcmp(last_items[1].subtitle,"1,200 subscribers") && !strcmp(last_items[2].title,"1 view"));
    p->list_catalog("details:abcdefghijk",0,items_cb,NULL);
    done("{\"api_version\":1,\"kind\":\"video_details\",\"id\":\"bcdefghijkl\",\"title\":\"Other\",\"channel\":{}}");assert(!last_ok && strstr(error_text,"details"));
    p->list_catalog("details:abcdefghijk",0,items_cb,NULL);done("{\"api_version\":1,\"kind\":\"video_details\",\"id\":\"abcdefghijk\"}");assert(!last_ok);
    refused("details:short");p->list_catalog("details:abcdefghijk",1,items_cb,NULL);assert(!last_ok && !pending_count);
    /* The description lists the like count when the companion has one. */
    p->list_catalog("description:abcdefghijk",0,items_cb,NULL);
    done("{\"kind\":\"description\",\"id\":\"abcdefghijk\",\"title\":\"Premiere\",\"description\":\"Text\",\"channel\":\"Channel A\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\","
        "\"published\":\"2026-10-01\",\"views\":\"10\",\"like_count\":\"4321\",\"truncated\":false}");
    assert(last_ok && !pending_count && last_count==5 && !strcmp(last_items[4].title,"4,321 likes") && !strcmp(last_items[3].title,"10 views"));
    p->list_catalog("description:abcdefghijk",0,items_cb,NULL);
    done("{\"kind\":\"description\",\"id\":\"abcdefghijk\",\"title\":\"Premiere\",\"description\":\"Text\",\"channel\":\"\",\"channel_id\":\"\",\"published\":\"\",\"views\":\"\",\"like_count\":\"\",\"truncated\":false}");
    assert(last_ok && last_count==1);
    /* Dislikes: off until the profile turns them on, and asked only of a companion whose host enabled the relay. */
    show("settings:extra:player");assert(find("Show dislikes: off")>=0 && !strcmp(row,"settings:dislikes") && strstr(last_items[find("Show dislikes: off")].subtitle,"Return YouTube Dislike"));
    show("settings:dislikes");assert(find("Show dislikes: on")>=0);
    int calls=item_calls;
    p->list_catalog("details:abcdefghijk",0,items_cb,NULL);done(details);
    assert(pending_count==1 && !strcmp(pending[0].method,"GET") && strstr(pending[0].url,"/v1/dislikes/abcdefghijk") && item_calls==calls);
    reply(0,1,200,"{\"api_version\":1,\"enabled\":true,\"id\":\"abcdefghijk\",\"available\":true,\"likes\":4321,\"dislikes\":1234}");
    assert(last_ok && !pending_count && last_count==8 && !strcmp(last_items[7].title,"1,234 dislikes") && strstr(last_items[7].subtitle,"Return YouTube Dislike"));
    const char *answers[]={"{\"api_version\":1,\"enabled\":false}","{\"api_version\":1,\"enabled\":true,\"id\":\"abcdefghijk\",\"available\":false}",
        "{\"api_version\":1,\"enabled\":true,\"id\":\"bcdefghijkl\",\"available\":true,\"likes\":1,\"dislikes\":2}",
        "{\"api_version\":1,\"enabled\":true,\"id\":\"abcdefghijk\",\"available\":true,\"likes\":1,\"dislikes\":\"2\"}",
        "{\"api_version\":1,\"enabled\":true,\"id\":\"abcdefghijk\",\"available\":true,\"likes\":1,\"dislikes\":1.5}","not json"};
    for(unsigned i=0;i<sizeof answers/sizeof *answers;++i) {
        p->list_catalog("details:abcdefghijk",0,items_cb,NULL);done(details);assert(pending_count==1);
        reply(0,1,200,answers[i]);assert(last_ok && !pending_count && last_count==7);
    }
    p->list_catalog("details:abcdefghijk",0,items_cb,NULL);done(details);reply(0,0,502,"");assert(last_ok && last_count==7); /* a failed relay never fails the page */
    p->list_catalog("description:abcdefghijk",0,items_cb,NULL);
    done("{\"kind\":\"description\",\"id\":\"abcdefghijk\",\"title\":\"Premiere\",\"description\":\"Text\",\"channel\":\"\",\"channel_id\":\"\",\"published\":\"\",\"views\":\"\",\"like_count\":\"7\",\"truncated\":false}");
    assert(pending_count==1 && strstr(pending[0].url,"/v1/dislikes/abcdefghijk"));
    reply(0,1,200,"{\"api_version\":1,\"enabled\":true,\"id\":\"abcdefghijk\",\"available\":true,\"likes\":7,\"dislikes\":1}");
    assert(last_ok && last_count==3 && !strcmp(last_items[1].title,"7 likes") && !strcmp(last_items[2].title,"1 dislike"));
    /* The switch belongs to the profile's settings and survives a restart. */
    show("library:profile:alice");show("settings:extra:player");assert(find("Show dislikes: off")>=0);
    p->list_catalog("details:abcdefghijk",0,items_cb,NULL);done(details);assert(last_ok && !pending_count && last_count==7);
    show("library:profile:fixture");
    p->shutdown();store=read_store();
    assert(cJSON_IsTrue(cJSON_GetObjectItem(profile_row(store,"fixture"),"show_dislikes")) && cJSON_IsFalse(cJSON_GetObjectItem(profile_row(store,"alice"),"show_dislikes")));
    cJSON_ReplaceItemInObject(profile_row(store,"alice"),"show_dislikes",cJSON_CreateString("yes")); /* corrupt reads as off */
    write_store(store);cJSON_Delete(store);assert(!p->init());
    show("settings:extra:player");assert(find("Show dislikes: on")>=0);
    /* A companion without the relay is never asked, whatever the switch says. */
    assert(!p->set_source("http://127.0.0.1:8765#123456"));show("library:profile:fixture");
    show("settings:extra:player");assert(find("Show dislikes: on")>=0);
    p->list_catalog("description:abcdefghijk",0,items_cb,NULL);pair();health(older);assert(direct());
    done("{\"kind\":\"description\",\"id\":\"abcdefghijk\",\"title\":\"Premiere\",\"description\":\"Text\",\"channel\":\"\",\"channel_id\":\"\",\"published\":\"\",\"views\":\"\",\"like_count\":\"7\",\"truncated\":false}");
    assert(last_ok && !pending_count && last_count==2);
    show("settings:extra:player");assert(find("Show dislikes: on")>=0 && strstr(last_items[find("Show dislikes: on")].subtitle,"has not enabled"));
    show("library:profile:alice");show("settings:extra:player");assert(find("Show dislikes: off")>=0);
    show("library:profile:fixture");
    /* Play the first result: the last typed search, answered by one playable video the host starts. */
    assert(!p->search("cats",0,items_cb,NULL));poll_job(none);
    show("settings:search");assert(find("Play the first result: cats")>=0);snprintf(id,sizeof id,"%s",row);
    p->list_catalog(id,0,items_cb,NULL);assert(!last_ok && !pending_count && strstr(error_text,"does not offer")); /* this companion has no such search */
    assert(!p->set_source("http://127.0.0.1:8766#123456"));show("library:profile:fixture");
    show("settings:search");assert(find("Play the first result: cats")<0); /* a new source has no last search */
    assert(!p->search("cats",0,items_cb,NULL));pair();poll_job(none);
    show("settings:search:type");show("settings:search");assert(find("Play the first result: cats")>=0);snprintf(id,sizeof id,"%s",row);
    p->list_catalog(id,0,items_cb,NULL);health(third);
    assert(direct() && !strcmp(field("action"),"search") && !strcmp(field("query"),"cats") && !strcmp(field("first_playable"),"true") && !strcmp(field("type"),"video"));
    done("{\"api_version\":1,\"kind\":\"search_first\",\"query\":\"cats\",\"found\":true,\"item\":{\"id\":\"cdefghijklm\",\"title\":\"First cat\",\"subtitle\":\"Cat channel\","
        "\"channel_id\":\"UCbbbbbbbbbbbbbbbbbbbbbb\",\"duration_sec\":60}}");
    assert(last_ok && last_count==1 && !last_items[0].is_folder && !strcmp(last_items[0].id,"cdefghijklm") && !strcmp(last_items[0].title,"First cat"));
    assert(tube_take_next(&next)==1 && !strcmp(next.id,"cdefghijklm") && !strcmp(next.title,"First cat"));
    assert(!tube_take_next(&next)); /* taken once */
    p->list_catalog(id,0,items_cb,NULL);done("{\"api_version\":1,\"kind\":\"search_first\",\"query\":\"cats\",\"found\":false,\"item\":null}");
    assert(last_ok && last_count==1 && last_items[0].is_folder && !strcmp(last_items[0].title,"No playable video on the first result page") && !tube_take_next(&next));
    p->list_catalog(id,0,items_cb,NULL);done("{\"api_version\":1,\"kind\":\"search_first\",\"query\":\"cats\",\"found\":true,\"item\":{\"id\":\"channel:UCbbbbbbbbbbbbbbbbbbbbbb\",\"is_folder\":true}}");
    assert(last_ok && last_items[0].is_folder && !tube_take_next(&next));
    p->list_catalog(id,0,items_cb,NULL);done("{\"kind\":\"catalog\",\"items\":[]}");assert(!last_ok && !tube_take_next(&next));
    refused("search-first:0");refused("search-first:");
    show("library:profile:alice");refused(id); /* another profile has not searched */
    p->shutdown();
    puts("PASS: Kids, stream reminders, playlist order, notification levels, details with likes and dislikes, first search result and recent playlist, all gated by the feature list");
}
