/* Exercise the shipping adapter against real driver JSON/ack handling. */
#define main remote_driver_test_main
#include "test_remote_native.c"
#undef main
#include "tube_phone.h"
#include "tube_volume.h"
static tube_remote_player engine;
static int active, browsed, stopped, keys, reject_seek;
static int deferred_caption, caption_result, caption_polls;
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
static tube_phone_controls controls = {NULL,snapshot,pause_engine,seek_engine,stop_engine,volume_engine,key_engine,caption_engine,browser,0,caption_status};
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
    test_clock += 10000; configure(1,"guest",identity,"screen-123"); remote_bind(test_clock);
    tube_remote_tick(test_clock + 1); poll_reply(0,commands); assert(tube_remote_state() == TUBE_REMOTE_LISTENING);
}
static unsigned take_start(const char *id, double position) {
    char video[12]; double offset = -1; unsigned value = tube_phone_take(video,&offset);
    assert(value && !strcmp(video,id) && offset == position);
    assert(tube_phone_launch_current(value)); assert(!tube_phone_take(video,&offset)); return value;
}
static void started_video(unsigned value, const char *id) {
    snprintf(engine.video,sizeof engine.video,"%s",id); active = 1; engine.position = 0;
    engine.selection = ++selection_serial;
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
    /* No remote launch: don't stop local media, even with the preference enabled. */
    feed("[{\"sequence\":1,\"action\":\"remoteDisconnected\",\"device_id\":\"phone-one\",\"name\":\"Phone\"}]");
    active = 1; strcpy(engine.video,"abcdefghijk"); tube_phone_tick(&controls); assert(active && !stopped);
    controls.stop_on_disconnect = 0;
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
int main(void) {
    remote_driver_test_main(); test_execution(); test_queue_and_eof(); test_retired_launch_and_failure(); test_duplicate_playlist_position(); test_gain(); test_disconnect_policy(); test_deferred_caption(); test_retired_receiver_eof(); cleanup();
    puts("Phone player/queue, deferred acknowledgements, fractional seek, EOF, ownership and PCM gain passed"); return 0;
}
