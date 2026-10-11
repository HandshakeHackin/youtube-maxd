/* Play from start and Play incognito start their video from the action itself, once per menu visit. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *p;
static char row[160];
static int find(const char *title) {
    for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].title,title)){snprintf(row,sizeof row,"%s",last_items[i].id);return i;}
    return -1;
}
static void menu(const char *title) {
    p->list_catalog("account:menu:abcdefghijk",0,items_cb,NULL);assert(last_ok && !pending_count && find(title)>=0);
}
static void show(const char *id) {p->list_catalog(id,0,items_cb,NULL);assert(last_ok && !pending_count);}
/* What the host does with a taken video: resolve it, then start the chosen stream. */
static void play(const char *id) {
    evo_stream_choice_t selected={0};snprintf(selected.url,sizeof selected.url,"http://127.0.0.1:8765/media/capability/p0");
    assert(!p->resolve(id,resolve_cb,NULL));
    char body[320];snprintf(body,sizeof body,"{\"state\":\"done\",\"result\":{\"id\":\"%s\",\"title\":\"First\",\"duration_sec\":100,\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/capability/p0\",\"label\":\"360p\",\"container\":\"mp4\"}]}}",id);
    poll_job(body);assert(last_ok);p->stream_chosen(id,&selected);
}
static int resume(void) {
    p->list_catalog("history",0,items_cb,NULL);assert(last_ok && last_count==1);
    return (int)last_items[0].resume_pos_sec;
}
int main(void) {
    char dir[]="/tmp/tube-menu-launch-XXXXXX",restart[160],incognito[160];evo_provider_item_t item;
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    p=&evo_provider_youtube;assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->list_catalog("playlist:PLfixture01",0,items_cb,NULL);
    reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"First\",\"duration_sec\":100}],\"has_more\":false}}");
    play("abcdefghijk");p->report_progress("abcdefghijk",37,100,EVO_PROVIDER_PLAY_STOP);assert(resume()==37);
    /* Opening the menu starts nothing; the action hands its video to the host once. */
    assert(!tube_take_next(&item));
    menu("Play from start");snprintf(restart,sizeof restart,"%s",row);assert(!tube_take_next(&item));
    show(restart);assert(last_count==1 && !last_items[0].is_folder && !strcmp(last_items[0].subtitle,"Cross plays from the start"));
    memset(&item,0,sizeof item);
    assert(tube_take_next(&item)==1 && !strcmp(item.id,"abcdefghijk") && !strcmp(item.title,"First") && !item.resume_pos_sec && !*item.playback_playlist);
    assert(!tube_take_next(&item));
    play("abcdefghijk");p->report_progress("abcdefghijk",55,100,EVO_PROVIDER_PLAY_STOP);
    /* Back from that playback the screen reloads: it offers the row and starts nothing. */
    show(restart);assert(!strcmp(last_items[0].subtitle,"Cross resumes this video") && last_items[0].resume_pos_sec==55 && !tube_take_next(&item));
    show(restart);assert(!tube_take_next(&item));
    /* A new menu visit starts it again. */
    menu("Play from start");show(restart);assert(tube_take_next(&item)==1 && !item.resume_pos_sec);
    p->report_progress("abcdefghijk",21,100,EVO_PROVIDER_PLAY_STOP);
    /* Leaving the screen, or pressing Cross on the row first, retires an untaken start. */
    menu("Play from start");show(restart);p->list_catalog("history",0,items_cb,NULL);assert(!tube_take_next(&item));
    menu("Play from start");show(restart);play("abcdefghijk");assert(!tube_take_next(&item));
    menu("Play from start");show(restart);assert(!tube_take_next(NULL) && !tube_take_next(&item)); /* a taker without storage consumes it */
    p->list_catalog(restart,1,items_cb,NULL);assert(!last_ok && !tube_take_next(&item));
    p->report_progress("abcdefghijk",44,100,EVO_PROVIDER_PLAY_STOP);assert(resume()==44);
    /* Incognito: the taken video plays without touching history; a reload offers it again privately. */
    menu("Play incognito");snprintf(incognito,sizeof incognito,"%s",row);
    show(incognito);assert(last_count==1 && !strcmp(last_items[0].subtitle,"Cross plays without history or account reports"));
    assert(tube_take_next(&item)==1 && !strcmp(item.id,"abcdefghijk") && item.resume_pos_sec==44);
    play("abcdefghijk");p->report_progress("abcdefghijk",80,100,EVO_PROVIDER_PLAY_STOP);
    show(incognito);assert(!tube_take_next(&item));assert(resume()==44);
    show(incognito);play("abcdefghijk");p->report_progress("abcdefghijk",90,100,EVO_PROVIDER_PLAY_STOP);assert(resume()==44);
    play("abcdefghijk");p->report_progress("abcdefghijk",60,100,EVO_PROVIDER_PLAY_STOP);assert(resume()==60); /* an ordinary start records again */
    /* The two actions are separate offers of one menu visit. */
    menu("Play incognito");show(incognito);assert(tube_take_next(&item)==1);
    show(restart);assert(tube_take_next(&item)==1 && !item.resume_pos_sec);
    show(incognito);assert(tube_take_next(&item)==1);show(incognito);assert(!tube_take_next(&item));
    /* A profile change retires the screen and anything it armed. */
    menu("Play from start");show(restart);
    p->list_catalog("library:profile:alice",0,items_cb,NULL);assert(!tube_take_next(&item));
    p->list_catalog(restart,0,items_cb,NULL);assert(!last_ok && !tube_take_next(&item));
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: single-press Play from start and Play incognito, one start per menu visit, reload after playback, retirement and incognito privacy");
}
