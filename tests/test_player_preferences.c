/* Real provider persistence and profile migration, with corrupt-state boundaries. */
#define main existing_provider_regression
#include "test_native.c"
#undef main

static void select_profile(const evo_provider_t *p, const char *profile) {
    char id[80]; snprintf(id,sizeof id,"library:profile:%s",profile);
    p->list_catalog(id,0,items_cb,NULL); assert(last_ok);
}
int main(void) {
    char dir[]="/tmp/tube-player-prefs-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t *p=&evo_provider_youtube; assert(!p->init());
    assert(tube_player_ok_mode()==0 && tube_player_hide_ms()==3000);
    assert(!tube_player_seek_mode());
    assert(tube_player_seek_seconds()==10);
    const int steps[]={1,2,3,5,7,10,15,20,30,60};
    for(unsigned i=0;i<sizeof steps/sizeof *steps;++i) assert(tube_player_set_seek_seconds(steps[i]) && tube_player_seek_seconds()==steps[i]);
    assert(!tube_player_set_seek_seconds(0) && !tube_player_set_seek_seconds(4) && !tube_player_set_seek_seconds(61));
    p->list_catalog("settings:seek-interval",0,items_cb,NULL);assert(last_ok && last_count==10);
    p->list_catalog("settings:seek-interval:7",0,items_cb,NULL);assert(last_ok && tube_player_seek_seconds()==7);
    p->list_catalog("settings:seek-interval:7junk",0,items_cb,NULL);assert(!last_ok);

    assert(!tube_player_set_seek_mode(-1) && !tube_player_set_seek_mode(3));
    p->list_catalog("settings:player-seek",0,items_cb,NULL); assert(last_ok && last_count==3);
    p->list_catalog("settings:player-seek:2",0,items_cb,NULL); assert(last_ok && tube_player_seek_mode()==2);
    p->list_catalog("settings:player-seek:2junk",0,items_cb,NULL); assert(!last_ok);
    assert(!tube_player_set_ok_mode(-1) && !tube_player_set_ok_mode(4));
    assert(!tube_player_set_hide_seconds(-1) && !tube_player_set_hide_seconds(16));
    p->list_catalog("settings:player-ok",0,items_cb,NULL); assert(last_ok && last_count==4);
    p->list_catalog("settings:player-hide",0,items_cb,NULL); assert(last_ok && last_count==16);
    p->list_catalog("settings:player-ok:2",0,items_cb,NULL); assert(last_ok && tube_player_ok_mode()==2);
    p->list_catalog("settings:player-hide:0",0,items_cb,NULL); assert(last_ok && !tube_player_hide_ms());
    p->list_catalog("settings:player-ok:2junk",0,items_cb,NULL); assert(!last_ok);
    p->list_catalog("settings:player-hide:99999999999999999",0,items_cb,NULL); assert(!last_ok);
    select_profile(p,"fixture"); assert(tube_player_ok_mode()==0 && tube_player_hide_ms()==3000);
    assert(tube_player_seek_seconds()==10 && tube_player_set_seek_seconds(30));
    assert(!tube_player_seek_mode() && tube_player_set_seek_mode(1));
    assert(tube_player_set_ok_mode(3) && tube_player_set_hide_seconds(15));
    p->shutdown(); assert(!p->init());
    assert(tube_player_seek_seconds()==30);
    assert(tube_player_ok_mode()==3 && tube_player_hide_ms()==15000);
    assert(tube_player_seek_mode()==1);
    select_profile(p,"guest"); assert(tube_player_seek_seconds()==7); assert(tube_player_ok_mode()==2 && !tube_player_hide_ms());
    assert(tube_player_seek_mode()==2);
    p->shutdown();
    cJSON *root=read_store(); cJSON_DeleteItemFromObject(root,"player_preferences");
    cJSON *records=cJSON_GetObjectItem(root,"records"), *record=cJSON_CreateObject();
    cJSON_AddStringToObject(record,"profile","legacy"); cJSON_AddItemToArray(records,record);
    write_store(root); cJSON_Delete(root); assert(!p->init());
    assert(tube_player_ok_mode()==1 && tube_player_hide_ms()==3500);
    assert(!tube_player_seek_mode());
    select_profile(p,"legacy"); assert(tube_player_ok_mode()==1 && tube_player_hide_ms()==3500);
    select_profile(p,"new"); assert(tube_player_ok_mode()==0 && tube_player_hide_ms()==3000);
    p->shutdown(); root=read_store();
    cJSON_ReplaceItemInObject(root,"player_preferences",cJSON_Parse("["
        "{\"profile\":\"new\",\"ok_mode\":2.5,\"hide_ms\":3000},"
        "{\"profile\":\"new\",\"ok_mode\":2,\"hide_ms\":3501},"
        "{\"profile\":\"bad/profile\",\"ok_mode\":2,\"hide_ms\":3000},"
        "{\"profile\":\"new\",\"ok_mode\":\"3\",\"hide_ms\":3000},"
        "{\"profile\":\"new\",\"ok_mode\":3,\"hide_ms\":3500,\"seek_mode\":1.5,\"seek_seconds\":4},"
        "{\"profile\":\"new\",\"ok_mode\":0,\"hide_ms\":0}]"));
    write_store(root); cJSON_Delete(root); assert(!p->init());
    assert(tube_player_ok_mode()==3 && tube_player_hide_ms()==3500);
    assert(!tube_player_seek_mode() && tube_player_set_seek_mode(2));
    assert(tube_player_seek_seconds()==10);
    assert(tube_player_set_hide_seconds(1)); p->shutdown(); assert(!p->init());
    assert(tube_player_ok_mode()==3 && tube_player_hide_ms()==1000);
    assert(tube_player_seek_mode()==2);
    p->shutdown(); root=read_store(); cJSON *preferences=cJSON_CreateArray();
    for (int i=0;i<65;++i) {
        char profile[20]; snprintf(profile,sizeof profile,"profile%d",i);
        cJSON *row=cJSON_CreateObject(); cJSON_AddStringToObject(row,"profile",profile);
        cJSON_AddNumberToObject(row,"ok_mode",2); cJSON_AddNumberToObject(row,"hide_ms",1000);
        cJSON_AddItemToArray(preferences,row);
    }
    cJSON_ReplaceItemInObject(root,"player_preferences",preferences);
    write_store(root); cJSON_Delete(root); assert(!p->init());
    select_profile(p,"profile63"); assert(tube_player_ok_mode()==2 && tube_player_hide_ms()==1000);
    select_profile(p,"profile64"); assert(tube_player_ok_mode()==0 && tube_player_hide_ms()==3000);
    assert(!tube_player_set_ok_mode(1) && !tube_player_set_hide_seconds(5));
    assert(!tube_player_set_seek_mode(2));
    assert(!tube_player_set_seek_seconds(7));
    p->shutdown(); unlink(evo_data_path("youtube.json")); assert(!rmdir(dir));
    puts("PASS: player preferences, chooser validation, per-profile persistence, legacy migration and corrupt rows");
    return 0;
}
