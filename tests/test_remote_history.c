/* "Disable history when using remote control": real receiver ownership, provider records and reports. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
#include "tube_remote.h"
#include "tube_phone.h"
static const evo_provider_t *p;
static tube_remote_player snapshot_value;
static int snapshot_active;
static int snapshot(void *ctx,tube_remote_player *out) {(void)ctx;*out=snapshot_value;return snapshot_active;}
static void show(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(last_ok);}
/* Receiver state reports share the mock transport; count and answer account reports only. */
static int reports(void) {
    int count=0;
    for(int i=0;i<pending_count;++i)if(strstr(pending[i].body,"watch_progress"))++count;
    return count;
}
static void settle(void) {
    for(int i=pending_count-1;i>=0;--i)if(strstr(pending[i].body,"watch_progress")) {
        cJSON *job=cJSON_Parse(pending[i].body);
        char body[96];snprintf(body,sizeof body,"{\"success\":true,\"position_sec\":%d}",cJSON_GetObjectItem(job,"position_sec")->valueint);
        cJSON_Delete(job);reply(i,1,200,body);
    }
}
static double position(void) {
    cJSON *store=read_store(),*entry;double value=-1;
    cJSON_ArrayForEach(entry,cJSON_GetObjectItem(store,"records"))
        if(!strcmp(cJSON_GetObjectItem(entry,"id")->valuestring,"abcdefghijk") && !strcmp(cJSON_GetObjectItem(entry,"profile")->valuestring,"fixture"))
            value=cJSON_GetObjectItem(entry,"position")->valuedouble;
    cJSON_Delete(store);return value;
}
static const char *preference(void) {
    static char title[192];show("settings:remote");
    for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].id,"settings:remote:history")){snprintf(title,sizeof title,"%s",last_items[i].title);return title;}
    return "";
}
static int saved(void) {
    cJSON *store=read_store();int value=cJSON_IsTrue(cJSON_GetObjectItem(store,"remote_disable_history"));
    cJSON_Delete(store);return value;
}
static void resolve_video(void) {
    evo_stream_choice_t choice={0};snprintf(choice.url,sizeof choice.url,"http://127.0.0.1:8765/media/capability/p0");
    assert(!p->resolve("abcdefghijk",resolve_cb,NULL));
    int job=-1;for(int i=0;i<pending_count;++i)if(strstr(pending[i].body,"\"action\":\"resolve\""))job=i;
    assert(job>=0);reply(job,1,200,"{\"job\":\"fixture_job\",\"state\":\"pending\"}");
    usleep(300000);tube_poll();job=-1;
    for(int i=0;i<pending_count;++i)if(strstr(pending[i].url,"/v1/jobs/fixture_job"))job=i;
    assert(job>=0);reply(job,1,200,"{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"title\":\"First\",\"duration_sec\":100,\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/capability/p0\",\"label\":\"360p\",\"container\":\"mp4\"}]}}");
    assert(last_ok);p->stream_chosen("abcdefghijk",&choice);
    p->report_progress("abcdefghijk",0,100,EVO_PROVIDER_PLAY_START);
}
int main(void) {
    char dir[]="/tmp/tube-remote-history-XXXXXX";assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    cJSON *root=cJSON_Parse("{\"version\":2,\"source\":\"http://127.0.0.1:8765\",\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\",\"library_profile\":\"fixture\",\"remote_enabled\":true,\"remote_device\":\"01234567-89ab-cdef-0123-456789abcdef\",\"remote_screen\":\"screen-123\",\"records\":[{\"id\":\"abcdefghijk\",\"profile\":\"fixture\",\"title\":\"First\",\"duration\":100}]}");
    write_store(root);cJSON_Delete(root);p=&evo_provider_youtube;assert(!p->init());
    assert(!strcmp(preference(),"History for phone playback: on") && !saved());
    tube_phone_sync_scope();tube_remote_tick(1000000000000LL);assert(pending_count==1);
    reply(0,1,200,"{\"api_version\":1,\"session\":\"0123456789abcdef0123456789abcdef\",\"device_id\":\"01234567-89ab-cdef-0123-456789abcdef\",\"screen_id\":\"screen-123\",\"state\":\"listening\"}");
    tube_remote_tick(1000000000001LL);reply(0,1,200,"{\"api_version\":1,\"session\":\"0123456789abcdef0123456789abcdef\",\"state\":\"listening\",\"ack\":0,\"commands\":[]}");
    assert(tube_remote_current_owner() && !tube_phone_owns_media());
    /* The phone starts a video; the adapter owns it once the player reports that selection. */
    tube_remote_tick(1000000001000LL);
    reply(0,1,200,"{\"api_version\":1,\"session\":\"0123456789abcdef0123456789abcdef\",\"state\":\"listening\",\"ack\":0,\"commands\":[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[],\"playlist\":\"\",\"index\":0,\"position_sec\":0,\"ctt\":\"\"}]}");
    tube_phone_controls controls={0};controls.snapshot=snapshot;
    tube_phone_tick_at(&controls,1000);char id[12];double start;
    unsigned ticket=tube_phone_take(id,&start);assert(ticket && !strcmp(id,"abcdefghijk"));
    resolve_video();
    assert(tube_phone_launch_bind(ticket,tube_phone_selection()) && tube_phone_launch_finish(ticket,1));
    snapshot_active=1;strcpy(snapshot_value.video,id);snapshot_value.selection=tube_phone_selection();
    snapshot_value.state=1;snapshot_value.position=1;snapshot_value.duration=100;
    tube_phone_tick_at(&controls,1001);assert(tube_phone_owns_media());
    /* Default: phone-owned playback is recorded and reported like any other. */
    p->report_progress("abcdefghijk",12,100,EVO_PROVIDER_PLAY_UPDATE);assert(reports()==1);settle();
    /* Cast-only records have no catalog artwork; both History and Continue watching use this catalog. */
    show("history");assert(last_count==1 && !strcmp(last_items[0].id,"abcdefghijk"));
    assert(!strcmp(last_items[0].art_url,"https://i.ytimg.com/vi/abcdefghijk/hqdefault.jpg") && tube_art_url_allowed(last_items[0].art_url));
    /* On: no local position or history time, and no account report, while the phone owns playback. */
    show("settings:remote:history");assert(saved() && position()==12);
    assert(!strcmp(preference(),"History for phone playback: off") && tube_phone_owns_media());
    p->report_progress("abcdefghijk",40,100,EVO_PROVIDER_PLAY_STOP);tube_poll();
    assert(!reports() && position()==12);
    /* Off again: the same session was never closed by the suppressed stop. */
    show("settings:remote:history");assert(!saved());
    p->report_progress("abcdefghijk",41,100,EVO_PROVIDER_PLAY_STOP);
    assert(reports()==1 && position()==41);settle();
    /* The preference never affects playback the phone does not own. */
    show("settings:remote:history");tube_phone_clear();assert(!tube_phone_owns_media());
    resolve_video();p->report_progress("abcdefghijk",50,100,EVO_PROVIDER_PLAY_STOP);
    assert(reports()==1 && position()==50);settle();
    /* Device-owned: profiles share it, reload keeps it, and a corrupt value is off. */
    show("library:profile:guest");assert(saved() && !strcmp(preference(),"History for phone playback: off"));
    p->shutdown();while(pending_count)reply(0,1,200,"{\"api_version\":1,\"state\":\"stopped\"}");
    assert(!p->init());assert(saved() && !strcmp(preference(),"History for phone playback: off"));
    show("library:profile:fixture");show("history");
    assert(last_count==1 && !strcmp(last_items[0].art_url,"https://i.ytimg.com/vi/abcdefghijk/hqdefault.jpg"));
    p->shutdown();while(pending_count)reply(0,1,200,"{\"api_version\":1,\"state\":\"stopped\"}");
    root=read_store();cJSON_ReplaceItemInObject(root,"remote_disable_history",cJSON_CreateString("yes"));
    cJSON_ReplaceItemInObject(root,"remote_enabled",cJSON_CreateFalse());
    cJSON *entry=cJSON_GetArrayItem(cJSON_GetObjectItem(root,"records"),0);
    cJSON_ReplaceItemInObject(entry,"art_url",cJSON_CreateString("https://i.ytimg.com/vi/abcdefghijk/hq1.jpg"));
    write_store(root);cJSON_Delete(root);
    assert(!p->init());assert(!strcmp(preference(),"History for phone playback: on"));
    show("settings:captions");assert(!saved());
    show("history");assert(last_count==1 && !strcmp(last_items[0].art_url,"https://i.ytimg.com/vi/abcdefghijk/hq1.jpg"));
    p->shutdown();while(pending_count)reply(0,1,200,"{\"api_version\":1,\"state\":\"stopped\"}");
    unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: remote-control history preference, phone ownership scope, unowned playback, device persistence and corrupt value");
}
