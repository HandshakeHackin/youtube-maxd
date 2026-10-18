#define main existing_provider_regression
#include "test_native.c"
#undef main
#include "maxd/experience.h"
static void choose(const char *id){evo_provider_youtube.list_catalog(id,0,items_cb,NULL);assert(last_ok);}
static void enter(const char *text,char *next){tube_playlist_prompt_t prompt;assert(tube_take_playlist_prompt(&prompt));assert(!tube_submit_playlist_name(prompt.ticket,text,next,160));}
static void drain(void){while(pending_count)reply(0,0,0,"");}
static evo_stream_choice_t choice;
static void selected(int ok,const evo_stream_choice_t *choices,int count,void *ud){(void)ud;last_ok=ok;if(ok){assert(count==1);choice=choices[0];}}
static void play(const char *id,const char *channel) {
    const evo_provider_t *p=&evo_provider_youtube;assert(!p->resolve(id,selected,NULL));
    char json[2048];snprintf(json,sizeof json,"{\"state\":\"done\",\"result\":{\"id\":\"%s\",\"title\":\"Fixture\",\"channel_id\":\"%s\",\"duration_sec\":100,\"choices\":[{\"url\":\"%s/media/cap/d0.mpd\",\"container\":\"dash\",\"video_codec\":\"avc\",\"audio_codec\":\"aac\"}],\"captions\":[{\"language\":\"en\",\"name\":\"English\",\"url\":\"%s/media/cap/c0.srt\"}]}}",id,channel,p->get_source(),p->get_source());
    poll_job(json);if(pending_count)reply(0,1,200,"1\n00:00:01,000 --> 00:00:03,000\nHello captions\n\n");
    if(last_ok)p->stream_chosen(id,&choice);
}
int main(void) {
    char dir[]="/tmp/maxd-experience-XXXXXX",next[160];assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t *p=&evo_provider_youtube;assert(!p->init());SEPARATE_SETTINGS();
    choose("settings:session:new");enter("Half hour session",next);choose(next);assert(strstr(last_items[1].title,"Time budget"));
    cJSON *root=read_store(),*rules=cJSON_GetObjectItem(root,"maxd_smart_playlists");assert(cJSON_GetArraySize(rules)==1 && cJSON_GetObjectItem(cJSON_GetArrayItem(rules,0),"session_minutes")->valueint==30);cJSON_Delete(root);
    assert(!p->set_source("http://127.0.0.1:8765#123456"));p->search("fixture",0,items_cb,NULL);reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");poll_job("{\"state\":\"done\",\"result\":{\"items\":[]}}");
    play("abcdefghijk","UCabcdefghijklmnopqrstuv");assert(last_ok && tube_caption_selection());
    unsigned owner=tube_caption_selection();assert(tube_maxd_bookmark_add(owner,12.25,"Opening"));assert(!tube_maxd_bookmark_add(owner+1,13,"Stale"));
    assert(tube_maxd_bookmark_count()==1 && tube_maxd_bookmark_seconds(0)==12.25);
    assert(tube_maxd_transcript_begin() && pending_count==1);reply(0,1,200,"WEBVTT\n\n00:01.000 --> 00:03.000\nHello <b>world</b>\n\n00:05.500 --> 00:07.000\nSecond moment\n\n");
    assert(tube_maxd_transcript_state()==1 && tube_maxd_transcript_count()==2 && !strcmp(tube_maxd_transcript_text(0),"Hello world"));
    choose("library:profile:maxd_family_1");assert(!tube_maxd_bookmark_count() && tube_maxd_transcript_state()==-1);
    choose("settings:session:new");enter("Local family session",next);choose(next);
    root=read_store();rules=cJSON_GetObjectItem(root,"maxd_smart_playlists");assert(cJSON_GetObjectItem(cJSON_GetArrayItem(rules,1),"source")->valueint==0);cJSON_Delete(root);
    choose("settings:family");enter("1234",next);choose(next);
    choose("settings:family:channel");enter("UCabcdefghijklmnopqrstuv",next);choose(next);
    choose("settings:family:time");choose("settings:family:enabled");choose("settings:family:lock");
    p->search("blocked",0,items_cb,NULL);assert(!last_ok);
    choose("library:profile:guest");assert(!strcmp(tube_browse_profile(),"maxd_family_1"));
    tube_playlist_prompt_t prompt;assert(tube_take_playlist_prompt(&prompt));assert(tube_submit_playlist_name(prompt.ticket,"9999",next,sizeof next)<0);assert(!tube_submit_playlist_name(prompt.ticket,"1234",next,sizeof next));
    play("bcdefghijkl","UCzyxwvutsrqponmlkjihgfe");assert(!last_ok);
    play("bcdefghijkl","UCabcdefghijklmnopqrstuv");assert(last_ok);
    assert(!tube_maxd_family_tick(100000,0));for(int i=1;i<1800;++i)assert(!tube_maxd_family_tick(100000+i*1000,1));assert(tube_maxd_family_tick(1900000,1));
    choose("library:profile:guest");assert(!strcmp(tube_browse_profile(),"guest"));play("abcdefghijk","UCabcdefghijklmnopqrstuv");assert(tube_maxd_bookmark_count()==1);
    assert(tube_maxd_bookmark_remove(0) && !tube_maxd_bookmark_count());
    p->shutdown();drain();assert(!p->init());choose("library:profile:maxd_family_1");
    p->search("still blocked",0,items_cb,NULL);assert(!last_ok);p->shutdown();drain();
    puts("PASS: session creation, transcript download, bookmark ownership, parent PIN, profile escape gate, channel enforcement, time limit and persistence");
}
