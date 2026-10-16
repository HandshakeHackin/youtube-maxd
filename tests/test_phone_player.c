/* Exercise the shipping adapter against real driver JSON/ack handling. */
#define main remote_driver_test_main
#include "test_remote_native.c"
#undef main
#include "tube_phone.h"
#include "tube_volume.h"
static tube_remote_player engine;
static int active, browsed, stopped, keys, reject_seek;
static int deferred_caption, caption_result, caption_polls;
static int voice_starts, voice_cancels;
static int app_closes, activities;
static char deferred_language[33];
static double sought;
static unsigned selection_serial;
static int snapshot(void *ctx, tube_remote_player *p) { (void)ctx; *p = engine; return active; }
static int pause_engine(void *ctx, int paused) { (void)ctx; engine.state = paused ? 2 : 1; return active; }
static int seek_engine(void *ctx, double position) { (void)ctx; if (reject_seek) return 0; sought = engine.position = position; return active; }
static int stop_engine(void *ctx) { (void)ctx; active = 0; ++stopped; return 1; }
static int volume_engine(void *ctx, int value) { (void)ctx; engine.volume = value; engine.muted = !value; return 1; }
static int key_engine(void *ctx, const char *key) { (void)ctx; assert(!strcmp(key, "ENTER")); ++keys; return 1; }
static int caption_engine(void *ctx, const char *lang, unsigned owner, int sequence) {
    (void)ctx; assert(tube_remote_command_current(owner,sequence));
    if (deferred_caption) { snprintf(deferred_language,sizeof deferred_language,"%s",lang); return 2; }
    snprintf(engine.language, sizeof engine.language, "%s", lang); return 1;
}
static int caption_status(void *ctx) {
    (void)ctx; ++caption_polls;
    if (caption_result > 0) strcpy(engine.language,deferred_language);
    return caption_result;
}
static void browser(void *ctx) { (void)ctx; ++browsed; }
static int voice_engine(void *ctx, int started, unsigned owner) {
    (void)ctx; assert(tube_remote_owner_current(owner));
    if (started) ++voice_starts; else ++voice_cancels;
    return 1;
}
static int close_engine(void *ctx) { (void)ctx; ++app_closes; return 1; }
static void activity(void *ctx) { (void)ctx; ++activities; }
static tube_phone_controls controls = {NULL,snapshot,pause_engine,seek_engine,stop_engine,volume_engine,key_engine,caption_engine,browser,0,caption_status,voice_engine,0,close_engine,activity,NULL,NULL};
static int64_t test_clock = 100000;
static void cleanup(void) {
    tube_remote_shutdown(); tube_phone_clear();
    while (count) {
        cJSON *r = cJSON_Parse(pending[0].body); assert(r);
        int stop = !strcmp(cJSON_GetObjectItem(r,"action")->valuestring,"stop"); cJSON_Delete(r);
        reply(0,200,stop ? "{\"api_version\":1,\"state\":\"stopped\"}" : accepted);
    }
}
static void feed(const char *commands) {
    cleanup(); memset(&engine,0,sizeof engine); engine.volume = 100; engine.duration = 300; engine.state = 1;
    active = browsed = stopped = keys = reject_seek = 0;
    deferred_caption = caption_result = caption_polls = 0;
    voice_starts = voice_cancels = 0;
    app_closes = 0;
    activities = 0;
    test_clock += 10000; configure(1,"guest",identity,"screen-123"); remote_bind(test_clock);
    tube_remote_tick(test_clock + 1); poll_reply(0,commands); assert(tube_remote_state() == TUBE_REMOTE_LISTENING);
}
static unsigned take_start(const char *id, double position) {
    assert(tube_phone_launch_pending());
    char video[12]; double offset = -1; unsigned value = tube_phone_take(video,&offset);
    assert(!tube_phone_launch_pending());
    assert(value && !strcmp(video,id) && offset == position);
    assert(tube_phone_launch_current(value)); assert(!tube_phone_take(video,&offset)); return value;
}
static void started_video(unsigned value, const char *id) {
    snprintf(engine.video,sizeof engine.video,"%s",id); active = 1; engine.position = 0;
    engine.selection = ++selection_serial;
    assert(tube_phone_launch_bind(value, engine.selection));
    assert(tube_phone_launch_finish(value,1)); assert(!tube_phone_launch_finish(value,1));
}
static void feedback(int index, int previous, int next) {
    tube_remote_tick(test_clock+3000); cJSON *r=request("update"), *p=cJSON_GetObjectItem(r,"player");
    assert(cJSON_GetObjectItem(p,"index")->valueint == index);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(p,"has_previous")) == previous);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(p,"has_next")) == next);
    assert(!strcmp(cJSON_GetObjectItem(p,"video_id")->valuestring,engine.video));
    assert(cJSON_GetObjectItem(p,"position_sec")->valuedouble == engine.position);
    cJSON_Delete(r); reply(0,200,accepted); test_clock += 3000;
}
static void test_execution(void) {
    feed("[{\"sequence\":1,\"action\":\"loungeStatus\",\"devices\":[]},{\"sequence\":2,\"action\":\"getNowPlaying\"},{\"sequence\":3,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"zyxwvutsrqp\"],\"playlist\":\"PLtest\",\"index\":7,\"position_sec\":13.25,\"ctt\":\"opaque\"},{\"sequence\":4,\"action\":\"pause\"},{\"sequence\":5,\"action\":\"seekTo\",\"position_sec\":27.125},{\"sequence\":6,\"action\":\"setVolume\",\"volume\":40},{\"sequence\":7,\"action\":\"play\"},{\"sequence\":8,\"action\":\"next\"},{\"sequence\":9,\"action\":\"previous\"},{\"sequence\":10,\"action\":\"dpadCommand\",\"key\":\"ENTER\"},{\"sequence\":11,\"action\":\"setSubtitlesTrack\",\"video_id\":\"abcdefghijk\",\"language\":\"en\"},{\"sequence\":12,\"action\":\"stopVideo\"}]");
    tube_phone_tick(&controls); tube_phone_tick(&controls); tube_phone_tick(&controls);
    assert(activities==1); /* Status/query traffic does not postpone sleep. */
    assert(browsed == 1); unsigned value = take_start("abcdefghijk",13.25);
    tube_remote_command c; assert(tube_remote_take(&c) && c.sequence == 3);
    tube_phone_tick(&controls); assert(tube_remote_take(&c) && c.sequence == 3 && engine.state == 1);
    started_video(value,"abcdefghijk"); tube_phone_tick(&controls); assert(engine.state == 2);
    tube_phone_tick(&controls); assert(sought == 27.125 && engine.state == 2);
    tube_phone_tick(&controls); assert(engine.volume == 40);
    tube_phone_tick(&controls); assert(engine.state == 1);
    feedback(7,0,1);
    tube_phone_tick(&controls); started_video(take_start("zyxwvutsrqp",0),"zyxwvutsrqp");
    tube_phone_tick(&controls); started_video(take_start("abcdefghijk",0),"abcdefghijk");
    tube_phone_tick(&controls); assert(keys == 1);
    tube_phone_tick(&controls); assert(!strcmp(engine.language,"en"));
    tube_phone_tick(&controls); assert(stopped == 1 && !active && !tube_remote_take(&c));
}
static void test_queue_and_eof(void) {
    feed("[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"zyxwvutsrqp\"],\"playlist\":\"PLtest\",\"index\":5,\"position_sec\":0,\"ctt\":\"opaque\"},{\"sequence\":2,\"action\":\"updatePlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"12345678901\",\"abcdefghijk\",\"zyxwvutsrqp\"],\"playlist\":\"PLtest\",\"index\":5,\"position_sec\":0,\"ctt\":\"opaque\"}]");
    tube_phone_tick(&controls); started_video(take_start("abcdefghijk",0),"abcdefghijk");
    tube_phone_tick(&controls); tube_phone_tick(&controls); assert(browsed == 1);
    feedback(5,1,1); assert(tube_phone_finished()); assert(!tube_phone_finished());
    started_video(take_start("zyxwvutsrqp",0),"zyxwvutsrqp"); tube_phone_tick(&controls);
    feedback(6,1,0); assert(tube_phone_finished()); assert(!tube_phone_finished());
    strcpy(engine.video,"12345678901"); tube_phone_tick(&controls); assert(!tube_phone_finished());
}
static void test_retired_launch_and_failure(void) {
    const char *commands="[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[],\"playlist\":\"\",\"index\":0,\"position_sec\":0.375,\"ctt\":\"\"}]";
    feed(commands); tube_phone_tick(&controls); unsigned stale=take_start("abcdefghijk",0.375);
    configure(1,"other-profile",identity,"screen-123"); assert(!tube_phone_launch_current(stale));
    assert(!tube_phone_launch_finish(stale,1));
    feed(commands); tube_phone_tick(&controls); unsigned fresh=take_start("abcdefghijk",0.375);
    assert(fresh != stale && !tube_phone_launch_finish(stale,1));
    assert(!tube_phone_launch_finish(fresh,0) && tube_remote_state() == TUBE_REMOTE_FAILED);
    feed("[{\"sequence\":1,\"action\":\"seekTo\",\"position_sec\":10.5}]");
    active = 1; strcpy(engine.video,"abcdefghijk"); reject_seek = 1;
    tube_phone_tick(&controls); assert(tube_remote_state() == TUBE_REMOTE_FAILED);
}
static void test_duplicate_playlist_position(void) {
    feed("[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"zyxwvutsrqp\",\"abcdefghijk\"],\"playlist\":\"PLtest\",\"index\":2,\"position_sec\":0,\"ctt\":\"\"}]");
    tube_phone_tick(&controls); started_video(take_start("abcdefghijk",0),"abcdefghijk");
    tube_phone_tick(&controls); feedback(2,1,0); assert(tube_phone_finished());
    char video[12]; double position; assert(!tube_phone_take(video,&position));
}
static void test_gain(void) {
    int16_t input[]={-32768,-100,0,100,32767}, output[5];
    tube_volume_pcm(output,input,5,100); assert(!memcmp(input,output,sizeof input));
    tube_volume_pcm(output,input,5,50); assert(output[0] == -16384 && output[1] == -50 && output[4] == 16383);
    tube_volume_pcm(output,input,5,0); for(int i=0;i<5;++i) assert(output[i] == 0);
    tube_volume_pcm(output,input,5,120); assert(!memcmp(input,output,sizeof input));
    tube_volume_pcm(output,input,5,-1); assert(output[0] == 0);
}
static void test_disconnect_policy(void) {
    const char *commands = "[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"zyxwvutsrqp\"],\"playlist\":\"PLtest\",\"index\":0,\"position_sec\":0,\"ctt\":\"\"},{\"sequence\":2,\"action\":\"remoteDisconnected\",\"device_id\":\"phone-one\",\"name\":\"Phone\"}]";
    feed(commands); controls.stop_on_disconnect = 0;
    tube_phone_tick(&controls); started_video(take_start("abcdefghijk",0),"abcdefghijk");
    tube_phone_tick(&controls); assert(active && !stopped); feedback(0,0,1);
    feed(commands); controls.stop_on_disconnect = 1;
    tube_phone_tick(&controls); started_video(take_start("abcdefghijk",0),"abcdefghijk");
    tube_phone_tick(&controls); assert(!active && stopped == 1 && tube_remote_state() == TUBE_REMOTE_LISTENING);
    assert(!tube_phone_finished()); /* Stopped queue cannot auto-advance. */
    /* Changing media manually makes the disconnection irrelevant to playback. */
    feed(commands); tube_phone_tick(&controls); started_video(take_start("abcdefghijk",0),"abcdefghijk");
    strcpy(engine.video,"12345678901"); tube_phone_tick(&controls); assert(active && !stopped);
    feed(commands); tube_phone_tick(&controls); started_video(take_start("abcdefghijk",0),"abcdefghijk");
    ++engine.selection; tube_phone_tick(&controls); assert(active && !stopped);
    /* No remote launch: don't stop local media, even with the preference enabled. */
    feed("[{\"sequence\":1,\"action\":\"remoteDisconnected\",\"device_id\":\"phone-one\",\"name\":\"Phone\"}]");
    active = 1; strcpy(engine.video,"abcdefghijk"); tube_phone_tick(&controls); assert(active && !stopped);
    controls.stop_on_disconnect = 0;
}
static void test_disconnect_close_policy(void) {
    const char *commands = "[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"zyxwvutsrqp\"],\"playlist\":\"PLtest\",\"index\":0,\"position_sec\":0,\"ctt\":\"\"},{\"sequence\":2,\"action\":\"remoteDisconnected\",\"device_id\":\"phone-one\",\"name\":\"Phone\"}]";
    feed(commands); controls.close_on_disconnect = 1;
    tube_phone_tick(&controls); started_video(take_start("abcdefghijk",0),"abcdefghijk");
    tube_phone_tick(&controls); assert(app_closes==1 && !stopped && active);
    assert(!tube_phone_finished()); tube_phone_tick(&controls); assert(app_closes==1);
    feed(commands); tube_phone_tick(&controls); started_video(take_start("abcdefghijk",0),"abcdefghijk");
    ++engine.selection; tube_phone_tick(&controls); assert(app_closes==1 && active);
    feed("[{\"sequence\":1,\"action\":\"remoteDisconnected\",\"device_id\":\"phone-one\",\"name\":\"Phone\"}]");
    tube_phone_tick(&controls); assert(app_closes==1); /* Explicit whole-app choice also applies while browsing. */
    feed(commands); tube_phone_tick(&controls); started_video(take_start("abcdefghijk",0),"abcdefghijk");
    configure(1,"another",identity,"screen-123");
    tube_phone_tick(&controls); assert(!app_closes); /* Retired receiver cannot close the app. */
    controls.close_on_disconnect = 0;
}
static void test_deferred_caption(void) {
    const char *commands = "[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"zyxwvutsrqp\"],\"playlist\":\"PLtest\",\"index\":0,\"position_sec\":0,\"ctt\":\"\"},{\"sequence\":2,\"action\":\"setSubtitlesTrack\",\"video_id\":\"abcdefghijk\",\"language\":\"fr\"},{\"sequence\":3,\"action\":\"pause\"}]";
    feed(commands); tube_phone_tick(&controls); started_video(take_start("abcdefghijk",0),"abcdefghijk");
    deferred_caption = 1; strcpy(engine.language,"en"); engine.position = 42.125; engine.state = 2;
    tube_phone_tick(&controls); tube_remote_command c;
    assert(tube_remote_take(&c) && c.sequence == 2 && !caption_polls);
    tube_phone_tick(&controls); assert(caption_polls == 1 && !strcmp(engine.language,"en"));
    assert(engine.state == 2 && engine.position == 42.125 && tube_phone_owns_media());
    assert(!tube_phone_finished()); /* The Player's ownership guard suppresses local EOF while deferred. */
    caption_result = 1; tube_phone_tick(&controls);
    assert(!strcmp(engine.language,"fr") && engine.state == 2 && engine.position == 42.125);
    assert(tube_remote_take(&c) && c.sequence == 3); tube_phone_tick(&controls);
    feed(commands); tube_phone_tick(&controls); started_video(take_start("abcdefghijk",0),"abcdefghijk");
    deferred_caption = 1; strcpy(engine.language,"en"); tube_phone_tick(&controls);
    caption_result = -1; tube_phone_tick(&controls);
    assert(tube_remote_state() == TUBE_REMOTE_FAILED && !strcmp(engine.language,"en"));
    feed(commands); tube_phone_tick(&controls); started_video(take_start("abcdefghijk",0),"abcdefghijk");
    deferred_caption = 1; tube_phone_tick(&controls);
    configure(1,"retired-profile",identity,"screen-123"); tube_phone_tick(&controls); assert(!caption_polls);
    /* Reopening the same ID must not apply the old phone's pending language. */
    feed(commands); tube_phone_tick(&controls); started_video(take_start("abcdefghijk",0),"abcdefghijk");
    deferred_caption = 1; strcpy(engine.language,"en"); tube_phone_tick(&controls);
    engine.selection = ++selection_serial; caption_result = 1; tube_phone_tick(&controls);
    assert(!caption_polls && !strcmp(engine.language,"en") && tube_remote_state() == TUBE_REMOTE_FAILED);
}
static void test_retired_receiver_eof(void) {
    const char *commands = "[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\"],\"playlist\":\"PLtest\",\"index\":0,\"position_sec\":0,\"ctt\":\"\"}]";
    feed(commands); tube_phone_tick(&controls); started_video(take_start("abcdefghijk",0),"abcdefghijk");
    tube_phone_tick(&controls); assert(tube_phone_owns_media());
    configure(0,"guest",identity,"screen-123"); tube_phone_tick(&controls);
    assert(tube_phone_owns_media() && tube_phone_finished()); assert(!tube_phone_finished());
    /* A manual reopening of the same video is a new selection, not remote-owned. */
    feed(commands); tube_phone_tick(&controls); started_video(take_start("abcdefghijk",0),"abcdefghijk");
    tube_phone_tick(&controls); engine.selection = ++selection_serial; tube_phone_tick(&controls);
    assert(!tube_phone_owns_media() && !tube_phone_finished());
}
static void test_voice_controls(void) {
    feed("[{\"sequence\":1,\"action\":\"voiceCommand\",\"status\":\"INITIATED\"},{\"sequence\":2,\"action\":\"voiceCommand\",\"status\":\"CANCELED\"}]");
    active = 0; /* voice search does not require an already selected video */
    tube_phone_tick(&controls); tube_phone_tick(&controls);
    assert(voice_starts == 1 && voice_cancels == 1 && tube_remote_state() == TUBE_REMOTE_LISTENING);
    tube_remote_command command; assert(!tube_remote_take(&command));
}
static void test_windowed_duplicate_and_refresh(void) {
    feed("[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"zyxwvutsrqp\",\"abcdefghijk\"],\"playlist\":\"PLlarge\",\"index\":102,\"window_start\":100,\"position_sec\":0,\"ctt\":\"click\"},{\"sequence\":2,\"action\":\"updatePlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"zyxwvutsrqp\",\"abcdefghijk\",\"12345678901\"],\"playlist\":\"PLlarge\",\"index\":102,\"window_start\":101,\"position_sec\":0,\"ctt\":\"click\"},{\"sequence\":3,\"action\":\"next\"}]");
    tube_phone_tick(&controls); started_video(take_start("abcdefghijk",0),"abcdefghijk");
    tube_phone_tick(&controls); assert(browsed == 1); /* refill does not reopen playback */
    assert(activities==1); /* Automatic refill does not reset inactivity. */
    feedback(102,1,0); /* selected duplicate was the third occurrence in its old window */
    tube_phone_tick(&controls); unsigned next = take_start("12345678901",0);
    started_video(next,"12345678901"); tube_phone_tick(&controls); feedback(103,1,0);
}
static void test_sparse_occurrences(void) {
    feed("[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"zyxwvutsrqp\",\"abcdefghijk\"],\"video_indices\":[100,105,110],\"playlist\":\"PLlarge\",\"index\":110,\"window_start\":100,\"position_sec\":0,\"ctt\":\"click\"},{\"sequence\":2,\"action\":\"previous\"},{\"sequence\":3,\"action\":\"next\"}]");
    tube_phone_tick(&controls); started_video(take_start("abcdefghijk",0),"abcdefghijk");
    tube_phone_tick(&controls); unsigned previous = take_start("zyxwvutsrqp",0);
    started_video(previous,"zyxwvutsrqp");
    /* Reports the service occurrence, not the compact local array offset. */
    tube_phone_tick(&controls); unsigned next = take_start("abcdefghijk",0);
    feedback(105,1,1);
    started_video(next,"abcdefghijk"); tube_phone_tick(&controls); feedback(110,1,0);
    assert(tube_phone_finished()); char video[12]; double position;
    assert(!tube_phone_take(video,&position));
    feed("[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\"],\"playlist\":\"PLlarge\",\"index\":100,\"window_start\":100,\"position_sec\":0,\"ctt\":\"click\"},{\"sequence\":2,\"action\":\"updatePlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"zyxwvutsrqp\",\"abcdefghijk\"],\"video_indices\":[100,105,110],\"playlist\":\"PLlarge\",\"index\":100,\"window_start\":100,\"position_sec\":0,\"ctt\":\"click\"}]");
    tube_phone_tick(&controls); started_video(take_start("abcdefghijk",0),"abcdefghijk");
    tube_phone_tick(&controls); assert(browsed==1); /* Sparse refill preserves playback. */
    tube_phone_tick(&controls); feedback(100,0,1);
    assert(tube_phone_finished()); started_video(take_start("zyxwvutsrqp",0),"zyxwvutsrqp");
    tube_phone_tick(&controls); feedback(105,1,1);
}

