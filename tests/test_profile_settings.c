/* Exercise shipping settings actions and JSON persistence, without network access. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static void action(const char *id) {
    evo_provider_youtube.list_catalog(id,0,items_cb,NULL); assert(last_ok);
}
static void check(int height,int captions,int sponsor,int autoplay,int chat,int sleep,const char *language) {
    assert(tube_player_set_hide_seconds(3)); /* persist the current in-memory preference snapshot */
    cJSON *root=read_store(); assert(root);
    assert(cJSON_GetObjectItem(root,"max_height")->valueint==height);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(root,"captions"))==captions);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(root,"sponsorblock"))==sponsor);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(root,"autoplay"))==autoplay);
    assert(cJSON_GetObjectItem(root,"player_chat_mode")->valueint==chat);
    assert(cJSON_GetObjectItem(root,"sleep_timer_minutes")->valueint==sleep);
    assert(!strcmp(cJSON_GetObjectItem(root,"language")->valuestring,language));
    cJSON_Delete(root);
}
int main(void) {
    char dir[]="/tmp/tube-profile-settings-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t *p=&evo_provider_youtube; assert(!p->init());
    action("settings:remote:disconnect"); assert(tube_phone_stop_on_disconnect());
    action("settings:quality"); action("settings:captions"); action("settings:sponsor");
    action("settings:autoplay"); action("settings:player-chat"); action("settings:sleep:60");
    action("settings:lang:fr"); action("settings:audio:de"); action("settings:codec");
    action("settings:automatic"); action("settings:sb:paid"); action("settings:dearrow:titles");
    action("settings:dearrow:thumbnails"); action("settings:ask-stream"); action("settings:history-sync");
    action("settings:sb:exclude:UCabcdefghijklmnopqrstuv");
    check(1440,0,1,1,1,60,"fr");
    action("library:profile:alice"); check(1080,1,0,0,0,0,"en");
    assert(tube_phone_stop_on_disconnect());
    action("settings:quality"); action("settings:quality"); action("settings:sleep:120");
    action("settings:lang:es"); check(2160,1,0,0,0,120,"es");
    p->shutdown(); assert(!p->init()); action("library"); check(2160,1,0,0,0,120,"es");
    action("library:profile:guest"); check(1440,0,1,1,1,60,"fr");
    cJSON *root=read_store(), *rows=cJSON_GetObjectItem(root,"profile_settings");
    cJSON *guest=cJSON_GetArrayItem(rows,0);
    assert(cJSON_GetObjectItem(guest,"codec")->valueint==1);
    assert(!strcmp(cJSON_GetObjectItem(guest,"audio_language")->valuestring,"de"));
    assert(cJSON_IsTrue(cJSON_GetObjectItem(guest,"automatic_captions")));
    assert(cJSON_IsTrue(cJSON_GetObjectItem(guest,"paid_content_notifications")));
    assert(cJSON_IsTrue(cJSON_GetObjectItem(guest,"dearrow_titles")));
    assert(cJSON_IsTrue(cJSON_GetObjectItem(guest,"dearrow_thumbnails")));
    assert(cJSON_IsTrue(cJSON_GetObjectItem(guest,"ask_stream")));
    assert(cJSON_IsFalse(cJSON_GetObjectItem(guest,"history_sync")));
    assert(cJSON_GetArraySize(cJSON_GetObjectItem(cJSON_GetObjectItem(guest,"sponsor_settings"),"excluded_channels"))==1);
    cJSON_Delete(root);
    /* Source change returns to the saved guest preferences; remote identity stays device-owned. */
    action("library:profile:alice"); assert(!p->set_source("http://127.0.0.1:8766"));
    check(1440,0,1,1,1,60,"fr"); p->shutdown();
    root=read_store(); cJSON_DeleteItemFromObject(root,"profile_settings");
    cJSON *record=cJSON_CreateObject(); cJSON_AddStringToObject(record,"id","abcdefghijk");
    cJSON_AddStringToObject(record,"profile","alice"); cJSON_AddItemToArray(cJSON_GetObjectItem(root,"records"),record);
    write_store(root); cJSON_Delete(root); assert(!p->init());
    action("library:profile:alice"); check(1440,0,1,1,1,60,"fr");
    action("library:profile:fresh"); check(1080,1,0,0,0,0,"en"); p->shutdown();
    root=read_store(); cJSON_ReplaceItemInObject(root,"profile_settings",cJSON_Parse("["
        "{\"profile\":\"bad/profile\",\"max_height\":2160},"
        "{\"profile\":\"fresh\",\"max_height\":1440.5,\"codec\":-1,\"captions\":\"false\",\"sleep_timer_minutes\":31,\"language\":\"../x\"},"
        "{\"profile\":\"fresh\",\"max_height\":2160}]"));
    write_store(root); cJSON_Delete(root); assert(!p->init()); action("library");
    check(1080,1,0,0,0,0,"en"); p->shutdown();
    root=read_store(); cJSON *bounded=cJSON_CreateArray();
    for (int i=0;i<130;++i) {
        char id[20]; snprintf(id,sizeof id,"profile%d",i); cJSON *row=cJSON_CreateObject();
        cJSON_AddStringToObject(row,"profile",id); cJSON_AddNumberToObject(row,"max_height",720);
        cJSON_AddItemToArray(bounded,row);
    }
    cJSON_ReplaceItemInObject(root,"profile_settings",bounded); write_store(root); cJSON_Delete(root);
    assert(!p->init()); action("library:profile:profile125"); check(720,1,0,0,0,0,"en");
    p->list_catalog("library:profile:profile126",0,items_cb,NULL); assert(!last_ok);
    check(720,1,0,0,0,0,"en"); action("library:profile:guest"); check(1080,1,0,0,0,0,"en");
    root=read_store(); assert(cJSON_GetArraySize(cJSON_GetObjectItem(root,"profile_settings"))==128);
    cJSON_Delete(root); p->shutdown();
    /* Reinitializing a removed store must not retain the previous profile/settings/source. */
    unlink(evo_data_path("youtube.json")); assert(!p->init()); action("settings:quality");
    check(1440,1,0,0,0,0,"en"); root=read_store();
    assert(!strcmp(cJSON_GetObjectItem(root,"library_profile")->valuestring,"guest"));
    assert(!*cJSON_GetObjectItem(root,"source")->valuestring); cJSON_Delete(root);
    p->shutdown(); unlink(evo_data_path("youtube.json")); assert(!rmdir(dir));
    puts("PASS: profile settings isolation, legacy migration, reload, source reset and corrupt rows");
    return 0;
}
