/* Profile persistence, menu paging and active-video isolation through real provider. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
#include "tube_video_geometry.h"
static void menu(const char* id,int page){evo_provider_youtube.list_catalog(id,page,items_cb,NULL);assert(last_ok);}
int main(void){
    char dir[]="/tmp/tube-video-geometry-XXXXXX";assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t* p=&evo_provider_youtube;assert(!p->init());
    assert(!tube_player_resize_mode() && !tube_player_zoom_percent() && !tube_player_video_geometry());
    const int expected[]={50,55,60,65,70,75,80,85,90,95,96,97,98,99,100,
        105,110,115,120,125,130,135,140,145,150,155,160,165,170,175,180,
        185,190,195,200,205,210,215,220,225,230,235,240,245,250,255,260,
        265,270,275,280,285,290,295,300};
    int total=0;
    for(int page=0;page<3;++page){menu("settings:video-zoom",page);assert(last_count==(page<2?24:12));assert(last_more==(page<2));
        for(int i=0;i<last_count;++i)if(total+i>=5){char id[64];snprintf(id,sizeof id,"settings:zoom:%d",expected[total+i-5]);assert(!strcmp(last_items[i].id,id));}
        total+=last_count;}
    assert(total==60 && !strcmp(last_items[last_count-1].id,"settings:zoom:300"));
    menu("settings:video-zoom",3);assert(!last_count && !last_more);
    for(unsigned index=0;index<sizeof expected/sizeof *expected;++index) {
        int zoom=expected[index];
        char id[64];snprintf(id,sizeof id,"settings:zoom:%d",zoom);menu(id,0);assert(tube_player_zoom_percent()==zoom && !tube_player_resize_mode());
    }
    for(int mode=0;mode<5;++mode){assert(tube_player_set_resize_mode(mode));assert(tube_player_resize_mode()==mode && !tube_player_zoom_percent());}
    const char* invalid[]={"settings:zoom:49","settings:zoom:101","settings:zoom:301","settings:zoom:150junk","settings:resize:5","settings:resize:-1"};
    for(unsigned i=0;i<sizeof invalid/sizeof *invalid;++i){p->list_catalog(invalid[i],0,items_cb,NULL);assert(!last_ok);}
    assert(tube_player_set_zoom_percent(150));
    evo_stream_choice_t choice={0};p->stream_chosen("abcdefghijk",&choice);tube_video_geometry_enable(1);
    assert(tube_player_video_geometry()==(150u<<4|1u));
    assert(tube_player_set_zoom_percent(200) && tube_player_video_geometry()==(200u<<4|1u));
    menu("library:profile:other",0);assert(!tube_player_resize_mode() && !tube_player_zoom_percent());
    assert(tube_player_set_resize_mode(3));assert(tube_player_video_geometry()==(200u<<4|1u));
    tube_video_geometry_enable(0);assert(!tube_player_video_geometry());
    p->stream_chosen("zyxwvutsrqp",&choice);tube_video_geometry_enable(1);assert(tube_player_video_geometry()==7u);
    p->shutdown();assert(!tube_player_video_geometry());assert(!p->init());
    assert(tube_player_resize_mode()==3 && !tube_player_zoom_percent());
    menu("library:profile:guest",0);assert(!tube_player_resize_mode() && tube_player_zoom_percent()==200);
    assert(!tube_player_video_geometry());
    p->shutdown();cJSON* root=read_store();cJSON* rows=cJSON_GetObjectItem(root,"player_preferences");
    cJSON* guest=cJSON_GetArrayItem(rows,0);assert(!strcmp(cJSON_GetObjectItem(guest,"profile")->valuestring,"guest"));
    cJSON_ReplaceItemInObject(guest,"resize_mode",cJSON_CreateNumber(7));cJSON_ReplaceItemInObject(guest,"zoom_percent",cJSON_CreateNumber(105.5));
    write_store(root);cJSON_Delete(root);assert(!p->init());assert(!tube_player_resize_mode() && !tube_player_zoom_percent());
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: all 60 sizing options, profile persistence, invalid/legacy values and active-picture isolation");
}