static void test_unavailable_retry(void) {
    feed("[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"abcdefghijk\",\"zyxwvutsrqp\"],\"video_indices\":[100,105,110],\"playlist\":\"PLfailed\",\"index\":100,\"window_start\":100,\"position_sec\":9.125,\"ctt\":\"click\"}]");
    tube_phone_tick_at(&controls,test_clock); unsigned first=take_start("abcdefghijk",9.125);
    assert(!tube_phone_launch_unavailable(first,"12345678901",test_clock));
    assert(!tube_phone_launch_unavailable(first,"abcdefghijk",-1));
    assert(!tube_phone_launch_unavailable(first,"abcdefghijk",INT64_MAX));
    assert(tube_phone_launch_unavailable(first,"abcdefghijk",test_clock)==1);
    assert(!tube_phone_launch_current(first) && !tube_phone_launch_bind(first,9));
    assert(!tube_phone_launch_finish(first,1) && tube_phone_retry_waiting());
    assert(!tube_phone_launch_pending());
    tube_remote_command c; assert(tube_remote_take(&c) && c.sequence==1); /* No early startup ack. */
    tube_phone_tick_at(&controls,test_clock-100); assert(tube_phone_retry_waiting());
    tube_phone_tick_at(&controls,test_clock+4999); assert(tube_phone_retry_waiting());
    char video[12];double position;assert(!tube_phone_take(video,&position));
    test_clock+=5000;tube_phone_tick_at(&controls,test_clock);
    unsigned second=take_start("abcdefghijk",0);assert(second!=first);
    assert(tube_phone_launch_unavailable(second,"abcdefghijk",test_clock)==1);
    test_clock+=5000;tube_phone_tick_at(&controls,test_clock);
    unsigned third=take_start("zyxwvutsrqp",0);assert(third!=second);
    assert(!tube_phone_launch_finish(third,1)); /* A retry still needs its own bound selection. */
    assert(tube_remote_state()==TUBE_REMOTE_FAILED);
    /* Repeat the chain and prove that a successful native bind retains occurrence 110. */
    feed("[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"zyxwvutsrqp\"],\"video_indices\":[100,110],\"playlist\":\"PLfailed\",\"index\":100,\"window_start\":100,\"position_sec\":9.125,\"ctt\":\"click\"}]");
    tube_phone_tick_at(&controls,test_clock);first=take_start("abcdefghijk",9.125);
    assert(tube_phone_launch_unavailable(first,"abcdefghijk",test_clock)==1);
    test_clock+=5000;tube_phone_tick_at(&controls,test_clock);
    second=take_start("zyxwvutsrqp",0);started_video(second,"zyxwvutsrqp");
    tube_phone_tick(&controls);feedback(110,1,0);
}
static void test_unavailable_terminal_and_cancel(void) {
    const char *commands="[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"zyxwvutsrqp\"],\"playlist\":\"PLfailed\",\"index\":0,\"position_sec\":0,\"ctt\":\"\"},{\"sequence\":2,\"action\":\"getNowPlaying\"}]";
    feed(commands);tube_phone_tick_at(&controls,test_clock);unsigned first=take_start("abcdefghijk",0);
    assert(tube_phone_launch_unavailable(first,"abcdefghijk",test_clock)==1);
    test_clock+=5000;tube_phone_tick_at(&controls,test_clock);unsigned second=take_start("zyxwvutsrqp",0);
    assert(tube_phone_launch_unavailable(second,"zyxwvutsrqp",test_clock)==3);
    assert(tube_phone_retry_waiting()); test_clock+=30000; tube_phone_tick_at(&controls,test_clock);
    assert(tube_remote_state()==TUBE_REMOTE_LISTENING && !tube_phone_launch_pending() && !tube_phone_retry_waiting());
    assert(!tube_phone_launch_finish(second,1));tube_phone_tick(&controls);tube_remote_command c;assert(!tube_remote_take(&c));
    assert(!tube_phone_finished() && !tube_phone_owns_media());
    feed(commands);tube_phone_tick_at(&controls,test_clock);first=take_start("abcdefghijk",0);
    assert(tube_phone_launch_unavailable(first,"abcdefghijk",test_clock)==1);
    tube_phone_cancel();assert(tube_remote_state()==TUBE_REMOTE_LISTENING);
    test_clock+=6000;tube_phone_tick_at(&controls,test_clock);assert(!tube_phone_launch_pending() && !tube_remote_take(&c));
    feed(commands);tube_phone_tick_at(&controls,test_clock);first=take_start("abcdefghijk",0);
    assert(tube_phone_launch_unavailable(first,"abcdefghijk",test_clock)==1);
    configure(1,"other-profile",identity,"screen-123");
    test_clock+=6000;tube_phone_tick_at(&controls,test_clock);
    assert(!tube_phone_launch_pending() && !tube_phone_retry_waiting() && !tube_phone_launch_finish(first,1));
}
static void test_unavailable_interrupts(void) {
    feed("[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"zyxwvutsrqp\"],\"playlist\":\"PLfailed\",\"index\":0,\"position_sec\":0,\"ctt\":\"\"},{\"sequence\":2,\"action\":\"setVolume\",\"volume\":17},{\"sequence\":3,\"action\":\"stopVideo\"}]");
    tube_phone_tick_at(&controls,test_clock);unsigned first=take_start("abcdefghijk",0);
    assert(tube_phone_launch_unavailable(first,"abcdefghijk",test_clock)==1);
    tube_phone_tick_at(&controls,test_clock);assert(engine.volume==17 && !tube_phone_retry_waiting());
    tube_phone_tick_at(&controls,test_clock);assert(tube_remote_state()==TUBE_REMOTE_LISTENING);
    assert(!tube_phone_launch_pending());tube_remote_command c;assert(!tube_remote_take(&c));
    assert(!tube_phone_launch_finish(first,1));
    feed("[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"zyxwvutsrqp\"],\"playlist\":\"PLfailed\",\"index\":0,\"position_sec\":0,\"ctt\":\"\"},{\"sequence\":2,\"action\":\"setPlaylist\",\"video_id\":\"12345678901\",\"video_ids\":[\"12345678901\"],\"playlist\":\"PLnew\",\"index\":900,\"window_start\":900,\"position_sec\":0.375,\"ctt\":\"newclick\"}]");
    tube_phone_tick_at(&controls,test_clock);first=take_start("abcdefghijk",0);
    assert(tube_phone_launch_unavailable(first,"abcdefghijk",test_clock)==1);
    tube_phone_tick_at(&controls,test_clock);unsigned replacement=take_start("12345678901",.375);
    assert(replacement!=first && !tube_phone_launch_bind(first,99));
    assert(tube_remote_take(&c) && c.sequence==2);
    started_video(replacement,"12345678901");tube_phone_tick(&controls);feedback(900,0,0);
    assert(!tube_remote_launch_interrupted(c.owner-1,0));
}


