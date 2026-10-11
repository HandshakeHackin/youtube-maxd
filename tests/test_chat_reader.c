/* Shipping replay reader: explicit time, retained cursor, ended rewind, scope. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static void replay(int position, const char *cursor, int ended) {
    char body[1024];
    snprintf(body,sizeof body,"{\"api_version\":1,\"kind\":\"live_chat\",\"id\":\"abcdefghijk\",\"chat_mode\":\"top\",\"is_replay\":true,\"position_ms\":%d,\"has_more\":false,\"ended\":%s,\"poll_after_ms\":1000,\"cursor\":\"%s\",\"items\":[{\"id\":\"one\",\"author\":\"Viewer\",\"content\":\"Replay message\"}]}",position,ended?"true":"false",cursor);
    reply(0,1,200,body);
}
int main(void) {
    char dir[]="/tmp/tube-chat-reader-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t *p=&evo_provider_youtube; assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->list_catalog("section:live",0,items_cb,NULL);
    reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":false}}");
    const char *parent="chat:top:abcdefghijk";
    assert(tube_chat_reader_position(parent)<0);
    p->list_catalog(parent,0,items_cb,NULL);
    assert(pending_count==1 && strstr(pending[0].body,"\"chat_history\":true") && strstr(pending[0].body,"\"chat_position_sec\":0.000"));
    assert(!tube_chat_reader_seek(parent,60));
    replay(0,"reader-zero",0);
    assert(last_ok && last_count==1 && tube_chat_reader_position(parent)==0);
    assert(!tube_chat_due(parent)); /* Replay reader is stationary until a seek. */
    p->list_catalog("message:compose",0,items_cb,NULL);
    assert(!last_ok && !pending_count); tube_chat_prompt_t prompt;
    assert(!tube_take_chat_prompt(&prompt));
    assert(!tube_chat_reader_seek(parent,-1) && !tube_chat_reader_seek(parent,NAN));
    assert(!tube_chat_reader_seek(parent,1000001) && !tube_chat_reader_seek("chat:top:zyxwvutsrqp",60));
    assert(tube_chat_reader_seek(parent,60.125) && tube_chat_due(parent));
    p->list_catalog(parent,0,items_cb,NULL);
    assert(strstr(pending[0].body,"reader-zero") && strstr(pending[0].body,"\"chat_position_sec\":60.125"));
    assert(!tube_chat_reader_seek(parent,120)); /* No old response can race a seek. */
    replay(60125,"reader-ended",1);
    assert(tube_chat_reader_position(parent)==60.125 && !tube_chat_due(parent));
    assert(tube_chat_reader_seek(parent,0) && tube_chat_due(parent));
    p->list_catalog(parent,0,items_cb,NULL);
    assert(strstr(pending[0].body,"reader-ended"));
    replay(0,"reader-rewind",0);
    assert(last_ok && tube_chat_reader_position(parent)==0);
    assert(tube_chat_reader_seek(parent,120)); p->list_catalog(parent,0,items_cb,NULL);
    replay(119000,"reader-wrong-time",0);
    assert(!last_ok && tube_chat_reader_position(parent)<0);
    assert(pending_count==2); /* Close both returned and previously owned cursors. */
    while (pending_count) {
        assert(strstr(pending[0].body,"chat_close")); reply(0,1,200,"{\"success\":true}");
    }
    p->list_catalog("library:profile:other",0,items_cb,NULL);
    assert(!tube_chat_reader_seek(parent,60));
    p->shutdown();
    puts("PASS: replay reader time, stationary view, fractional seeks, ended rewind, mismatched time and scope");
    return 0;
}
