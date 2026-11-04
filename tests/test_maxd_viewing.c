/* Actual provider regressions for everyday viewing; no live account needed. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
#include "tube_ui_style.h"
static const evo_provider_t *p;
static void choose(const char *id) {int rc=p->list_catalog(id,0,items_cb,NULL);if(rc||!last_ok)fprintf(stderr,"Failed viewing action: %s\n",id);assert(!rc && last_ok);}
static void export_page(const char *file) {
    const char *dir=getenv("MAXD_VIEWING_FIXTURE_DIR");if(!dir)return;
    cJSON *root=cJSON_CreateObject(),*rows=cJSON_AddArrayToObject(root,"items");cJSON_AddBoolToObject(root,"has_more",last_more);
    for(int i=0;i<last_count;++i) {
        const evo_provider_item_t *item=&last_items[i];cJSON *row=cJSON_CreateObject();
        cJSON_AddStringToObject(row,"id",item->id);cJSON_AddStringToObject(row,"title",item->title);cJSON_AddStringToObject(row,"subtitle",item->subtitle);
        cJSON_AddStringToObject(row,"overview",item->overview);cJSON_AddBoolToObject(row,"is_folder",item->is_folder);
        cJSON_AddBoolToObject(row,"interface",1);cJSON_AddNumberToObject(row,"duration_sec",item->duration_sec);cJSON_AddNumberToObject(row,"resume_sec",item->resume_pos_sec);
        const char *facts[]={"kind","label","value","screen","argument"};
        for(int j=0;j<5;++j){const char *value=tube_row_fact_get(item->id,facts[j]);if(*value)cJSON_AddStringToObject(row,facts[j],value);}
        cJSON_AddItemToArray(rows,row);
    }
    char path[1024];snprintf(path,sizeof path,"%s/%s.json",dir,file);FILE *f=fopen(path,"w");assert(f);
    char *json=cJSON_Print(root);assert(json);assert(fputs(json,f)>=0 && !fclose(f));free(json);cJSON_Delete(root);
}
static void drain(void) {while(pending_count)reply(0,0,500,"{}");}
int main(void) {
    char dir[]="/tmp/maxd-viewing-XXXXXX";assert(mkdtemp(dir));assert(!setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    choose("settings:presets");assert(last_count==3);export_page("settings_presets");
    choose("settings:presets:guest:0");export_page("settings_presets_guest_0");
    choose("settings:max-quality:720");choose("settings:presets:guest:0:save");
    choose("settings:max-quality:2160");choose("settings:presets:guest:0:apply");
    choose("settings:max-quality");assert(strstr(last_items[4].subtitle,"Selected"));
    choose("settings:accessibility:guest:scale");assert(tube_ui_prefs()->ui_scale==115);
    choose("settings:accessibility:guest:motion");assert(tube_ui_prefs()->reduced_motion);
    choose("settings:accessibility:guest:contrast");assert(tube_ui_prefs()->scheme==3);export_page("settings_accessibility");
    choose("settings:accessibility:guest:caption-background");assert(tube_extra_subtitle_style_index()==1);
    choose("settings:accessibility:guest:caption-background");assert(tube_extra_subtitle_style_index()==2);
    choose("settings:accessibility:guest:caption-background");assert(tube_extra_subtitle_style_index()==3);
    char css[12000];assert(tube_ui_rules(tube_ui_prefs(),css,sizeof css)>0 && strstr(css,"transition: none"));
    choose("library:profile:alice");
    p->list_catalog("settings:presets:guest:0:apply",0,items_cb,NULL);assert(!last_ok);
    p->list_catalog("settings:accessibility:guest:scale",0,items_cb,NULL);assert(!last_ok);
    choose("settings:presets:alice:0:apply");choose("settings:max-quality");assert(strstr(last_items[7].subtitle,"Selected"));
    choose("library:profile:guest");choose("settings:presets:guest:0:apply");
    p->shutdown();assert(!p->init());choose("settings:presets:guest:0:apply");
    assert(tube_ui_prefs()->reduced_motion && tube_ui_prefs()->ui_scale==115);
    /* Real playback progress, including profile isolation and completed removal. */
    p->stream_chosen("abcdefghijk",NULL);p->report_progress("abcdefghijk",0,900,EVO_PROVIDER_PLAY_START);
    p->report_progress("abcdefghijk",123,900,EVO_PROVIDER_PLAY_STOP);
    choose("continue-watching");assert(last_count==1 && last_items[0].resume_pos_sec==123 && strstr(last_items[0].subtitle,"2:03"));
    choose("library:profile:alice");choose("continue-watching");assert(!last_count);
    choose("library:profile:guest");p->stream_chosen("abcdefghijk",NULL);p->report_progress("abcdefghijk",900,900,EVO_PROVIDER_PLAY_STOP);
    choose("continue-watching");assert(!last_count);
    /* Catch-up rule metadata is independent of general shared preferences. */
    choose("settings:catchup:new:guest");choose("settings:smart:1");
    export_page("settings_smart_1_catchup");
    choose("settings:catchup");export_page("settings_catchup");
    choose("settings:rest");export_page("settings_rest");
    choose("settings:smart:1:channel:UCabcdefghijklmnopqrstuv");
    choose("settings:smart:1:shorts");
    cJSON *store=read_store(),*rule=cJSON_GetArrayItem(cJSON_GetObjectItem(store,"maxd_smart_playlists"),0);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(rule,"catchup")) && cJSON_GetObjectItem(rule,"source")->valueint==1);
    assert(cJSON_GetArraySize(cJSON_GetObjectItem(rule,"channels"))==1 && cJSON_IsFalse(cJSON_GetObjectItem(rule,"hide_shorts")));cJSON_Delete(store);
    choose("library:profile:alice");p->list_catalog("settings:smart:1:shorts",0,items_cb,NULL);assert(!last_ok);
    p->list_catalog("settings:catchup:new:guest",0,items_cb,NULL);assert(!last_ok);
    p->shutdown();drain();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: Continue Watching progress/completion/profile isolation, saved playback presets and stale actions, accessibility persistence/captions, catch-up channel persistence");
}
