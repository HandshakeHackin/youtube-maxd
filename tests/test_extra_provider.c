/* Player extras in the provider: per-profile storage, settings screens, the save hook,
 * resolve facts, Shorts rules, speed lists and the section-playlist switch. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
#include "tube_player_extra.h"
#include <sys/stat.h>
static const evo_provider_t *p;
static evo_stream_choice_t choice;
static const char *A="abcdefghijk",*B="bcdefghijkl";
static void show(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(last_ok && !pending_count);}
static void refused(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(!last_ok && !pending_count);}
static int find(const char *id) {for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].id,id))return i;return -1;}
static cJSON *profile_row(cJSON *store,const char *profile) {
    cJSON *row;
    cJSON_ArrayForEach(row,cJSON_GetObjectItem(store,"profile_settings"))
        if(!strcmp(cJSON_GetObjectItem(row,"profile")->valuestring,profile))return row;
    return NULL;
}
static int saved(const char *profile,const char *key) {
    cJSON *store=read_store(),*value=cJSON_GetObjectItem(profile_row(store,profile),key);
    int result=cJSON_IsNumber(value)?value->valueint:cJSON_IsBool(value)?cJSON_IsTrue(value):-99;
    cJSON_Delete(store);return result;
}
static void selected(int ok,const evo_stream_choice_t *choices,int count,void *ud) {(void)ud;assert(ok && count==1);choice=choices[0];}
/* Resolve and start a video whose result carries the given extra fields. */
static void resolve(const char *id,int duration,const char *facts) {
    char body[4096];
    assert(!p->resolve(id,selected,NULL) && pending_count==1 && !strstr(pending[0].url,"/v1/health"));
    snprintf(body,sizeof body,"{\"state\":\"done\",\"result\":{\"id\":\"%s\",\"duration_sec\":%d,\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/cap/d0.mpd\","
        "\"container\":\"dash\",\"video_codec\":\"avc\",\"audio_codec\":\"aac\"}]%s}}",id,duration,facts);
    poll_job(body);assert(!pending_count);
}
static void play(const char *id,int duration,const char *facts) {resolve(id,duration,facts);p->stream_chosen(id,&choice);}
static int64_t resume_of(const char *id) {
    p->list_catalog("history",0,items_cb,NULL);assert(last_ok);
    for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].id,id))return last_items[i].resume_pos_sec;
    return -1;
}
static const char *facts=",\"loudness_db\":6.0,\"is_short\":true,\"chapters\":[{\"title\":\"Intro\",\"start\":0,\"end\":10},{\"title\":\"Main part\",\"start\":10,\"end\":50}],"
    "\"storyboards\":[{\"width\":160,\"height\":90,\"columns\":5,\"rows\":5,\"count\":120,\"sheets\":5,\"interval_ms\":1000,\"url\":\"http://127.0.0.1:8765/media/cap/sb1-$M.jpg\"}]";
