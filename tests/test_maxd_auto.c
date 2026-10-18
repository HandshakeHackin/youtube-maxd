/* Adaptive reopen on shipping provider; manual choices and account scopes win. */
#define MAXD_STREAM_HELPERS_ONLY
#include "test_stream_choice.c"
static uint64_t time_ms,frames;
static int sample(int paused,int buffering) {
    time_ms+=1000;if(!buffering)frames+=30;
    return tube_maxd_auto_sample(time_ms,frames,0,paused,buffering,42.5,0,1);
}
int main(void) {
    char dir[]="/tmp/maxd-auto-XXXXXX";assert(mkdtemp(dir));assert(!setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t *p=&evo_provider_youtube;assert(!p->init());SEPARATE_SETTINGS();
    assert(!tube_maxd_auto_enabled());p->list_catalog("settings:auto-max",0,items_cb,NULL);assert(tube_maxd_auto_enabled());
    p->list_catalog("library:profile:alice",0,items_cb,NULL);assert(!tube_maxd_auto_enabled());
    p->list_catalog("library:profile:guest",0,items_cb,NULL);assert(tube_maxd_auto_enabled());
    p->shutdown();assert(!p->init() && tube_maxd_auto_enabled());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));p->search("fixture",0,items_cb,NULL);
    reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");poll_job("{\"state\":\"done\",\"result\":{\"items\":[]}}");
    resolve_video("en-US.4",NULL);p->stream_chosen("abcdefghijk",&listed[0]);
    p->list_catalog("settings:ask-stream",0,items_cb,NULL);assert(tube_choose_stream_enabled());
    for(int i=0;i<25;++i)assert(!sample(0,1));
    assert(!tube_recovery_pending());
    p->list_catalog("settings:ask-stream",0,items_cb,NULL);assert(!tube_choose_stream_enabled());
    /* Warmup, then three seconds of actual buffering starts one reopen. */
    for(int i=0;i<11;++i)assert(!sample(0,0));
    assert(!sample(0,1) && !sample(0,1));assert(sample(0,1)==1 && tube_recovery_pending());
    evo_provider_item_t item;double position;int subtitles;
    assert(tube_take_recovery(&item,&position,&subtitles) && position==42.5 && subtitles);
    resolve_video("en-US.4",NULL);int choice=tube_recovery_choice(listed,listed_count);assert(choice==2);
    p->stream_chosen("abcdefghijk",&listed[choice]);tube_recovery_finish();
    for(int i=0;i<15;++i)assert(!sample(0,1)); /* no immediate oscillation */
    /* A manually pinned quality stops adaptive switching. */
    assert(tube_player_quality_select(1,42.5,0,1)==2);
    for(int i=0;i<150;++i)assert(!sample(0,1));
    assert(!tube_recovery_pending());
    assert(!tube_maxd_auto_sample(time_ms+1000,frames,0,0,1,42.5,1,1)); /* live */
    p->list_catalog("library:profile:alice",0,items_cb,NULL);
    assert(!tube_maxd_auto_sample(time_ms+2000,frames,0,0,1,42.5,0,1));
    p->shutdown();puts("PASS: Auto Max persistence, scoped reopen/position/captions, cooldown, startup picker/manual pin, live and profile guards");
}
