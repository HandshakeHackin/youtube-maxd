/* Shipping provider: editable rules, fresh feed snapshots and local playback groups. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *p;
static evo_stream_choice_t smart_choice;
static void smart_resolved(int ok,const evo_stream_choice_t *choices,int count,void *ud) {
    (void)ud;assert(ok && count==1);smart_choice=choices[0];
}
static void choose(const char *id) { assert(!p->list_catalog(id,0,items_cb,NULL) && last_ok); }
static void typed(const char *action,const char *text,char *next) {
    choose(action);tube_playlist_prompt_t prompt;assert(tube_take_playlist_prompt(&prompt));
    assert(!tube_submit_playlist_name(prompt.ticket,text,next,160));choose(next);
}
static void drain(void) { while(pending_count)reply(0,0,500,"{}"); }
static void health(void) {
    while(pending_count && strstr(pending[0].url,"/v1/health"))
        reply(0,1,200,"{\"api_version\":1,\"features\":[\"search\",\"library\"]}");
}
int main(void) {
    char dir[]="/tmp/maxd-smart-XXXXXX",editor[160],row[256];assert(mkdtemp(dir));
    assert(!setenv("EVO_DATA_DIR_OVERRIDE",dir,1));p=&evo_provider_youtube;assert(!p->init());
    char next[160];typed("settings:smart:new","Gaming tonight",editor);assert(!strcmp(editor,"settings:smart:1"));
    snprintf(row,sizeof row,"%s:keyword",editor);typed(row,"gaming",next);
    snprintf(row,sizeof row,"%s:creator",editor);typed(row,"studio",next);
    snprintf(row,sizeof row,"%s:duration",editor);choose(row);choose(row); /* 10–30 minutes */
    snprintf(row,sizeof row,"%s:source",editor);choose(row);choose(row); /* local Favorites */
    /* Seed real persisted video records through the ordinary search path. */
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->search("fixture",0,items_cb,NULL);reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"items\":["
        "{\"id\":\"abcdefghij1\",\"title\":\"Gaming one\",\"subtitle\":\"Studio\",\"duration_sec\":900},"
        "{\"id\":\"abcdefghij2\",\"title\":\"Gaming two\",\"subtitle\":\"Studio\",\"duration_sec\":1800},"
        "{\"id\":\"abcdefghij3\",\"title\":\"Gaming short\",\"subtitle\":\"Studio\",\"duration_sec\":599},"
        "{\"id\":\"abcdefghij4\",\"title\":\"Cooking\",\"subtitle\":\"Studio\",\"duration_sec\":900}]}}");
    /* Pairing changes no rules; the selected guest still owns them. */
    for(int i=1;i<=4;++i){char id[]="abcdefghij1";id[10]=(char)('0'+i);assert(tube_toggle_favorite(id));}
    choose("maxd:smart:run:1");assert(last_count==2 && !last_more);
    evo_provider_item_t first=last_items[0];assert(!strncmp(first.playback_playlist,"group:6d617864-",15));
    assert(first.playback_scope && first.playback_index==0);
    assert(tube_select_playlist_context(first.id,first.playback_playlist,first.playback_index,first.playback_scope));
    assert(!p->resolve(first.id,smart_resolved,NULL));
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghij1\",\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/s/d0.mpd\",\"label\":\"720p\",\"container\":\"dash\",\"height\":720,\"width\":1280,\"fps\":30,\"video_codec\":\"h264\",\"audio_codec\":\"aac\",\"video_range\":\"SDR\"}]}}");
    p->stream_chosen(first.id,&smart_choice);
    tube_playlist_window_t window;
    assert(tube_playlist_window_begin() && tube_playlist_window_status(&window)==1);
    assert(window.count==2 && window.playlist_size==2 && window.current==0 && !pending_count);
    assert(!strcmp(window.ids[0],first.id) && !strcmp(window.ids[1],"abcdefghij2"));
    /* Refresh uses current local watched state and does not mutate favorites. */
    p->report_progress("abcdefghij1",900,900,EVO_PROVIDER_PLAY_STOP);
    choose("maxd:smart:run:1");assert(last_count==1 && !strcmp(last_items[0].id,"abcdefghij2"));
    assert(strcmp(first.playback_playlist,last_items[0].playback_playlist));
    assert(tube_playlist_window_begin() && tube_playlist_window_status(&window)==1 && window.count==2); /* playing snapshot survives refresh */
    snprintf(row,sizeof row,"%s:keyword",editor);typed(row,"",next);
    assert(!*tube_row_fact_get(row,"text") && !*tube_row_fact_get(row,"value"));
    p->shutdown();drain();assert(!p->init());choose(editor);assert(last_count==9);
    /* Editing is isolated even when the app shares ordinary settings. */
    choose("library:profile:alice");choose("settings:smart");assert(last_count==1);
    p->list_catalog(editor,0,items_cb,NULL);assert(!last_ok);
    typed("settings:smart:new","Alice playlist",next);assert(strcmp(editor,next));
    choose("library:profile:guest");choose("settings:smart");assert(last_count==2);
    /* A network feed refresh collects later pages, drops duplicates, and never
       returns an earlier callback after another screen/profile takes over. */
    snprintf(row,sizeof row,"%s:source",editor);choose(row);choose(row); /* Favorites -> Queue -> Home */
    snprintf(row,sizeof row,"%s:duration",editor);choose(row);choose(row);choose(row); /* any */
    snprintf(row,sizeof row,"%s:creator",editor);typed(row,"",next);
    int before=item_calls;p->list_catalog("maxd:smart:run:1",0,items_cb,NULL);health();
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"abcdefghij5\",\"title\":\"New\",\"duration_sec\":100}],\"has_more\":true,\"cursor\":\"smartPage2\"}}");
    assert(item_calls==before && pending_count && strstr(pending[0].body,"smartPage2"));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"abcdefghij5\",\"title\":\"Duplicate\",\"duration_sec\":100},{\"id\":\"abcdefghij6\",\"title\":\"New two\",\"duration_sec\":100}],\"has_more\":false}}");
    assert(item_calls==before+1 && last_ok && last_count==2);
    p->list_catalog("maxd:smart:run:1",0,items_cb,NULL);health();assert(pending_count);
    before=item_calls;choose("library:profile:alice");drain();assert(item_calls==before+2); /* cancellation + profile rows */
    choose("library:profile:guest");snprintf(row,sizeof row,"%s:delete",editor);choose(row);assert(last_count==1);
    snprintf(row,sizeof row,"%s:delete-confirm",editor);choose(row);assert(last_count==1);
    /* Forgetting a saved account also forgets its private local rules. */
    choose("library:profile:alice");choose("settings:smart");assert(last_count==2);
    choose("account:remove");assert(last_count==1);
    char apply[160];snprintf(apply,sizeof apply,"%s",last_items[0].id);
    p->list_catalog(apply,0,items_cb,NULL);assert(pending_count);
    poll_job("{\"state\":\"done\",\"result\":{\"success\":true,\"profile\":\"alice\"}}");
    assert(last_ok && !strcmp(tube_browse_profile(),"guest"));
    choose("library:profile:alice");choose("settings:smart");assert(last_count==1);
    p->shutdown();drain();assert(!p->init());choose("settings:smart");assert(last_count==1);
    p->shutdown();drain();puts("PASS: smart playlist editing, reload, profile isolation/removal, filtered playback windows, watched filters, fresh pages, dedupe and cancelled callbacks");
}
