/* Shipping scope and full UTF-8 query request, not a duplicate serializer. */
#define main native_provider_regression_main
#include "test_native.c"
#undef main
int main(void) {
    char dir[] = "/tmp/prosperotube-voice-search-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE", dir, 1));
    cJSON *seed = cJSON_Parse("{\"version\":2,\"source\":\"http://127.0.0.1:8765\",\"token\":\"synthetic-token\",\"library_profile\":\"profile-one\",\"records\":[]}");
    assert(seed); write_store(seed); cJSON_Delete(seed);
    const evo_provider_t *p = &evo_provider_youtube; assert(!p->init());
    char source[512], token[128], profile[33];
    assert(tube_voice_scope(source, sizeof source, token, sizeof token, profile, sizeof profile));
    assert(!strcmp(source, "http://127.0.0.1:8765") && !strcmp(token, "synthetic-token") && !strcmp(profile, "profile-one"));
    assert(!tube_voice_scope(source, sizeof source, token, 1, profile, sizeof profile));
    assert(!*source && !*token && !*profile);
    char query[801];
    for (int i = 0; i < 200; ++i) memcpy(query + i * 4, "\xf0\x9f\x8e\xb5", 4);
    query[800] = 0;
    assert(!p->search(query, 0, items_cb, NULL) && pending_count == 1);
    cJSON *request = cJSON_Parse(pending[0].body); assert(request);
    assert(!strcmp(cJSON_GetObjectItem(request, "query")->valuestring, query));
    assert(!strcmp(cJSON_GetObjectItem(request, "profile")->valuestring, "profile-one"));
    cJSON_Delete(request);
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[],\"has_more\":false}}");
    assert(last_ok && !last_count);
    memset(query, 'a', 201); query[201] = 0;
    assert(p->search(query, 0, items_cb, NULL) < 0 && !pending_count);
    assert(p->search("\xc0\xaf", 0, items_cb, NULL) < 0 && !pending_count);
    assert(!p->set_source("http://127.0.0.1:9999#123456"));
    assert(!tube_voice_scope(source, sizeof source, token, sizeof token, profile, sizeof profile));
    assert(!*source && !*token && !*profile);
    p->shutdown(); puts("Voice search: paired scope and complete UTF-8 request passed"); return 0;
}
