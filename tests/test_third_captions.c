/* "Unlock more subtitles" asks a companion that offers them for machine translations and
 * marks them in the caption track data; "Remember subtitles per channel" keeps captions on
 * or off for each channel, per profile. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *p;
static const char *A="abcdefghijk",*B="bcdefghijkl",*C="cdefghijklm";
static const char *third="{\"api_version\":1,\"features\":[\"search\",\"library\",\"translated_captions\"]}";
static const char *older="{\"api_version\":1,\"features\":[\"search\",\"library\"]}";
static const char *cue="1\n00:00:00,000 --> 00:00:02,000\nCaption cue\n\n";
static void show(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(last_ok && !pending_count);}
static int find(const char *title) {for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].title,title))return i;return -1;}
static void pair(void) {assert(pending_count==1 && strstr(pending[0].url,"/v1/pair"));reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");}
static void health(const char *body) {
    assert(pending_count==1 && !strcmp(pending[0].method,"GET") && strstr(pending[0].url,"/v1/health"));reply(0,1,200,body);
}
static int direct(void) {return pending_count==1 && !strcmp(pending[0].method,"POST") && strstr(pending[0].url,"/v1/jobs")!=NULL;}
/* The translated_captions field of the pending resolve: true, false or <absent>. */
static const char *asked(void) {
    static char value[16];
    cJSON *job=cJSON_Parse(pending[0].body),*item=cJSON_GetObjectItem(cJSON_GetObjectItem(job,"playback"),"translated_captions");
    assert(job && !cJSON_GetObjectItem(job,"translated_captions"));
    snprintf(value,sizeof value,"%s",cJSON_IsTrue(item)?"true":cJSON_IsFalse(item)?"false":"<absent>");
    cJSON_Delete(job);return value;
}
static void selected(int ok,const evo_stream_choice_t *choices,int count,void *ud) {(void)ud;(void)choices;assert(ok && count==1);++resolve_calls;}
/* Finish the pending resolve of a video and start it; 1 = the provider fetched its caption track first. */
static int started(const char *video) {
    char body[1024];evo_stream_choice_t choice={0};int fetched=0;const char *host=p->get_source();
    snprintf(body,sizeof body,"{\"state\":\"done\",\"result\":{\"id\":\"%s\",\"duration_sec\":100,\"choices\":[{\"url\":\"%s/media/cap/d0.mpd\","
        "\"container\":\"dash\",\"video_codec\":\"avc\",\"audio_codec\":\"aac\"}],\"captions\":["
        "{\"language\":\"en\",\"name\":\"English\",\"url\":\"%s/media/cap/c0.srt\",\"automatic\":false,\"translated\":false},"
        "{\"language\":\"fr-en\",\"name\":\"French\",\"url\":\"%s/media/cap/c1.srt\",\"automatic\":true,\"translated\":true},"
        "{\"language\":\"de\",\"name\":\"German\",\"url\":\"%s/media/cap/c2.srt\",\"automatic\":true}]}}",video,host,host,host,host);
    int calls=resolve_calls;poll_job(body);
    if(pending_count) { /* the profile's caption language, fetched before playback starts */
        assert(pending_count==1 && !strcmp(pending[0].method,"GET") && strstr(pending[0].url,"/media/cap/c0.srt"));reply(0,1,200,cue);fetched=1;
    }
    assert(resolve_calls==calls+1 && !pending_count);
    snprintf(choice.url,sizeof choice.url,"%s/media/cap/d0.mpd",host);p->stream_chosen(video,&choice);
    return fetched;
}
static void resolve(const char *video) {assert(!p->resolve(video,selected,NULL));}
static int play(const char *video) {resolve(video);assert(direct());return started(video);}
static int memories(const char *profile,const char *channel,int enabled) {
    cJSON *store=read_store(),*entry;int found=0;
    cJSON_ArrayForEach(entry,cJSON_GetObjectItem(store,"caption_memories"))
        if(!strcmp(cJSON_GetObjectItem(entry,"profile")->valuestring,profile) && !strcmp(cJSON_GetObjectItem(entry,"channel")->valuestring,channel) &&
           cJSON_IsTrue(cJSON_GetObjectItem(entry,"enabled"))==enabled)++found;
    cJSON_Delete(store);return found;
}
static int memory_total(void) {cJSON *store=read_store();int n=cJSON_GetArraySize(cJSON_GetObjectItem(store,"caption_memories"));cJSON_Delete(store);return n;}
int main(void) {
    char dir[]="/tmp/tube-third-captions-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    SEPARATE_SETTINGS();
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    /* Three videos: A and B of one channel, C of another. */
    p->list_catalog("playlist:PLfixture01",0,items_cb,NULL);pair();
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":["
        "{\"id\":\"abcdefghijk\",\"title\":\"One\",\"subtitle\":\"Channel A\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\"},"
        "{\"id\":\"bcdefghijkl\",\"title\":\"Two\",\"subtitle\":\"Channel A\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\"},"
        "{\"id\":\"cdefghijklm\",\"title\":\"Three\",\"subtitle\":\"Channel C\",\"channel_id\":\"UCcccccccccccccccccccccc\"}],\"has_more\":false}}");
    assert(last_ok && last_count==3);
    /* At the defaults nothing is asked and nothing changes; the companion's own marks are still read. */
    resolve(A);assert(direct() && !strcmp(asked(),"<absent>"));assert(started(A)==1);
    assert(tube_caption_track_count()==3 && !tube_caption_track_translated(0) && tube_caption_track_translated(1)==1 && !tube_caption_track_translated(2));
    assert(!tube_caption_track_translated(-1) && !tube_caption_track_translated(3) && tube_caption_track_automatic(1));
    assert(!strcmp(tube_caption_track_language(1),"fr-en") && !strcmp(tube_caption_track_name(1),"French"));
    /* The switch sits with the caption settings and is saved with the profile. */
    show("settings:extra:captions");assert(find("Unlock more subtitles: off")>=0 && find("Remember subtitles per channel: off")>=0);
    show("settings:more-subtitles");assert(find("Unlock more subtitles: on")>=0 && find("Remember subtitles per channel: off")>=0);
    resolve(A);health(third);assert(direct() && !strcmp(asked(),"true"));started(A);
    resolve(B);assert(direct() && !strcmp(asked(),"true"));started(B);
    show("library:profile:alice");resolve(A);assert(direct() && !strcmp(asked(),"<absent>"));started(A);
    show("settings:extra:captions");assert(find("Unlock more subtitles: off")>=0);
    show("library:profile:guest");
    p->shutdown();assert(!p->init());
    show("settings:extra:captions");assert(find("Unlock more subtitles: on")>=0);
    resolve(A);health(third);assert(!strcmp(asked(),"true"));started(A);
    /* An older companion is not sent the field. */
    assert(!p->set_source("http://127.0.0.1:8766#123456"));
    resolve(A);pair();health(older);assert(direct() && !strcmp(asked(),"<absent>"));started(A);
    show("settings:extra:captions");assert(find("Unlock more subtitles: on")>=0 && strstr(last_items[find("Unlock more subtitles: on")].subtitle,"does not list"));
    show("settings:more-subtitles");assert(find("Unlock more subtitles: off")>=0);
    resolve(A);assert(direct() && !strcmp(asked(),"<absent>"));started(A);
    /* Remember subtitles per channel. Off: the profile's Captions setting decides and nothing is kept. */
    assert(tube_captions_for(A)==1 && !tube_remember_captions(A,0) && tube_captions_for(A)==1 && !memory_total());
    show("settings:caption-memory");assert(find("Remember subtitles per channel: on")>=0);
    assert(play(A)==1 && tube_captions_for(A)==1);
    /* Turned off while watching: off for this channel's videos, on for the others. */
    assert(tube_remember_captions(A,0)==1 && !tube_captions_for(A) && !tube_captions_for(B) && tube_captions_for(C)==1);
    assert(memories("guest","UCaaaaaaaaaaaaaaaaaaaaaa",0)==1 && memory_total()==1);
    assert(play(B)==0 && play(C)==1);
    /* Choosing a track turns them on again for the channel. */
    assert(play(B)==0 && tube_caption_switch_begin("en",0,0)==2);reply(0,1,200,cue);
    const char *media="";assert(tube_caption_switch_status(&media)==1);tube_caption_switch_finish(1);
    assert(tube_captions_for(A)==1 && memories("guest","UCaaaaaaaaaaaaaaaaaaaaaa",1)==1 && memory_total()==1);
    assert(play(A)==1);
    /* The other way round: Captions off for the profile, on for one channel. */
    show("settings:captions");assert(play(C)==0 && tube_captions_for(C)==0 && tube_captions_for(A)==1 && play(A)==1);
    assert(tube_remember_captions(C,1)==1 && tube_captions_for(C)==1 && play(C)==1 && memory_total()==2);
    assert(!tube_remember_captions("unknownvid0",1) && !tube_remember_captions("bad",1) && tube_captions_for("unknownvid0")==0);
    /* Each profile has its own memory; turning the switch off keeps the list but stops using it. */
    show("library:profile:alice");assert(tube_captions_for(A)==1 && !tube_remember_captions(A,0));
    p->list_catalog("playlist:PLfixture01",0,items_cb,NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"One\",\"subtitle\":\"Channel A\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\"}],\"has_more\":false}}");
    show("settings:caption-memory");assert(tube_remember_captions(A,0)==1 && !tube_captions_for(A));
    assert(memories("alice","UCaaaaaaaaaaaaaaaaaaaaaa",0)==1 && memories("guest","UCaaaaaaaaaaaaaaaaaaaaaa",1)==1 && memory_total()==3);
    show("library:profile:guest");assert(tube_captions_for(A)==1 && tube_captions_for(C)==1);
    show("settings:caption-memory");assert(find("Remember subtitles per channel: off")>=0 && tube_captions_for(C)==0 && memory_total()==3);
    show("settings:caption-memory");assert(tube_captions_for(C)==1);
    /* Kept across a restart; malformed and repeated entries are dropped one by one. */
    p->shutdown();
    cJSON *store=read_store(),*list=cJSON_GetObjectItem(store,"caption_memories");
    cJSON_AddItemToArray(list,cJSON_Parse("{\"profile\":\"guest\",\"channel\":\"UCcccccccccccccccccccccc\",\"enabled\":false}"));
    cJSON_AddItemToArray(list,cJSON_Parse("{\"profile\":\"guest\",\"channel\":\"not-a-channel\",\"enabled\":true}"));
    cJSON_AddItemToArray(list,cJSON_Parse("{\"profile\":\"bad profile\",\"channel\":\"UCdddddddddddddddddddddd\",\"enabled\":true}"));
    cJSON_AddItemToArray(list,cJSON_Parse("{\"profile\":\"guest\",\"channel\":\"UCdddddddddddddddddddddd\",\"enabled\":\"yes\"}"));
    cJSON_AddItemToArray(list,cJSON_CreateNumber(1));
    write_store(store);cJSON_Delete(store);assert(!p->init());
    assert(tube_captions_for(C)==1 && tube_captions_for(A)==1);
    p->shutdown();assert(memory_total()==3 && memories("guest","UCcccccccccccccccccccccc",1)==1);assert(!p->init());
    /* A file from before the switches existed keeps them off. */
    p->shutdown();store=read_store();cJSON *entry;
    cJSON_ArrayForEach(entry,cJSON_GetObjectItem(store,"profile_settings")) {
        cJSON_DeleteItemFromObject(entry,"more_subtitles");cJSON_DeleteItemFromObject(entry,"caption_memory");
    }
    cJSON_DeleteItemFromObject(store,"caption_memories");write_store(store);cJSON_Delete(store);assert(!p->init());
    show("settings:extra:captions");assert(find("Unlock more subtitles: off")>=0 && find("Remember subtitles per channel: off")>=0);
    assert(tube_captions_for(A)==0); /* the profile's Captions setting, turned off above */
    p->shutdown();
    puts("PASS: translated caption request and track marks gated by the feature list, per-channel caption memory per profile, persistence and corrupt values");
}
