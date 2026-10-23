/* The search screen's completions and best matches for a typed text: one that fails for an
 * account profile is asked once more as Guest, so a failing account search never leaves the
 * screen without them, and the log names which failed and why (code and status only). */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *p;
static char logged[8][256];
static int log_count;
void evo_boot_log(const char *format, ...) {
    va_list ap; va_start(ap, format);
    if (log_count < 8) vsnprintf(logged[log_count++], sizeof logged[0], format, ap);
    va_end(ap);
}
static const char *features="{\"api_version\":1,\"features\":[\"search\",\"library\"]}";
static int direct(void) {return pending_count==1 && !strcmp(pending[0].method,"POST") && strstr(pending[0].url,"/v1/jobs")!=NULL;}
static const char *field(const char *key) {
    static char value[256];
    cJSON *job=cJSON_Parse(pending[0].body),*library=cJSON_GetObjectItem(job,"library"),*item=cJSON_GetObjectItem(library?library:job,key);
    assert(job);snprintf(value,sizeof value,"%s",cJSON_IsString(item)?item->valuestring:"<absent>");
    cJSON_Delete(job);return value;
}
static void settle(void) {
    for(int i=0;i<4 && pending_count==1 && !direct();++i) {
        if(strstr(pending[0].url,"/v1/pair"))reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
        else if(strstr(pending[0].url,"/v1/health"))reply(0,1,200,features);
        else abort();
    }
}
#define FAILED "{\"state\":\"failed\",\"error\":{\"code\":\"upstream_format\",\"message\":\"YouTube returned an unsupported layout for minecraft.\"}}"
#define MATCHES "{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"A match\"}],\"has_more\":false,\"cursor\":\"\"}}"
#define COMPLETIONS "{\"state\":\"done\",\"result\":{\"api_version\":1,\"kind\":\"search_suggestions\",\"query\":\"minecr\",\"suggestions\":[\"minecraft\",\"minecraft music\"]}}"
int main(void) {
    char dir[]="/tmp/tube-typed-search-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->list_catalog("library:profile:fixture",0,items_cb,NULL);assert(last_ok);
    /* Best matches of an account: the account's search fails, the Guest one answers. */
    p->list_catalog("search-preview:minecr",0,items_cb,NULL);settle();
    assert(direct() && !strcmp(field("action"),"search") && !strcmp(field("profile"),"fixture"));
    poll_job(FAILED);
    assert(direct() && !strcmp(field("action"),"search") && !strcmp(field("profile"),"guest") && !strcmp(field("query"),"minecr"));
    poll_job(MATCHES);
    assert(last_ok && last_count==1 && !strcmp(last_items[0].id,"abcdefghijk"));
    assert(log_count==1 && !strcmp(logged[0],"YouTube Max’d search: best matches failed code=upstream_format status=200 retry=guest"));
    /* Completions likewise. */
    p->list_catalog("search-suggest:minecr",0,items_cb,NULL);settle();
    assert(direct() && !strcmp(field("action"),"search_suggestions") && !strcmp(field("profile"),"fixture"));
    poll_job(FAILED);
    assert(direct() && !strcmp(field("action"),"search_suggestions") && !strcmp(field("profile"),"guest"));
    poll_job(COMPLETIONS);
    assert(last_ok && last_count==2 && !strcmp(last_items[0].title,"minecraft"));
    assert(log_count==2 && !strcmp(logged[1],"YouTube Max’d search: suggestions failed code=upstream_format status=200 retry=guest"));
    /* Guest is asked once: a second failure is the answer, logged without a retry, the text never in the log. */
    p->list_catalog("search-preview:minecra",0,items_cb,NULL);settle();poll_job(FAILED);
    assert(direct() && !strcmp(field("profile"),"guest"));poll_job(FAILED);
    assert(!last_ok && !pending_count && log_count==4 && !strcmp(logged[3],"YouTube Max’d search: best matches failed code=upstream_format status=200"));
    for(int i=0;i<log_count;++i)assert(!strstr(logged[i],"minec") && !strstr(logged[i],"fixture"));
    /* A Guest's own failure is not asked again. */
    p->list_catalog("library:profile:guest",0,items_cb,NULL);
    p->list_catalog("search-preview:zoo",0,items_cb,NULL);settle();assert(!strcmp(field("profile"),"guest"));poll_job(FAILED);
    assert(!last_ok && !pending_count && log_count==5 && !strcmp(logged[4],"YouTube Max’d search: best matches failed code=upstream_format status=200"));
    p->shutdown();
    puts("PASS: typed completions and best matches fall back to Guest once and log why they failed");
    return 0;
}
