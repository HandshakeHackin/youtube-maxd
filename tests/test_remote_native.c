/* Native driver with real JSON and deterministic transport. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_remote.h"
#include "evo_net.h"
#include "cJSON.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char body[8192], url[544], bearer[160]; evo_net_cb cb; void *ud; } Pending;
static Pending pending[16]; static int count, reject;
static const char *session = "0123456789abcdef0123456789abcdef";
static const char *identity = "01234567-89ab-cdef-0123-456789abcdef";
static const char *started = "{\"api_version\":1,\"session\":\"0123456789abcdef0123456789abcdef\",\"device_id\":\"01234567-89ab-cdef-0123-456789abcdef\",\"screen_id\":\"screen-123\",\"state\":\"listening\"}";
static const char *accepted = "{\"api_version\":1,\"session\":\"0123456789abcdef0123456789abcdef\",\"state\":\"listening\"}";
int evo_net_request_async(const char *method,const char *url,const char *body,const char **headers,int n,evo_net_cb cb,void *ud) {
    assert(!strcmp(method,"POST") && n==2 && count<16 && strlen(body)<8192);
    if (reject) { reject=0; return -1; }
    Pending *p = &pending[count++]; memset(p,0,sizeof *p);
    snprintf(p->body,sizeof p->body,"%s",body); snprintf(p->url,sizeof p->url,"%s",url);
    snprintf(p->bearer,sizeof p->bearer,"%s",headers[1]); p->cb=cb; p->ud=ud; return 0;
}
static void reply(int index,int status,const char *body) {
    assert(index<count); Pending p=pending[index];
    memmove(pending+index,pending+index+1,(size_t)(--count-index)*sizeof *pending);
    p.cb(status==200,status,body,strlen(body)+1,p.ud);
}
static cJSON *request(const char *action) {
    assert(count==1); cJSON *root=cJSON_Parse(pending[0].body); assert(root);
    assert(!strcmp(cJSON_GetObjectItem(root,"action")->valuestring,action));
    assert(!strcmp(pending[0].url,"http://192.168.4.34:8765/v1/remote"));
    assert(!strcmp(pending[0].bearer,"Authorization: Bearer synthetic-token")); return root;
}
static void clear_stop(void) {
    while (count) { cJSON *root=request("stop"); cJSON_Delete(root); reply(0,200,"{\"api_version\":1,\"state\":\"stopped\"}"); }
}
static void configure(int active,const char *selected,const char *id,const char *screen) {
    tube_remote_configure("http://192.168.4.34:8765","synthetic-token",selected,active,id,screen);
}
static void remote_bind(int64_t now) {
    tube_remote_tick(now); cJSON *root=request("start"); cJSON_Delete(root); reply(0,200,started);
    assert(tube_remote_state()==TUBE_REMOTE_LISTENING);
}
static char *poll(int ack,const char *commands) {
    char *body=malloc(strlen(commands)+256); assert(body);
    sprintf(body,"{\"api_version\":1,\"session\":\"%s\",\"state\":\"listening\",\"ack\":%d,\"commands\":%s}",session,ack,commands); return body;
}
static void poll_reply(int ack,const char *commands) { char *body=poll(ack,commands); reply(0,200,body); free(body); }
static void test_commands_and_feedback(void) {
    configure(1,"guest","",""); remote_bind(1000);
    assert(!strcmp(tube_remote_device(),identity) && !strcmp(tube_remote_screen(),"screen-123"));
    tube_remote_pair(); tube_remote_tick(1001); cJSON *root=request("pair"); cJSON_Delete(root);
    reply(0,200,"{\"api_version\":1,\"session\":\"0123456789abcdef0123456789abcdef\",\"state\":\"listening\",\"pairing_code\":\"123456789012\"}");
    assert(!strcmp(tube_remote_code(),"123456789012"));
    tube_remote_tick(1002); root=request("poll"); assert(cJSON_GetObjectItem(root,"ack")->valueint==0); cJSON_Delete(root);
    poll_reply(0,"[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"zyxwvutsrqp\"],\"playlist\":\"RDabcdefghijk\",\"index\":0,\"position_sec\":13.25,\"ctt\":\"opaque\"},{\"sequence\":2,\"action\":\"pause\"}]");
    tube_remote_command event; assert(tube_remote_take(&event) && event.action==TUBE_REMOTE_PLAYLIST && event.count==2 && event.position==13.25);
    assert(!tube_remote_finish(event.owner,2,1));
    tube_remote_command repeated; assert(tube_remote_take(&repeated) && repeated.sequence==1);
    tube_remote_tick(1502); root=request("poll"); assert(cJSON_GetObjectItem(root,"ack")->valueint==0); cJSON_Delete(root);
    assert(tube_remote_finish(event.owner,event.sequence,1));
    /* Poll submitted with ack 0, while the main thread has since applied seq 1. */
    poll_reply(0,"[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[\"abcdefghijk\",\"zyxwvutsrqp\"],\"playlist\":\"RDabcdefghijk\",\"index\":0,\"position_sec\":13.25,\"ctt\":\"opaque\"},{\"sequence\":2,\"action\":\"pause\"},{\"sequence\":3,\"action\":\"seekTo\",\"position_sec\":27.5}]");
    assert(tube_remote_take(&event) && event.sequence==2); assert(tube_remote_finish(event.owner,event.sequence,1));
    assert(tube_remote_take(&event) && event.action==TUBE_REMOTE_SEEK && event.position==27.5); assert(tube_remote_finish(event.owner,event.sequence,1));
    assert(!tube_remote_take(&event));
    tube_remote_player snapshot={0}; snprintf(snapshot.video,sizeof snapshot.video,"abcdefghijk");
    snprintf(snapshot.playlist,sizeof snapshot.playlist,"RDabcdefghijk"); snprintf(snapshot.ctt,sizeof snapshot.ctt,"opaque");
    snprintf(snapshot.language,sizeof snapshot.language,"en"); snprintf(snapshot.vss_id,sizeof snapshot.vss_id,"a.en");
    snapshot.state=2; snapshot.position=27.5; snapshot.duration=300; snapshot.volume=40; snapshot.muted=1; snapshot.next=1;
    tube_remote_report(&snapshot); tube_remote_tick(2002); root=request("update");
    cJSON *p=cJSON_GetObjectItem(root,"player"); assert(cJSON_GetObjectItem(p,"position_sec")->valuedouble==27.5);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(p,"muted"))); assert(cJSON_GetObjectItem(root,"update_id")->valueint==1); cJSON_Delete(root);
    reply(0,200,accepted); tube_remote_tick(2003); root=request("poll"); assert(cJSON_GetObjectItem(root,"ack")->valueint==3); cJSON_Delete(root);
    poll_reply(3,"[]"); tube_remote_shutdown(); clear_stop();
    assert(!*tube_remote_code() && tube_remote_state()==TUBE_REMOTE_OFF);
}
static void test_scope_and_late_start_cleanup(void) {
    configure(1,"guest","",""); tube_remote_tick(3000); cJSON *root=request("start"); cJSON_Delete(root);
    configure(1,"another","",""); tube_remote_tick(3001); assert(count==1);
    reply(0,200,started); assert(tube_remote_state()==TUBE_REMOTE_STARTING && !*tube_remote_code());
    tube_remote_tick(3002); root=request("stop"); cJSON_Delete(root); clear_stop();
    remote_bind(3003); tube_remote_tick(3004); root=request("poll"); cJSON_Delete(root);
    poll_reply(0,"[{\"sequence\":1,\"action\":\"play\"}]"); tube_remote_command old;
    assert(tube_remote_take(&old)); configure(1,"guest",identity,"screen-123"); clear_stop(); remote_bind(4000);
    tube_remote_tick(4001); root=request("poll"); cJSON_Delete(root); poll_reply(0,"[{\"sequence\":1,\"action\":\"pause\"}]");
    assert(!tube_remote_finish(old.owner,old.sequence,1)); tube_remote_command fresh; assert(tube_remote_take(&fresh));
    assert(tube_remote_finish(fresh.owner,fresh.sequence,1)); tube_remote_shutdown(); clear_stop();
}
static void test_atomic_validation(void) {
    const char *bad[] = {
        "[{\"sequence\":1,\"action\":\"play\"},{\"sequence\":3,\"action\":\"pause\"}]",
        "[{\"sequence\":1,\"action\":\"seekTo\",\"position_sec\":-1}]",
        "[{\"sequence\":1,\"action\":\"setVolume\",\"volume\":101}]",
        "[{\"sequence\":1,\"action\":\"dpadCommand\",\"key\":\"EXEC\"}]",
        "[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"file:///bad\"}]",
        "[{\"sequence\":1,\"action\":\"voiceCommand\",\"status\":\"EXEC\"}]",
        "[{\"sequence\":1,\"action\":\"play\"},{\"sequence\":2,\"action\":\"unknown\"}]"};
    for (unsigned i=0;i<sizeof bad/sizeof *bad;++i) {
        configure(1,"guest",identity,"screen-123"); tube_remote_retry(); remote_bind(5000+i*1000);
        tube_remote_tick(5001+i*1000); poll_reply(0,bad[i]); tube_remote_command event;
        assert(tube_remote_state()==TUBE_REMOTE_FAILED && !tube_remote_take(&event)); clear_stop();
    }
    tube_remote_retry(); remote_bind(15000); tube_remote_tick(15001); reply(0,200,"{\"api_version\":1} garbage");
    assert(tube_remote_state()==TUBE_REMOTE_FAILED); clear_stop();
    tube_remote_retry(); remote_bind(16000); tube_remote_tick(16001);
    poll_reply(0,"[{\"sequence\":1,\"action\":\"play\"}]"); tube_remote_command event; assert(tube_remote_take(&event));
    assert(!tube_remote_finish(event.owner,event.sequence,0) && tube_remote_state()==TUBE_REMOTE_FAILED); clear_stop();
    tube_remote_shutdown();
}
static void test_all_commands_and_pending_invalidation(void) {
    configure(1,"guest",identity,"screen-123"); remote_bind(20000); tube_remote_tick(20001);
    poll_reply(0,"["
        "{\"sequence\":1,\"action\":\"updatePlaylist\",\"video_id\":\"\",\"video_ids\":[\"abcdefghijk\"],\"playlist\":\"\",\"index\":0,\"position_sec\":0,\"ctt\":\"\"},"
        "{\"sequence\":2,\"action\":\"next\"},{\"sequence\":3,\"action\":\"previous\"},{\"sequence\":4,\"action\":\"stopVideo\"},"
        "{\"sequence\":5,\"action\":\"getNowPlaying\"},{\"sequence\":6,\"action\":\"setVolume\",\"volume\":75,\"delta\":-5},"
        "{\"sequence\":7,\"action\":\"remoteConnected\",\"device_id\":\"phone\",\"name\":\"日本語 😀\"},"
        "{\"sequence\":8,\"action\":\"remoteDisconnected\",\"device_id\":\"phone\",\"name\":\"Phone\"},"
        "{\"sequence\":9,\"action\":\"loungeStatus\",\"devices\":[{\"device_id\":\"phone\",\"name\":\"Phone\"}]},"
        "{\"sequence\":10,\"action\":\"dpadCommand\",\"key\":\"ENTER\"},"
        "{\"sequence\":11,\"action\":\"voiceCommand\",\"status\":\"INITIATED\"},"
        "{\"sequence\":12,\"action\":\"setSubtitlesTrack\",\"video_id\":\"abcdefghijk\",\"language\":\"pt-BR\"}]");
    const int actions[]={TUBE_REMOTE_QUEUE,TUBE_REMOTE_NEXT,TUBE_REMOTE_PREVIOUS,TUBE_REMOTE_STOP,TUBE_REMOTE_QUERY,TUBE_REMOTE_VOLUME,
        TUBE_REMOTE_CONNECTED,TUBE_REMOTE_DISCONNECTED,TUBE_REMOTE_DEVICES,TUBE_REMOTE_DPAD,TUBE_REMOTE_VOICE,TUBE_REMOTE_CAPTIONS};
    tube_remote_command event;
    for (int i=0;i<12;++i) {
        assert(tube_remote_take(&event) && event.sequence==i+1 && event.action==actions[i]);
        if (i==5) assert(event.has_volume && event.has_delta && event.volume==75 && event.delta==-5);
        if (i==6) assert(!strcmp(event.name,"日本語 😀"));
        if (i==8) assert(event.devices==1);
        assert(tube_remote_finish(event.owner,event.sequence,1));
    }
    tube_remote_tick(20501); assert(count==1); configure(1,"different",identity,"screen-123"); assert(count==2);
    char *late=poll(12,"[{\"sequence\":13,\"action\":\"play\"}]"); reply(0,200,late); free(late);
    assert(!tube_remote_take(&event) && tube_remote_state()==TUBE_REMOTE_STARTING); clear_stop();
    remote_bind(21000); tube_remote_shutdown(); clear_stop();
}
static void test_original_source_cleanup(void) {
    configure(1,"guest",identity,"screen-123"); tube_remote_tick(25000); assert(count==1);
    tube_remote_configure("http://192.168.4.34:9000","second-token","guest",1,identity,"screen-123");
    reply(0,200,started); assert(count==1);
    assert(!strcmp(pending[0].url,"http://192.168.4.34:8765/v1/remote"));
    assert(!strcmp(pending[0].bearer,"Authorization: Bearer synthetic-token"));
    clear_stop(); tube_remote_tick(25001); assert(count==1);
    assert(!strcmp(pending[0].url,"http://192.168.4.34:9000/v1/remote"));
    assert(!strcmp(pending[0].bearer,"Authorization: Bearer second-token"));
    reply(0,200,started); tube_remote_shutdown(); assert(count==1);
    assert(!strcmp(pending[0].url,"http://192.168.4.34:9000/v1/remote"));
    reply(0,200,"{\"api_version\":1,\"state\":\"stopped\"}");
}
static void test_voice_capability(void) {
    configure(1,"guest",identity,"screen-123"); tube_remote_voice_available(1); tube_remote_tick(35000);
    cJSON *root=request("start"); assert(cJSON_IsTrue(cJSON_GetObjectItem(root,"voice_enabled"))); cJSON_Delete(root);
    reply(0,200,started); unsigned owner=tube_remote_current_owner(); assert(owner);
    tube_remote_voice_available(0); assert(!tube_remote_owner_current(owner) && tube_remote_state()==TUBE_REMOTE_STARTING);
    clear_stop(); tube_remote_tick(35001);
    root=request("start"); assert(!cJSON_GetObjectItem(root,"voice_enabled")); cJSON_Delete(root);
    reply(0,200,started); tube_remote_shutdown(); clear_stop();
}
int main(void) { test_commands_and_feedback(); test_scope_and_late_start_cleanup(); test_atomic_validation();
    test_all_commands_and_pending_invalidation(); test_original_source_cleanup(); test_voice_capability();
    configure(1,"guest",identity,"screen-123"); reject=1; tube_remote_tick(30000);
    assert(tube_remote_state()==TUBE_REMOTE_FAILED && count==0);
    tube_remote_voice_available(1); assert(tube_remote_state()==TUBE_REMOTE_FAILED && count==0);
    tube_remote_shutdown();
    puts("Native Lounge scope, command delivery, feedback and shutdown passed"); return 0; }
