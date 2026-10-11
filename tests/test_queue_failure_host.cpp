/* Execute the shipping ProviderHostScreen scheduling and cancellation blocks. */
#include <cassert>
#include <cstdio>
#include <cstring>
struct PendingPlay { char provider[16]="youtube", item_id[160]="abcdefghijk"; int choice_count=4; };
static long long now_ms=1000, g_queue_retry_deadline;
static bool pending, phone_pending, skip, g_tube_recovery_launch, g_resolve_finished, m_navigatingToPlayer;
static unsigned g_phone_ticket, pressed;
static int continues, cancelled, tuning=-1, status_error=-1, toasts;
static char status_text[200];
static long long queue_retry_now_ms() { return now_ms; }
static bool tube_queue_skip_unavailable(const char *id) { assert(!std::strcmp(id,"abcdefghijk")); pending=skip; return skip; }
static bool tube_queue_retry_pending() { return pending; }
static bool tube_phone_launch_pending() { return phone_pending; }
static void tube_queue_retry_cancel() { ++cancelled; pending=false; }
static void toast(const char *title,const char *text) { assert(*title && std::strstr(text,"5 seconds")); ++toasts; }
static void evo_rmlui_provider_set_tuning(int value) { tuning=value; }
static void evo_rmlui_provider_set_loading(int loading,const char *text) { assert(!loading && !*text); }
static void evo_rmlui_provider_set_status(const char *text,int error) { std::snprintf(status_text,sizeof status_text,"%s",text); status_error=error; }
static void failure(PendingPlay *pp) {
#include "schedule.inc"
    ++continues;
}
static void wait_for_next() {
#include "wait.inc"
    ++continues;
}
static void input() {
#include "input.inc"
}
static void exit_screen() {
#include "exit.inc"
}
int main() {
    PendingPlay p; skip=false; failure(&p); assert(continues==1 && !g_queue_retry_deadline);
    skip=true; g_phone_ticket=1; failure(&p); assert(continues==2 && !g_queue_retry_deadline);
    g_phone_ticket=0; g_tube_recovery_launch=true; failure(&p); assert(continues==3 && !g_queue_retry_deadline);
    g_tube_recovery_launch=false; std::strcpy(p.provider,"other"); failure(&p); assert(continues==4);
    std::strcpy(p.provider,"youtube"); failure(&p);
    assert(continues==4 && g_queue_retry_deadline==6000 && g_resolve_finished && !p.choice_count);
    assert(!tuning && !status_error && toasts==1 && std::strstr(status_text,"cancels"));
    now_ms=5999; wait_for_next(); assert(continues==4 && g_queue_retry_deadline==6000);
    now_ms=6000; wait_for_next(); assert(continues==5 && !g_queue_retry_deadline && pending);
    failure(&p); pending=false; wait_for_next(); assert(continues==6 && !g_queue_retry_deadline && cancelled==1);
    failure(&p); phone_pending=true; wait_for_next(); assert(continues==7 && !g_queue_retry_deadline && cancelled==2);
    phone_pending=false; failure(&p); pressed=0; input(); assert(g_queue_retry_deadline && cancelled==2);
    pressed=1; input(); assert(!g_queue_retry_deadline && cancelled==3 && !pending);
    assert(std::strstr(status_text,"cancelled")); input(); assert(cancelled==3);
    failure(&p); m_navigatingToPlayer=true; exit_screen(); assert(!g_queue_retry_deadline && pending && cancelled==3);
    failure(&p); m_navigatingToPlayer=false; exit_screen(); assert(!g_queue_retry_deadline && !pending && cancelled==4);
    puts("PASS: shipping host five-second advance, failure owner, input/exit cancellation and phone priority");
}
