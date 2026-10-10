/* Exercise downloaded caption identity with the shipping provider, not preferences. */
#define main native_provider_regression_main
#include "test_native.c"
#undef main
int main(void) {
    char dir[]="/tmp/prosperotube-caption-XXXXXX";
    assert(mkdtemp(dir)); assert(!setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    cJSON *seed=cJSON_Parse("{\"version\":2,\"source\":\"http://127.0.0.1:8765\",\"token\":\"synthetic-token\",\"language\":\"en\",\"captions\":true,\"records\":[]}");
    assert(seed); write_store(seed); cJSON_Delete(seed);
    const evo_provider_t *p=&evo_provider_youtube; assert(!p->init());
    assert(!*tube_phone_caption_language());
    p->resolve("abcdefghijk",resolve_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/session/video\"}],\"captions\":[{\"language\":\"en\",\"url\":\"http://127.0.0.1:8765/media/session/c0.srt\"}]}}");
    assert(pending_count==1 && !strcmp(pending[0].method,"GET") && !resolve_calls);
    Pending caption=pending[0]; pending_count=0;
    const char *srt="1\n00:00:00,000 --> 00:00:02,000\nEnglish cue\n\n";
    caption.cb(1,200,srt,strlen(srt),caption.ud);
    assert(resolve_calls==1 && last_ok && !strcmp(tube_phone_caption_language(),"en"));
    /* Preferences apply to the next video, not the track already downloaded. */
    p->list_catalog("settings:lang:fr",0,items_cb,NULL);
    assert(!strcmp(tube_phone_caption_language(),"en"));
    p->list_catalog("settings:lang:de",0,items_cb,NULL);
    assert(!strcmp(tube_phone_caption_language(),"en"));
    /* The next resolve retires track identity; failed fetching doesn't claim German. */
    p->resolve("abcdefghijk",resolve_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/session/new\"}],\"captions\":[{\"language\":\"de\",\"url\":\"http://127.0.0.1:8765/media/session/c1.srt\"}]}}");
    assert(!*tube_phone_caption_language()); caption=pending[0]; pending_count=0;
    caption.cb(0,503,"",0,caption.ud);
    assert(resolve_calls==2 && last_ok && !*tube_phone_caption_language());
    p->shutdown(); assert(!p->init()); assert(!*tube_phone_caption_language()); p->shutdown();
    assert(!unlink(evo_data_path("youtube.json")));
    assert(!unlink(evo_data_path("active.srt"))); assert(!rmdir(dir));
    puts("Downloaded caption identity survives preference changes and retires on resolve/failure");
    return 0;
}
