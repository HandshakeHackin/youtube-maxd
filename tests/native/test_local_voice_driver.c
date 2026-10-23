/* Local voice lifecycle: no network/audio files. GPL-3.0-or-later. */
#include "tube_voice.h"
#include "local_asr.h"
#include "evo_net.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int capture = -1, busy, model = 1, asr, starts, net_calls, fail_start;
static int16_t audio[96000];
static const char *recognized = "aurora chasing";
static char selected_model[512];
int tube_remote_owner_current(unsigned owner) { return owner == 7; }
int tube_microphone_begin(unsigned ms) { assert(ms == 6000); if (busy) return 0; busy = 1; capture = 0; return 1; }
void tube_microphone_cancel(void) { capture = -1; }
void tube_microphone_finish(void) { capture = 1; }
void tube_microphone_release(void) { if (capture != 0) busy = 0; }
void tube_microphone_shutdown(void) { capture = -1; busy = 0; }
int tube_microphone_poll(const int16_t **pcm, size_t *samples) {
    if (pcm) *pcm = capture == 1 ? audio : NULL;
    if (samples) *samples = capture == 1 ? 96000 : 0;
    return capture;
}
int maxd_asr_available(const char *path) {
    if (!strcmp(path, "/app0/assets/voice/ggml-tiny.bin")) return model & 1;
    if (!strcmp(path, "/system_ex/app/PPSA99991/assets/voice/ggml-tiny.bin")) return model & 2;
    assert(!strcmp(path, "/data/homebrew/PPSA99991/assets/voice/ggml-tiny.bin"));
    return model & 4;
}
int maxd_asr_start(const char *path, const int16_t *pcm, size_t samples) {
    assert(path && pcm == audio && samples == 96000);
    snprintf(selected_model, sizeof selected_model, "%s", path);
    ++starts; asr = 0; return !fail_start;
}
int maxd_asr_poll(char *out, size_t cap) { if (asr == 1) { assert(strlen(recognized) < cap); strcpy(out, recognized); } return asr; }
void maxd_asr_cancel(void) { asr = -1; }
void maxd_asr_release(void) {}
void maxd_asr_shutdown(void) { asr = -1; }
const char *maxd_asr_error(void) { return "Synthetic recognition failure"; }
int evo_net_request_async(const char *method, const char *url, const char *body,
    const char **headers, int n, evo_net_cb cb, void *ctx) {
    (void)method; (void)url; (void)body; (void)headers; (void)n; (void)cb; (void)ctx;
    ++net_calls; return -1;
}
static void transcribe(void) {
    assert(tube_voice_begin(0, 100));
    assert(!tube_voice_begin(0, 101));
    tube_voice_finish(); tube_voice_tick(200);
    assert(tube_voice_state() == TUBE_VOICE_TRANSCRIBING && !busy);
}
int main(void) {
    tube_voice_configure_local("guest");
    assert(tube_voice_available() == 1 && !tube_voice_begin(7, 0) && !tube_voice_begin(0, -1));
    tube_voice_tick(0); assert(!net_calls);
    transcribe(); asr = 1; tube_voice_tick(250);
    assert(tube_voice_state() == TUBE_VOICE_READY && !strcmp(tube_voice_result(), recognized));
    char query[801]; assert(!tube_voice_take(query, 3) && tube_voice_take(query, sizeof query));
    assert(!strcmp(query, recognized) && !*tube_voice_result());
    transcribe(); tube_voice_configure_local("other"); asr = 1; tube_voice_tick(300);
    assert(tube_voice_state() == TUBE_VOICE_IDLE && !tube_voice_take(query, sizeof query));
    transcribe(); tube_voice_cancel(); asr = 1; tube_voice_tick(300);
    assert(tube_voice_state() == TUBE_VOICE_IDLE && !*tube_voice_result());
    assert(tube_voice_begin(0, 100)); tube_voice_configure_local("guest");
    tube_voice_finish(); tube_voice_tick(200); assert(tube_voice_state() == TUBE_VOICE_IDLE);
    transcribe(); asr = -1; tube_voice_tick(300);
    assert(tube_voice_state() == TUBE_VOICE_FAILED && strstr(tube_voice_error(), "Synthetic"));
    tube_voice_cancel(); transcribe(); recognized = "bad\nquery"; asr = 1; tube_voice_tick(300);
    assert(tube_voice_state() == TUBE_VOICE_FAILED && !tube_voice_take(query, sizeof query));
    tube_voice_cancel(); assert(tube_voice_begin(0, 100)); capture = -1; tube_voice_tick(200);
    assert(tube_voice_state() == TUBE_VOICE_FAILED);
    tube_voice_cancel(); fail_start = 1; assert(tube_voice_begin(0, 100)); tube_voice_finish(); tube_voice_tick(200);
    assert(tube_voice_state() == TUBE_VOICE_FAILED); fail_start = 0;
    tube_voice_cancel(); transcribe(); tube_voice_tick(75100);
    assert(tube_voice_state() == TUBE_VOICE_FAILED);
    /* A later model failure must replace an earlier microphone/timeout error. */
    model = 0;
    assert(!tube_voice_begin(0, 100));
    assert(strstr(tube_voice_error(), "Synthetic recognition failure"));
    model = 0; tube_voice_configure_local("missing-model");
    assert(strstr(tube_voice_error(), "Synthetic recognition failure"));
    assert(tube_voice_available() == -1 && !tube_voice_begin(0, 100));
    /* A failed startup probe must not disable L3 for the entire profile.
     * Model visibility can change after the app's launch/access transition. */
    model = 1; tube_voice_configure_local("missing-model");
    assert(tube_voice_begin(0, 100));
    assert(tube_voice_available() == 1);
    tube_voice_cancel();
    /* A model removed since configuration must fail before microphone capture. */
    model = 0;
    assert(!tube_voice_begin(0, 100) && !busy);
    assert(tube_voice_available() == -1);
    /* Installed model is readable, but not mounted at /app0. Exercise capture
     * and transcription, not just the availability predicate. */
    recognized = "погода завтра";
    for (int location = 2; location <= 4; location += 2) {
        model = location;
        transcribe();
        assert(strstr(selected_model, location == 2 ? "/system_ex/app/" : "/data/homebrew/"));
        asr = 1; tube_voice_tick(250);
        assert(tube_voice_take(query, sizeof query) && !strcmp(query, recognized));
    }
    /* Prefer the app mount if more than one install tree exists. */
    model = 7; transcribe();
    assert(!strcmp(selected_model, "/app0/assets/voice/ggml-tiny.bin"));
    tube_voice_cancel();
    model = 1; tube_voice_configure_local("guest");
    assert(tube_voice_begin(0, 100)); tube_voice_shutdown(); asr = 1; tube_voice_tick(200);
    assert(tube_voice_state() == TUBE_VOICE_IDLE && !tube_voice_available());
    assert(starts >= 7 && net_calls == 0);
    puts("PASS: local voice capture, query validation, profile retirement, cancel, timeout, model errors, physical ownership and zero network calls");
}
