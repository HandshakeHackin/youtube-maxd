/* Real provider async selection, queue persistence and recovery paths. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const char *listing = "{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"Music video\"},{\"id\":\"12345678901\"}],\"has_more\":false}}";
static const char *resolved_music = "{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"duration_sec\":100,\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/test/a.mpd\"}]}}";
static const evo_provider_t *provider;
static evo_stream_choice_t selected;
static void browse(const char *parent) {
    assert(!provider->list_catalog(parent,0,items_cb,NULL));
    if(!strncmp(parent,"search:",7) || !strcmp(parent,"section:music")) poll_job(listing);
    assert(last_ok);
}
static void play(void) {
    assert(!provider->resolve("abcdefghijk",resolve_cb,NULL));
    poll_job(resolved_music); assert(last_ok && !pending_count);
    provider->stream_chosen("abcdefghijk",&selected);
}
int main(void) {
    char dir[]="/tmp/tube-music-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    provider=&evo_provider_youtube; assert(!provider->init());
    assert(!provider->set_source("http://127.0.0.1:8765#123456"));
    provider->search("fixture",0,items_cb,NULL);
    reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}"); poll_job(listing);
    browse("settings:captions"); browse("settings:speed:175");
    snprintf(selected.url,sizeof selected.url,"http://127.0.0.1:8765/media/test/a.mpd");
    browse("section:music"); play(); assert(tube_playback_speed_for("abcdefghijk")==100);
    assert(tube_playback_speed()==175); // Music does not erase the global preference.
    assert(tube_remember_playback_speed("abcdefghijk",125));
    assert(tube_playback_speed_for("abcdefghijk")==125); // Explicit speed change is permitted.
    assert(tube_queue_recovery(10,0,0));
    evo_provider_item_t row; double position; int subs;
    assert(tube_take_recovery(&row,&position,&subs)); play(); tube_recovery_finish();
    assert(tube_playback_speed_for("abcdefghijk")==125); // Recovery retains current-session choice.
    browse("section:music"); play(); assert(tube_playback_speed_for("abcdefghijk")==100);
    browse("search:music"); play(); assert(tube_playback_speed_for("abcdefghijk")==125);
    browse("history"); play(); assert(tube_playback_speed_for("abcdefghijk")==125);
    // Navigation cancels the old resolve; its late result cannot mark history as Music.
    browse("section:music"); assert(!provider->resolve("abcdefghijk",resolve_cb,NULL));
    browse("history"); reply(0,1,200,resolved_music); assert(!last_ok && !pending_count);
    play(); assert(tube_playback_speed_for("abcdefghijk")==125);
    // Queue origin survives reload and another page containing the same video.
    browse("section:music"); assert(tube_toggle_queue("abcdefghijk")==1);
    browse("search:music"); assert(!provider->init());
    browse("queue"); play(); assert(tube_playback_speed_for("abcdefghijk")==100);
    browse("favorites"); play(); assert(tube_playback_speed_for("abcdefghijk")==125);
    browse("settings:autoplay");
    provider->stream_chosen("12345678901",&selected);
    assert(tube_finish_and_next(100,100) && tube_take_next(&row));
    assert(!strcmp(row.id,"abcdefghijk"));
    play(); assert(tube_playback_speed_for("abcdefghijk")==100);
    // Profile change cannot reuse the prior queue's origin.
    browse("library:profile:other"); play(); assert(tube_playback_speed_for("abcdefghijk")==125);
    browse("library:profile:guest"); browse("section:music");
    assert(!provider->set_source("http://127.0.0.1:8765"));
    play(); assert(tube_playback_speed_for("abcdefghijk")==125); // Source setup clears old navigation.
    provider->shutdown();
    puts("PASS: Music origin normalization, explicit speed, async resolution, recovery, queue reload/autoplay and profile isolation");
}
