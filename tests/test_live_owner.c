/* Live ownership/persistence on the shipping provider; no FFmpeg or network. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sched.h>
static const int64_t point_identity[3]={3,1760000006000000LL,2000000};
static tube_live_owner read_owner;
static int64_t read_window[7];
static _Atomic unsigned read_count, retired_count;
static _Atomic int retired, stop_reader;
static void *restore_reader(void *unused) {
    (void)unused;
    while (!atomic_load(&stop_reader)) {
        int revoked=atomic_load(&retired);
        int64_t point=tube_live_restore(&read_owner,read_window,point_identity);
        if (revoked) { assert(!point); atomic_fetch_add(&retired_count,1); }
        atomic_fetch_add(&read_count,1);
    }
    return NULL;
}
static const evo_provider_t *p;
static evo_stream_choice_t live;
static const char *A = "abcdefghijk";
static void selected(int ok, const evo_stream_choice_t *choices, int count, void *ud) {
    (void)ud; assert(ok && count == 1); live = choices[0];
}
static void play(void) {
    assert(!p->resolve(A,selected,NULL));
    poll_job("{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"title\":\"Live\",\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/capability/h0.m3u8\",\"container\":\"hls\",\"is_live\":true,\"width\":640,\"height\":360,\"fps\":30,\"video_codec\":\"avc\",\"audio_codec\":\"aac\",\"audio_channels\":2}]}}");
    p->stream_chosen(A,&live);
}
static void window(int64_t out[7], int shift) {
    struct timespec t; assert(!clock_gettime(CLOCK_MONOTONIC,&t));
    int64_t now = (int64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000;
    int64_t value[7] = {1,shift,shift+4,1400000+shift*2000000LL,9400000+shift*2000000LL,
        1760000000000000LL+shift*2000000LL,now+6000000};
    memcpy(out,value,sizeof value);
}
int main(void) {
    char dir[]="/tmp/tube-live-owner-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    cJSON *root=cJSON_Parse("{\"version\":2,\"source\":\"http://127.0.0.1:8765\",\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\",\"library_profile\":\"guest\",\"records\":[]}");
    write_store(root);cJSON_Delete(root);p=&evo_provider_youtube;assert(!p->init());
    play();tube_live_owner first,again,forged;
    assert(!tube_live_capture("bcdefghijkl",live.url,&first));
    assert(!tube_live_capture(A,"http://127.0.0.1:8765/media/other/h0.m3u8",&first));
    assert(tube_live_capture(A,live.url,&first) && tube_live_owner_current(first.token) && !first.saved_utc);
    int64_t bounds[7];window(bounds,0);
    const int64_t point=1760000006000000LL;
    assert(tube_live_remember(&first,bounds,point,point_identity));
    p->report_progress(A,0,0,EVO_PROVIDER_PLAY_STOP);
    root=read_store();char *stored=cJSON_PrintUnformatted(root);assert(stored && !strstr(stored,"capability"));free(stored);cJSON_Delete(root);
    assert(tube_live_capture(A,live.url,&again) && again.saved_utc==point && !tube_live_owner_current(first.token));
    window(bounds,2);assert(tube_live_restore(&again,bounds,point_identity)==point);
    int64_t wrong_identity[3];memcpy(wrong_identity,point_identity,sizeof wrong_identity);
    ++wrong_identity[0];assert(!tube_live_restore(&again,bounds,wrong_identity));
    memcpy(wrong_identity,point_identity,sizeof wrong_identity);++wrong_identity[1];assert(!tube_live_restore(&again,bounds,wrong_identity));
    memcpy(wrong_identity,point_identity,sizeof wrong_identity);++wrong_identity[2];assert(!tube_live_restore(&again,bounds,wrong_identity));
    assert(!tube_live_restore(&again,bounds,NULL) && !tube_live_remember(&again,bounds,point,NULL));
    forged=again;snprintf(forged.profile,sizeof forged.profile,"fixture");assert(!tube_live_restore(&forged,bounds,point_identity));assert(!tube_live_remember(&forged,bounds,point,point_identity));
    forged=again;snprintf(forged.video,sizeof forged.video,"bcdefghijkl");assert(!tube_live_restore(&forged,bounds,point_identity));
    forged=again;strcat(forged.url,"x");assert(!tube_live_restore(&forged,bounds,point_identity));
    forged=again;strcat(forged.origin,"x");assert(!tube_live_restore(&forged,bounds,point_identity));
    forged=again;strcat(forged.format,"x");assert(!tube_live_restore(&forged,bounds,point_identity));
    forged=again;++forged.saved_utc;assert(!tube_live_restore(&forged,bounds,point_identity));
    bounds[6]=0;assert(!tube_live_restore(&again,bounds,point_identity) && !tube_live_remember(&again,bounds,point,point_identity));
    window(bounds,4);assert(!tube_live_restore(&again,bounds,point_identity));
    window(bounds,0);assert(!tube_live_remember(&again,bounds,INT64_MAX,point_identity));
    /* Shutdown/reopen keeps only the checked UTC/format/source record. */
    p->shutdown();assert(!tube_live_owner_current(again.token));assert(!p->init());play();
    assert(tube_live_capture(A,live.url,&again) && again.saved_utc==point && tube_live_restore(&again,bounds,point_identity)==point);
    read_owner=again;memcpy(read_window,bounds,sizeof bounds);
    pthread_t reader;assert(!pthread_create(&reader,NULL,restore_reader,NULL));
    while(atomic_load(&read_count)<1000)sched_yield();
    evo_stream_choice_t other=live;other.height=720;p->stream_chosen(A,&other);
    assert(!tube_live_owner_current(again.token));assert(tube_live_capture(A,other.url,&first) && !first.saved_utc);
    atomic_store(&retired,1);while(atomic_load(&retired_count)<1000)sched_yield();
    atomic_store(&stop_reader,1);assert(!pthread_join(reader,NULL));
    /* A different profile, source or selection cannot consume the old receipt. */
    p->list_catalog("library:profile:fixture",0,items_cb,NULL);assert(last_ok && !tube_live_owner_current(first.token));play();
    assert(tube_live_capture(A,live.url,&first) && !first.saved_utc);
    p->list_catalog("library:profile:guest",0,items_cb,NULL);assert(last_ok && !tube_live_owner_current(first.token));play();
    assert(tube_live_capture(A,live.url,&again) && again.saved_utc==point);
    assert(!p->set_source("http://127.0.0.1:8766#123456") && !tube_live_owner_current(again.token));
    assert(!tube_live_capture(A,live.url,&first));
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    /* Pair again, then disabled history suppresses both restore and saving. */
    assert(!p->search("fixture",0,items_cb,NULL));reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[]}}");
    play();assert(tube_live_capture(A,live.url,&first) && !first.saved_utc);
    /* Malformed persisted microsecond values do not pass the bounded loader. */
    for(int malformed=0;malformed<2;++malformed) {
        p->shutdown();root=read_store();cJSON *entry;
        cJSON_ArrayForEach(entry,cJSON_GetObjectItem(root,"records"))
            if(!strcmp(cJSON_GetObjectItem(entry,"profile")->valuestring,"guest"))
                cJSON_ReplaceItemInObject(entry,"live_utc",malformed?cJSON_CreateNumber(1e30):cJSON_CreateString("1760000006000000"));
        write_store(root);cJSON_Delete(root);assert(!p->init());play();
        assert(tube_live_capture(A,live.url,&first) && !first.saved_utc);
    }
    assert(tube_live_remember(&first,bounds,point,point_identity));p->report_progress(A,0,0,EVO_PROVIDER_PLAY_STOP);
    assert(tube_live_capture(A,live.url,&first) && first.saved_utc==point);
    p->list_catalog("settings:clear-history",0,items_cb,NULL);assert(last_ok && !tube_live_owner_current(first.token));
    assert(tube_live_capture(A,live.url,&first) && !first.saved_utc);
    p->list_catalog("settings:history:2",0,items_cb,NULL);assert(last_ok && !tube_live_owner_current(first.token));
    assert(tube_live_capture(A,live.url,&first) && !first.saved_utc && !tube_live_remember(&first,bounds,point,point_identity));
    p->shutdown();assert(!tube_live_owner_current(first.token));
    puts("PASS: live source/session/profile/video/format ownership, expiry, UTC persistence and privacy");
    return 0;
}
