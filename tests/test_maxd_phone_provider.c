/* Actual provider settings, queue ownership and report export. GPL-3.0-or-later. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
#include "maxd/phone_remote.h"
#include "maxd/diagnostics.h"
static void choose(const char *id){evo_provider_youtube.list_catalog(id,0,items_cb,NULL);assert(last_ok);}
int main(void) {
    char dir[]="/tmp/maxd-phone-provider-XXXXXX";assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t *p=&evo_provider_youtube;assert(!p->init());SEPARATE_SETTINGS();
    assert(!maxd_remote_running());
    choose("settings:phone-remote:start");assert(maxd_remote_running() && tube_maxd_remote_scope());
    choose("link:maxd-remote");assert(last_count==1 && strstr(last_items[0].overview,":8767/#"));
    assert(!strcmp(tube_row_fact_get("link:maxd-remote","screen"),"reader"));
    assert(tube_maxd_remote_queue(MAXD_REMOTE_ADD,"abcdefghijk"));assert(tube_maxd_remote_queue(MAXD_REMOTE_ADD,"abcdefghijk"));
    assert(tube_maxd_remote_queue(MAXD_REMOTE_ADD,"bcdefghijkl"));
    MaxdRemoteState state={0};tube_maxd_remote_queue_state(&state);assert(state.count==2 && !strcmp(state.queue[0].id,"abcdefghijk"));
    assert(tube_maxd_remote_queue(MAXD_REMOTE_NEXT,"bcdefghijkl"));memset(&state,0,sizeof state);tube_maxd_remote_queue_state(&state);assert(state.count==2 && !strcmp(state.queue[0].id,"bcdefghijkl"));
    assert(tube_maxd_remote_queue(MAXD_REMOTE_REMOVE,"abcdefghijk"));assert(tube_maxd_remote_queue(MAXD_REMOTE_REMOVE,"abcdefghijk"));
    assert(!tube_maxd_remote_queue(MAXD_REMOTE_ADD,"https://example.com"));
    choose("settings:bug-report:capture");assert(*maxd_report_json());assert(!access(evo_data_path("bug-report.json"),F_OK));
    choose("settings:bug-report:preview");assert(last_count==25 && !strcmp(last_items[0].title,"schema") && !strcmp(last_items[24].title,"dropped_frames"));
    choose("library:profile:maxd_family_1");tube_phone_sync_scope();assert(!maxd_remote_running() && !*maxd_report_json());
    assert(!tube_maxd_remote_queue(MAXD_REMOTE_REMOVE,"bcdefghijkl"));
    choose("queue");assert(!last_count); /* another local profile never sees or controls the host queue */
    choose("settings:phone-remote:start");assert(maxd_remote_running());
    choose("settings:phone-remote:stop");assert(!maxd_remote_running());
    choose("settings:bug-report:clear");assert(access(evo_data_path("bug-report.json"),F_OK));
    p->shutdown();assert(!maxd_remote_running());
    puts("PASS: real provider opt-in/QR, idempotent queue add/remove, play-next order, report capture/preview/clear, profile isolation and shutdown");
}
