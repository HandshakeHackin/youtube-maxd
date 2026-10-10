/* Shipping native sidebar ownership, requests, rows and settings. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
int main(void) {
    char dir[]="/tmp/tube-player-chat-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t *p=&evo_provider_youtube; assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->list_catalog("section:live",0,items_cb,NULL);
    reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":false}}");
    p->stream_chosen("abcdefghijk",NULL);
    tube_player_chat_tick(1); assert(!pending_count && !tube_player_chat_mode());
    p->list_catalog("settings:player-chat",0,items_cb,NULL);
    tube_player_chat_tick(1);
    assert(pending_count==1 && strstr(pending[0].body,"\"action\":\"live_chat\"") && strstr(pending[0].body,"\"chat_history\":false"));
    reply(0,1,200,"{\"api_version\":1,\"kind\":\"live_chat\",\"id\":\"abcdefghijk\",\"chat_mode\":\"top\",\"is_replay\":false,\"has_more\":false,\"ended\":false,\"poll_after_ms\":1000,\"cursor\":\"sidebar-cursor\",\"items\":[{\"id\":\"message-1\",\"author\":\"Viewer\",\"content\":\"Hello <world> & all\"}]}");
    const tube_player_chat_row_t *rows=NULL;
    assert(tube_player_chat_mode()==1 && tube_player_chat_rows(&rows)==1 && rows);
    assert(!strcmp(rows[0].author,"Viewer") && !strcmp(rows[0].message,"Hello <world> & all"));
    assert(tube_player_chat_preference()==1);
    assert(!tube_player_chat_set_mode(-1) && !tube_player_chat_set_mode(3));
    assert(tube_player_chat_set_mode(2) && tube_player_chat_preference()==2);
    assert(tube_player_chat_mode()==2 && tube_player_chat_rows(&rows)==1 && !pending_count);
    assert(tube_player_chat_set_mode(1) && tube_player_chat_mode()==1 && !pending_count);
    tube_player_chat_tick(1);assert(!pending_count); /* no background/early polling */
    usleep(1100000);tube_player_chat_tick(1);
    assert(pending_count==1 && strstr(pending[0].body,"\"action\":\"chat_poll\""));
    char update[4096];size_t used=(size_t)snprintf(update,sizeof update,"{\"api_version\":1,\"kind\":\"live_chat\",\"id\":\"abcdefghijk\",\"chat_mode\":\"top\",\"is_replay\":false,\"has_more\":false,\"ended\":false,\"poll_after_ms\":1000,\"cursor\":\"sidebar-next\",\"items\":[");
    char long_author[201],long_message[1201];
    for(int i=0;i<100;i++)memcpy(long_author+i*2,"α",2);
    long_author[200]=0;
    for(int i=0;i<600;i++)memcpy(long_message+i*2,"α",2);
    long_message[1200]=0;
    for(int i=0;i<8;i++) {
        char author[32],message[64];snprintf(author,sizeof author,"Viewer %d",i);snprintf(message,sizeof message,"Recent message %d",i);
        used+=(size_t)snprintf(update+used,sizeof update-used,"%s{\"id\":\"message-%d\",\"author\":\"%s\",\"content\":\"%s\"}",i?",":"",i,i==7?long_author:author,i==7?long_message:message);
    }
    assert(used+3<sizeof update);snprintf(update+used,sizeof update-used,"]}");
    reply(0,1,200,update);
    assert(tube_player_chat_rows(&rows)==6 && !strcmp(rows[0].author,"Viewer 2"));
    assert(strlen(rows[5].author)==126 && strlen(rows[5].message)>0 && strlen(rows[5].message)%2==0);
    for(size_t i=0;i<strlen(rows[5].message);i+=2)assert(!memcmp(rows[5].message+i,"α",2));
    tube_player_chat_tick(0);assert(!tube_player_chat_mode() && !tube_player_chat_rows(&rows));
    assert(pending_count==1 && strstr(pending[0].body,"\"action\":\"chat_close\""));
    reply(0,1,200,"{\"success\":true}");
    p->list_catalog("settings:player-chat",0,items_cb,NULL);
    cJSON *saved=read_store();assert(cJSON_GetObjectItem(saved,"player_chat_mode")->valueint==2);cJSON_Delete(saved);
    p->list_catalog("section:news",0,items_cb,NULL);
    tube_player_chat_tick(1);assert(pending_count==1 && strstr(pending[0].body,"\"action\":\"topics\""));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":false}}");
    p->list_catalog("chat:top:abcdefghijk",0,items_cb,NULL);
    reply(0,1,200,"{\"api_version\":1,\"kind\":\"live_chat\",\"id\":\"abcdefghijk\",\"chat_mode\":\"top\",\"is_replay\":false,\"has_more\":false,\"ended\":false,\"poll_after_ms\":1000,\"cursor\":\"reader-cursor\",\"items\":[]}");
    tube_player_chat_tick(1);assert(!pending_count && !tube_player_chat_mode()); /* reader owns context */
    tube_chat_stop();reply(0,1,200,"{\"success\":true}");
    tube_player_chat_tick(1);assert(pending_count==1);
    reply(0,1,503,"{\"error\":{\"code\":\"chat_unavailable\",\"message\":\"No chat\"}}");
    assert(tube_player_chat_mode()==2 && !tube_player_chat_rows(&rows) && strstr(tube_player_chat_status(),"unavailable"));
    tube_player_chat_tick(1);assert(!pending_count); /* terminal failure never spins */
    assert(tube_player_chat_set_mode(2)); /* selecting the same side explicitly retries */
    assert(!tube_player_chat_mode());tube_player_chat_tick(1);assert(pending_count==1);
    tube_player_chat_tick(0);
    reply(0,1,200,"{\"api_version\":1,\"kind\":\"live_chat\",\"id\":\"abcdefghijk\",\"chat_mode\":\"top\",\"is_replay\":false,\"has_more\":false,\"ended\":false,\"poll_after_ms\":1000,\"cursor\":\"late-cursor\",\"items\":[]}");
    assert(!tube_player_chat_mode() && !tube_player_chat_rows(&rows));
    assert(pending_count==1 && strstr(pending[0].body,"late-cursor"));reply(0,1,200,"{\"success\":true}");
    p->shutdown();puts("PASS: player chat setting, request ownership, rows, foreground priority and close");
}