static void test_unavailable_eof_and_disconnect(void) {
    feed("[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"zyxwvutsrqp\",\"12345678901\"],\"video_indices\":[10,20,50],\"playlist\":\"PLeof\",\"index\":10,\"window_start\":10,\"position_sec\":0,\"ctt\":\"\"}]");
    tube_phone_tick_at(&controls,test_clock);started_video(take_start("abcdefghijk",0),"abcdefghijk");
    tube_phone_tick_at(&controls,test_clock);assert(tube_phone_finished());
    unsigned failed=take_start("zyxwvutsrqp",0);active=0;
    assert(tube_phone_launch_unavailable(failed,"zyxwvutsrqp",test_clock)==1);
    test_clock+=5000;tube_phone_tick_at(&controls,test_clock);
    unsigned next=take_start("12345678901",0);assert(next!=failed);
    started_video(next,"12345678901");tube_phone_tick(&controls);feedback(50,1,0);
    feed("[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[],\"playlist\":\"\",\"index\":0,\"position_sec\":0,\"ctt\":\"\"}]");
    tube_phone_tick_at(&controls,test_clock);failed=take_start("abcdefghijk",0);
    assert(tube_phone_launch_unavailable(failed,"abcdefghijk",test_clock)==2);
    assert(tube_remote_state()==TUBE_REMOTE_LISTENING && !tube_phone_retry_waiting());
    feed("[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"zyxwvutsrqp\"],\"playlist\":\"PLfailed\",\"index\":0,\"position_sec\":0,\"ctt\":\"\"},{\"sequence\":2,\"action\":\"remoteDisconnected\",\"device_id\":\"phone-one\",\"name\":\"Phone\"}]");
    controls.close_on_disconnect=1;tube_phone_tick_at(&controls,test_clock);failed=take_start("abcdefghijk",0);
    assert(tube_phone_launch_unavailable(failed,"abcdefghijk",test_clock)==1);
    tube_phone_tick_at(&controls,test_clock);assert(app_closes==1 && !tube_phone_retry_waiting());
    assert(!tube_phone_launch_pending() && !tube_phone_launch_finish(failed,1));controls.close_on_disconnect=0;
}
static void test_unavailable_late_stop(void) {
    const char *initial="[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"zyxwvutsrqp\"],\"playlist\":\"PLfailed\",\"index\":0,\"position_sec\":0,\"ctt\":\"\"}]";
    feed(initial);tube_phone_tick_at(&controls,test_clock);unsigned first=take_start("abcdefghijk",0);
    assert(tube_phone_launch_unavailable(first,"abcdefghijk",test_clock)==1);
    test_clock+=5000;tube_phone_tick_at(&controls,test_clock);unsigned retry=take_start("zyxwvutsrqp",0);
    char combined[2000];snprintf(combined,sizeof combined,"%.*s,{\"sequence\":2,\"action\":\"stopVideo\"}]",(int)strlen(initial)-1,initial);
    int delivered=0;
    for(int i=0;i<8 && !delivered;++i) {
        tube_remote_tick(test_clock+100+i*500);
        if(!count)continue;
        cJSON *r=cJSON_Parse(pending[0].body);assert(r);const char *action=cJSON_GetObjectItem(r,"action")->valuestring;
        int poll=!strcmp(action,"poll");assert(poll || !strcmp(action,"update"));cJSON_Delete(r);
        if(poll){poll_reply(0,combined);delivered=1;}else reply(0,200,accepted);
    }
    assert(delivered && tube_remote_state()==TUBE_REMOTE_LISTENING);
    assert(!tube_phone_launch_current(retry) && !tube_phone_launch_bind(retry,999) && !tube_phone_launch_finish(retry,1));
    tube_phone_tick_at(&controls,test_clock+5000);assert(!tube_phone_launch_pending());
    tube_remote_command c;assert(!tube_remote_take(&c) && tube_remote_state()==TUBE_REMOTE_LISTENING);
}

