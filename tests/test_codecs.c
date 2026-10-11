/* Codec requests and reload use the shipping provider and HTTP harness. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
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
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: five codec choices, request identity, persistence and wraparound");
    return 0;
}
