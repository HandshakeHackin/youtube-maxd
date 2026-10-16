#include "tube_update.h"
#include <assert.h>
#include <string.h>
static tube_update_status state;
static int accepted, applied, cancelled, fail_apply;
void tube_update_get(tube_update *u, tube_update_status *out) { (void)u; *out = state; }
int tube_update_check(tube_update *u, int manual) { (void)u; (void)manual; return 1; }
void tube_update_free(tube_update *u) { (void)u; }
int tube_update_take_offer(tube_update *u) { (void)u; return 0; }
const char *tube_update_notes(tube_update *u) { (void)u; return "Notes"; }
void tube_update_set_notify(tube_update *u, int on) { (void)u; (void)on; }
void tube_update_skip(tube_update *u, const char *version) { (void)u; (void)version; }
int tube_update_download(tube_update *u) { (void)u; if (state.blocked) return 0; ++accepted; state.phase = TUBE_UPDATE_DOWNLOADING; return 1; }
void tube_update_cancel(tube_update *u) { (void)u; ++cancelled; state.phase = TUBE_UPDATE_IDLE; }
void tube_update_discard(tube_update *u) { (void)u; }
int tube_update_apply(tube_update *u) { (void)u; if (fail_apply || state.blocked) { strcpy(state.error, "Apply failed"); return 0; } ++applied; state.phase = TUBE_UPDATE_APPLIED; return 1; }
int main(void) {
    int token;
    state.phase = TUBE_UPDATE_STAGED;
    tube_update_app_use((tube_update *)&token);
    tube_update_status snapshot;
    assert(tube_update_app_offer(&snapshot)); // recovered prepared update still needs consent
    tube_update_app_tick(1); assert(!applied && !tube_update_app_take_restart());
    tube_update_app_classic_notice(); assert(strstr(tube_update_app_take_notice(), "Settings, About"));
    tube_update_app_classic_notice(); assert(!tube_update_app_take_notice());
    tube_update_app_action("install"); tube_update_app_tick(1);
    assert(applied == 1 && tube_update_app_take_restart() && !tube_update_app_take_restart());
    state.phase = TUBE_UPDATE_IDLE; tube_update_app_use((tube_update *)&token);
    tube_update_app_action("install"); assert(accepted == 1);
    tube_update_app_action("cancel"); assert(cancelled == 1);
    state.phase = TUBE_UPDATE_STAGED; tube_update_app_tick(1); assert(applied == 1);
    fail_apply = 1; tube_update_app_action("install"); tube_update_app_tick(1);
    assert(!tube_update_app_take_restart() && strstr(tube_update_app_take_notice(), "failed"));
    state.blocked = 1; state.phase = TUBE_UPDATE_IDLE; tube_update_app_use((tube_update *)&token);
    tube_update_app_action("install"); assert(!tube_update_app_progress(&snapshot) && accepted == 1);
}
