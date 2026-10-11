"""Unavailable-only phone queue advance on the normal resolver path."""


def apply_phone_queue_failure_overlay(replace, host):
    replace(host, '        // End ProsperoTube unavailable queue schedule.\n        if (g_phone_ticket)', '''        // End ProsperoTube unavailable queue schedule.
        // ProsperoTube unavailable phone schedule.
        if (g_phone_ticket && tube_resolve_unavailable(pp->item_id)) {
            const int disposition = tube_phone_launch_unavailable(g_phone_ticket, pp->item_id, queue_retry_now_ms());
            if (disposition) {
                g_phone_ticket = 0; g_resolve_finished = true; pp->choice_count = 0;
                evo_rmlui_provider_set_tuning(0); evo_rmlui_provider_set_loading(0, "");
                evo_rmlui_provider_set_status(disposition == 1 ?
                    "Video unavailable. Next phone playlist item in 5 seconds; any button cancels." :
                    "Video unavailable. No further phone playlist item.", 0);
                return;
            }
        }
        // End ProsperoTube unavailable phone schedule.
        if (g_phone_ticket)''')
    replace(host, '    // End ProsperoTube unavailable queue wait.\n    evo_provider_selection_t sel;', '''    // End ProsperoTube unavailable queue wait.
    // ProsperoTube unavailable phone wait.
    if (tube_phone_retry_waiting()) return;
    // End ProsperoTube unavailable phone wait.
    evo_provider_selection_t sel;''')
    replace(host, '    // End ProsperoTube unavailable queue input cancellation.\n    (void)held;', '''    // End ProsperoTube unavailable queue input cancellation.
    // ProsperoTube unavailable phone input cancellation.
    if (pressed && tube_phone_retry_waiting()) {
        tube_phone_cancel(); g_phone_ticket = 0;
        evo_rmlui_provider_set_status("Phone queue advance cancelled. Choose a video to play.", 0);
    }
    // End ProsperoTube unavailable phone input cancellation.
    (void)held;''')
