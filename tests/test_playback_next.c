/* Shipping next-video jobs, playback ownership and mode policy. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
int main(void) {
    char dir[]="/tmp/tube-playback-next-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    cJSON *root=cJSON_Parse("{\"version\":2,\"source\":\"http://127.0.0.1:8765\",\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\",\"captions\":false,\"records\":[]}");
    write_store(root);cJSON_Delete(root);
    const evo_provider_t *p=&evo_provider_youtube;assert(!p->init());
    evo_stream_choice_t choice={0};evo_provider_item_t next;
    const int modes[]={TUBE_MODE_ALL,TUBE_MODE_SHUFFLE,TUBE_MODE_REVERSE};
    for(int i=0;i<3;++i) {
        assert(tube_set_playback_mode(modes[i]));p->stream_chosen("abcdefghijk",&choice);
        assert(!tube_finish_drained_and_next(100) && tube_playlist_eof_pending());
        assert(pending_count==1 && strstr(pending[0].body,"\"action\":\"playback_next\""));
        assert(!tube_finish_drained_and_next(100) && pending_count==1);
        poll_job("{\"state\":\"done\",\"result\":{\"api_version\":1,\"video_id\":\"abcdefghijk\",\"next_video_id\":\"bcdefghijkl\"}}");
        assert(tube_finish_drained_and_next(100) && tube_take_next(&next));
        assert(!strcmp(next.id,"bcdefghijkl") && !next.playback_scope && !next.resume_pos_sec);
        assert(!tube_take_next(&next) && !tube_playlist_eof_pending());
    }
    const int stops[]={TUBE_MODE_LIST,TUBE_MODE_PAUSE,TUBE_MODE_CLOSE,TUBE_MODE_ONE};
    for(int i=0;i<4;++i) {
        assert(tube_set_playback_mode(stops[i]));p->stream_chosen("abcdefghijk",&choice);
        assert(tube_finish_drained_and_next(100)==(stops[i]==TUBE_MODE_CLOSE));
        assert(!pending_count && !tube_playlist_eof_pending());
    }
    assert(tube_set_playback_mode(TUBE_MODE_ALL));
    const char *results[]={
        "{\"api_version\":1,\"video_id\":\"abcdefghijk\",\"next_video_id\":null}",
        "{\"api_version\":1,\"video_id\":\"abcdefghijk\",\"next_video_id\":\"abcdefghijk\"}",
        "{\"api_version\":1,\"video_id\":\"bcdefghijkl\",\"next_video_id\":\"12345678901\"}"};
    for(int i=0;i<3;++i) {
        p->stream_chosen("abcdefghijk",&choice);assert(!tube_finish_drained_and_next(100));
        char body[512];snprintf(body,sizeof body,"{\"state\":\"done\",\"result\":%s}",results[i]);poll_job(body);
        assert(!tube_finish_drained_and_next(100) && !tube_take_next(&next) && !tube_playlist_eof_pending());
        assert(tube_take_completion_action()==TUBE_COMPLETION_PAUSE);
    }
    p->stream_chosen("abcdefghijk",&choice);assert(!tube_finish_drained_and_next(100));
    tube_playlist_eof_cancel();
    reply(0,0,401,"{\"error\":{\"message\":\"retired recommendation\"}}");
    root=read_store();assert(!strcmp(cJSON_GetObjectItem(root,"token")->valuestring,"abcdefghijklmnopqrstuvwxabcdefghijkl"));cJSON_Delete(root);
    assert(!tube_playlist_eof_pending() && !tube_take_next(&next));
    p->stream_chosen("abcdefghijk",&choice);assert(!tube_finish_drained_and_next(100));
    p->list_catalog("library:profile:other",0,items_cb,NULL);
    reply(0,1,200,"{\"job\":\"retired_next\",\"state\":\"pending\"}");
    assert(!tube_playlist_eof_pending() && !tube_take_next(&next));
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: continuous recommendations, playlist-only isolation, terminal results and retired ownership");
}
