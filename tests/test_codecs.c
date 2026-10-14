/* Codec requests and reload use the shipping provider and HTTP harness. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const char *expected_range;
static void range_cb(int ok, const evo_stream_choice_t *choices, int count, void *ud) {
    (void)ud;
    assert(ok && count == 1 && !strcmp(choices[0].video_range, expected_range));
}
int main(void) {
    char dir[]="/tmp/tube-codecs-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t *p=&evo_provider_youtube; assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    assert(!p->search("fixture",0,items_cb,NULL));
    reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[]}}");
    p->list_catalog("settings:captions",0,items_cb,NULL);
    const char *codecs[]={"auto","avc","vp9","av1","hevc","auto"};
    for(unsigned i=0;i<sizeof codecs/sizeof *codecs;++i) {
        if(i) {p->list_catalog("settings:codec",0,items_cb,NULL);assert(last_ok);}
        p->shutdown(); assert(!p->init());
        assert(!p->resolve("abcdefghijk",resolve_cb,NULL));
        char expected[40];snprintf(expected,sizeof expected,"\"codec\":\"%s\"",codecs[i]);
        assert(strstr(pending[0].body,expected));
        poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/capability/d0.mpd\",\"container\":\"dash\",\"video_codec\":\"hevc\",\"audio_codec\":\"aac\"}]}}");
        assert(last_ok && !pending_count);
    }
    expected_range="HDR10";
    assert(!p->resolve("abcdefghijk",range_cb,NULL));
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/capability/hdr.mpd\",\"video_range\":\"HDR10\"}]}}");
    expected_range="HLG";
    assert(!p->resolve("abcdefghijk",range_cb,NULL));
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/capability/hlg.mpd\",\"video_range\":\"HLG\"}]}}");
    expected_range="SDR";
    assert(!p->resolve("abcdefghijk",range_cb,NULL));
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/capability/other.mpd\",\"video_range\":\"invalid\"}]}}");
    /* The codec is a preference: a video without it is asked for once more in any codec and plays; the setting stays. */
    for(int i=0;i<4;++i) {p->list_catalog("settings:codec",0,items_cb,NULL);assert(last_ok);}
    const char *refused="{\"state\":\"failed\",\"error\":{\"code\":\"unsupported_media\",\"message\":\"No compatible stream was returned.\"}}";
    int calls=resolve_calls,codec=0;
    assert(!p->resolve("abcdefghijk",resolve_cb,NULL) && strstr(pending[0].body,"\"codec\":\"hevc\""));
    poll_job(refused);
    assert(resolve_calls==calls && pending_count==1 && strstr(pending[0].url,"/v1/jobs") && !strcmp(pending[0].method,"POST"));
    assert(strstr(pending[0].body,"\"codec\":\"auto\"") && !strstr(pending[0].body,"hevc") && strstr(pending[0].body,"\"id\":\"abcdefghijk\""));
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/capability/d0.mpd\",\"container\":\"dash\",\"video_codec\":\"h264\",\"audio_codec\":\"aac\"}]}}");
    assert(resolve_calls==calls+1 && last_ok && !pending_count);
    tube_player_quality(NULL,NULL,&codec);assert(codec==4);
    /* Once only: a video with nothing playable fails after the second answer. */
    assert(!p->resolve("abcdefghijk",resolve_cb,NULL));poll_job(refused);assert(pending_count==1);poll_job(refused);
    assert(resolve_calls==calls+2 && !last_ok && !pending_count && strstr(error_text,"No compatible stream"));
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: five codec choices, request identity, persistence, wraparound and one retry in any codec");
    return 0;
}