static void test_unavailable_window_refill(void) {
    const char *initial="[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\"],\"playlist\":\"PLrefilltest\",\"index\":105,\"window_start\":105,\"position_sec\":0,\"ctt\":\"click\"}";
    const char *update=",{\"sequence\":2,\"action\":\"updatePlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"abcdefghijk\",\"zyxwvutsrqp\"],\"video_indices\":[100,105,110],\"playlist\":\"PLrefilltest\",\"index\":105,\"window_start\":100,\"position_sec\":0,\"ctt\":\"click\"}]";
    char commands[2048];snprintf(commands,sizeof commands,"%s%s",initial,update);
    feed(commands);tube_phone_tick_at(&controls,test_clock);
    unsigned first=take_start("abcdefghijk",0);
    assert(tube_phone_launch_unavailable(first,"abcdefghijk",test_clock)==3);
    assert(tube_phone_retry_waiting() && !tube_phone_launch_current(first));
    tube_phone_tick_at(&controls,test_clock); /* Consumes refill but observes original delay. */
    assert(tube_phone_retry_waiting() && !tube_phone_launch_pending());
    tube_phone_tick_at(&controls,test_clock+4999);assert(!tube_phone_launch_pending());
    test_clock+=5000;tube_phone_tick_at(&controls,test_clock);
    unsigned next=take_start("zyxwvutsrqp",0);assert(next!=first);
    started_video(next,"zyxwvutsrqp");tube_phone_tick_at(&controls,test_clock);feedback(110,1,0);

    /* Cancellation and scope loss cannot let a late refill reopen playback. */
    for(int mode=0;mode<4;++mode) {
        feed(commands);tube_phone_tick_at(&controls,test_clock);first=take_start("abcdefghijk",0);
        assert(tube_phone_launch_unavailable(first,"abcdefghijk",test_clock)==3);
        if(mode==0)tube_phone_cancel();
        if(mode==1)configure(1,"other-profile",identity,"screen-123");
        if(mode==2)test_clock+=30000;
        if(mode==3){active=1;strcpy(engine.video,"12345678901");engine.selection=++selection_serial;}
        tube_phone_tick_at(&controls,test_clock);
        assert(!tube_phone_retry_waiting() && !tube_phone_launch_pending());
    }
    /* A matching terminal response ends the wait without replaying the anchor. */
    snprintf(commands,sizeof commands,"%s,{\"sequence\":2,\"action\":\"updatePlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\"],\"playlist\":\"PLrefilltest\",\"index\":105,\"window_start\":105,\"position_sec\":0,\"ctt\":\"click\"}]",initial);
    feed(commands);tube_phone_tick_at(&controls,test_clock);first=take_start("abcdefghijk",0);
    assert(tube_phone_launch_unavailable(first,"abcdefghijk",test_clock)==3);
    tube_phone_tick_at(&controls,test_clock);assert(!tube_phone_retry_waiting() && !tube_phone_launch_pending());
    assert(tube_remote_state()==TUBE_REMOTE_LISTENING);

    /* Stopped feedback carries the failed occurrence, never a media binding. */
    snprintf(commands,sizeof commands,"%s]",initial);feed(commands);
    tube_phone_tick_at(&controls,test_clock);first=take_start("abcdefghijk",0);
    assert(tube_phone_launch_unavailable(first,"abcdefghijk",test_clock)==3);
    tube_phone_tick_at(&controls,test_clock);tube_remote_tick(test_clock+3000);
    cJSON *r=request("update"), *p=cJSON_GetObjectItem(r,"player");
    assert(cJSON_GetObjectItem(p,"state")->valueint==0);
    assert(cJSON_GetObjectItem(p,"index")->valueint==105);
    assert(!strcmp(cJSON_GetObjectItem(p,"video_id")->valuestring,"abcdefghijk"));
    assert(!cJSON_IsTrue(cJSON_GetObjectItem(p,"has_next")));
    assert(!tube_phone_owns_media());cJSON_Delete(r);reply(0,200,accepted);

    /* Pending Stop takes priority over refill, preserving intervening volume. */
    snprintf(commands,sizeof commands,"%s,{\"sequence\":2,\"action\":\"setVolume\",\"volume\":17},{\"sequence\":3,\"action\":\"stopVideo\"}]",initial);
    feed(commands);tube_phone_tick_at(&controls,test_clock);first=take_start("abcdefghijk",0);
    assert(tube_phone_launch_unavailable(first,"abcdefghijk",test_clock)==3);
    tube_phone_tick_at(&controls,test_clock);assert(engine.volume==17 && !tube_phone_retry_waiting());
    tube_phone_tick_at(&controls,test_clock);assert(!tube_phone_launch_pending());
    assert(tube_remote_state()==TUBE_REMOTE_LISTENING);

    /* A response from another click/playlist cannot satisfy this wait. */
    snprintf(commands,sizeof commands,"%s,{\"sequence\":2,\"action\":\"updatePlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"zyxwvutsrqp\"],\"playlist\":\"PLrefilltest\",\"index\":105,\"window_start\":105,\"position_sec\":0,\"ctt\":\"other\"}]",initial);
    feed(commands);tube_phone_tick_at(&controls,test_clock);first=take_start("abcdefghijk",0);
    assert(tube_phone_launch_unavailable(first,"abcdefghijk",test_clock)==3);
    tube_phone_tick_at(&controls,test_clock);assert(tube_phone_retry_waiting() && !tube_phone_launch_pending());
    tube_phone_cancel();assert(tube_remote_state()==TUBE_REMOTE_LISTENING);
}

