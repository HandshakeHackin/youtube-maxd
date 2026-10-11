/* Run the shipping host's typed unavailable gate, wait and input blocks. */
#include <cassert>
#include <cstdio>
#include <cstring>
struct PendingPlay { char item_id[160]="abcdefghijk";int choice_count=4; };
static unsigned g_phone_ticket, pressed;
static bool typed, waiting, g_resolve_finished;
static int disposition, calls, cancelled, continued, tuning;
static char status[200];
static long long queue_retry_now_ms() { return 1000; }
static int tube_resolve_unavailable(const char* id) { assert(!std::strcmp(id,"abcdefghijk"));return typed; }
static int tube_phone_launch_unavailable(unsigned ticket,const char* id,long long now) {
    assert(ticket==11 && !std::strcmp(id,"abcdefghijk") && now==1000);++calls;return disposition;
}
static int tube_phone_retry_waiting() { return waiting; }
static void tube_phone_cancel() { ++cancelled;waiting=false; }
static void evo_rmlui_provider_set_tuning(int value) { tuning=value; }
static void evo_rmlui_provider_set_loading(int value,const char* text) { assert(!value && !*text); }
static void evo_rmlui_provider_set_status(const char* text,int error) { assert(!error);std::snprintf(status,sizeof status,"%s",text); }
static void failure(PendingPlay* pp) {
#include "phone-schedule.inc"
    ++continued;
}
static void launch() {
#include "phone-wait.inc"
    ++continued;
}
static void input() {
#include "phone-input.inc"
}
int main() {
    PendingPlay p;typed=true;failure(&p);assert(continued==1 && !calls);
    g_phone_ticket=11;typed=false;failure(&p);assert(continued==2 && !calls && g_phone_ticket==11);
    typed=true;disposition=0;failure(&p);assert(continued==3 && calls==1 && g_phone_ticket==11);
    disposition=1;failure(&p);assert(continued==3 && calls==2 && !g_phone_ticket && g_resolve_finished && !p.choice_count && !tuning);
    assert(std::strstr(status,"5 seconds") && std::strstr(status,"cancels"));
    g_phone_ticket=11;p.choice_count=4;disposition=2;failure(&p);
    assert(continued==3 && calls==3 && !g_phone_ticket && !p.choice_count && std::strstr(status,"No further"));
    g_phone_ticket=11;p.choice_count=4;disposition=3;failure(&p);
    assert(continued==3 && calls==4 && !g_phone_ticket && !p.choice_count && std::strstr(status,"30 seconds") && std::strstr(status,"cancels"));
    launch();assert(continued==4);waiting=true;launch();assert(continued==4);
    pressed=0;input();assert(waiting && !cancelled);
    pressed=1;g_phone_ticket=11;input();assert(!waiting && cancelled==1 && !g_phone_ticket && std::strstr(status,"cancelled"));
    input();assert(cancelled==1);launch();assert(continued==5);
    puts("Shipping phone unavailable gate, terminal/wait and any-button cancellation passed");
}
