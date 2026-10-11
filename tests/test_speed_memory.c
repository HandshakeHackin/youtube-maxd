/* Real provider persistence and source/profile scoping, no network traffic. */
#include "tube_provider.h"
#include "evo_net.h"
#include "evo_data_path.h"
#include "cJSON.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int evo_net_request_async(const char *m,const char *u,const char *b,const char **h,int n,evo_net_cb cb,void *ud) {
    (void)m;(void)u;(void)b;(void)h;(void)n;(void)cb;(void)ud;assert(!"Unexpected network request");return -1;
}
void evo_error_set(const char *fmt,...) { (void)fmt; }
static int ok;
static void items(int success,const evo_provider_item_t *rows,int count,int more,void *ud) {
    (void)rows;(void)more;(void)ud;assert(count<=24);ok=success;
}
static void action(const char *id) {
    assert(evo_provider_youtube.list_catalog(id,0,items,NULL)==0 && ok);
}
static cJSON *read_store(void) {
    FILE *f=fopen(evo_data_path("youtube.json"),"rb");assert(f);
    assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>0 && n<2*1024*1024);rewind(f);
    char *text=calloc(1,(size_t)n+1);assert(text);assert(fread(text,1,n,f)==(size_t)n);assert(!fclose(f));
    cJSON *root=cJSON_Parse(text);free(text);assert(root);return root;
}
static void write_store(cJSON *root) {
    char *text=cJSON_PrintUnformatted(root);assert(text);
    FILE *f=fopen(evo_data_path("youtube.json"),"wb");assert(f);
    assert(fwrite(text,1,strlen(text),f)==strlen(text));assert(!fclose(f));free(text);
}
static void choose(const char *id) { evo_provider_youtube.stream_chosen(id,NULL); }
static void reload(void) { assert(evo_provider_youtube.init()==0); }
int main(void) {
    char dir[]="/tmp/prosperotube-speed-memory-XXXXXX";
    assert(mkdtemp(dir));assert(!setenv("EVO_DATA_DIR_OVERRIDE",dir,1));reload();
    assert(tube_playback_speed_for("abcdefghijk")==100);
    cJSON *root=cJSON_Parse("{\"version\":2,\"records\":["
        "{\"id\":\"abcdefghijk\",\"profile\":\"guest\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\"},"
        "{\"id\":\"12345678901\",\"profile\":\"guest\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\"},"
        "{\"id\":\"ABCDEFGHIJK\",\"profile\":\"guest\",\"channel_id\":\"UCbbbbbbbbbbbbbbbbbbbbbb\"}]}");
    assert(root);write_store(root);cJSON_Delete(root);reload();
    choose("abcdefghijk");assert(tube_remember_playback_speed("abcdefghijk",125));
    assert(tube_playback_speed_for("abcdefghijk")==125 && tube_last_playback_speed()==125);
    choose("12345678901");assert(tube_playback_speed_for("12345678901")==125);
    choose("ABCDEFGHIJK");assert(tube_playback_speed_for("ABCDEFGHIJK")==100);
    reload();choose("abcdefghijk");assert(tube_playback_speed_for("abcdefghijk")==125);
    action("library:profile:fixture");choose("abcdefghijk");
    assert(tube_playback_speed_for("abcdefghijk")==100);
    assert(tube_remember_playback_speed("abcdefghijk",175));
    reload();choose("abcdefghijk");assert(tube_playback_speed_for("abcdefghijk")==175);
    action("library:profile:guest");choose("abcdefghijk");assert(tube_playback_speed_for("abcdefghijk")==125);
    assert(tube_remember_playback_speed("abcdefghijk",100));
    assert(tube_last_playback_speed()==125); // Return to normal preserves the prior selected rate.
    choose("12345678901");assert(tube_playback_speed_for("12345678901")==100);
    action("settings:speed-mode:video");choose("abcdefghijk");
    assert(tube_remember_playback_speed("abcdefghijk",200));
    choose("12345678901");assert(tube_playback_speed_for("12345678901")==100);
    choose("abcdefghijk");assert(tube_playback_speed_for("abcdefghijk")==200);
    reload();choose("abcdefghijk");assert(tube_playback_speed_for("abcdefghijk")==200);
    action("settings:speed-mode:none");choose("abcdefghijk");
    assert(tube_playback_speed_for("abcdefghijk")==100);
    assert(tube_remember_playback_speed("abcdefghijk",150));
    assert(tube_playback_speed_for("abcdefghijk")==150); // Track/speed reopen within this session.
    choose("abcdefghijk");assert(tube_playback_speed_for("abcdefghijk")==100); // New playback.
    reload();choose("abcdefghijk");assert(tube_playback_speed_for("abcdefghijk")==100);
    action("settings:speed:175");choose("ABCDEFGHIJK");assert(tube_playback_speed_for("ABCDEFGHIJK")==175);
    choose("12345678901");assert(tube_playback_speed_for("12345678901")==175);
    /* Library account changes cannot reinterpret the old video's speed policy or update the new account. */
    action("library:profile:fixture"); assert(tube_playback_speed()==175);
    assert(tube_remember_playback_speed("12345678901",225));
    assert(tube_playback_speed()==175 && tube_last_playback_speed()==225);
    choose("ABCDEFGHIJK"); assert(tube_playback_speed_for("ABCDEFGHIJK")==100);
    assert(tube_last_playback_speed()==175); reload();
    action("library:profile:guest"); choose("ABCDEFGHIJK");
    assert(tube_playback_speed_for("ABCDEFGHIJK")==225 && tube_last_playback_speed()==225);
    action("settings:speed:175");
    assert(!tube_remember_playback_speed(NULL,125) && !tube_remember_playback_speed("abcdefghijk",126));
    assert(tube_playback_speed_for(NULL)==100);
    root=read_store();
    assert(cJSON_GetObjectItem(root,"speed_memory_mode")->valueint==0);
    cJSON_DeleteItemFromObject(root,"speed_memory_mode"); cJSON_DeleteItemFromObject(root,"profile_settings");
    cJSON_DeleteItemFromObject(root,"last_speed_percent");
    write_store(root);cJSON_Delete(root);reload(); // Legacy global speed remains global.
    assert(tube_playback_speed_for("ABCDEFGHIJK")==175 && tube_last_playback_speed()==175);
    action("settings:speed-mode:video");
    root=read_store();cJSON *rows=cJSON_GetObjectItem(root,"speed_memories");assert(cJSON_IsArray(rows));
    cJSON *bad=cJSON_Parse("{\"profile\":\"guest\",\"key\":\"12345678901\",\"mode\":2,\"percent\":125.5}");
    cJSON_AddItemToArray(rows,bad);
    bad=cJSON_Parse("{\"profile\":\"../invalid\",\"key\":\"ABCDEFGHIJK\",\"mode\":2,\"percent\":400}");
    cJSON_AddItemToArray(rows,bad);write_store(root);cJSON_Delete(root);reload();
    assert(tube_playback_speed_for("12345678901")==100 && tube_playback_speed_for("ABCDEFGHIJK")==100);
    assert(tube_playback_speed_for("abcdefghijk")==200); // Valid memory survives mode switching/corrupt neighbors.
    root=read_store();cJSON_DeleteItemFromObject(root,"speed_memories");
    rows=cJSON_AddArrayToObject(root,"speed_memories");assert(rows);
    for(int i=0;i<1025;++i) {
        char key[25];snprintf(key,sizeof key,"v%010d",i);
        cJSON *row=cJSON_CreateObject();assert(row);
        cJSON_AddStringToObject(row,"profile","guest");cJSON_AddStringToObject(row,"key",key);
        cJSON_AddNumberToObject(row,"mode",2);cJSON_AddNumberToObject(row,"percent",125);
        cJSON_AddItemToArray(rows,row);
    }
    write_store(root);cJSON_Delete(root);reload();
    assert(tube_playback_speed_for("v0000000000")==125 && tube_playback_speed_for("v0000001024")==100);
    assert(tube_remember_playback_speed("v0000001024",200));
    reload();assert(tube_playback_speed_for("v0000000000")==100 && tube_playback_speed_for("v0000001024")==200);
    root=read_store();assert(cJSON_GetArraySize(cJSON_GetObjectItem(root,"speed_memories"))==1024);cJSON_Delete(root);
    evo_provider_youtube.shutdown();
    puts("PASS: real provider channel/video/session/global speed memory, profile isolation, migration and last-speed retention");
}