static void test_eof_window_refill(void) {
    const char *initial="[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\"],\"playlist\":\"PLeofrefill\",\"index\":105,\"window_start\":105,\"position_sec\":0,\"ctt\":\"click\"}]";
    for(int mode=0;mode<5;++mode) {
        feed(initial);tube_phone_tick_at(&controls,test_clock);
        started_video(take_start("abcdefghijk",0),"abcdefghijk");
        engine.position=engine.duration=300;engine.state=0;tube_phone_tick_at(&controls,test_clock);
        assert(tube_phone_finished_at(test_clock) && !tube_phone_finished_at(test_clock));
        assert(tube_phone_retry_waiting() && !tube_phone_owns_media());
        tube_phone_tick_at(&controls,test_clock); /* Still-open drained decoder must not cancel. */
        assert(tube_phone_retry_waiting());
        active=0;tube_phone_tick_at(&controls,test_clock);
        if(mode==0)tube_phone_cancel();
        if(mode==1)test_clock+=30000;
        if(mode==2){active=1;engine.state=1;} /* Replay/seek of same selection retires EOF wait. */
        if(mode==3){active=1;engine.selection=++selection_serial;} /* Same-ID new selection. */
        if(mode<4){tube_phone_tick_at(&controls,test_clock);assert(!tube_phone_retry_waiting());continue;}
        tube_remote_tick(test_clock+3000);cJSON *r=request("update"),*p=cJSON_GetObjectItem(r,"player");
        assert(cJSON_GetObjectItem(p,"state")->valueint==0 && cJSON_GetObjectItem(p,"index")->valueint==105);
        assert(cJSON_GetObjectItem(p,"position_sec")->valuedouble==300 && cJSON_GetObjectItem(p,"duration_sec")->valuedouble==300);
        cJSON_Delete(r);reply(0,200,accepted);
        const char *update="[{\"sequence\":2,\"action\":\"updatePlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"abcdefghijk\",\"zyxwvutsrqp\"],\"video_indices\":[100,105,110],\"playlist\":\"PLeofrefill\",\"index\":105,\"window_start\":100,\"position_sec\":300,\"ctt\":\"click\"}]";
        int delivered=0;
        for(int i=0;i<8 && !delivered;++i) {
            tube_remote_tick(test_clock+3500+i*500);if(!count)continue;
            r=cJSON_Parse(pending[0].body);assert(r);int poll=!strcmp(cJSON_GetObjectItem(r,"action")->valuestring,"poll");cJSON_Delete(r);
            if(poll){poll_reply(1,update);delivered=1;}else reply(0,200,accepted);
        }
        assert(delivered);tube_phone_tick_at(&controls,test_clock+8000);
        assert(!tube_phone_retry_waiting()); /* Natural EOF has no unavailable-video delay. */
        started_video(take_start("zyxwvutsrqp",0),"zyxwvutsrqp");
        tube_phone_tick_at(&controls,test_clock+8000);
        assert(tube_phone_owns_media());
    }
    feed(initial);tube_phone_tick_at(&controls,test_clock);started_video(take_start("abcdefghijk",0),"abcdefghijk");
    tube_phone_tick_at(&controls,test_clock);assert(tube_phone_finished_at(-1));assert(!tube_phone_retry_waiting());
}


