/* Incognito playback and the Auto / Enabled / Disabled history state on the shipping provider. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *p;
static evo_stream_choice_t selected;
static char row[160];
static const char *A="abcdefghijk",*B="bcdefghijkl";
static void show(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(last_ok);}
/* Confirm each account report with the position it carried. */
static void drain(void) {
    while(pending_count) {
        cJSON *job=cJSON_Parse(pending[0].body);const cJSON *at=cJSON_GetObjectItem(job,"position_sec");
        char body[96];snprintf(body,sizeof body,"{\"success\":true,\"position_sec\":%d}",at?at->valueint:0);
        cJSON_Delete(job);reply(0,1,200,body);
    }
}
static int find(const char *title) {
    for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].title,title)){snprintf(row,sizeof row,"%s",last_items[i].id);return i;}
    return -1;
}
static void arm(const char *id) {
    char parent[64];snprintf(parent,sizeof parent,"account:menu:%s",id);show(parent);
    assert(find("Play incognito")>=0);show(row);
    assert(last_count==1 && !last_items[0].is_folder && !strcmp(last_items[0].id,id));
    assert(!strcmp(last_items[0].subtitle,"Cross plays without history or account reports") && !pending_count);
}
static void play(const char *id) {
    assert(!p->resolve(id,resolve_cb,NULL));
    char body[320];snprintf(body,sizeof body,"{\"state\":\"done\",\"result\":{\"id\":\"%s\",\"title\":\"Fixture\",\"duration_sec\":100,\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/capability/p0\",\"label\":\"360p\",\"container\":\"mp4\"}]}}",id);
    poll_job(body);assert(last_ok && !pending_count);p->stream_chosen(id,&selected);
    p->report_progress(id,0,100,EVO_PROVIDER_PLAY_START);
}
static void stop(const char *id,int position) {p->report_progress(id,position,100,EVO_PROVIDER_PLAY_STOP);}
/* Saved record field for one profile; -1 when the record is absent. */
static double field(const char *profile,const char *id,const char *key) {
    cJSON *store=read_store(),*entry;double value=-1;
    cJSON_ArrayForEach(entry,cJSON_GetObjectItem(store,"records"))
        if(!strcmp(cJSON_GetObjectItem(entry,"id")->valuestring,id) && !strcmp(cJSON_GetObjectItem(entry,"profile")->valuestring,profile)) {
            const cJSON *item=cJSON_GetObjectItem(entry,key);value=cJSON_IsBool(item)?cJSON_IsTrue(item):item->valuedouble;
        }
    cJSON_Delete(store);return value;
}
static int state_of(const char *profile) {
    cJSON *store=read_store(),*entry;int value=-1;
    cJSON_ArrayForEach(entry,cJSON_GetObjectItem(store,"profile_settings"))
        if(!strcmp(cJSON_GetObjectItem(entry,"profile")->valuestring,profile)) {
            value=cJSON_GetObjectItem(entry,"history_state")->valueint;
            assert(cJSON_IsTrue(cJSON_GetObjectItem(entry,"history_sync"))==(value!=2)); /* older builds keep reading the boolean */
        }
    cJSON_Delete(store);return value;
}
static const char *history_title(void) {
    static char title[192];
    for(int page=0;;++page) {
        p->list_catalog("settings",page,items_cb,NULL);assert(last_ok);
        for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].id,"settings:history")){snprintf(title,sizeof title,"%s",last_items[i].title);return title;}
        assert(last_more);
    }
}
int main(void) {
    char dir[]="/tmp/tube-history-modes-XXXXXX",offer[160];
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    snprintf(selected.url,sizeof selected.url,"http://127.0.0.1:8765/media/capability/p0");
    p=&evo_provider_youtube;assert(!p->init());
    SEPARATE_SETTINGS();
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    show("library:profile:fixture");
    p->search("fixture",0,items_cb,NULL);reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"First\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"duration_sec\":100},{\"id\":\"bcdefghijkl\",\"title\":\"Second\",\"duration_sec\":100}],\"has_more\":false}}");
    assert(last_count==2 && !strcmp(history_title(),"History: automatic"));
    /* Ordinary playback writes the local record and reports to the account. */
    play(A);p->report_progress(A,1,100,EVO_PROVIDER_PLAY_UPDATE);
    assert(pending_count==1 && strstr(pending[0].body,"watch_progress") && strstr(pending[0].body,"\"profile\":\"fixture\""));drain();
    stop(A,30);assert(pending_count==1 && strstr(pending[0].body,"\"state\":\"stopped\""));drain();
    assert(field("fixture",A,"position")==30 && field("fixture",A,"updated")>0);
    const double stamp=field("fixture",A,"updated");
    /* Incognito: the offer keeps the saved position, and the playback it starts
     * writes no position, watched marker, history time or account report. */
    arm(A);assert(last_items[0].resume_pos_sec==30);snprintf(offer,sizeof offer,"%s",row);
    p->list_catalog(offer,1,items_cb,NULL);assert(!last_ok);
    show(offer);play(A);
    p->report_progress(A,5,100,EVO_PROVIDER_PLAY_UPDATE);p->report_progress(A,50,100,EVO_PROVIDER_PLAY_UPDATE);
    tube_poll();assert(!pending_count);
    /* Recovery re-resolves the same playback and stays private. */
    evo_provider_item_t recovered;double recovered_position;int recovered_subtitles;
    assert(tube_queue_recovery(50,0,0) && tube_take_recovery(&recovered,&recovered_position,&recovered_subtitles) && tube_recovery_active());
    assert(!p->resolve(A,resolve_cb,NULL));
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"title\":\"Fixture\",\"duration_sec\":100,\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/capability/p0\",\"label\":\"360p\",\"container\":\"mp4\"}]}}");
    p->stream_chosen(A,&selected);tube_recovery_finish();
    p->report_progress(A,60,100,EVO_PROVIDER_PLAY_UPDATE);(void)tube_finish_drained_and_next(99);
    stop(A,99);tube_poll();assert(!pending_count);
    assert(field("fixture",A,"position")==30 && field("fixture",A,"played")==0 && field("fixture",A,"updated")==stamp);
    /* One shot: the next ordinary selection is recorded again. */
    play(A);stop(A,44);assert(pending_count==1);drain();assert(field("fixture",A,"position")==44);
    /* Any other selection, another video, or a profile or source change retires an unused offer. */
    arm(A);show("favorites");play(A);stop(A,12);drain();assert(field("fixture",A,"position")==12);
    arm(A);play(B);stop(B,9);drain();assert(field("fixture",B,"position")==9);
    play(A);stop(A,13);drain();assert(field("fixture",A,"position")==13);
    arm(A);snprintf(offer,sizeof offer,"%s",row);
    show("library:profile:guest");show("library:profile:fixture");
    p->list_catalog(offer,0,items_cb,NULL);assert(!last_ok);
    play(A);stop(A,14);drain();assert(field("fixture",A,"position")==14);
    arm(A);assert(!p->set_source("http://127.0.0.1:8765"));
    play(A);stop(A,15);drain();assert(field("fixture",A,"position")==15);
    /* History state: Disabled stops local history, resume and account reports for this profile only. */
    show("settings:history");assert(last_count==3 && !strcmp(last_items[0].title,"Automatic") && !strcmp(last_items[0].subtitle,"Selected"));
    show("settings:history:2");assert(!strcmp(last_items[2].title,"Disabled") && !strcmp(last_items[2].subtitle,"Selected"));
    assert(state_of("fixture")==2 && state_of("guest")==0 && !strcmp(history_title(),"History: disabled"));
    play(B);p->report_progress(B,5,100,EVO_PROVIDER_PLAY_UPDATE);(void)tube_finish_drained_and_next(99);stop(B,70);
    tube_poll();assert(!pending_count && field("fixture",B,"position")==9 && field("fixture",B,"played")==0);
    show("library:profile:guest");
    p->search("fixture",0,items_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"bcdefghijkl\",\"title\":\"Second\",\"duration_sec\":100}],\"has_more\":false}}");
    play(B);stop(B,33);drain();assert(field("guest",B,"position")==33 && field("fixture",B,"position")==9);
    show("library:profile:fixture");
    p->shutdown();assert(!p->init());assert(!strcmp(history_title(),"History: disabled"));
    cJSON *store=read_store();
    assert(cJSON_GetObjectItem(store,"history_state")->valueint==2 && cJSON_IsFalse(cJSON_GetObjectItem(store,"history_sync")));
    cJSON_Delete(store);
    play(B);stop(B,71);tube_poll();assert(!pending_count && field("fixture",B,"position")==9);
    show("settings:history:1");assert(state_of("fixture")==1 && !strcmp(history_title(),"History: enabled"));
    stop(B,72);assert(field("fixture",B,"position")==72);drain();
    const char *invalid[]={"settings:history:","settings:history:3","settings:history:1x","settings:history:-1","settings:history: 1","settings:history:99999999999999999999"};
    for(unsigned i=0;i<sizeof invalid/sizeof *invalid;++i){p->list_catalog(invalid[i],0,items_cb,NULL);assert(!last_ok);}
    assert(state_of("fixture")==1);
    /* Migration: the old boolean maps off to Disabled; corrupt states fall back to it or to Automatic. */
    p->shutdown();store=read_store();
    cJSON_ReplaceItemInObject(store,"profile_settings",cJSON_Parse("["
        "{\"profile\":\"guest\",\"history_sync\":false},{\"profile\":\"old\",\"history_sync\":true},"
        "{\"profile\":\"both\",\"history_state\":1,\"history_sync\":false},{\"profile\":\"corrupt\",\"history_state\":7,\"history_sync\":false},"
        "{\"profile\":\"text\",\"history_state\":\"2\"},{\"profile\":\"fraction\",\"history_state\":1.5},{\"profile\":\"fixture\"}]"));
    write_store(store);cJSON_Delete(store);assert(!p->init());show("settings:captions");
    assert(state_of("guest")==2 && state_of("old")==0 && state_of("both")==1 && state_of("corrupt")==2);
    assert(state_of("text")==0 && state_of("fraction")==0 && state_of("fixture")==0);
    /* A store older than per-profile settings carries its single boolean to every profile. */
    p->shutdown();store=read_store();cJSON_DeleteItemFromObject(store,"profile_settings");
    cJSON_DeleteItemFromObject(store,"history_state");cJSON_ReplaceItemInObject(store,"history_sync",cJSON_CreateFalse());
    write_store(store);cJSON_Delete(store);assert(!p->init());show("settings:captions");
    assert(state_of("guest")==2 && state_of("fixture")==2 && !strcmp(history_title(),"History: disabled"));
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: incognito playback, offer retirement, recovery privacy, history tri-state, profile isolation, legacy migration and corrupt states");
}
