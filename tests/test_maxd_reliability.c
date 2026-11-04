/* Real provider paths for channel overrides, interrupted queues and maintenance. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
#include "tube_backup.h"
static const evo_provider_t *p;
static unsigned long long thumbnail_bytes=2ull*1024*1024;static int thumbnails=2;
int tube_maxd_thumbnail_cache(unsigned long long *bytes,int *count,int clear) {if(clear){thumbnail_bytes=0;thumbnails=0;}if(bytes)*bytes=thumbnail_bytes;if(count)*count=thumbnails;return 1;}
static void choose(const char *id) {int rc=p->list_catalog(id,0,items_cb,NULL);if(rc||!last_ok)fprintf(stderr,"Refused action: %s\n",id);assert(!rc && last_ok);}
static void export_page(const char *file) {
    const char *dir=getenv("MAXD_RELIABILITY_FIXTURE_DIR");if(!dir)return;
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
static void health(void) {while(pending_count && strstr(pending[0].url,"/v1/health"))reply(0,1,200,"{\"api_version\":1,\"features\":[\"search\",\"library\"]}");}
static evo_stream_choice_t choice;
static void resolved(int ok,const evo_stream_choice_t *choices,int count,void *ud){(void)ud;assert(ok&&count==1);choice=choices[0];}
static void resolve_a(int ceiling) {
    assert(!p->resolve("abcdefghijk",resolved,NULL));char expected[64];snprintf(expected,sizeof expected,"\"max_height\":%d",ceiling);assert(pending_count && strstr(pending[0].body,expected));
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"title\":\"Fixture\",\"duration_sec\":900,\"loudness_db\":6,\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/capability/p0\",\"label\":\"720p\",\"container\":\"mp4\",\"height\":720,\"fps\":30,\"video_codec\":\"h264\"}]}}");health();
}
int main(void) {
    char dir[]="/tmp/maxd-reliability-XXXXXX";assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));p=&evo_provider_youtube;assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));p->search("seed",0,items_cb,NULL);reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"First\",\"subtitle\":\"Channel One\",\"channel_id\":\"UCabcdefghijklmnopqrstuv\",\"duration_sec\":900},{\"id\":\"bcdefghijkl\",\"title\":\"Next\",\"subtitle\":\"Channel One\",\"channel_id\":\"UCabcdefghijklmnopqrstuv\",\"duration_sec\":900}]}}");health();
    choose("settings");assert(last_count>0);
    choose("settings:reliability");assert(last_count==6);export_page("settings_reliability");
    choose("settings:channel-prefs");assert(last_count==1 && !strcmp(last_items[0].title,"Channel One"));
    const char *quality="settings:channel-prefs:guest:UCabcdefghijklmnopqrstuv:quality";
    choose(quality);choose(quality); /* channel ceiling 720 */
    choose("settings:channel-prefs:guest:UCabcdefghijklmnopqrstuv:speed");
    choose("settings:channel-prefs:guest:UCabcdefghijklmnopqrstuv:captions");
    choose("settings:channel-prefs:guest:UCabcdefghijklmnopqrstuv:leveling");choose("settings:channel-prefs:guest:UCabcdefghijklmnopqrstuv:leveling");
    choose("settings:channel-prefs:guest:UCabcdefghijklmnopqrstuv");export_page("settings_channel-prefs_guest_UCabcdefghijklmnopqrstuv");
    assert(tube_playback_speed_for("bcdefghijkl")==50 && !tube_captions_for("bcdefghijkl"));
    resolve_a(720);p->stream_chosen("abcdefghijk",&choice);p->report_progress("abcdefghijk",0,900,EVO_PROVIDER_PLAY_START);
    assert(!tube_extra_auto_volume() && tube_extra_gain_milli()>0 && tube_extra_gain_milli()<1000);
    (void)tube_error_fix_begin(TUBE_ERROR_VIDEO_DECODER,42,0,0);
    choose("settings:playback-diagnostics");export_page("settings_playback-diagnostics");
    assert(strstr(tube_maxd_playback_diagnosis(),"video decoder") && !strstr(tube_maxd_playback_diagnosis(),"://"));
    /* Clean stop clears the offer; checkpoints survive only missing stop reports. */
    p->report_progress("abcdefghijk",50,900,EVO_PROVIDER_PLAY_STOP);p->shutdown();drain();assert(!p->init());
    choose("settings:session-recovery");assert(last_count==1 && strstr(last_items[0].title,"No interrupted"));
    choose("settings:max-quality:480");resolve_a(480);p->stream_chosen("abcdefghijk",&choice);
    p->report_progress("abcdefghijk",0,900,EVO_PROVIDER_PLAY_START);p->report_progress("abcdefghijk",123,900,EVO_PROVIDER_PLAY_UPDATE);
    assert(tube_toggle_queue("bcdefghijkl")); /* also commits the latest checkpoint */
    p->shutdown();drain();assert(!p->init());health();
    choose("settings:session-recovery");assert(last_count==3 && strstr(last_items[0].subtitle,"2:03"));assert(!strcmp(tube_row_fact_get(last_items[0].id,"screen"),"grid"));export_page("settings_session-recovery");
    choose("queue");assert(last_count==1 && !strcmp(last_items[0].id,"bcdefghijkl"));
    choose("library:profile:alice");p->list_catalog("settings:session-recovery:guest:resume",0,items_cb,NULL);assert(!last_ok);
    p->list_catalog(quality,0,items_cb,NULL);assert(!last_ok);assert(strstr(tube_maxd_playback_diagnosis(),"No playback failure"));
    choose("library:profile:guest");choose("settings:session-recovery:guest:resume");assert(last_count==1 && !last_items[0].is_folder && last_items[0].resume_pos_sec==123);
    p->shutdown();drain();assert(!p->init());choose("settings:session-recovery:guest:dismiss");
    choose("queue");assert(last_count==1);
    choose("settings:quick-switch");export_page("settings_quick-switch");assert(last_count==4 && !strcmp(tube_row_fact_get("search-history","screen"),"search"));
    choose("settings:storage");export_page("settings_storage");
    choose("settings:storage:guest:thumbnails");assert(!thumbnail_bytes && !thumbnails);
    char path[512];snprintf(path,sizeof path,"%s/active.srt",dir);FILE *file=fopen(path,"w");assert(file && fputs("temporary captions",file)>=0 && !fclose(file));
    drain();choose("settings:storage:guest:captions");assert(access(path,F_OK));
    assert(!access(evo_data_path("youtube.json"),F_OK));
    choose("settings:backup:create");tube_backup_entry entries[4];int count=tube_backup_list(dir,entries,4);assert(count>0);
    tube_backup_summary summary;assert(!tube_backup_inspect(dir,entries[0].name,&summary));assert(summary.queued==1 && summary.channels==1 && summary.records>=2);
    snprintf(path,sizeof path,"settings:backup:entry:%s",entries[0].name);choose(path);assert(last_count==3 && strstr(last_items[2].subtitle,"1 channel rules"));assert(strstr(last_items[0].overview,"Stored data preview"));export_page("settings_backup_entry_reliability");
    /* History-disabled playback must not create an interruption record. */
    choose("settings:history:2");p->stream_chosen("bcdefghijkl",&choice);p->report_progress("bcdefghijkl",20,900,EVO_PROVIDER_PLAY_START);
    p->shutdown();drain();assert(!p->init());choose("settings:session-recovery");assert(last_count==1 && strstr(last_items[0].title,"No interrupted"));
    p->shutdown();drain();
    for(int i=0;i<count;++i){assert(!tube_backup_delete(dir,entries[i].name));}
    snprintf(path,sizeof path,"%s/backup",dir);rmdir(path);unlink(evo_data_path("youtube.json"));rmdir(dir);
    puts("PASS: channel quality/speed/captions/leveling, profile isolation, interrupted position/queue recovery without autoplay, clean-stop/privacy behavior, diagnostics, cache controls and offline restore preview");
}
