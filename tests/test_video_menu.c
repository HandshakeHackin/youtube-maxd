/* Video menu navigation, restart, links and SponsorBlock exclusion on the shipping provider. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *p;
static char row[160];
static int find(const char *title) {
    for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].title,title)){snprintf(row,sizeof row,"%s",last_items[i].id);return i;}
    return -1;
}
static void menu(const char *id) {
    char parent[64];snprintf(parent,sizeof parent,"account:menu:%s",id);
    p->list_catalog(parent,0,items_cb,NULL);assert(last_ok && !pending_count);
}
static void show(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(last_ok && !pending_count);}
static void play(const char *id) {
    evo_stream_choice_t selected={0};snprintf(selected.url,sizeof selected.url,"http://127.0.0.1:8765/media/capability/p0");
    assert(!p->resolve(id,resolve_cb,NULL));
    char body[320];snprintf(body,sizeof body,"{\"state\":\"done\",\"result\":{\"id\":\"%s\",\"title\":\"First\",\"duration_sec\":100,\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/capability/p0\",\"label\":\"360p\",\"container\":\"mp4\"}]}}",id);
    poll_job(body);assert(last_ok);p->stream_chosen(id,&selected);
}
static int excluded(const char *profile) {
    cJSON *store=read_store(),*entry;int count=-1;
    cJSON_ArrayForEach(entry,cJSON_GetObjectItem(store,"profile_settings"))
        if(!strcmp(cJSON_GetObjectItem(entry,"profile")->valuestring,profile))
            count=cJSON_GetArraySize(cJSON_GetObjectItem(cJSON_GetObjectItem(entry,"sponsor_settings"),"excluded_channels"));
    cJSON_Delete(store);return count;
}
static const char *listed="{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":["
    "{\"id\":\"abcdefghijk\",\"title\":\"First\",\"subtitle\":\"Fixture channel\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"duration_sec\":100,\"playback_context\":{\"playlist\":\"PLfixture01\",\"index\":0}},"
    "{\"id\":\"bcdefghijkl\",\"title\":\"Second\",\"channel_id\":\"not-a-channel\"}],\"has_more\":false}}";
int main(void) {
    char dir[]="/tmp/tube-menu-XXXXXX",stale[160];
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->list_catalog("playlist:PLfixture01",0,items_cb,NULL);
    reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");poll_job(listed);
    assert(last_ok && last_count==2 && last_items[0].playback_scope);
    /* Navigation rows exist only for a real channel identity and a listed occurrence. */
    menu("bcdefghijkl");assert(last_count==8);
    assert(find("Open channel")<0 && find("Open playlist")<0 && find("Exclude channel from SponsorBlock")<0);
    assert(find("Play from start")>=0 && find("Play incognito")>=0 && find("Share link")>=0);
    show(row);assert(last_count==2 && !strcmp(last_items[0].title,"https://youtu.be/bcdefghijkl") &&
        !strcmp(last_items[1].title,"https://www.youtube.com/embed/bcdefghijkl"));
    menu("abcdefghijk");assert(last_count==12);
    assert(find("Open channel")>=0 && !strcmp(row,"channel:UCaaaaaaaaaaaaaaaaaaaaaa"));
    p->list_catalog(row,0,items_cb,NULL);
    assert(pending_count==1 && strstr(pending[0].body,"\"parent\":\"channel:UCaaaaaaaaaaaaaaaaaaaaaa\""));
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[],\"has_more\":false}}");assert(last_ok);
    menu("abcdefghijk");assert(find("Open playlist")>=0 && !strcmp(row,"playlist:PLfixture01"));
    p->list_catalog(row,0,items_cb,NULL);
    assert(pending_count==1 && strstr(pending[0].body,"\"parent\":\"playlist:PLfixture01\""));
    poll_job(listed);assert(last_ok && last_count==2);
    /* Share shows readable links; each row is an inert notice with its own id. */
    menu("abcdefghijk");assert(find("Share link")>=0);show(row);
    assert(last_count==4 && !strcmp(last_items[0].title,"https://youtu.be/abcdefghijk") && !strcmp(last_items[0].subtitle,"Video link"));
    assert(!strcmp(last_items[1].title,"https://www.youtube.com/embed/abcdefghijk") && !strcmp(last_items[1].subtitle,"Embed link"));
    assert(!strcmp(last_items[2].title,"https://www.youtube.com/channel/UCaaaaaaaaaaaaaaaaaaaaaa"));
    assert(!strcmp(last_items[3].title,"https://www.youtube.com/playlist?list=PLfixture01"));
    for(int i=0;i<4;++i)for(int j=i+1;j<4;++j)assert(strcmp(last_items[i].id,last_items[j].id));
    show(last_items[3].id);assert(last_count==1 && last_items[0].is_folder);
    p->list_catalog(row,1,items_cb,NULL);assert(!last_ok);
    /* Play from start forgets only the resume position and offers a playable row. */
    assert(tube_mark_watched("abcdefghijk")==1);
    p->report_progress("abcdefghijk",37,100,EVO_PROVIDER_PLAY_STOP);
    p->list_catalog("history",0,items_cb,NULL);assert(last_count==1 && last_items[0].played && last_items[0].resume_pos_sec==37);
    menu("abcdefghijk");assert(find("Play from start")>=0);snprintf(stale,sizeof stale,"%s",row);show(row);
    assert(last_count==1 && !last_items[0].is_folder && !strcmp(last_items[0].id,"abcdefghijk"));
    assert(!last_items[0].resume_pos_sec && last_items[0].played && !strcmp(last_items[0].subtitle,"Cross plays from the start"));
    p->list_catalog("history",0,items_cb,NULL);assert(last_count==1 && last_items[0].played && !last_items[0].resume_pos_sec);
    cJSON *store=read_store();
    assert(cJSON_GetObjectItem(cJSON_GetArrayItem(cJSON_GetObjectItem(store,"records"),0),"position")->valueint==0);
    cJSON_Delete(store);
    /* Reloading the offer after the playback it started keeps the new position. */
    show(stale);play("abcdefghijk");
    p->report_progress("abcdefghijk",55,100,EVO_PROVIDER_PLAY_STOP);
    show(stale);assert(last_items[0].resume_pos_sec==55 && !strcmp(last_items[0].subtitle,"Cross resumes this video"));
    menu("abcdefghijk");show(stale);assert(!last_items[0].resume_pos_sec); /* a new menu visit applies again */
    p->report_progress("abcdefghijk",21,100,EVO_PROVIDER_PLAY_STOP);
    /* Channel exclusion reuses the profile's SponsorBlock list and never flips on repeat. */
    menu("abcdefghijk");assert(find("Exclude channel from SponsorBlock")>=0);show(row);
    assert(last_count==1 && !strcmp(last_items[0].title,"SponsorBlock is off for this channel") && excluded("guest")==1);
    show(row);assert(excluded("guest")==1);
    show("settings:sb:exclusions");assert(last_count==1 && !strcmp(last_items[0].title,"Excluded: Fixture channel"));
    menu("abcdefghijk");assert(find("Exclude channel from SponsorBlock")<0 && find("Use SponsorBlock on this channel")>=0);
    show(row);assert(!strcmp(last_items[0].title,"SponsorBlock is on for this channel") && excluded("guest")==0);
    show(row);assert(excluded("guest")==0);
    menu("abcdefghijk");assert(find("Exclude channel from SponsorBlock")>=0);show(row);assert(excluded("guest")==1);
    /* Profile change retires menu actions and the listed occurrence; settings stay separate. */
    show("library:profile:alice");assert(excluded("alice")==0 && excluded("guest")==1);
    p->list_catalog(row,0,items_cb,NULL);assert(!last_ok);
    p->list_catalog(stale,0,items_cb,NULL);assert(!last_ok);
    menu("abcdefghijk");assert(last_count==16 && find("Open playlist")<0 && find("Open channel")<0);
    p->list_catalog("library:playlist:PLfixture01",0,items_cb,NULL);poll_job(listed);assert(last_count==2);
    menu("abcdefghijk");assert(find("Open playlist")>=0 && !strcmp(row,"library:playlist:PLfixture01"));
    assert(find("Play from start")>=0);show(row);assert(!last_items[0].resume_pos_sec);
    show("library:profile:guest");
    p->list_catalog("history",0,items_cb,NULL);assert(last_count==1 && last_items[0].resume_pos_sec==21);
    menu("abcdefghijk");assert(find("Open playlist")<0 && find("Use SponsorBlock on this channel")>=0);
    /* Reload keeps the exclusion; a full list is reported without changing it. */
    p->shutdown();assert(!p->init());assert(excluded("guest")==1);
    menu("abcdefghijk");assert(find("Use SponsorBlock on this channel")>=0);show(row);assert(excluded("guest")==0);
    for(int i=0;i<128;++i) {
        char id[64];snprintf(id,sizeof id,"settings:sb:exclude:UC%022d",i);p->list_catalog(id,0,items_cb,NULL);assert(last_ok);
    }
    assert(excluded("guest")==128);
    menu("abcdefghijk");assert(find("Exclude channel from SponsorBlock")>=0);
    p->list_catalog(row,0,items_cb,NULL);assert(!last_ok && strstr(error_text,"full") && excluded("guest")==128);
    p->list_catalog("local:video:0:share:abcdefghijk",0,items_cb,NULL);assert(!last_ok);
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: video menu channel and playlist navigation, play from start, share links, SponsorBlock exclusion, profile isolation and stale actions");
}