/* Deliver commands through the real poll/ack path while playback is ending. */
static void deliver_after_start(const char *commands) {
    int delivered=0;
    for(int i=0;i<8 && !delivered;++i) {
        tube_remote_tick(test_clock+100+i*500);if(!count)continue;
        cJSON *r=cJSON_Parse(pending[0].body);assert(r);
        const char *action=cJSON_GetObjectItem(r,"action")->valuestring;
        int poll=!strcmp(action,"poll");assert(poll || !strcmp(action,"update"));cJSON_Delete(r);
        if(poll){poll_reply(1,commands);delivered=1;}else reply(0,200,accepted);
    }
    assert(delivered && tube_remote_state()==TUBE_REMOTE_LISTENING);
}
/* Decoder replacement stops the old source while the new command remains
 * outstanding. Poll replay and inactive snapshots must not retire that launch. */
static void test_switch_held_open(void) {
    const char *first="[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[],\"playlist\":\"\",\"index\":0,\"position_sec\":0,\"ctt\":\"\"}]";
    const char *second="[{\"sequence\":2,\"action\":\"setPlaylist\",\"video_id\":\"zyxwvutsrqp\",\"video_ids\":[],\"playlist\":\"\",\"index\":0,\"position_sec\":5.04,\"ctt\":\"\"}]";
    feed(first);tube_phone_tick(&controls);
    started_video(take_start("abcdefghijk",0),"abcdefghijk");
    tube_phone_tick(&controls);assert(tube_phone_owns_media());
    deliver_after_start(second);tube_phone_tick(&controls);
    unsigned value=take_start("zyxwvutsrqp",5.04);
    assert(tube_phone_launch_reason(value)==0);
    assert(tube_phone_launch_reason(value+1)==2);
    active=0; /* startPlaybackSource stops the old engine before opening the new. */
    for(int i=1;i<=8;++i) {
        configure(1,"guest",identity,"screen-123");
        tube_remote_tick(test_clock+5000+i*500);
        if(count) {
            cJSON *r=cJSON_Parse(pending[0].body);assert(r);
            const char *action=cJSON_GetObjectItem(r,"action")->valuestring;
            int polling=!strcmp(action,"poll");assert(polling || !strcmp(action,"update"));cJSON_Delete(r);
            if(polling)poll_reply(1,second);else reply(0,200,accepted);
        }
        assert(tube_phone_launch_current(value));
        assert(tube_phone_launch_reason(value)==0);
    }
    started_video(value,"zyxwvutsrqp");tube_phone_tick(&controls);
    assert(tube_phone_owns_media() && tube_remote_state()==TUBE_REMOTE_LISTENING);
    tube_remote_command c;assert(!tube_remote_take(&c));
    /* Retain the numeric reason even when another read prunes stale ownership. */
    feed(first);tube_phone_tick(&controls);value=take_start("abcdefghijk",0);
    configure(1,"other-profile",identity,"screen-123");
    assert(tube_phone_launch_reason(value)==3);
    assert(!tube_phone_launch_current(value));
    assert(tube_phone_launch_reason(value)==3);
    feed(first);tube_phone_tick(&controls);value=take_start("abcdefghijk",0);
    tube_phone_cancel();assert(tube_phone_launch_reason(value)==7);
    tube_phone_clear();assert(tube_phone_launch_reason(value)==8);
}
static void test_eof_interruption(void) {
    const char *initial="[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"zyxwvutsrqp\"],\"playlist\":\"PLeof\",\"index\":0,\"position_sec\":0,\"ctt\":\"\"}]";
    const char *interrupts[]={
        "{\"sequence\":3,\"action\":\"stopVideo\"}",
        "{\"sequence\":3,\"action\":\"setPlaylist\",\"video_id\":\"12345678901\",\"video_ids\":[\"12345678901\"],\"playlist\":\"PLnew\",\"index\":900,\"window_start\":900,\"position_sec\":0.375,\"ctt\":\"newclick\"}",
        "{\"sequence\":3,\"action\":\"remoteDisconnected\",\"device_id\":\"phone-one\",\"name\":\"Phone\"}"
    };
    for(int action=0;action<3;++action)for(int stage=0;stage<4;++stage) {
        feed(initial);controls.close_on_disconnect=1;
        tube_phone_tick_at(&controls,test_clock);started_video(take_start("abcdefghijk",0),"abcdefghijk");
        tube_phone_tick_at(&controls,test_clock);
        unsigned next=0;
        if(stage)assert(tube_phone_finished_at(test_clock));
        if(stage>=2)next=take_start("zyxwvutsrqp",0);
        if(stage==3)assert(tube_phone_launch_bind(next,++selection_serial));
        char commands[1200];snprintf(commands,sizeof commands,
            "[{\"sequence\":2,\"action\":\"setVolume\",\"volume\":17},%s]",interrupts[action]);
        deliver_after_start(commands);
        if(!stage)assert(tube_phone_finished_at(test_clock));
        assert(!tube_phone_launch_pending());
        if(next)assert(!tube_phone_launch_current(next) && !tube_phone_launch_bind(next,999) && !tube_phone_launch_finish(next,1));
        assert(!tube_phone_owns_media() && !tube_phone_finished_at(test_clock));
        tube_phone_tick_at(&controls,test_clock);assert(engine.volume==17);
        tube_phone_tick_at(&controls,test_clock);
        if(action==1) {
            started_video(take_start("12345678901",.375),"12345678901");
            tube_phone_tick_at(&controls,test_clock);feedback(900,0,0);
        } else {
            assert(!tube_phone_launch_pending());
            if(action==0)assert(stopped==1 && !active);
            else assert(app_closes==1);
        }
        tube_remote_command c;assert(!tube_remote_take(&c));
        assert(tube_remote_state()==TUBE_REMOTE_LISTENING);
        controls.close_on_disconnect=0;
    }
    /* Ordinary queued controls must not cancel automatic advancement. */
    feed(initial);tube_phone_tick_at(&controls,test_clock);
    started_video(take_start("abcdefghijk",0),"abcdefghijk");tube_phone_tick_at(&controls,test_clock);
    assert(tube_phone_finished_at(test_clock));unsigned next=take_start("zyxwvutsrqp",0);
    deliver_after_start("[{\"sequence\":2,\"action\":\"setVolume\",\"volume\":17},{\"sequence\":3,\"action\":\"getNowPlaying\"}]");
    started_video(next,"zyxwvutsrqp");tube_phone_tick_at(&controls,test_clock);assert(engine.volume==17);
    tube_phone_tick_at(&controls,test_clock);assert(tube_phone_owns_media());feedback(1,1,0);
}

