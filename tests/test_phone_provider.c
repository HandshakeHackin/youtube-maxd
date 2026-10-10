/* Shipping provider pairing/storage integration. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_provider.h"
#include "tube_remote.h"
#include "evo_net.h"
#include "evo_data_path.h"
#include "cJSON.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
static int64_t clock_ms=100000;
int tube_test_clock_gettime(clockid_t id,struct timespec *t) {
    assert(id==CLOCK_MONOTONIC); t->tv_sec=clock_ms/1000; t->tv_nsec=(clock_ms%1000)*1000000; return 0;
}
typedef struct { char body[8192],url[544]; evo_net_cb cb; void *ud; } Pending;
static Pending pending[16]; static int count, rows;
static evo_provider_item_t items[24];
static const char *session="0123456789abcdef0123456789abcdef";
int evo_net_request_async(const char *method,const char *url,const char *body,const char **headers,int n,evo_net_cb cb,void *ud) {
    assert(!strcmp(method,"POST") && strstr(url,"/v1/remote") && count<16 && n==2);
    assert(!strcmp(headers[1],"Authorization: Bearer synthetic-companion-token"));
    Pending *p=&pending[count++]; snprintf(p->body,sizeof p->body,"%s",body); snprintf(p->url,sizeof p->url,"%s",url); p->cb=cb; p->ud=ud; return 0;
}
void evo_error_set(const char *fmt,...) { (void)fmt; }
static void reply(const char *body) {
    assert(count==1); Pending p=pending[0]; --count; p.cb(1,200,body,strlen(body)+1,p.ud);
}
static void list(int success,const evo_provider_item_t *value,int n,int more,void *ud) {
    (void)ud; assert(success && !more && n<=24); rows=n; memcpy(items,value,(size_t)n*sizeof *items);
}
static void page(const char *id) { evo_provider_youtube.list_catalog(id,0,list,NULL); }
static cJSON *read_store(void) {
    FILE *f=fopen(evo_data_path("youtube.json"),"rb"); assert(f); char data[8192]={0};
    assert(fread(data,1,sizeof data-1,f)); fclose(f); cJSON *root=cJSON_Parse(data); assert(root); return root;
}
static void write_store(const cJSON *root) {
    char *body=cJSON_PrintUnformatted(root); assert(body); FILE *f=fopen(evo_data_path("youtube.json"),"wb"); assert(f);
    assert(fwrite(body,1,strlen(body),f)==strlen(body)); assert(!fclose(f)); free(body);
}
static const char *str(const cJSON *root,const char *name) { const cJSON *v=cJSON_GetObjectItemCaseSensitive(root,name); assert(cJSON_IsString(v)); return v->valuestring; }
static cJSON *request(const char *action) {
    assert(count==1); cJSON *root=cJSON_Parse(pending[0].body); assert(root); assert(!strcmp(str(root,"action"),action)); return root;
}
static void stop(void) { cJSON *root=request("stop"); assert(!strcmp(str(root,"session"),session)); cJSON_Delete(root); reply("{\"api_version\":1,\"state\":\"stopped\"}"); }
static void started(const char *device) {
    char body[512]; snprintf(body,sizeof body,"{\"api_version\":1,\"state\":\"listening\",\"session\":\"%s\",\"device_id\":\"%s\",\"screen_id\":\"screen-123\"}",session,device); reply(body);
}
int main(void) {
    char dir[]="/tmp/prosperotube-phone-XXXXXX"; assert(mkdtemp(dir)); assert(!setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    cJSON *seed=cJSON_Parse("{\"version\":2,\"source\":\"http://127.0.0.1:8765\",\"token\":\"synthetic-companion-token\",\"records\":[]}");
    assert(seed); write_store(seed); cJSON_Delete(seed); const evo_provider_t *p=&evo_provider_youtube;
    assert(!p->init()); tube_poll(); assert(!count); page("settings:remote"); assert(rows==4 && !count);
    evo_stream_choice_t choice={0}; snprintf(choice.url,sizeof choice.url,"http://127.0.0.1:8765/media/fixture.mpd");
    char video[12]; assert(!tube_phone_video(choice.url,video));
    p->stream_chosen("abcdefghijk",&choice);
    assert(tube_phone_video(choice.url,video) && !strcmp(video,"abcdefghijk"));
    assert(!tube_phone_video("http://127.0.0.1:8765/media/unrelated.mpd",video));
    page("settings:remote:pair"); assert(rows==4 && !count);
    cJSON *stored=read_store(); char device[37]; snprintf(device,sizeof device,"%s",str(stored,"remote_device"));
    assert(tube_remote_identity_valid(device) && device[14]=='4' && cJSON_IsTrue(cJSON_GetObjectItem(stored,"remote_enabled")));
    assert(!*str(stored,"remote_screen")); cJSON_Delete(stored);
    tube_poll(); cJSON *root=request("start"); assert(!strcmp(str(root,"device_id"),device)); cJSON_Delete(root); started(device);
    tube_poll(); root=request("pair"); cJSON_Delete(root);
    stored=read_store(); assert(!strcmp(str(stored,"remote_screen"),"screen-123")); cJSON_Delete(stored);
    unsigned revision=tube_phone_revision();
    reply("{\"api_version\":1,\"state\":\"listening\",\"session\":\"0123456789abcdef0123456789abcdef\",\"pairing_code\":\"123456789012\"}");
    assert(tube_phone_revision()!=revision); page("settings:remote"); assert(rows==5 && !strcmp(items[1].title,"TV code: 123 456 789 012"));
    stored=read_store(); char *data=cJSON_PrintUnformatted(stored); assert(data && !strstr(data,session) && !strstr(data,"123456789012") && !strstr(data,"lounge_token")); free(data); cJSON_Delete(stored);
    p->shutdown(); stop(); assert(!p->init()); tube_poll(); root=request("start");
    assert(!strcmp(str(root,"device_id"),device) && !strcmp(str(root,"screen_id"),"screen-123")); cJSON_Delete(root); started(device);
    page("settings:remote:toggle"); assert(rows==4); stop(); tube_poll(); assert(!count);
    char blocked[512]; snprintf(blocked,sizeof blocked,"%s",evo_data_path("youtube.json.tmp")); assert(!mkdir(blocked,0700));
    page("settings:remote:reset"); assert(strstr(items[0].title,"local data not saved"));
    stored=read_store(); assert(!strcmp(str(stored,"remote_device"),device)); cJSON_Delete(stored); assert(!rmdir(blocked));
    page("settings:remote:reset"); stored=read_store();
    assert(cJSON_IsFalse(cJSON_GetObjectItem(stored,"remote_enabled")) && !*str(stored,"remote_device") && !*str(stored,"remote_screen")); cJSON_Delete(stored);
    /* A failed atomic save must not advertise an unpersisted receiver. */
    assert(!mkdir(blocked,0700));
    page("settings:remote:pair"); tube_poll(); assert(!count && tube_remote_state()==TUBE_REMOTE_OFF);
    page("settings:remote"); assert(strstr(items[0].title,"local data not saved"));
    stored=read_store(); assert(cJSON_IsFalse(cJSON_GetObjectItem(stored,"remote_enabled")) && !*str(stored,"remote_device")); cJSON_Delete(stored);
    assert(!rmdir(blocked)); page("settings:remote:pair"); tube_poll(); root=request("start");
    char next_device[37]; snprintf(next_device,sizeof next_device,"%s",str(root,"device_id")); cJSON_Delete(root);
    assert(tube_remote_identity_valid(next_device) && strcmp(next_device,device));
    /* Reset while registering: the late callback must close, never restore IDs. */
    page("settings:remote:reset"); started(next_device); stop(); tube_poll(); assert(!count && !*tube_remote_device());
    stored=read_store(); assert(!*str(stored,"remote_device") && !*str(stored,"remote_screen"));
    cJSON_ReplaceItemInObject(stored,"version",cJSON_CreateNumber(9)); write_store(stored); cJSON_Delete(stored);
    assert(p->init()<0); page("settings:remote:pair"); tube_poll(); assert(!count); p->shutdown();
    stored=read_store(); assert(cJSON_GetObjectItem(stored,"version")->valueint==9); cJSON_Delete(stored);
    assert(!unlink(evo_data_path("youtube.json"))); assert(!rmdir(dir));
    puts("Native phone pairing, stable identity, atomic storage, reset and late cleanup passed"); return 0;
}
