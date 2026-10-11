/* Shipping resolver/queue failure classification and bounded automatic advance. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *provider;
static void action(const char *id) { provider->list_catalog(id,0,items_cb,NULL); assert(last_ok); }
static void fail(const char *id,const char *code,const char *state) {
    assert(!provider->resolve(id,resolve_cb,NULL));
    char body[256]; snprintf(body,sizeof body,"{\"state\":\"%s\",\"error\":{\"code\":\"%s\",\"message\":\"This fixture cannot be played.\"}}",state,code);
    poll_job(body); assert(!last_ok);
}
static void unchanged(const char *id) {
    cJSON *root=read_store(), *row; int found=0;
    cJSON_ArrayForEach(row,cJSON_GetObjectItem(root,"records")) {
        if (strcmp(cJSON_GetObjectItem(row,"id")->valuestring,id)) continue;
        ++found; assert(cJSON_IsTrue(cJSON_GetObjectItem(row,"queued")));
        assert(!cJSON_IsTrue(cJSON_GetObjectItem(row,"played")));
        assert(cJSON_GetObjectItem(row,"position")->valueint==9);
    }
    assert(found==1); cJSON_Delete(root);
}
int main(void) {
    char dir[]="/tmp/tube-queue-failure-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1)); provider=&evo_provider_youtube;
    cJSON *root=cJSON_Parse("{\"version\":2,\"source\":\"http://127.0.0.1:8765\",\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\",\"autoplay\":true,\"captions\":false,\"records\":["
        "{\"id\":\"abcdefghijk\",\"profile\":\"guest\",\"queued\":true,\"queue_order\":1,\"position\":9},"
        "{\"id\":\"12345678901\",\"profile\":\"guest\",\"queued\":true,\"queue_order\":2,\"position\":9},"
        "{\"id\":\"ABCDEFGHIJK\",\"profile\":\"guest\",\"queued\":true,\"queue_order\":3,\"position\":9}]}");
    write_store(root); cJSON_Delete(root); assert(!provider->init()); action("queue");
    const char *codes[]={"authentication_required","youtube_challenge","youtube_rate_limited","resolver_network","extraction_failed","unknown","video_unavailable extra"};
    for (unsigned i=0;i<sizeof codes/sizeof *codes;++i) {
        fail("abcdefghijk",codes[i],"failed"); assert(!tube_resolve_unavailable("abcdefghijk"));
        assert(!tube_queue_skip_unavailable("abcdefghijk") && !tube_queue_retry_pending());
    }
    fail("abcdefghijk","video_unavailable","canceled"); assert(!tube_resolve_unavailable("abcdefghijk"));
    fail("abcdefghijk","video_unavailable","failed"); assert(tube_resolve_unavailable("abcdefghijk"));
    assert(!tube_queue_skip_unavailable("12345678901")); assert(tube_queue_skip_unavailable("abcdefghijk"));
    assert(tube_queue_retry_pending() && !tube_queue_skip_unavailable("abcdefghijk"));
    evo_provider_item_t next; assert(tube_take_next(&next) && !strcmp(next.id,"12345678901"));
    fail(next.id,"video_unavailable","failed"); assert(tube_queue_skip_unavailable(next.id));
    assert(tube_take_next(&next) && !strcmp(next.id,"ABCDEFGHIJK"));
    assert(!provider->resolve(next.id,resolve_cb,NULL));
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"ABCDEFGHIJK\",\"title\":\"Good video\",\"duration_sec\":30,\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/test/good.mp4\",\"container\":\"mp4\",\"label\":\"360p\"}]}}");
    assert(last_ok && !tube_resolve_unavailable(next.id)); provider->stream_chosen(next.id,NULL);
    assert(!tube_finish_and_next(30,30)); assert(!tube_take_next(&next));
    unchanged("abcdefghijk"); unchanged("12345678901");
    /* Exhaustion, explicit cancellation, account/source changes and autoplay off cannot loop. */
    assert(!provider->init()); action("queue"); fail("abcdefghijk","video_unavailable","failed");
    assert(tube_queue_skip_unavailable("abcdefghijk")); assert(tube_take_next(&next));
    fail(next.id,"video_unavailable","failed"); assert(!tube_queue_skip_unavailable(next.id));
    assert(!tube_take_next(&next)); unchanged("abcdefghijk"); unchanged("12345678901");
    fail("abcdefghijk","video_unavailable","failed"); assert(tube_queue_skip_unavailable("abcdefghijk"));
    assert(tube_toggle_queue("12345678901")==0);
    assert(!tube_queue_retry_pending() && !tube_take_next(&next));
    assert(tube_toggle_queue("12345678901")==1); /* an explicit new insertion may be tried in a later run */
    fail("abcdefghijk","video_unavailable","failed"); assert(tube_queue_skip_unavailable("abcdefghijk"));
    tube_queue_retry_cancel(); assert(!tube_queue_retry_pending() && !tube_take_next(&next));
    fail("abcdefghijk","video_unavailable","failed"); action("library:profile:other");
    assert(!tube_resolve_unavailable("abcdefghijk") && !tube_queue_skip_unavailable("abcdefghijk"));
    action("library:profile:guest"); action("queue"); fail("abcdefghijk","video_unavailable","failed");
    action("settings:autoplay"); assert(!tube_queue_skip_unavailable("abcdefghijk")); action("settings:autoplay");
    action("history"); fail("abcdefghijk","video_unavailable","failed");
    assert(tube_resolve_unavailable("abcdefghijk") && !tube_queue_skip_unavailable("abcdefghijk"));
    action("queue"); assert(!provider->resolve("abcdefghijk",resolve_cb,NULL));
    reply(0,0,404,"{\"error\":{\"code\":\"video_unavailable\",\"message\":\"Unavailable\"}}");
    assert(tube_queue_skip_unavailable("abcdefghijk"));
    action("settings:autoplay"); assert(!tube_queue_retry_pending() && !tube_take_next(&next));
    action("settings:autoplay"); action("queue");
    assert(!provider->resolve("abcdefghijk",resolve_cb,NULL));
    assert(!provider->resolve("12345678901",resolve_cb,NULL));
    assert(pending_count==2);
    reply(0,1,404,"{\"error\":{\"code\":\"video_unavailable\",\"message\":\"Late old failure\"}}");
    assert(!tube_resolve_unavailable("abcdefghijk") && !tube_resolve_unavailable("12345678901"));
    poll_job("{\"state\":\"failed\",\"error\":{\"code\":\"video_unavailable\",\"message\":\"Unavailable\"}}");
    assert(tube_resolve_unavailable("12345678901") && !tube_resolve_unavailable("abcdefghijk"));
    assert(tube_queue_skip_unavailable("12345678901"));
    assert(!provider->set_source("http://127.0.0.1:8766")); assert(!tube_queue_retry_pending() && !tube_take_next(&next));
    assert(!provider->set_source("http://127.0.0.1:8766#123456")); action("queue");
    assert(!provider->resolve("abcdefghijk",resolve_cb,NULL)); assert(strstr(pending[0].url,"/v1/pair"));
    reply(0,1,404,"{\"error\":{\"code\":\"video_unavailable\",\"message\":\"Invalid pairing response\"}}");
    assert(!tube_resolve_unavailable("abcdefghijk") && !tube_queue_skip_unavailable("abcdefghijk"));
    provider->shutdown(); unlink(evo_data_path("youtube.json")); assert(!rmdir(dir));
    puts("PASS: typed queue failures, unavailable-only skip, bounded run, preserved history and cancellation scopes");
}
