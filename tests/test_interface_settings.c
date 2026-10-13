/* Interface preferences on the shipping provider: the settings screens, every
 * value, persistence per profile, corrupt and older files, the arranged video
 * actions menu in the browser and in the player, and the Circle rules. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
#include "tube_ui_style.h"
static const evo_provider_t *p;
static char row[160];
static int find(const char *title) {
    for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].title,title)){snprintf(row,sizeof row,"%s",last_items[i].id);return i;}
    return -1;
}
static int at(const char *id) {
    for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].id,id))return i;
    return -1;
}
static void show(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(last_ok && !pending_count);}
static void refuse(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(!last_ok && !pending_count);}
static void menu(const char *id) {
    char parent[64];snprintf(parent,sizeof parent,"account:menu:%s",id);show(parent);
}
static const TubeUiPrefs *ui(void) { return tube_ui_prefs(); }
static cJSON *profile_row(cJSON *store,const char *profile) {
    cJSON *entry;
    cJSON_ArrayForEach(entry,cJSON_GetObjectItem(store,"profile_settings"))
        if(!strcmp(cJSON_GetObjectItem(entry,"profile")->valuestring,profile))return entry;
    return NULL;
}
static void play(const char *id) {
    evo_stream_choice_t selected={0};snprintf(selected.url,sizeof selected.url,"http://127.0.0.1:8765/media/capability/p0");
    assert(!p->resolve(id,resolve_cb,NULL));
    char body[320];snprintf(body,sizeof body,"{\"state\":\"done\",\"result\":{\"id\":\"%s\",\"title\":\"First\",\"duration_sec\":100,\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/capability/p0\",\"label\":\"360p\",\"container\":\"mp4\"}]}}",id);
    poll_job(body);assert(last_ok);p->stream_chosen(id,&selected);
}
static const char *listed="{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":["
    "{\"id\":\"abcdefghijk\",\"title\":\"First\",\"subtitle\":\"Fixture channel\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"duration_sec\":100,\"playback_context\":{\"playlist\":\"PLfixture01\",\"index\":0}},"
    "{\"id\":\"bcdefghijkl\",\"title\":\"Second\",\"channel_id\":\"not-a-channel\"}],\"has_more\":false}}";
int main(void) {
    char dir[]="/tmp/tube-interface-XXXXXX",label[160];
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    SEPARATE_SETTINGS();
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->list_catalog("playlist:PLfixture01",0,items_cb,NULL);
    reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");poll_job(listed);
    assert(last_ok && last_count==2);

    /* Defaults: the browser as it was. */
    TubeUiPrefs defaults;tube_ui_defaults(&defaults);
    assert(!memcmp(ui(),&defaults,sizeof defaults) && tube_ui_plain(ui()));
    /* The Interface screen is a row of YouTube preferences; Search options carries the search rule. */
    { int found=0;
      for(int page=0;page<4 && !found;++page){p->list_catalog("settings",page,items_cb,NULL);assert(last_ok);found=at("settings:ui")>=0;if(!last_more)break;}
      assert(found && find("Interface")>=0); }
    show("settings:ui");assert(last_count==TUBE_UI_GROUPS);
    assert(find("Interface scale: 1x")==0 && find("Video grid scale: 1x")==1 && find("Card title lines: 1")==2 && find("Scroll long titles: Off")==3);
    assert(find("Title scroll speed: 1x")==4 && find("Multiline subtitles: Off")==5 && find("Rounded card corners: On")==6);
    assert(find("Theme: Afterglow")==7 && find("Circle on the main menu: Does nothing")==8 && find("Video actions menu")==9);
    assert(!strcmp(row,"settings:menu") && at("settings:ui:search-exit")<0);
    show("settings:search");assert(find("Leaving search results: Press Circle once")==last_count-1 && !strcmp(row,"settings:ui:search-exit"));
    /* Every group lists its values with the current one marked, and every value can be chosen. */
    for(int group=0;group<TUBE_UI_GROUPS;++group) {
        if(group==TUBE_UI_GROUP_SCHEME) {
            /* Theme: the four themes and the row of the classic colours; the classic schemes are that row's list. */
            for(int i=TUBE_UI_SCHEMES-1;i>=0;--i) {
                char id[80],expected[96];snprintf(id,sizeof id,"%s:%d",g_tube_ui_groups[group].id,i);
                tube_ui_group_label(group,i,expected,sizeof expected);
                const unsigned revision=tube_ui_revision();
                const int classic=i>=TUBE_UI_THEMES,slot=classic?i-TUBE_UI_THEMES:i;
                show(id);assert(ui()->scheme==i && last_count==(classic?TUBE_UI_SCHEMES-TUBE_UI_THEMES:TUBE_UI_THEMES+1));
                assert(!strcmp(last_items[slot].id,id) && !strcmp(last_items[slot].title,expected));
                for(int k=0;k<last_count;++k)assert((k==slot)==!strcmp(last_items[k].subtitle,"Selected"));
                assert(tube_ui_revision()!=revision);            /* the browser restyles itself */
            }
            assert(!strcmp(last_items[TUBE_UI_THEMES].id,"settings:ui:scheme:classic") && !strcmp(last_items[TUBE_UI_THEMES].title,"Classic colours"));
            continue;
        }
        show(g_tube_ui_groups[group].id);
        const int count=tube_ui_group_count(group),selected=tube_ui_group_selected(ui(),group);
        assert(last_count==count && !strcmp(last_items[selected].subtitle,"Selected"));
        for(int i=0;i<count;++i) {
            char id[80],expected[96];snprintf(id,sizeof id,"%s:%d",g_tube_ui_groups[group].id,i);
            tube_ui_group_label(group,i,expected,sizeof expected);
            assert(!strcmp(last_items[i].id,id) && !strcmp(last_items[i].title,expected) && (i==selected)==!strcmp(last_items[i].subtitle,"Selected"));
        }
        for(int i=count-1;i>=0;--i) {
            char id[80];snprintf(id,sizeof id,"%s:%d",g_tube_ui_groups[group].id,i);
            const unsigned revision=tube_ui_revision();
            show(id);assert(tube_ui_group_selected(ui(),group)==i && last_count==count && !strcmp(last_items[i].subtitle,"Selected"));
            assert(tube_ui_revision()!=revision);            /* the browser restyles itself */
        }
        assert(tube_ui_group_selected(ui(),group)==0);
        char id[80];snprintf(id,sizeof id,"%s:%d",g_tube_ui_groups[group].id,selected);show(id);
    }
    assert(!memcmp(ui(),&defaults,sizeof defaults));
    /* Unlisted values are refused and change nothing. */
    const char *invalid[]={"settings:ui:scale:19","settings:ui:scale:-1","settings:ui:scale:x","settings:ui:scale:3junk","settings:ui:scale:",
        "settings:ui:nope","settings:ui:nope:1","settings:ui:grid:13","settings:ui:title-lines:4","settings:ui:scheme:13","settings:ui:scheme:classic:1","settings:ui:scheme:classics","settings:ui:exit:3",
        "settings:ui:search-exit:2","settings:ui:scale:99999999999999999999","settings:ui:"};
    for(unsigned i=0;i<sizeof invalid/sizeof *invalid;++i){refuse(invalid[i]);assert(!memcmp(ui(),&defaults,sizeof defaults));}
    p->list_catalog("settings:ui:nope",1,items_cb,NULL);assert(!last_ok);

    /* Saved per profile under their own names. */
    show("settings:ui:scale:6");show("settings:ui:grid:8");show("settings:ui:title-lines:2");show("settings:ui:title-scroll:1");
    show("settings:ui:scroll-speed:3");show("settings:ui:subtitle-lines:1");show("settings:ui:rounded:0");show("settings:ui:scheme:8");
    show("settings:ui:exit:1");show("settings:ui:search-exit:1");
    assert(ui()->ui_scale==80 && ui()->grid_scale==125 && ui()->title_lines==3 && ui()->title_scroll==1 && ui()->scroll_speed==25);
    assert(ui()->subtitle_lines==2 && ui()->square==1 && ui()->scheme==8 && ui()->app_exit==TUBE_UI_EXIT_DOUBLE && ui()->search_exit==1);
    show("settings:ui");assert(find("Interface scale: 0.8x")==0 && find("Video grid scale: 1.25x")==1 && find("Rounded card corners: Off")==6 &&
        find("Theme: Dark grey (true black)")==7 && find("Circle on the main menu: Press twice to close ProsperoTube")==8);
    cJSON *store=read_store(),*guest=profile_row(store,"guest");assert(guest);
    assert(cJSON_GetObjectItem(guest,"ui_scale")->valueint==80 && cJSON_GetObjectItem(guest,"video_grid_scale")->valueint==125);
    assert(cJSON_GetObjectItem(guest,"card_title_lines")->valueint==3 && cJSON_IsTrue(cJSON_GetObjectItem(guest,"card_title_scroll")));
    assert(cJSON_GetObjectItem(guest,"card_scroll_speed")->valueint==25 && cJSON_IsTrue(cJSON_GetObjectItem(guest,"card_multiline_subtitle")));
    assert(cJSON_IsTrue(cJSON_GetObjectItem(guest,"card_square_corners")) && !strcmp(cJSON_GetObjectItem(guest,"color_scheme")->valuestring,"dark-grey-oled"));
    assert(cJSON_GetObjectItem(guest,"app_exit")->valueint==1 && cJSON_IsTrue(cJSON_GetObjectItem(guest,"search_exit_double")));
    assert(!cJSON_GetObjectItem(guest,"video_menu"));           /* the built-in menu is not written out */
    cJSON_Delete(store);
    TubeUiPrefs chosen=*ui();
    const unsigned before=tube_ui_revision();
    show("library:profile:alice");assert(!memcmp(ui(),&defaults,sizeof defaults) && tube_ui_revision()!=before);
    show("settings:ui:scheme:7");show("settings:ui:scale:12");
    show("library:profile:guest");assert(!memcmp(ui(),&chosen,sizeof chosen));
    p->shutdown();assert(!p->init());assert(!memcmp(ui(),&chosen,sizeof chosen));
    show("library:profile:alice");assert(ui()->scheme==7 && ui()->ui_scale==110 && ui()->grid_scale==100 && !ui()->app_exit);
    show("library:profile:guest");

    /* Theme lists the four themes and the row of the classic colours; that row lists the nine classic schemes,
     * and a choice made in either list shows that list again. */
    show("settings:ui:scheme");assert(last_count==TUBE_UI_THEMES+1 && find("Afterglow")==0 && find("Onyx")==1 && find("Daybreak")==2 &&
        find("High contrast")==3 && find("Classic colours: Dark grey (true black)")==4 && !strcmp(row,"settings:ui:scheme:classic"));
    show("settings:ui:scheme:classic");assert(last_count==TUBE_UI_SCHEMES-TUBE_UI_THEMES && find("Classic ProsperoTube")==0 && find("Teal")==1 &&
        find("Dark blue (true black)")==8 && find("Afterglow")<0 && ui()->scheme==8);
    show("settings:ui:scheme:5");assert(ui()->scheme==5 && last_count==TUBE_UI_SCHEMES-TUBE_UI_THEMES && find("Teal")==1);
    show("settings:ui:scheme:1");assert(ui()->scheme==1 && last_count==TUBE_UI_THEMES+1 && find("Onyx")==1 && find("Classic colours")==4);
    show("settings:ui:scheme:8");
    /* A file written before the themes: "default", which every profile that never chose carried, is the first
     * theme; a classic scheme that was chosen stays chosen. Afterglow is saved under its own name. */
    chosen=*ui();
    p->shutdown();store=read_store();guest=profile_row(store,"guest");
    cJSON_ReplaceItemInObject(guest,"color_scheme",cJSON_CreateString("default"));
    write_store(store);cJSON_Delete(store);assert(!p->init());
    assert(!ui()->scheme && !strcmp(g_tube_schemes[ui()->scheme].id,"afterglow") && ui()->ui_scale==chosen.ui_scale);
    p->shutdown();store=read_store();guest=profile_row(store,"guest");
    cJSON_ReplaceItemInObject(guest,"color_scheme",cJSON_CreateString("teal-oled"));
    write_store(store);cJSON_Delete(store);assert(!p->init());
    assert(ui()->scheme==9 && !strcmp(g_tube_schemes[9].label,"Teal (true black)"));
    show("settings:ui:scheme:0");
    store=read_store();guest=profile_row(store,"guest");
    assert(!strcmp(cJSON_GetObjectItem(guest,"color_scheme")->valuestring,"afterglow"));cJSON_Delete(store);
    show("settings:ui:scheme:8");

    /* Corrupt values fall back one by one; a file without the fields reads as the defaults. */
    p->shutdown();store=read_store();guest=profile_row(store,"guest");
    cJSON_ReplaceItemInObject(guest,"ui_scale",cJSON_CreateNumber(99));
    cJSON_ReplaceItemInObject(guest,"video_grid_scale",cJSON_CreateString("big"));
    cJSON_ReplaceItemInObject(guest,"card_title_lines",cJSON_CreateNumber(2.5));
    cJSON_ReplaceItemInObject(guest,"card_scroll_speed",cJSON_CreateNumber(27));
    cJSON_ReplaceItemInObject(guest,"color_scheme",cJSON_CreateString("neon"));
    cJSON_ReplaceItemInObject(guest,"app_exit",cJSON_CreateNumber(7));
    cJSON_DeleteItemFromObject(guest,"video_menu");cJSON_AddItemToObject(guest,"video_menu",cJSON_CreateString("everything"));
    write_store(store);cJSON_Delete(store);assert(!p->init());
    assert(ui()->ui_scale==100 && ui()->grid_scale==100 && ui()->title_lines==1 && ui()->scroll_speed==10 && !ui()->scheme && !ui()->app_exit);
    assert(ui()->title_scroll==1 && ui()->subtitle_lines==2 && ui()->square==1 && ui()->search_exit==1);   /* the valid ones stay */
    for(int i=0;i<TUBE_MENU_KEYS;++i)assert(ui()->menu_order[i]==i);
    p->shutdown();store=read_store();guest=profile_row(store,"guest");
    for(const char **key=(const char *[]){"ui_scale","video_grid_scale","card_title_lines","card_title_scroll","card_scroll_speed",
            "card_multiline_subtitle","card_square_corners","color_scheme","app_exit","search_exit_double","video_menu",NULL};*key;++key)
        cJSON_DeleteItemFromObject(guest,*key);
    write_store(store);cJSON_Delete(store);assert(!p->init());
    assert(!memcmp(ui(),&defaults,sizeof defaults));

    /* The video actions menu: the defaults list it as before. */
    static char original[24][EVO_PROVIDER_MAX_TITLE];
    menu("bcdefghijkl");assert(last_count==11);
    const int rows=last_count;
    for(int i=0;i<rows;++i)snprintf(original[i],sizeof original[i],"%s",last_items[i].title);
    assert(!strcmp(original[0],"Choose an account profile first") && !strcmp(original[1],"Play next") && !strcmp(original[rows-1],"Refresh section"));
    const int share=find("Share link"),restart=find("Play from start");assert(share>restart && restart>0);
    /* Its settings: every row in the profile's order, then one screen per row. */
    show("settings:menu");assert(last_count==24 && last_more && !strcmp(last_items[0].id,"settings:menu:like") && !strcmp(last_items[0].title,"Like video: shown"));
    p->list_catalog("settings:menu",1,items_cb,NULL);assert(last_ok && last_count==TUBE_MENU_KEYS+1-24 && !last_more);
    assert(!strcmp(last_items[last_count-1].id,"settings:menu:reset") && !strcmp(last_items[last_count-2].id,"settings:menu:refresh"));
    show("settings:menu:share");assert(last_count==4 && !strcmp(last_items[0].title,"Share link: shown") && find("Move up")==1 && find("Move down")==2 && find("Hide row")==3);
    show("settings:menu:like");assert(last_count==3 && find("Move up")<0 && find("Move down")==1);        /* the first row cannot go up */
    show("settings:menu:refresh");assert(last_count==3 && find("Move up")==1 && find("Move down")<0);       /* nor the last one down */
    /* Hide a row. */
    show("settings:menu:share:toggle");assert(!strcmp(last_items[0].title,"Share link: hidden") && find("Show row")==3);
    menu("bcdefghijkl");assert(last_count==rows-1 && find("Share link")<0);
    for(int i=0,k=0;i<rows;++i)if(strcmp(original[i],"Share link"))assert(!strcmp(last_items[k++].title,original[i]));
    /* Move a row to the top: it follows the notice, the others keep their order. */
    for(int i=0;i<TUBE_MENU_KEYS-1;++i)show("settings:menu:refresh:up");
    assert(last_count==3 && find("Move up")<0);
    menu("bcdefghijkl");assert(last_count==rows-1 && find("Refresh section")==1 && find("Play next")==2 && find("Choose an account profile first")==0);
    show("settings:menu:restart:up");
    menu("bcdefghijkl");assert(find("Play from start")==find("Video details")-1);
    show("settings:menu:restart:down");
    menu("bcdefghijkl");assert(find("Play from start")==find("Video details")+1);
    show("settings:menu");assert(!strcmp(last_items[0].id,"settings:menu:refresh"));
    /* Saved with the profile, and another profile keeps its own menu. */
    store=read_store();guest=profile_row(store,"guest");
    cJSON *saved=cJSON_GetObjectItem(guest,"video_menu");assert(cJSON_GetArraySize(saved)==TUBE_MENU_KEYS);
    assert(!strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(saved,0),"id")->valuestring,"refresh"));
    { int hidden=0;cJSON *entry;
      cJSON_ArrayForEach(entry,saved)if(cJSON_IsTrue(cJSON_GetObjectItem(entry,"hidden"))){++hidden;assert(!strcmp(cJSON_GetObjectItem(entry,"id")->valuestring,"share"));}
      assert(hidden==1); }
    cJSON_Delete(store);
    p->shutdown();assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->list_catalog("playlist:PLfixture01",0,items_cb,NULL);
    if(pending_count && strstr(pending[0].url,"/v1/pair"))reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job(listed);assert(last_ok && last_count==2);
    menu("bcdefghijkl");assert(last_count==rows-1 && find("Refresh section")==1 && find("Share link")<0);
    /* The in-player list of video actions is the same menu, less the rows the player leaves out. */
    show("settings:menu:related:toggle");
    for(int i=0;i<TUBE_MENU_KEYS;++i)show("settings:menu:details:up");
    play("abcdefghijk");
    assert(tube_player_menu_begin(TUBE_PLAYER_ACTIONS)==TUBE_MENU_OPENED && tube_player_menu_status()==1);
    { int details=-1,watched=-1,related=-1;
      for(int i=0;i<tube_player_menu_count();++i) {
          const char *title=tube_player_menu_row(i)->title;
          if(!strcmp(title,"Video details"))details=i;
          if(!strcmp(title,"Mark as watched locally"))watched=i;
          if(!strcmp(title,"Related videos"))related=i;
      }
      assert(details==1 && watched>details && related<0 && tube_player_menu_count()>5); }
    tube_player_menu_close();
    show("library:profile:alice");
    menu("bcdefghijkl");assert(find("Share link")>=0 && find("Refresh section")==last_count-1);
    show("library:profile:guest");
    /* A saved menu that names unknown, repeated or too few rows keeps what it can. */
    p->shutdown();store=read_store();guest=profile_row(store,"guest");
    cJSON *partial=cJSON_Parse("[{\"id\":\"details\"},{\"id\":\"nonsense\",\"hidden\":true},{\"id\":\"details\",\"hidden\":true},{\"id\":\"share\",\"hidden\":true},7]");
    cJSON_ReplaceItemInObject(guest,"video_menu",partial);write_store(store);cJSON_Delete(store);assert(!p->init());
    assert(ui()->menu_order[0]==tube_ui_menu_find("details") && ui()->menu_order[1]==tube_ui_menu_find("share") && ui()->menu_order[2]==0);
    assert(ui()->menu_hidden==(uint64_t)1<<tube_ui_menu_find("share"));
    /* Everything hidden says where to bring the rows back; the default menu returns with one row. */
    for(int key=0;key<TUBE_MENU_KEYS;++key) if(key!=tube_ui_menu_find("share")) {
        snprintf(label,sizeof label,"settings:menu:%s:toggle",g_tube_menu_keys[key].id);show(label);
    }
    menu("bcdefghijkl");assert(last_count==1 && !strcmp(last_items[0].title,"Choose an account profile first"));
    show("settings:menu:reset");assert(last_count==24 && !strcmp(last_items[0].title,"Like video: shown"));
    assert(!ui()->menu_hidden && !memcmp(ui()->menu_order,defaults.menu_order,sizeof defaults.menu_order));
    const char *bad[]={"settings:menu:nope","settings:menu:share:sideways","settings:menu:","settings:menu:share:up:up","settings:menu:nope:toggle"};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;++i){refuse(bad[i]);assert(!ui()->menu_hidden);}
    p->list_catalog("settings:menu:nope",1,items_cb,NULL);assert(!last_ok);

    /* Circle on the main menu: nothing by default; twice, or at once, when chosen. */
    assert(tube_app_exit_press(1000)==TUBE_UI_PRESS_PASS && tube_app_exit_press(1500)==TUBE_UI_PRESS_PASS);
    show("settings:ui:exit:1");
    assert(tube_app_exit_press(2000)==TUBE_UI_PRESS_ARMED && tube_app_exit_press(4000)==TUBE_UI_PRESS_NOW);
    assert(tube_app_exit_press(5000)==TUBE_UI_PRESS_ARMED && tube_app_exit_press(7001)==TUBE_UI_PRESS_ARMED);   /* too late: asks again */
    assert(tube_app_exit_press(7500)==TUBE_UI_PRESS_NOW);
    assert(tube_app_exit_press(8000)==TUBE_UI_PRESS_ARMED);
    show("settings:ui:exit:1");assert(tube_app_exit_press(8100)==TUBE_UI_PRESS_ARMED);                          /* a settings change disarms */
    tube_ui_press_reset();assert(tube_app_exit_press(8200)==TUBE_UI_PRESS_ARMED);
    show("settings:ui:exit:2");assert(tube_app_exit_press(9000)==TUBE_UI_PRESS_NOW);
    show("settings:ui:exit:0");assert(tube_app_exit_press(9100)==TUBE_UI_PRESS_PASS);
    /* Circle on typed results: one press by default, two when chosen; the two rules do not arm each other. */
    assert(tube_search_exit_press(10000)==TUBE_UI_PRESS_NOW);
    show("settings:ui:search-exit:1");show("settings:ui:exit:1");
    assert(tube_search_exit_press(11000)==TUBE_UI_PRESS_ARMED && tube_app_exit_press(11100)==TUBE_UI_PRESS_ARMED);
    assert(tube_search_exit_press(11200)==TUBE_UI_PRESS_ARMED && tube_search_exit_press(11300)==TUBE_UI_PRESS_NOW);
    show("settings:search");assert(find("Leaving search results: Press Circle twice")==last_count-1);
    show("settings:ui:search-exit:0");show("settings:ui:exit:0");
    /* With one settings set for every profile the preferences are shared. */
    show("settings:ui:scheme:3");show("settings:separate-settings");
    show("library:profile:alice");
    show("settings:ui:scheme:1");show("library:profile:guest");assert(ui()->scheme==1);
    p->shutdown();
    puts("PASS: interface settings screens, values, persistence per profile, corrupt and older files, arranged video actions menu in browser and player, Circle rules");
    return 0;
}
