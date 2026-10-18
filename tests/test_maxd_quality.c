/* Real catalog actions and per-profile quality persistence, with mocked HTTPS. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static void choose(const char *id) {
    evo_provider_youtube.list_catalog(id,0,items_cb,NULL); assert(last_ok);
}
static void height(int expected) {
    assert(tube_player_set_hide_seconds(3));
    cJSON *store=read_store();
    assert(cJSON_GetObjectItem(store,"max_height")->valueint==expected);
    cJSON_Delete(store);
}
int main(void) {
    char dir[]="/tmp/maxd-quality-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t *p=&evo_provider_youtube;assert(!p->init());SEPARATE_SETTINGS();
    choose("settings:max-quality"); assert(last_count==8);
    assert(!strcmp(last_items[0].id,"settings:max-quality:144"));
    assert(!strcmp(last_items[7].id,"settings:max-quality:2160"));
    choose("settings:max-quality:720");height(720);
    choose("library:profile:alice");height(2160);
    choose("settings:max-quality:1080");height(1080);
    choose("library:profile:guest");height(720);
    p->shutdown();assert(!p->init());height(720);
    choose("library:profile:alice");height(1080);
    const char *bad[]={"settings:max-quality:","settings:max-quality:-1","settings:max-quality:0",
        "settings:max-quality:721","settings:max-quality:4320","settings:max-quality:720junk","settings:max-quality:-4294967152",
        "settings:max-quality:999999999999999999999999"};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;++i) {
        p->list_catalog(bad[i],0,items_cb,NULL);assert(!last_ok);height(1080);
    }
    /* The release's base shader path ignores imported rotation/mirror bits. */
    assert(tube_player_set_rotation_degrees(90) && tube_player_set_mirror_enabled(1));
    tube_video_geometry_enable(1);
    assert(!(tube_player_video_geometry() & ((3u<<20)|(1u<<22))));
    assert(tube_player_set_zoom_percent(125));
    tube_video_geometry_enable(1);
    assert(((tube_player_video_geometry()>>4)&511)==125);
    tube_video_geometry_enable(0);
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("Max'd quality selection, profile isolation, restart and invalid actions: passed");
}