static int lookup_calls, lookup_cancels, lookup_result, lookup_index;
static unsigned lookup_selection;
static char lookup_video[12], lookup_playlist[101], lookup_next[12], lookup_next_playlist[101];
static int lookup_next_index;
static int recommendation(void *ctx,unsigned owner,unsigned selection,const char *video,const char *list,int index,tube_phone_next_t *next) {
    (void)ctx;assert(tube_remote_owner_current(owner));assert(selection==engine.selection);
    ++lookup_calls;lookup_selection=selection;lookup_index=index;
    snprintf(lookup_video,sizeof lookup_video,"%s",video);
    snprintf(lookup_playlist,sizeof lookup_playlist,"%s",list);
    memcpy(next->video,lookup_next,12);memcpy(next->playlist,lookup_next_playlist,sizeof next->playlist);
    next->index=lookup_next_index;return lookup_result;
}
static void recommendation_cancel(void *ctx) { (void)ctx;++lookup_cancels; }
static void fallback_start(const char *list,int with_queue) {
    char commands[800];snprintf(commands,sizeof commands,
        "[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":%s,\"playlist\":\"%s\",\"index\":7,\"position_sec\":0,\"ctt\":\"click\"}]",
        with_queue?"[\"abcdefghijk\"]":"[]",list);
    feed(commands);lookup_calls=lookup_cancels=lookup_result=0;
    strcpy(lookup_next,"zyxwvutsrqp");memset(lookup_next_playlist,0,sizeof lookup_next_playlist);lookup_next_index=0;
    controls.recommendation=recommendation;controls.recommendation_cancel=recommendation_cancel;
    tube_phone_tick_at(&controls,test_clock);started_video(take_start("abcdefghijk",0),"abcdefghijk");
    engine.position=engine.duration=300;engine.state=0;tube_phone_tick_at(&controls,test_clock);
    assert(tube_phone_finished_at(test_clock));assert(tube_phone_retry_waiting());
}
static void test_recommendation_lifecycle(void) {
    /* No supplied queue: retain an ended anchor while asynchronous work polls. */
    fallback_start("",0);tube_phone_tick_at(&controls,test_clock);
    assert(lookup_calls==1 && lookup_selection==engine.selection && lookup_index==7);
    assert(!strcmp(lookup_video,"abcdefghijk") && !*lookup_playlist);
    assert(!tube_phone_launch_pending() && !tube_phone_finished_at(test_clock));
    active=0;tube_phone_tick_at(&controls,test_clock+1);assert(lookup_calls==2);
    tube_remote_tick(test_clock+3000);cJSON *r=request("update"),*p=cJSON_GetObjectItem(r,"player");
    assert(cJSON_GetObjectItem(p,"state")->valueint==0 && cJSON_GetObjectItem(p,"index")->valueint==7);
    assert(cJSON_GetObjectItem(p,"position_sec")->valuedouble==300);cJSON_Delete(r);reply(0,200,accepted);
    lookup_result=1;tube_phone_tick_at(&controls,test_clock+2);
    assert(lookup_cancels==1 && !tube_phone_retry_waiting());
    unsigned ticket=take_start("zyxwvutsrqp",0);started_video(ticket,"zyxwvutsrqp");
    engine.state=1;tube_phone_tick_at(&controls,test_clock+3);
    assert(tube_phone_owns_media() && !tube_phone_launch_current(ticket));
    assert(!*engine.playlist); /* No inherited phone playlist on the new launch. */

    /* Failure, self references, invalid characters and unterminated IDs fail closed. */
    for(int mode=0;mode<4;++mode) {
        fallback_start("",0);lookup_result=mode?1:-1;
        if(mode==1)strcpy(lookup_next,"abcdefghijk");
        if(mode==2)strcpy(lookup_next,"bad!bad!bad");
        if(mode==3)memset(lookup_next,'x',sizeof lookup_next);
        tube_phone_tick_at(&controls,test_clock);
        assert(lookup_calls==1 && lookup_cancels==1 && !tube_phone_retry_waiting() && !tube_phone_launch_pending());
    }
    /* Returned occurrences: distinct same-ID successor, new playlist feedback and
     * rejection of the ended occurrence, malformed lists and bad positions. */
    for(int mode=0;mode<6;++mode) {
        fallback_start("PLfallback",0);
        deliver_after_start("[{\"sequence\":2,\"action\":\"updatePlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\"],\"playlist\":\"PLfallback\",\"index\":7,\"position_sec\":300,\"ctt\":\"click\"}]");
        tube_phone_tick_at(&controls,test_clock);assert(tube_phone_retry_waiting());
        lookup_result=1;strcpy(lookup_next,mode==1?"zyxwvutsrqp":"abcdefghijk");
        strcpy(lookup_next_playlist,mode==1?"PLanother1":mode==4?"bad list!":"PLfallback");
        lookup_next_index=mode==2?7:mode==3?-1:mode==5?100001:9;
        tube_phone_tick_at(&controls,test_clock);
        assert(lookup_calls==1 && lookup_cancels==1 && !tube_phone_retry_waiting());
        if(mode>1) { assert(!tube_phone_launch_pending()); continue; }
        ++selection_serial;unsigned ticket=take_start(lookup_next,0);started_video(ticket,lookup_next);
        engine.state=1;tube_phone_tick_at(&controls,test_clock+1);assert(tube_phone_owns_media());
        tube_remote_tick(test_clock+3000);cJSON *r=request("update"),*p=cJSON_GetObjectItem(r,"player");
        assert(!strcmp(cJSON_GetObjectItem(p,"playlist")->valuestring,lookup_next_playlist));
        assert(cJSON_GetObjectItem(p,"index")->valueint==9);
        /* Click tracking stays with the sender's own playlist only. */
        assert(!strcmp(cJSON_GetObjectItem(p,"ctt")->valuestring,mode?"":"click"));
        cJSON_Delete(r);reply(0,200,accepted);
    }
    /* Deadline, replay, same-ID new selection, profile loss and callback removal. */
    for(int mode=0;mode<8;++mode) {
        fallback_start("",0);tube_phone_tick_at(&controls,test_clock);assert(lookup_calls==1);
        if(mode==0)test_clock+=30000;
        if(mode==1)engine.state=1;
        if(mode==2)engine.selection=++selection_serial;
        if(mode==3)configure(1,"other-profile",identity,"screen-123");
        if(mode==4)controls.recommendation=NULL;
        if(mode==5)tube_phone_cancel();
        if(mode==6)controls.context=&lookup_calls;
        if(mode==7)configure(1,"guest",identity,"different-screen");
        lookup_result=1;tube_phone_tick_at(&controls,test_clock);
        assert(lookup_calls==1 && lookup_cancels==1 && !tube_phone_retry_waiting() && !tube_phone_launch_pending());
        controls.context=NULL;
    }
    cleanup();controls.recommendation=NULL;controls.recommendation_cancel=NULL;
}
static void test_recommendation_commands(void) {
    const char *interrupts[]={
        "{\"sequence\":3,\"action\":\"stopVideo\"}",
        "{\"sequence\":3,\"action\":\"pause\"}",
        "{\"sequence\":3,\"action\":\"seekTo\",\"position_sec\":10}",
        "{\"sequence\":3,\"action\":\"setPlaylist\",\"video_id\":\"12345678901\",\"video_ids\":[],\"playlist\":\"\",\"index\":0,\"ctt\":\"\",\"position_sec\":0}",
        "{\"sequence\":3,\"action\":\"remoteDisconnected\",\"device_id\":\"phone-one\",\"name\":\"Phone\"}"
    };
    for(int mode=0;mode<5;++mode) {
        fallback_start("",0);tube_phone_tick_at(&controls,test_clock);lookup_result=1;
        char commands[700];snprintf(commands,sizeof commands,"[{\"sequence\":2,\"action\":\"setVolume\",\"volume\":17},%s]",interrupts[mode]);
        deliver_after_start(commands);tube_phone_tick_at(&controls,test_clock);assert(engine.volume==17);
        tube_phone_tick_at(&controls,test_clock);
        assert(lookup_calls==1 && lookup_cancels==1 && !tube_phone_retry_waiting());
        if(mode==3)started_video(take_start("12345678901",0),"12345678901");
        else assert(!tube_phone_launch_pending());
    }
    fallback_start("",0);tube_phone_tick_at(&controls,test_clock);lookup_result=1;
    deliver_after_start("[{\"sequence\":2,\"action\":\"setVolume\",\"volume\":17},{\"sequence\":3,\"action\":\"getNowPlaying\"}]");
    tube_phone_tick_at(&controls,test_clock);tube_phone_tick_at(&controls,test_clock);
    assert(engine.volume==17 && lookup_calls==1 && !lookup_cancels);
    tube_phone_tick_at(&controls,test_clock);started_video(take_start("zyxwvutsrqp",0),"zyxwvutsrqp");
    cleanup();controls.recommendation=NULL;controls.recommendation_cancel=NULL;
}
static void test_recommendation_queue_updates(void) {
    const char *terminal="[{\"sequence\":2,\"action\":\"updatePlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\"],\"playlist\":\"PLfallback\",\"index\":7,\"position_sec\":300,\"ctt\":\"click\"}]";
    fallback_start("PLfallback",1);tube_phone_tick_at(&controls,test_clock);assert(!lookup_calls);
    deliver_after_start(terminal);tube_phone_tick_at(&controls,test_clock);assert(tube_phone_retry_waiting());
    tube_phone_tick_at(&controls,test_clock);assert(lookup_calls==1 && !strcmp(lookup_playlist,"PLfallback"));
    lookup_result=1;tube_phone_tick_at(&controls,test_clock);started_video(take_start("zyxwvutsrqp",0),"zyxwvutsrqp");
    /* Repeated terminal windows preserve the pending lookup and original deadline. */
    fallback_start("PLfallback",0);tube_phone_tick_at(&controls,test_clock);assert(lookup_calls==1);
    deliver_after_start(terminal);tube_phone_tick_at(&controls,test_clock+29999);
    assert(lookup_calls==1 && !lookup_cancels && tube_phone_retry_waiting());
    lookup_result=1;tube_phone_tick_at(&controls,test_clock+30000);
    assert(lookup_calls==1 && lookup_cancels==1 && !tube_phone_launch_pending() && !tube_phone_retry_waiting());
    /* A queue-less anchor receiving an authoritative extension cannot index -1. */
    fallback_start("PLfallback",0);tube_phone_tick_at(&controls,test_clock);assert(lookup_calls==1);
    deliver_after_start("[{\"sequence\":2,\"action\":\"updatePlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"12345678901\"],\"playlist\":\"PLfallback\",\"index\":7,\"position_sec\":300,\"ctt\":\"click\"}]");
    lookup_result=1;tube_phone_tick_at(&controls,test_clock);
    assert(lookup_calls==1 && lookup_cancels==1 && !tube_phone_retry_waiting());
    started_video(take_start("12345678901",0),"12345678901");
    /* Keep the existing unavailable-video terminal behavior with callbacks enabled. */
    test_unavailable_window_refill();
    cleanup();controls.recommendation=NULL;controls.recommendation_cancel=NULL;
}

int main(void) {
    remote_driver_test_main(); test_switch_held_open(); test_execution(); test_queue_and_eof(); test_retired_launch_and_failure(); test_duplicate_playlist_position(); test_gain(); test_disconnect_policy(); test_disconnect_close_policy(); test_deferred_caption(); test_retired_receiver_eof(); test_voice_controls(); test_windowed_duplicate_and_refresh(); cleanup();
    test_sparse_occurrences(); cleanup();
    test_unavailable_retry();test_unavailable_terminal_and_cancel();test_unavailable_interrupts();test_unavailable_eof_and_disconnect();test_unavailable_late_stop();test_unavailable_window_refill();test_eof_window_refill();test_eof_interruption();cleanup();
    test_recommendation_lifecycle();test_recommendation_commands();test_recommendation_queue_updates();cleanup();
    puts("Phone player/queue, deferred acknowledgements, fractional seek, EOF, ownership, asynchronous recommendations and PCM gain passed"); return 0;
}
