/* Shipping provider jobs, real receiver ownership and remote-only fallback. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
#include "tube_remote.h"
#include "tube_phone.h"
static tube_remote_player snapshot_value;
static int snapshot_active;
static int snapshot(void *ctx,tube_remote_player *out) {(void)ctx;*out=snapshot_value;return snapshot_active;}
static int callback(void *ctx,unsigned owner,unsigned selection,const char *id,const char *playlist,int index,tube_phone_next_t *next) {
    (void)ctx;return tube_phone_recommendation(owner,selection,id,playlist,index,next);
}
static void cancel_callback(void *ctx) {(void)ctx;tube_phone_recommendation_cancel();}
static unsigned owner,selection;
static tube_phone_next_t next_result;
#define next_video next_result.video
static const char *lookup_list="";
static int lookup(void) {return tube_phone_recommendation(owner,selection,"abcdefghijk",lookup_list,7,&next_result);}
static void begin_lookup(void) {
    evo_stream_choice_t choice={0};evo_provider_youtube.stream_chosen("abcdefghijk",&choice);
    selection=tube_phone_selection();assert(lookup()==0 && pending_count==1);
    assert(strstr(pending[0].body,"\"remote_origin\":true") && strstr(pending[0].body,"\"require_name\":true"));
}
/* Returned playlist occurrences survive; only a distinct one may repeat the ID. */
static void occurrence_cases(void) {
    static const struct { const char *sent,*next,*context; int result,index; const char *list; } cases[]={
        {"","bcdefghijkl",",\"playback_context\":{\"playlist\":\"PLnext56789\",\"index\":3}",1,3,"PLnext56789"},
        {"PLsame56789","abcdefghijk",",\"playback_context\":{\"playlist\":\"PLsame56789\",\"index\":9}",1,9,"PLsame56789"},
        {"","abcdefghijk",",\"playback_context\":{\"playlist\":\"PLother6789\",\"index\":7}",1,7,"PLother6789"},
        {"PLsame56789","abcdefghijk",",\"playback_context\":{\"playlist\":\"PLsame56789\",\"index\":7}",-1,0,""},
        {"","abcdefghijk","",-1,0,""},
        {"","bcdefghijkl",",\"playback_context\":{\"playlist\":\"PLnext56789\",\"index\":-1}",-1,0,""},
        {"","bcdefghijkl",",\"playback_context\":{\"playlist\":\"bad list!\",\"index\":1}",-1,0,""},
        /* Catalog group keys are never exposed as a phone playlist. */
        {"","bcdefghijkl",",\"playback_context\":{\"channel\":\"UCbbbbbbbbbbbbbbbbbbbbbb\",\"index\":2}",1,0,""},
        {"","abcdefghijk",",\"playback_context\":{\"channel\":\"UCbbbbbbbbbbbbbbbbbbbbbb\",\"index\":2}",-1,0,""},
    };
    for(size_t i=0;i<sizeof cases/sizeof *cases;++i) {
        lookup_list=cases[i].sent;begin_lookup();char body[700];snprintf(body,sizeof body,
            "{\"state\":\"done\",\"result\":{\"api_version\":1,\"video_id\":\"abcdefghijk\",\"next_video_id\":\"%s\",\"next_channel_id\":\"UCbbbbbbbbbbbbbbbbbbbbbb\",\"next_channel_name\":\"Allowed\"%s}}",
            cases[i].next,cases[i].context);
        poll_job(body);assert(lookup()==cases[i].result);
        if(cases[i].result==1)assert(!strcmp(next_video,cases[i].next) && !strcmp(next_result.playlist,cases[i].list) && next_result.index==cases[i].index);
        else assert(!*next_video && !*next_result.playlist);
        assert(!pending_count);tube_phone_recommendation_cancel();
    }
    lookup_list="";
}
static const char *discovery="{\"state\":\"done\",\"result\":{\"api_version\":1,\"video_id\":\"abcdefghijk\",\"next_video_id\":null,\"fallback_context\":{\"group\":\"12345678-1234-1234-1234-123456789abc\",\"index\":0},\"fallback_count\":2}}";
static const char *window="{\"state\":\"done\",\"result\":{\"api_version\":1,\"playlist\":\"group:12345678-1234-1234-1234-123456789abc\",\"video_id\":\"bcdefghijkl\",\"index\":1,\"anchor_video_id\":\"abcdefghijk\",\"anchor_index\":0,\"window_start\":0,\"video_ids\":[\"abcdefghijk\",\"bcdefghijkl\",\"cdefghijklm\"],\"video_indices\":[0,1,2],\"video_channels\":[\"UCbbbbbbbbbbbbbbbbbbbbbb\",\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"UCbbbbbbbbbbbbbbbbbbbbbb\"],\"video_channel_names\":[\"Anchor\",\"Blocked\",\"Allowed\"],\"playlist_size\":3}}";
int main(void) {
    char dir[]="/tmp/tube-phone-next-XXXXXX";assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    cJSON *root=cJSON_Parse("{\"version\":2,\"source\":\"http://127.0.0.1:8765\",\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\",\"remote_enabled\":true,\"remote_device\":\"01234567-89ab-cdef-0123-456789abcdef\",\"remote_screen\":\"screen-123\",\"records\":[],\"blocked_channels\":[{\"profile\":\"guest\",\"channel\":\"UCaaaaaaaaaaaaaaaaaaaaaa\"},{\"profile\":\"guest\",\"channel\":\"\",\"name\":\"Blocked Name\"}]}");
    write_store(root);cJSON_Delete(root);const evo_provider_t *p=&evo_provider_youtube;assert(!p->init());
    tube_phone_sync_scope();tube_remote_tick(1000000000000LL);assert(pending_count==1);
    reply(0,1,200,"{\"api_version\":1,\"session\":\"0123456789abcdef0123456789abcdef\",\"device_id\":\"01234567-89ab-cdef-0123-456789abcdef\",\"screen_id\":\"screen-123\",\"state\":\"listening\"}");
    tube_remote_tick(1000000000001LL);reply(0,1,200,"{\"api_version\":1,\"session\":\"0123456789abcdef0123456789abcdef\",\"state\":\"listening\",\"ack\":0,\"commands\":[]}");
    owner=tube_remote_current_owner();assert(owner);
    const char *channels[]={"UCbbbbbbbbbbbbbbbbbbbbbb","UCaaaaaaaaaaaaaaaaaaaaaa","UCbbbbbbbbbbbbbbbbbbbbbb",""};
    const char *names[]={"Allowed","Allowed","bLoCkEd NaMe","Allowed"};
    for(int i=0;i<4;++i) {
        begin_lookup();char body[600];snprintf(body,sizeof body,"{\"state\":\"done\",\"result\":{\"api_version\":1,\"video_id\":\"abcdefghijk\",\"next_video_id\":\"bcdefghijkl\",\"next_channel_id\":\"%s\",\"next_channel_name\":\"%s\"}}",channels[i],names[i]);
        poll_job(body);assert(lookup()==(i?-1:1));if(!i)assert(!strcmp(next_video,"bcdefghijkl"));
        assert(!pending_count);tube_phone_recommendation_cancel();
    }
    occurrence_cases();
    /* Blocked primary suggestion advances inside its verified fallback group. */
    begin_lookup();poll_job(discovery);assert(lookup()==0 && pending_count==1);
    assert(strstr(pending[0].body,"\"action\":\"group_window\"") && strstr(pending[0].body,"\"target\":1"));
    poll_job(window);assert(lookup()==1 && !strcmp(next_video,"cdefghijklm"));
    evo_provider_item_t local;assert(!tube_take_next(&local));
    tube_phone_recommendation_cancel();
    /* A blocked window resumes strictly after its last verified occurrence. */
    begin_lookup();poll_job(discovery);assert(lookup()==0);
    root=cJSON_Parse(window);cJSON *result=cJSON_GetObjectItem(root,"result");
    const char *arrays[]={"video_ids","video_indices","video_channels","video_channel_names"};
    for(int i=0;i<4;++i)cJSON_DeleteItemFromArray(cJSON_GetObjectItem(result,arrays[i]),2);
    char *body=cJSON_PrintUnformatted(root);poll_job(body);free(body);cJSON_Delete(root);
    assert(lookup()==0 && strstr(pending[0].body,"\"target\":2"));
    root=cJSON_Parse(window);result=cJSON_GetObjectItem(root,"result");
    cJSON_ReplaceItemInObject(result,"index",cJSON_CreateNumber(2));
    cJSON_ReplaceItemInObject(result,"video_id",cJSON_CreateString("cdefghijklm"));
    body=cJSON_PrintUnformatted(root);poll_job(body);free(body);cJSON_Delete(root);
    assert(lookup()==1 && !strcmp(next_video,"cdefghijklm"));tube_phone_recommendation_cancel();
    /* Corrupt anchors, positions, identity arrays and group bounds fail closed. */
    for(int i=0;i<5;++i) {
        begin_lookup();poll_job(discovery);assert(lookup()==0);
        root=cJSON_Parse(window);cJSON *result=cJSON_GetObjectItem(root,"result");
        if(i==0)cJSON_ReplaceItemInObject(result,"anchor_index",cJSON_CreateNumber(1));
        if(i==1)cJSON_ReplaceItemInObject(result,"playlist_size",cJSON_CreateNumber(4));
        if(i==2)cJSON_ReplaceItemInArray(cJSON_GetObjectItem(result,"video_indices"),2,cJSON_CreateNumber(1));
        if(i==3)cJSON_DeleteItemFromObject(result,"video_channel_names");
        if(i==4)cJSON_ReplaceItemInObject(result,"video_id",cJSON_CreateString("12345678901"));
        char *body=cJSON_PrintUnformatted(root);poll_job(body);free(body);cJSON_Delete(root);
        assert(lookup()==-1 && !*next_video && !pending_count);tube_phone_recommendation_cancel();
    }
    /* Cancellation and new playback reject late authorization failures before token mutation. */
    for(int i=0;i<2;++i) {
        begin_lookup();
        if(i) {evo_stream_choice_t choice={0};p->stream_chosen("bcdefghijkl",&choice);}
        else tube_phone_recommendation_cancel();
        reply(0,0,401,"{\"error\":{\"message\":\"retired\"}}");
        root=read_store();assert(!strcmp(cJSON_GetObjectItem(root,"token")->valuestring,"abcdefghijklmnopqrstuvwxabcdefghijkl"));cJSON_Delete(root);
    }
    /* Cancelling provider work must retire ready results as well as HTTP jobs. */
    begin_lookup();poll_job(discovery);tube_cancel();assert(lookup()==-1 && !pending_count);
    /* Connect real driver -> phone adapter -> provider jobs -> next launch ticket. */
    tube_phone_recommendation_cancel();tube_remote_tick(1000000001000LL);
    reply(0,1,200,"{\"api_version\":1,\"session\":\"0123456789abcdef0123456789abcdef\",\"state\":\"listening\",\"ack\":0,\"commands\":[{\"sequence\":1,\"action\":\"setPlaylist\",\"video_id\":\"abcdefghijk\",\"video_ids\":[],\"playlist\":\"\",\"index\":7,\"position_sec\":0,\"ctt\":\"\"}]}");
    tube_phone_controls controls={0};controls.snapshot=snapshot;controls.recommendation=callback;controls.recommendation_cancel=cancel_callback;
    tube_phone_tick_at(&controls,1000);char id[12];double position;
    unsigned ticket=tube_phone_take(id,&position);assert(ticket && !strcmp(id,"abcdefghijk"));
    evo_stream_choice_t choice={0};p->stream_chosen(id,&choice);
    assert(tube_phone_launch_bind(ticket,tube_phone_selection()) && tube_phone_launch_finish(ticket,1));
    snapshot_active=1;strcpy(snapshot_value.video,id);snapshot_value.selection=tube_phone_selection();
    snapshot_value.state=0;snapshot_value.position=snapshot_value.duration=100;
    assert(tube_toggle_queue("12345678901")==1);
    root=read_store();char *before=cJSON_PrintUnformatted(cJSON_GetObjectItem(root,"records"));cJSON_Delete(root);
    tube_phone_tick_at(&controls,1000);assert(tube_phone_finished_at(1000));
    snapshot_active=0;tube_phone_tick_at(&controls,1001);assert(tube_phone_retry_waiting());
    tube_remote_tick(1000000001001LL);assert(pending_count==2);
    reply(1,1,200,"{\"api_version\":1,\"session\":\"0123456789abcdef0123456789abcdef\",\"state\":\"listening\"}");
    poll_job(discovery);tube_phone_tick_at(&controls,1002);poll_job(window);
    tube_phone_tick_at(&controls,1003);ticket=tube_phone_take(id,&position);
    assert(ticket && !strcmp(id,"cdefghijklm") && position==0 && !tube_phone_retry_waiting());
    root=read_store();char *after=cJSON_PrintUnformatted(cJSON_GetObjectItem(root,"records"));cJSON_Delete(root);
    assert(!strcmp(before,after));free(before);free(after);tube_phone_clear();
    p->shutdown();while(pending_count)reply(0,1,200,"{\"api_version\":1,\"state\":\"stopped\"}");
    unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: remote next metadata, playlist occurrences, verified fallback groups, block policy and retired requests");
}
