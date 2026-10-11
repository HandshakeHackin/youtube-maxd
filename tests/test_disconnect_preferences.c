/* Exercise shipping preference cycling, persistence and legacy migration. */
#define main native_provider_regression_main
#include "test_native.c"
#undef main
int main(void) {
    char dir[] = "/tmp/prosperotube-disconnect-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE", dir, 1));
    cJSON *seed = cJSON_Parse("{\"version\":2,\"source\":\"http://127.0.0.1:8765\",\"token\":\"synthetic-token\",\"remote_stop_on_disconnect\":true,\"records\":[]}");
    assert(seed); write_store(seed); cJSON_Delete(seed);
    const evo_provider_t *p = &evo_provider_youtube; assert(!p->init());
    assert(tube_phone_stop_on_disconnect() && !tube_phone_close_on_disconnect());
    p->list_catalog("settings:remote:disconnect",0,items_cb,NULL);
    assert(!tube_phone_stop_on_disconnect() && tube_phone_close_on_disconnect());
    p->shutdown();assert(!p->init());
    assert(!tube_phone_stop_on_disconnect() && tube_phone_close_on_disconnect());
    p->list_catalog("settings:remote:disconnect",0,items_cb,NULL);
    assert(!tube_phone_stop_on_disconnect() && !tube_phone_close_on_disconnect());
    p->list_catalog("settings:remote:disconnect",0,items_cb,NULL);
    assert(tube_phone_stop_on_disconnect() && !tube_phone_close_on_disconnect());
    p->shutdown();assert(!p->init());
    assert(tube_phone_stop_on_disconnect() && !tube_phone_close_on_disconnect());
    p->shutdown();
    puts("Disconnect choices cycle and persist with legacy stop preference preserved");
    return 0;
}