int main(void) {
    char dir[]="/tmp/tube-extra-provider-XXXXXX",id[96],label[96];
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    SEPARATE_SETTINGS();
    /* Defaults reproduce the player without the module. */
    assert(tube_extra_volume()==100 && !tube_extra_subtitle_style_index() && tube_extra_subtitle_scale()==100 && tube_extra_speed_list()==1);
    assert(!tube_extra_loop_shorts() && tube_extra_remember_short() && tube_extra_exit_mode()==TUBE_EXIT_MODE_ASK);
    /* YouTube preferences lead to three screens that hold every group once. */
    int screens=0,groups=0;
    for(int page=0;page<3;++page) {
        p->list_catalog("settings",page,items_cb,NULL);assert(last_ok);
        screens+=(find("settings:extra:captions")>=0)+(find("settings:extra:audio")>=0)+(find("settings:extra:player")>=0);
    }
    assert(screens==3);
    show("settings:extra:captions");assert(last_count==3 && !strcmp(last_items[0].id,"settings:subtitle-style") && !strcmp(last_items[0].title,"Subtitle style: Default"));
    assert(!strcmp(last_items[1].title,"Subtitle size: 1.0x") && !strcmp(last_items[2].title,"Subtitle position: Default"));groups+=last_count;
    show("settings:extra:audio");assert(last_count==5 && !strcmp(last_items[0].title,"Player volume: 100%") && !strcmp(last_items[1].title,"Mute: Off"));groups+=last_count;
    show("settings:extra:player");assert(last_count==24 && last_more && !strcmp(last_items[11].id,"settings:player-suggestions"));groups+=last_count;
    assert(!strcmp(last_items[8].title,"Speed list: Long speed list") && !strcmp(last_items[10].title,"Remember position of short videos: On"));
    assert(!strcmp(last_items[15].title,"Screen dimming: Never") && !strcmp(last_items[19].title,"Button: Like: On"));
    /* The player buttons continue on the next page, before the section switch. */
    p->list_catalog("settings:extra:player",1,items_cb,NULL);
    assert(last_ok && !last_more && !strcmp(last_items[last_count-1].id,"settings:use-section-playlist"));groups+=last_count-1;
    assert(groups==TUBE_EXTRA_GROUPS && !strcmp(last_items[last_count-2].title,"Button: Mirror video: On") && find("settings:button-share")>=0);
    /* A group lists its choices with the selection mark; a choice acts in place and is saved. */
    show("settings:subtitle-style");assert(last_count==7 && !strcmp(last_items[0].subtitle,"Selected") && strcmp(last_items[4].subtitle,"Selected"));
    show("settings:subtitle-style:4");
    assert(last_count==7 && !strcmp(last_items[4].subtitle,"Selected") && !strcmp(last_items[4].id,"settings:subtitle-style:4") && tube_extra_subtitle_style_index()==4);
    assert(saved("guest","subtitle_style")==4);
    for(int group=0;group<TUBE_EXTRA_GROUPS;++group) {
        const int count=tube_extra_group_count(group),last=count-1;
        assert(tube_extra_group_find(tube_extra_group_id(group))==group);
        show(tube_extra_group_id(group));assert(last_count==(count>24?24:count) && last_more==(count>24));
        snprintf(id,sizeof id,"%s:%d",tube_extra_group_id(group),last);
        p->list_catalog(tube_extra_group_id(group),last/24,items_cb,NULL);assert(last_ok && find(id)==last%24);
        tube_extra_group_label(group,last,label,sizeof label);assert(!strcmp(last_items[last%24].title,label));
        show(id);assert(tube_extra_group_selected(group)==last);
        p->list_catalog(id,last/24,items_cb,NULL);assert(last_ok && !strcmp(last_items[last%24].subtitle,"Selected")); /* a later page repeats nothing */
        snprintf(id,sizeof id,"%s:%d",tube_extra_group_id(group),count);refused(id);
        snprintf(id,sizeof id,"%s:-1",tube_extra_group_id(group));refused(id);
        snprintf(id,sizeof id,"%s:0x",tube_extra_group_id(group));refused(id);
        snprintf(id,sizeof id,"%s:",tube_extra_group_id(group));refused(id);
        assert(tube_extra_group_selected(group)==last);
    }
    assert(tube_extra_volume()==300 && tube_extra_muted() && tube_extra_audio_delay_ms()==2000 && tube_extra_audio_delay_enabled());
    assert(tube_extra_keys_ud()==TUBE_KEYS_SUGGESTIONS && tube_extra_seek_preview()==3 && tube_extra_speed_list()==2 && tube_extra_loop_shorts());
    assert(saved("guest","volume")==300 && saved("guest","keys_ud")==TUBE_KEYS_SUGGESTIONS && saved("guest","subtitle_position")==1000 && saved("guest","muted")==-99);
    show("settings:player-mute:0");assert(!tube_extra_muted());
    /* Profiles are isolated: a new one starts at the defaults and keeps its own values. */
    show("library:profile:alice");
    assert(tube_extra_volume()==100 && !tube_extra_subtitle_style_index() && tube_extra_speed_list()==1 && !tube_extra_loop_shorts());
    show("settings:player-volume:10");assert(tube_extra_volume()==50 && saved("alice","volume")==50 && saved("guest","volume")==300);
    /* A change made in the player reaches the store through the save hook. */
    assert(tube_extra_set_clock(1) && saved("alice","clock")==1 && saved("guest","clock")==1);
    assert(tube_extra_set_subtitle_scale(150) && saved("alice","subtitle_scale")==150 && saved("guest","subtitle_scale")==200);
    assert(!tube_extra_set_subtitle_scale(155) && tube_extra_subtitle_scale()==150 && saved("alice","subtitle_scale")==150);
    assert(tube_extra_volume_step(1)==55 && saved("alice","volume")==55);
    show("library:profile:guest");assert(tube_extra_volume()==300 && tube_extra_subtitle_scale()==200 && tube_extra_speed_list()==2);
    p->shutdown();assert(!p->init());
    assert(tube_extra_volume()==300 && tube_extra_subtitle_style_index()==6 && tube_extra_exit_mode()==TUBE_EXIT_MODE_SINGLE && !tube_extra_muted());
    show("library:profile:alice");assert(tube_extra_volume()==55 && tube_extra_clock() && tube_extra_subtitle_scale()==150);
    if(geteuid()) { /* a save that fails is reported to the player and leaves the file alone */
        assert(!chmod(dir,0500));
        assert(!tube_extra_set_volume(60) && saved("alice","volume")==55);
        assert(!chmod(dir,0700) && tube_extra_set_volume(55));
    }
    /* Corrupt values fall back field by field; a file without the fields reads as defaults. */
    p->shutdown();cJSON *store=read_store(),*row=profile_row(store,"alice");
    cJSON_ReplaceItemInObject(row,"volume",cJSON_CreateNumber(57));cJSON_ReplaceItemInObject(row,"subtitle_scale",cJSON_CreateString("150"));
    cJSON_ReplaceItemInObject(row,"seek_preview",cJSON_CreateNumber(9));cJSON_ReplaceItemInObject(row,"audio_delay_ms",cJSON_CreateNumber(99999));
    cJSON_ReplaceItemInObject(row,"exit_mode",cJSON_CreateNumber(1.5));cJSON_ReplaceItemInObject(row,"keys_lr",cJSON_CreateNumber(-1));
    row=profile_row(store,"guest");
    static const char *const keys[]={"subtitle_style","subtitle_scale","subtitle_position","volume","auto_volume","audio_delay_ms","audio_delay_enabled","clock",
        "quality_info","quality_bitrate","clock_12h","exit_mode","keys_lr","keys_ud","seek_preview","speed_list","loop_shorts","remember_short"};
    for(unsigned i=0;i<sizeof keys/sizeof *keys;++i){assert(cJSON_GetObjectItem(row,keys[i]));cJSON_DeleteItemFromObject(row,keys[i]);}
    cJSON_DeleteItemFromObject(row,"section_playlist");
    write_store(store);cJSON_Delete(store);assert(!p->init());
    assert(tube_extra_volume()==100 && tube_extra_subtitle_scale()==100 && !tube_extra_seek_preview() && !tube_extra_audio_delay_ms());
    assert(tube_extra_exit_mode()==TUBE_EXIT_MODE_ASK && tube_extra_keys_lr()==TUBE_KEYS_DEFAULT && tube_extra_clock()); /* the valid neighbour stays */
    show("library:profile:guest");
    assert(tube_extra_volume()==100 && !tube_extra_subtitle_style_index() && tube_extra_speed_list()==1 && tube_extra_remember_short() && !tube_extra_loop_shorts());
    p->list_catalog("settings:extra:player",1,items_cb,NULL);assert(last_ok && !strcmp(last_items[last_count-1].title,"Use current section as playlist: on"));
    /* A store from before per-profile rows gives every profile the defaults. */
    p->shutdown();store=read_store();cJSON_DeleteItemFromObject(store,"profile_settings");write_store(store);cJSON_Delete(store);
    assert(!p->init());assert(tube_extra_volume()==100 && tube_extra_speed_list()==1);
    show("library:profile:alice");assert(tube_extra_volume()==100 && !tube_extra_clock());
    show("library:profile:guest");
    /* Resolve facts reach the player when that video starts and leave when it stops. */
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->list_catalog("library:home",0,items_cb,NULL);reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":false}}");
    show("settings:auto-volume:1");assert(tube_extra_gain_milli()==1000);
    resolve(A,30,facts);
    assert(!tube_extra_is_short() && !tube_extra_chapter_count() && !tube_extra_storyboard_count()); /* resolved, not started */
    p->stream_chosen(A,&choice);
    assert(tube_extra_is_short() && tube_extra_chapter_count()==2 && !strcmp(tube_extra_chapter_title_at(12),"Main part") && tube_extra_storyboard_count()==1);
    assert(tube_extra_gain_milli()>0 && tube_extra_gain_milli()<1000 && tube_chapter_target(2,1)==10);
    p->report_progress(A,5,30,EVO_PROVIDER_PLAY_STOP);
    assert(!tube_extra_is_short() && !tube_extra_chapter_count() && !tube_extra_storyboard_count() && tube_extra_gain_milli()==1000);
    p->stream_chosen(A,&choice);assert(tube_extra_is_short() && tube_extra_chapter_count()==2); /* the same resolve starts again */
    play(B,30,"");assert(!tube_extra_is_short() && !tube_extra_chapter_count() && tube_extra_gain_milli()==1000);
    play(A,30,",\"loudness_db\":null,\"is_short\":\"yes\",\"chapters\":{\"title\":\"x\"},\"storyboards\":[{\"width\":0}]");
    assert(!tube_extra_is_short() && !tube_extra_chapter_count() && !tube_extra_storyboard_count() && tube_extra_gain_milli()==1000);
    play(A,30,facts);show("library:profile:alice");assert(!tube_extra_is_short() && !tube_extra_chapter_count()); /* retired with the profile */
    show("library:profile:guest");
    play(A,30,facts);assert(!p->set_source("http://127.0.0.1:8765"));assert(!tube_extra_is_short() && !tube_extra_chapter_count());
    /* Loop Shorts: a Short repeats at its end; other videos and the switched-off rule do not. */
    play(A,30,facts);assert(!tube_finish_drained_and_next(30) && tube_take_completion_action()==TUBE_COMPLETION_PAUSE);
    show("settings:loop-shorts:1");
    play(A,30,facts);assert(tube_extra_loop_current() && !tube_finish_drained_and_next(30) && tube_take_completion_action()==TUBE_COMPLETION_REPEAT);
    play(B,30,"");assert(!tube_extra_loop_current() && !tube_finish_drained_and_next(30) && tube_take_completion_action()==TUBE_COMPLETION_PAUSE);
    show("settings:loop-shorts:0");
    /* Remember position of short videos: off restarts a video under six minutes only. */
    play(A,100,"");p->report_progress(A,40,100,EVO_PROVIDER_PLAY_UPDATE);assert(resume_of(A)==40);
    show("settings:remember-short-position:0");
    play(A,100,"");p->report_progress(A,41,100,EVO_PROVIDER_PLAY_UPDATE);assert(resume_of(A)==0);
    p->report_progress(A,42,100,EVO_PROVIDER_PLAY_STOP);assert(resume_of(A)==0);
    play(B,1000,"");p->report_progress(B,43,1000,EVO_PROVIDER_PLAY_UPDATE);assert(resume_of(B)==43);
    play(B,0,"");p->report_progress(B,44,0,EVO_PROVIDER_PLAY_UPDATE);assert(resume_of(B)==44); /* an unknown duration keeps its place */
    show("library:profile:alice");play(A,100,"");p->report_progress(A,45,100,EVO_PROVIDER_PLAY_UPDATE);
    while(pending_count)reply(0,0,500,"{}"); /* the account profile's history report */
    assert(resume_of(A)==45); /* alice remembers */
    show("library:profile:guest");show("settings:remember-short-position:1");
    play(A,100,"");p->report_progress(A,46,100,EVO_PROVIDER_PLAY_UPDATE);assert(resume_of(A)==46);
    /* Speeds of the extra-long list are accepted, listed and remembered. */
    assert(!tube_set_playback_speed(126) && tube_playback_speed()==100);
    show("settings:speed");assert(last_count==24 && last_more && !strcmp(last_items[3].id,"settings:speed:80"));
    show("settings:speed-list:0");show("settings:speed");assert(last_count==16 && !last_more && !strcmp(last_items[3].id,"settings:speed:100"));
    show("settings:speed-list:2");assert(tube_extra_speed_count()==76 && tube_extra_speed_valid(155));
    show("settings:speed");assert(last_count==24 && last_more && !strcmp(last_items[3].id,"settings:speed:40"));
    p->list_catalog("settings:speed",3,items_cb,NULL);assert(last_ok && last_count==4 && !last_more && !strcmp(last_items[3].id,"settings:speed:400"));
    show("settings:speed:155");assert(tube_playback_speed()==155);
    play(A,100,"");assert(tube_remember_playback_speed(A,265) && tube_playback_speed_for(A)==265);
    p->shutdown();assert(!p->init());assert(tube_playback_speed()==265 && tube_last_playback_speed()==265);
    show("settings:speed-list:1");assert(tube_playback_speed()==265); /* a saved speed outlives its list */
    p->shutdown();assert(!p->init());assert(tube_playback_speed()==265);
    assert(tube_set_playback_speed(100));
    /* Use current section as playlist: off keeps YouTube playlists and drops section lists. */
    p->list_catalog("library:suggestions:abcdefghijk",0,items_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"bcdefghijkl\",\"playback_context\":{\"group\":\"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\",\"index\":2}}],\"has_more\":false}}");
    unsigned scope=last_items[0].playback_scope;char list[101];int index=-1;assert(scope);
    assert(tube_select_playlist_context(B,"group:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",2,scope));
    play(B,100,"");assert(tube_playback_playlist_context(list,sizeof list,&index)==1 && index==2);
    show("settings:use-section-playlist");assert(saved("guest","section_playlist")==0);
    p->list_catalog("settings:extra:player",1,items_cb,NULL);assert(last_ok && !strcmp(last_items[last_count-1].title,"Use current section as playlist: off"));
    p->list_catalog("library:suggestions:abcdefghijk",0,items_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"bcdefghijkl\",\"playback_context\":{\"group\":\"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\",\"index\":2}}],\"has_more\":false}}");
    scope=last_items[0].playback_scope;
    assert(tube_select_playlist_context(B,"group:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",2,scope));
    play(B,100,"");assert(!tube_playback_playlist_context(list,sizeof list,&index));
    assert(tube_select_playlist_context(B,"channel:UCaaaaaaaaaaaaaaaaaaaaaa",3,scope));
    play(B,100,"");assert(!tube_playback_playlist_context(list,sizeof list,&index));
    assert(tube_select_playlist_context(B,"PLabcdefghijk",4,scope));
    play(B,100,"");assert(tube_playback_playlist_context(list,sizeof list,&index)==1 && index==4 && !strcmp(list,"PLabcdefghijk"));
    show("library:profile:alice");assert(saved("alice","section_playlist")==1);
    show("library:profile:guest");show("settings:use-section-playlist");assert(saved("guest","section_playlist")==1);
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: player extras per profile, settings screens, save hook, legacy and corrupt stores, resolve facts, Shorts rules, speed lists and section playlists");
    return 0;
}
