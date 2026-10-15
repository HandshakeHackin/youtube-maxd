/* Real native parser with deterministic transport; no network. */
#define main remote_driver_base_main
#include "test_remote_native.c"
#undef main
static void delivered(int64_t now, int acknowledged, const char *commands) {
    tube_remote_tick(now); cJSON *root = request("poll"); cJSON_Delete(root);
    poll_reply(acknowledged, commands);
}
int main(void) {
    configure(1,"guest",identity,"screen-123"); remote_bind(1000);
    const char *first = "[{\"sequence\":1,\"action\":\"remoteConnected\",\"device_id\":\"phone-a\",\"name\":\"Phone A\"}]";
    delivered(1001,0,first);
    assert(tube_remote_connected_count()==1 && !strcmp(tube_remote_connected_name(0),"Phone A"));
    unsigned revision = tube_remote_revision();
    delivered(1501,0,first);
    assert(tube_remote_revision()==revision && tube_remote_connected_count()==1);
    tube_remote_command event; assert(tube_remote_take(&event));
    assert(tube_remote_finish(event.owner,event.sequence,1));
    delivered(2001,1,"[{\"sequence\":2,\"action\":\"loungeStatus\",\"devices\":[{\"device_id\":\"phone-a\",\"name\":\"Renamed\"},{\"device_id\":\"phone-b\",\"name\":\"Phone B\"}]},{\"sequence\":3,\"action\":\"remoteDisconnected\",\"device_id\":\"phone-a\",\"name\":\"Renamed\"}]");
    assert(tube_remote_connected_count()==1 && !strcmp(tube_remote_connected_name(0),"Phone B"));
    assert(!*tube_remote_connected_name(-1) && !*tube_remote_connected_name(1));
    while(tube_remote_take(&event)) assert(tube_remote_finish(event.owner,event.sequence,1));
    delivered(2501,3,"[{\"sequence\":4,\"action\":\"loungeStatus\",\"devices\":[]}]");
    assert(tube_remote_connected_count()==0);
    assert(tube_remote_take(&event)); assert(tube_remote_finish(event.owner,event.sequence,1));
    /* A malformed batch must expose neither a command nor new connection state. */
    delivered(3001,4,"[{\"sequence\":5,\"action\":\"remoteConnected\",\"device_id\":\"phone-c\",\"name\":\"Phone C\"},{\"sequence\":6,\"action\":\"seekTo\",\"position_sec\":-1}]");
    assert(tube_remote_state()==TUBE_REMOTE_FAILED && tube_remote_connected_count()==0);
    clear_stop(); tube_remote_retry(); remote_bind(4000);
    delivered(4001,0,first); assert(tube_remote_connected_count()==1);
    tube_remote_tick(4501); assert(count==1);
    configure(0,"guest",identity,"screen-123");
    assert(tube_remote_connected_count()==0);
    char *late=poll(0,first); reply(0,200,late); free(late);
    assert(tube_remote_connected_count()==0); clear_stop();
    configure(1,"guest",identity,"screen-123"); remote_bind(6000);
    for (int i=0;i<33;++i) {
        char commands[256];
        snprintf(commands,sizeof commands,"[{\"sequence\":%d,\"action\":\"remoteConnected\",\"device_id\":\"phone-%d\",\"name\":\"Phone\"}]",i+1,i);
        delivered(6001+i*500,i,commands);
        if (i<32) {
            assert(tube_remote_connected_count()==i+1);
            assert(tube_remote_take(&event)); assert(tube_remote_finish(event.owner,event.sequence,1));
        } else assert(tube_remote_state()==TUBE_REMOTE_FAILED && tube_remote_connected_count()==0);
    }
    clear_stop(); tube_remote_shutdown();
    puts("PASS: connected devices, snapshots, disconnect, duplicate poll, malformed batch and retired scope");
    return 0;
}
