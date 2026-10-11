/* Shipping inactivity timer: persistence, activity, once-only pause and recovery. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
#include <time.h>
static int64_t timer_clock=100000;
int tube_test_clock_gettime(clockid_t id, struct timespec *t) {
    assert(id==CLOCK_MONOTONIC);t->tv_sec=timer_clock/1000;t->tv_nsec=timer_clock%1000*1000000;return 0;
}
int main(void) {
    char dir[]="/tmp/tube-sleep-timer-XXXXXX";
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t *p=&evo_provider_youtube;assert(!p->init());
    assert(!tube_sleep_minutes() && !tube_sleep_tick(1));
    assert(!tube_sleep_set_minutes(-30) && !tube_sleep_set_minutes(15) && !tube_sleep_set_minutes(630));
    p->list_catalog("settings:sleep-timer",0,items_cb,NULL);assert(last_ok && last_count==21);
    p->list_catalog("settings:sleep:30",0,items_cb,NULL);assert(last_ok && tube_sleep_minutes()==30);
    timer_clock+=1799999;assert(!tube_sleep_tick(1));
    tube_sleep_activity();timer_clock+=1799999;assert(!tube_sleep_tick(1));
    ++timer_clock;assert(!tube_sleep_tick(0) && tube_sleep_tick(1) && !tube_sleep_tick(1));
    tube_sleep_activity();timer_clock+=1800000;assert(tube_sleep_tick(1));
    assert(tube_sleep_set_minutes(600));timer_clock+=35999999;assert(!tube_sleep_tick(1));
    ++timer_clock;assert(tube_sleep_tick(1));
    p->shutdown();assert(!p->init() && tube_sleep_minutes()==600);
    /* Runtime elapsed time is not persisted across application restarts. */
    assert(!tube_sleep_tick(1));timer_clock+=36000000;assert(tube_sleep_tick(1));
    p->stream_chosen("abcdefghijk",NULL);assert(!tube_sleep_tick(1));
    timer_clock-=500;assert(!tube_sleep_tick(1)); /* Clock regression restarts safely. */
    assert(tube_sleep_set_minutes(0));timer_clock+=999999999;assert(!tube_sleep_tick(1));
    p->list_catalog("settings:sleep:bad",0,items_cb,NULL);assert(!last_ok);
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: inactivity timer choices, persistence, reset, disabled/idle, exact expiry, one-shot and clock regression");
    return 0;
}
