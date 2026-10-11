"""SmartTube's five-second unplayable-item advance, with native queue ownership."""


def apply_queue_failure_overlay(replace, host):
    replace(host, '#include "evo/screens/ProviderHostScreen.hpp"', '#include "evo/screens/ProviderHostScreen.hpp"\n#include <chrono>')
    replace(host, 'static unsigned g_phone_ticket;', """static long long g_queue_retry_deadline;
static long long queue_retry_now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
static unsigned g_phone_ticket;""")
    replace(host, '    if (!ok || count <= 0 || !choices) {\n        if (g_phone_ticket)', """    if (!ok || count <= 0 || !choices) {
        // ProsperoTube unavailable queue schedule.
        if (!g_phone_ticket && !g_tube_recovery_launch && !std::strcmp(pp->provider,"youtube") &&
            tube_queue_skip_unavailable(pp->item_id)) {
            g_queue_retry_deadline = queue_retry_now_ms() + 5000;
            g_resolve_finished = true; pp->choice_count = 0;
            toast("VIDEO UNAVAILABLE", "Playing the next queued video in 5 seconds. Any button cancels.");
            evo_rmlui_provider_set_tuning(0); evo_rmlui_provider_set_loading(0, "");
            evo_rmlui_provider_set_status("Video unavailable. Next queued video in 5 seconds; any button cancels.",0);
            return;
        }
        // End ProsperoTube unavailable queue schedule.
        if (g_phone_ticket)""")
    replace(host, '    if (m_resolving || g_start_running) return;\n    evo_provider_selection_t sel;', """    if (m_resolving || g_start_running) return;
    // ProsperoTube unavailable queue wait.
    if (g_queue_retry_deadline && (!tube_queue_retry_pending() || tube_phone_launch_pending())) {
        g_queue_retry_deadline = 0; tube_queue_retry_cancel();
    }
    if (g_queue_retry_deadline && queue_retry_now_ms() < g_queue_retry_deadline) return;
    g_queue_retry_deadline = 0;
    // End ProsperoTube unavailable queue wait.
    evo_provider_selection_t sel;""")
    replace(host, '    (void)held;\n    (void)released;', """    // ProsperoTube unavailable queue input cancellation.
    if (pressed && g_queue_retry_deadline) {
        g_queue_retry_deadline = 0; tube_queue_retry_cancel();
        evo_rmlui_provider_set_status("Queue advance cancelled. Choose a video to play.",0);
    }
    // End ProsperoTube unavailable queue input cancellation.
    (void)held;
    (void)released;""")
    replace(host, '    const bool joined_phone_worker =', """    // ProsperoTube unavailable queue exit cancellation.
    g_queue_retry_deadline = 0;
    if (!m_navigatingToPlayer) tube_queue_retry_cancel();
    // End ProsperoTube unavailable queue exit cancellation.
    const bool joined_phone_worker =""")
