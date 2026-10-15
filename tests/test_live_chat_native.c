/* Shipping chat reader, deterministic HTTP/time and capability retirement.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_provider.h"
#include "evo_net.h"
#include "evo_data_path.h"
#include "cJSON.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static long long clock_ms = 100000;
int tube_test_clock_gettime(clockid_t id, struct timespec *t) {
    assert(id == CLOCK_MONOTONIC); t->tv_sec = clock_ms / 1000; t->tv_nsec = clock_ms % 1000 * 1000000; return 0;
}
typedef struct { char method[8], url[1024], body[2048], bearer[160]; evo_net_cb cb; void *ud; } Pending;
static Pending pending[32]; static int count, calls, ok, rows;
static evo_provider_item_t items[256];
int evo_net_request_async(const char *method, const char *url, const char *body,
                         const char **headers, int n, evo_net_cb callback, void *ud) {
    assert(count < 32); Pending *p = &pending[count++]; memset(p, 0, sizeof *p);
    snprintf(p->method, sizeof p->method, "%s", method); snprintf(p->url, sizeof p->url, "%s", url);
    snprintf(p->body, sizeof p->body, "%s", body ? body : "");
    for (int i = 0; i < n; ++i) if (!strncmp(headers[i], "Authorization:", 14))
        snprintf(p->bearer, sizeof p->bearer, "%s", headers[i]);
    p->cb = callback; p->ud = ud; return 0;
}
void evo_error_set(const char *format, ...) { (void)format; }
static void reply(int i, int status, const char *body) {
    assert(i < count); Pending p = pending[i];
    memmove(pending+i, pending+i+1, (size_t)(--count-i) * sizeof *pending);
    p.cb(status == 200, status, body, strlen(body)+1, p.ud);
}
static void received(int success, const evo_provider_item_t *value, int n, int more, void *ud) {
    (void)ud; assert(!more); ++calls; ok = success; rows = n; assert(n <= 256);
    if (value) memcpy(items, value, (size_t)n * sizeof *items);
}
static cJSON *frame(const char *cursor, const char *mode, int first, int total) {
    cJSON *root = cJSON_CreateObject(), *list = cJSON_CreateArray(); assert(root && list);
    cJSON_AddNumberToObject(root,"api_version",1); cJSON_AddStringToObject(root,"kind","live_chat");
    cJSON_AddStringToObject(root,"id","abcdefghijk"); cJSON_AddStringToObject(root,"chat_mode",mode);
    cJSON_AddStringToObject(root,"cursor",cursor); cJSON_AddBoolToObject(root,"has_more",0);
    cJSON_AddBoolToObject(root,"ended",0); cJSON_AddBoolToObject(root,"is_replay",0);
    cJSON_AddNumberToObject(root,"poll_after_ms",2000); cJSON_AddItemToObject(root,"items",list);
    for (int i = first; i < first+total; ++i) {
        cJSON *row = cJSON_CreateObject(); char id[32]; snprintf(id,sizeof id,"server/chat+=%d",i);
        cJSON_AddStringToObject(row,"id",id); cJSON_AddStringToObject(row,"author","Viewer");
        cJSON_AddStringToObject(row,"content","Literal <b>message</b> & café.\nSecond line.");
        cJSON_AddStringToObject(row,"published","now"); cJSON_AddStringToObject(row,"type","LiveChatPaidMessage");
        cJSON_AddStringToObject(row,"amount","$5.00"); cJSON_AddBoolToObject(row,"moderator",1);
        cJSON_AddItemToArray(list,row);
    }
    return root;
}
static void respond(cJSON *root) { char *json = cJSON_PrintUnformatted(root); assert(json); reply(0,200,json); free(json); cJSON_Delete(root); }
static void cleanup(void) {
    while (count) { assert(strstr(pending[0].body,"chat_close") || !strcmp(pending[0].method,"DELETE")); reply(0,200,"{}"); }
}
static void privacy(void) {
    FILE *file = fopen(evo_data_path("youtube.json"),"rb"); assert(file);
    char data[32768]={0}; assert(fread(data,1,sizeof data-1,file) > 0); assert(!fclose(file));
    assert(!strstr(data,"chat-cap") && !strstr(data,"Literal") && !strstr(data,"server/chat"));
}
int main(void) {
    char directory[]="/tmp/prosperotube-chat-XXXXXX"; assert(mkdtemp(directory)); assert(!setenv("EVO_DATA_DIR_OVERRIDE",directory,1));
    const evo_provider_t *p=&evo_provider_youtube; assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    assert(!p->list_catalog("chat:top:abcdefghijk",0,received,NULL));
    assert(count==1 && strstr(pending[0].url,"/v1/pair"));
    reply(0,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    assert(count==1 && strstr(pending[0].url,"/v1/library") && strstr(pending[0].body,"live_chat"));
    assert(!strstr(pending[0].body,"\"library\""));
    respond(frame("chat-cap-one","top",0,24));
    assert(ok && rows==24 && items[0].is_folder && strstr(items[0].subtitle,"Super Chat | $5.00 | Moderator"));
    assert(!strncmp(items[0].id,"chat:leaf:",10) && !strstr(items[0].id,"server/chat"));
    assert(!strcmp(tube_comment_text(items[0].id),"Literal <b>message</b> & café.\nSecond line."));
    assert(!*tube_comment_rating(items[0].id));
    char stable[160], removed[160]; snprintf(stable,sizeof stable,"%s",items[1].id); snprintf(removed,sizeof removed,"%s",items[0].id);
    assert(!tube_chat_due("chat:top:abcdefghijk")); clock_ms += 1999;
    assert(!tube_chat_due("chat:top:abcdefghijk")); ++clock_ms;
    assert(tube_chat_due("chat:top:abcdefghijk") && !tube_chat_due("chat:live:abcdefghijk"));
    assert(!p->list_catalog("chat:top:abcdefghijk",0,received,NULL));
    assert(count==1 && strstr(pending[0].body,"chat_poll") && strstr(pending[0].body,"chat-cap-one"));
    clock_ms += 5000; tube_poll(); assert(count==1 && !tube_chat_due("chat:top:abcdefghijk"));
    respond(frame("chat-cap-two","top",1,24));
    assert(ok && rows==24 && !strcmp(items[0].id,stable) && !*tube_comment_text(removed));
    assert(!count); privacy();
    /* Full retained history remains readable; moderation removes old text. */
    clock_ms += 2000;
    assert(!p->list_catalog("chat:top:abcdefghijk",0,received,NULL));
    assert(strstr(pending[0].body,"\"chat_history\":true"));
    respond(frame("chat-cap-history","top",0,256));
    assert(ok && rows==256 && *tube_comment_text(items[255].id));
    snprintf(removed,sizeof removed,"%s",items[0].id);
    clock_ms += 2000; assert(!p->list_catalog("chat:top:abcdefghijk",0,received,NULL));
    respond(frame("chat-cap-two","top",1,255));
    assert(ok && rows==255 && !*tube_comment_text(removed));
    /* Mode change closes the old context and opens a new one. */
    assert(!p->list_catalog("chat:live:abcdefghijk",0,received,NULL));
    assert(count==2 && strstr(pending[0].body,"chat_close") && strstr(pending[0].body,"chat-cap-two"));
    reply(0,200,"{}"); assert(strstr(pending[0].body,"live_chat") && strstr(pending[0].body,"\"live\""));
    cJSON *ended=frame("chat-cap-live","live",1,1); cJSON_ReplaceItemInObject(ended,"ended",cJSON_CreateTrue()); respond(ended);
    clock_ms += 30000; assert(!tube_chat_due("chat:live:abcdefghijk"));
    tube_cancel(); assert(count==1 && strstr(pending[0].body,"chat-cap-live")); cleanup();
    /* A canceled open can still return a capability: close it at its captured origin. */
    assert(!p->list_catalog("chat:top:abcdefghijk",0,received,NULL));
    assert(!p->set_source("http://127.0.0.1:8766#654321"));
    int previous=calls; respond(frame("chat-cap-late","top",0,1));
    assert(calls==previous+1 && !ok && !tube_chat_due("chat:top:abcdefghijk"));
    assert(count==1 && strstr(pending[0].url,":8765/v1/library") && strstr(pending[0].body,"chat-cap-late"));
    assert(strstr(pending[0].bearer,"abcdefghijkl")); cleanup();
    assert(!p->list_catalog("chat:top:abcdefghijk",0,received,NULL)); reply(0,200,"{\"token\":\"zyxwvutsrqponmlkjihgfedcbzyxwvuts\"}");
    respond(frame("chat-cap-good","top",0,1)); assert(ok); snprintf(stable,sizeof stable,"%s",items[0].id);
    /* Duplicate/invalid response leaves the displayed snapshot intact and stops polling. */
    clock_ms += 2000; assert(!p->list_catalog("chat:top:abcdefghijk",0,received,NULL));
    cJSON *bad=frame("chat-cap-bad","top",0,1); cJSON *list=cJSON_GetObjectItemCaseSensitive(bad,"items");
    cJSON_AddItemToArray(list,cJSON_Duplicate(cJSON_GetArrayItem(list,0),1)); respond(bad);
    assert(!ok && !tube_chat_due("chat:top:abcdefghijk") && *tube_comment_text(stable)); cleanup();
    /* Reopen after a failed update uses a fresh context, never a blind retry loop. */
    assert(!p->list_catalog("chat:top:abcdefghijk",0,received,NULL)); assert(strstr(pending[0].body,"live_chat"));
    bad=frame("chat-cap-replay","top",0,1); cJSON_ReplaceItemInObject(bad,"is_replay",cJSON_CreateTrue()); respond(bad);
    assert(!ok && !tube_chat_due("chat:top:abcdefghijk")); cleanup();
    assert(!p->list_catalog("chat:leaf:bogus",0,received,NULL) && !ok && !count);
    assert(!p->list_catalog("chat:top:abcdefghijk",1,received,NULL) && !ok && !count);
    privacy(); p->shutdown(); cleanup(); unlink(evo_data_path("youtube.json")); rmdir(directory);
    puts("PASS: native chat pairing, direct polls, bounded snapshots, stable identities, retirement, mode/end/failure/source lifecycle and storage privacy");
    return 0;
}

int evo_net_callback_error(void) { return 0; }
