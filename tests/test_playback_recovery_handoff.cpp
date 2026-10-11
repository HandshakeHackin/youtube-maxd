/* Exact generated callback/launch worker and completion guard; engine/UI mocks. */
#include "evo_provider.h"
#include "tube_provider.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <string>

struct PlaybackSource { std::string url, title, provider, item_id; bool is_live=false; };
struct Playback {
    PlaybackSource opened; double position=0; bool answer=true; int stops=0;
    bool startPlaybackSource(const PlaybackSource& source, double resume) {
        opened=source; position=resume; return answer;
    }
    void stopPlayback() { ++stops; }
} playback;
struct Application {
    static Application& getInstance() { static Application app; return app; }
    Playback* getPlaybackController() { return &playback; }
};
constexpr int kMaxChoices=9;
#include "recovery_pending.inc"
PendingPlay g_pending;
enum ChoiceKind { KindListed, KindVariant, KindGuess, KindSource };
enum class PickStage { Idle, Ready };
enum class VariantsState { None, Reading, Done, Failed };
PickStage g_pick_stage=PickStage::Idle;
VariantsState g_variants=VariantsState::None;
std::chrono::steady_clock::time_point g_variants_started;
unsigned g_pick_gen=42;
bool g_resolve_finished=false, g_tube_recovery_launch=true;
double g_start_resume=0, g_tube_recovery_resume=37.625;
int g_tube_recovery_subtitles=0, prospero_subtitle_enabled=1;
int g_tube_recovery_subtitle_delay=450, g_tube_recovery_secondary_delay=-300;
double g_tube_recovery_subtitle_scale=1.25;
int prospero_subtitle_delay_ms=0, prospero_secondary_delay_ms=0;
double prospero_subtitle_time_scale=1;
unsigned g_phone_ticket=0;
double g_phone_resume=0;
static int64_t g_queue_retry_deadline;
static int64_t queue_retry_now_ms() { return 1000; }
extern "C" int tube_queue_skip_unavailable(const char*) { return 0; }
extern "C" int tube_resolve_unavailable(const char*) { return 0; }
extern "C" int tube_phone_launch_unavailable(unsigned,const char*,int64_t) { assert(false);return 0; }
extern "C" int tube_phone_launch_current(unsigned) { assert(false);return 0; }
extern "C" int tube_phone_launch_finish(unsigned,int) { assert(false);return 0; }
extern "C" void tube_phone_sync_scope(void) {}
PlaybackSource g_start_src;
pthread_t g_start_thread;
std::atomic<bool> g_start_running{false}, g_start_done{false}, g_start_success{false};
char evo_stream_headers[1024], evo_stream_user_agent[128];
static int chosen_count, match=1, finish_count;
static bool active=true;
static std::string chosen_url, error;
static evo_provider_t provider;
extern "C" const evo_provider_t* evo_provider_find(const char*) { return &provider; }
extern "C" int tube_recovery_choice(const evo_stream_choice_t*, int) { return active ? match : -1; }
extern "C" int tube_recovery_active(void) { return active; }
extern "C" void tube_recovery_finish(void) { ++finish_count; active=false; }
extern "C" void tube_set_error(const char* text) { error=text; }
static void chosen(const char*, const evo_stream_choice_t* choice) { ++chosen_count; chosen_url=choice->url; }
static void evo_bt(const char*, ...) {}
static void toast(const char*, const char*) {}
static int evo_failure_reason(char*, size_t) { return 0; }
static void evo_rmlui_provider_set_tuning(int) {}
static void evo_rmlui_provider_set_loading(int, const char*) {}
static void evo_rmlui_provider_set_status(const char*, int) {}
static bool choice_is_hls(const evo_stream_choice_t& c) { return !strcmp(c.container,"hls"); }
static void on_variants() {}
template<class... A> static int evo_hls_variants_fetch(A...) { assert(false); return -1; }
#include "recovery_handoff.inc"
static void complete_worker() {
#include "recovery_complete.inc"
}
static void prepare() {
    assert(!g_start_running);
    memset(&g_pending,0,sizeof g_pending);
    snprintf(g_pending.provider,sizeof g_pending.provider,"youtube");
    snprintf(g_pending.item_id,sizeof g_pending.item_id,"abcdefghijk");
    snprintf(g_pending.title,sizeof g_pending.title,"Recovery fixture");
    g_pending.ask=1; g_pending.resume_sec=37;
    g_resolve_finished=false; g_tube_recovery_launch=true; active=true;
    g_start_done=false; g_start_success=false;
}
static void join_worker() {
    assert(g_start_running); assert(!pthread_join(g_start_thread,nullptr));
    g_start_running=false; assert(g_start_done);
}
int main() {
    provider.caps=EVO_PROVIDER_CAP_PICK; provider.stream_chosen=chosen;
    evo_stream_choice_t choices[2]={};
    snprintf(choices[0].url,sizeof choices[0].url,"http://127.0.0.1:8765/media/wrong/720");
    snprintf(choices[1].url,sizeof choices[1].url,"http://127.0.0.1:8765/media/fresh/1080");
    snprintf(choices[1].headers,sizeof choices[1].headers,"X-Fixture: selected\r\n");
    prepare();
    on_tube_resolved(1,choices,2,(void*)(uintptr_t)41);
    assert(chosen_count==0 && !g_start_running); // late callback cannot launch
    on_tube_resolved(1,choices,2,(void*)(uintptr_t)42); join_worker();
    assert(chosen_count==1 && chosen_url==choices[1].url && g_pending.choice_count==1 && !g_pending.ask);
    assert(playback.opened.url==choices[1].url && playback.opened.item_id=="abcdefghijk");
    assert(playback.position==37.625 && !playback.opened.is_live);
    assert(!strcmp(evo_stream_headers,choices[1].headers));
    complete_worker(); assert(g_start_success && !active && !g_tube_recovery_launch);
    assert(prospero_subtitle_delay_ms==450 && prospero_secondary_delay_ms==-300 && prospero_subtitle_time_scale==1.25);
    assert(prospero_subtitle_enabled==0 && finish_count==1);
    prepare(); choices[1].is_live=1;
    on_tube_resolved(1,choices,2,(void*)(uintptr_t)42); join_worker();
    assert(playback.opened.is_live && playback.position==0);
    complete_worker();
    prepare(); match=-1;
    int before=chosen_count;
    on_tube_resolved(1,choices,2,(void*)(uintptr_t)42);
    assert(chosen_count==before && !g_start_running && g_resolve_finished && !g_tube_recovery_launch);
    assert(!error.empty());
    prepare(); match=1; choices[1].is_live=0;
    on_tube_resolved(1,choices,2,(void*)(uintptr_t)42); join_worker();
    active=false; int stops=playback.stops;
    complete_worker(); assert(!g_start_success && playback.stops==stops+1);
    prepare(); playback.answer=false; prospero_subtitle_enabled=1;
    on_tube_resolved(1,choices,2,(void*)(uintptr_t)42); join_worker(); complete_worker();
    assert(!g_start_success && !g_tube_recovery_launch && prospero_subtitle_enabled==1);
    printf("PASS: exact generated recovery callback, pthread launch, fractional resume, live edge, captions and changed-context stop\n");
}
