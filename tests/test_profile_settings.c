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
    SEPARATE_SETTINGS();
    /* Legacy quality actions must not lower the fixed cap. */
    const int heights[]={144,240,360,480,720,1080,1440,2160};
    for(int i=0;i<8;++i) {
        char id[64]; snprintf(id,sizeof id,"settings:quality:%d",heights[i]);action(id);
        check(2160,1,0,0,0,0,"en");
    }
    action("settings:quality:1080");action("settings:frame-rate");assert(last_count==3);
    action("settings:frame-rate:30");
    const char *invalid[]={"settings:frame-rate:","settings:frame-rate:30junk","settings:frame-rate:61"};
    for(unsigned i=0;i<sizeof invalid/sizeof *invalid;++i){p->list_catalog(invalid[i],0,items_cb,NULL);assert(!last_ok);}
    action("settings:remote:disconnect"); assert(tube_phone_stop_on_disconnect());
    action("settings:quality:1440"); action("settings:captions"); action("settings:sponsor");
    action("settings:autoplay"); action("settings:player-chat"); action("settings:sleep:60");
    action("settings:lang:fr"); action("settings:audio:de"); action("settings:codec"); action("settings:codec"); action("settings:codec"); action("settings:codec");
    action("settings:automatic"); action("settings:sb:paid"); action("settings:dearrow:titles");
    action("settings:dearrow:thumbnails"); action("settings:ask-stream"); action("settings:history:2");
    action("settings:sb:exclude:UCabcdefghijklmnopqrstuv");
    check(2160,0,1,1,1,60,"fr");
    action("library:profile:alice"); check(2160,1,0,0,0,0,"en");
    cJSON *fps_store=read_store();assert(cJSON_GetObjectItem(fps_store,"max_fps")->valueint==0);cJSON_Delete(fps_store);
    action("settings:frame-rate:60");
    assert(tube_phone_stop_on_disconnect());
    action("settings:quality:2160"); action("settings:sleep:120");
    action("settings:lang:es"); check(2160,1,0,0,0,120,"es");
    p->shutdown(); assert(!p->init()); action("library"); check(2160,1,0,0,0,120,"es");
    fps_store=read_store();assert(cJSON_GetObjectItem(fps_store,"max_fps")->valueint==60);cJSON_Delete(fps_store);
    action("library:profile:guest"); check(2160,0,1,1,1,60,"fr");
    cJSON *root=read_store(), *rows=cJSON_GetObjectItem(root,"profile_settings");
    cJSON *guest=cJSON_GetArrayItem(rows,0);
    assert(cJSON_GetObjectItem(guest,"codec")->valueint==4);
    assert(cJSON_GetObjectItem(guest,"max_fps")->valueint==30);
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
    check(2160,0,1,1,1,60,"fr"); p->shutdown();
    root=read_store(); cJSON_DeleteItemFromObject(root,"profile_settings");
    cJSON *record=cJSON_CreateObject(); cJSON_AddStringToObject(record,"id","abcdefghijk");
    cJSON_AddStringToObject(record,"profile","alice"); cJSON_AddItemToArray(cJSON_GetObjectItem(root,"records"),record);
    write_store(root); cJSON_Delete(root); assert(!p->init());
    action("library:profile:alice"); check(2160,0,1,1,1,60,"fr");
    action("library:profile:fresh"); check(2160,1,0,0,0,0,"en"); p->shutdown();
    root=read_store(); cJSON_ReplaceItemInObject(root,"profile_settings",cJSON_Parse("["
        "{\"profile\":\"bad/profile\",\"max_height\":2160},"
        "{\"profile\":\"fresh\",\"max_height\":1440.5,\"codec\":-1,\"captions\":\"false\",\"sleep_timer_minutes\":31,\"language\":\"../x\"},"
        "{\"profile\":\"fresh\",\"max_height\":2160}]"));
    write_store(root); cJSON_Delete(root); assert(!p->init()); action("library");
    check(2160,1,0,0,0,0,"en"); p->shutdown();
    root=read_store(); cJSON *bounded=cJSON_CreateArray();
    for (int i=0;i<130;++i) {
        char id[20]; snprintf(id,sizeof id,"profile%d",i); cJSON *row=cJSON_CreateObject();
        cJSON_AddStringToObject(row,"profile",id); cJSON_AddNumberToObject(row,"max_height",720);
        cJSON_AddItemToArray(bounded,row);
    }
    cJSON_ReplaceItemInObject(root,"profile_settings",bounded); write_store(root); cJSON_Delete(root);
    assert(!p->init()); action("library:profile:profile125"); check(2160,1,0,0,0,0,"en");
    p->list_catalog("library:profile:profile126",0,items_cb,NULL); assert(!last_ok);
    check(2160,1,0,0,0,0,"en"); action("library:profile:guest"); check(2160,1,0,0,0,0,"en");
    root=read_store(); assert(cJSON_GetArraySize(cJSON_GetObjectItem(root,"profile_settings"))==128);
    cJSON_Delete(root); p->shutdown();
    /* Reinitializing a removed store must not retain the previous profile/settings/source. */
    unlink(evo_data_path("youtube.json")); assert(!p->init()); SEPARATE_SETTINGS(); action("settings:quality:1440");
    check(2160,1,0,0,0,0,"en"); root=read_store();
    assert(!strcmp(cJSON_GetObjectItem(root,"library_profile")->valuestring,"guest"));
    assert(!*cJSON_GetObjectItem(root,"source")->valuestring); cJSON_Delete(root);
    action("settings:playback-mode");assert(last_count==7);
    action("settings:playback-mode:4");assert(tube_playback_mode()==TUBE_MODE_SHUFFLE);
    assert(!tube_queue_respects_playback_mode());
    action("settings:queue-respects-mode");assert(tube_queue_respects_playback_mode());
    action("library:profile:modes");assert(tube_playback_mode()==TUBE_MODE_PAUSE);
    assert(!tube_queue_respects_playback_mode());
    action("settings:playback-mode:6");p->shutdown();assert(!p->init());
    assert(tube_playback_mode()==TUBE_MODE_REVERSE);
    action("library:profile:guest");assert(tube_playback_mode()==TUBE_MODE_SHUFFLE);
    assert(tube_queue_respects_playback_mode());
    for(int mode=0;mode<8;++mode) {
        if(mode==7)assert(!tube_set_playback_mode(mode));
    }
    assert(tube_playback_mode()==TUBE_MODE_SHUFFLE);
    action("settings:playback-mode:1");assert(!tube_autoplay_enabled());
    p->shutdown();assert(!p->init());assert(tube_playback_mode()==TUBE_MODE_CLOSE && !tube_autoplay_enabled());
    action("settings:playback-mode:4");
    p->shutdown();root=read_store();
    cJSON_DeleteItemFromObject(root,"playback_mode");
    cJSON *mode_row; cJSON_ArrayForEach(mode_row,cJSON_GetObjectItem(root,"profile_settings"))cJSON_DeleteItemFromObject(mode_row,"playback_mode");
    write_store(root);cJSON_Delete(root);assert(!p->init());assert(tube_playback_mode()==TUBE_MODE_ALL);
    /* Startup: the section is profile-owned, the account chooser is device-owned. */
    assert(!*tube_boot_section() && !tube_select_account_on_boot());
    action("settings:boot-section");assert(last_count==23 && !strcmp(last_items[0].subtitle,"Selected"));
    action("settings:boot-section:8");
    assert(!strcmp(tube_boot_section(),"section:music") && !strcmp(last_items[8].subtitle,"Selected") && strcmp(last_items[0].subtitle,"Selected"));
    action("settings:account-on-boot");assert(tube_select_account_on_boot());
    int boot_row=-1,account_row=-1;
    for(int page=0;boot_row<0 || account_row<0;++page) {
        p->list_catalog("settings",page,items_cb,NULL);assert(last_ok);
        for(int i=0;i<last_count;++i) {
            assert(strcmp(last_items[i].id,"settings:quality"));
            if(!strcmp(last_items[i].id,"settings:boot-section")){boot_row=i;assert(!strcmp(last_items[i].title,"Start in: Music"));}
            if(!strcmp(last_items[i].id,"settings:account-on-boot")){account_row=i;assert(!strcmp(last_items[i].title,"Choose account at startup: on"));}
        }
        assert(last_more || (boot_row>=0 && account_row>=0));
    }
    action("library:profile:boot");assert(!*tube_boot_section() && tube_select_account_on_boot());
    action("settings:boot-section:12");assert(!strcmp(tube_boot_section(),"history"));
    const char *bad_boot[]={"settings:boot-section:","settings:boot-section:23","settings:boot-section:1x","settings:boot-section:-1","settings:boot-section:99999999999999999999"};
    for(unsigned i=0;i<sizeof bad_boot/sizeof *bad_boot;++i){p->list_catalog(bad_boot[i],0,items_cb,NULL);assert(!last_ok);}
    assert(!strcmp(tube_boot_section(),"history"));
    p->shutdown();assert(!p->init());assert(!strcmp(tube_boot_section(),"history") && tube_select_account_on_boot());
    action("library:profile:guest");assert(!strcmp(tube_boot_section(),"section:music"));
    action("settings:boot-section:0");assert(!*tube_boot_section());
    action("settings:boot-section:8");action("settings:account-on-boot");action("settings:account-on-boot");
    assert(!p->set_source("http://127.0.0.1:8766"));assert(!strcmp(tube_boot_section(),"section:music") && tube_select_account_on_boot());
    p->shutdown();root=read_store();
    assert(cJSON_IsTrue(cJSON_GetObjectItem(root,"select_account_on_boot")));
    cJSON_ReplaceItemInObject(root,"select_account_on_boot",cJSON_CreateString("yes"));
    cJSON_ArrayForEach(mode_row,cJSON_GetObjectItem(root,"profile_settings")) {
        int guest_row=!strcmp(cJSON_GetObjectItem(mode_row,"profile")->valuestring,"guest");
        if(guest_row)assert(!strcmp(cJSON_GetObjectItem(mode_row,"boot_section")->valuestring,"section:music"));
        cJSON_ReplaceItemInObject(mode_row,"boot_section",guest_row?cJSON_CreateString("settings:clear-history"):cJSON_CreateNumber(12));
    }
    write_store(root);cJSON_Delete(root);assert(!p->init());
    assert(!*tube_boot_section() && !tube_select_account_on_boot());
    action("library:profile:boot");assert(!*tube_boot_section());
    p->shutdown(); unlink(evo_data_path("youtube.json")); assert(!rmdir(dir));
    puts("PASS: profile settings isolation, startup preferences, legacy migration, reload, source reset and corrupt rows");
    return 0;
}
