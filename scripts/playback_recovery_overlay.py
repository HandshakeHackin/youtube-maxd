"""Reopen failed YouTube input through the existing bounded launch worker."""


def apply_playback_recovery_overlay(replace, player, host):
    replace(host, "static double g_start_resume = 0.0;", """static double g_start_resume = 0.0;
static bool g_tube_recovery_launch;
static double g_tube_recovery_resume;
static int g_tube_recovery_subtitles;
static int g_tube_recovery_subtitle_delay, g_tube_recovery_secondary_delay;
static double g_tube_recovery_subtitle_scale;""")
    replace(host, "    g_start_resume = src.is_live ? 0.0 : (double)g_pending.resume_sec;", """    g_start_resume = src.is_live ? 0.0 : g_tube_recovery_launch
        ? g_tube_recovery_resume : (double)g_pending.resume_sec;""")
    replace(host, """    if ((unsigned)(uintptr_t)ud == g_pick_gen)
        on_resolved(ok, choices, count, &g_pending);""", """    if ((unsigned)(uintptr_t)ud != g_pick_gen) return;
    if (g_tube_recovery_launch) {
        int selected = ok ? tube_recovery_choice(choices, count) : -1;
        if (selected < 0) {
            tube_recovery_finish(); g_tube_recovery_launch = false;
            tube_set_error("The previous stream could not be restored. Select the video again.");
            on_resolved(0, nullptr, 0, &g_pending); return;
        }
        g_pending.ask = 0;
        on_resolved(1, &choices[selected], 1, &g_pending);
        return;
    }
    on_resolved(ok, choices, count, &g_pending);""")
    replace(host, "if (rp && (rp->caps & EVO_PROVIDER_CAP_PROGRESS) && !(rp->caps & EVO_PROVIDER_CAP_LIVE))", "if (!tube_recovery_pending() && rp && (rp->caps & EVO_PROVIDER_CAP_PROGRESS) && !(rp->caps & EVO_PROVIDER_CAP_LIVE))")
    replace(host, "    evo_provider_selection_t sel;", """    if (m_resolving || g_start_running) return;
    evo_provider_selection_t sel;""")
    replace(host, """    evo_provider_item_t queued;
    if (tube_take_next(&queued)) {""", """    evo_provider_item_t queued;
    if (tube_take_recovery(&queued, &g_tube_recovery_resume, &g_tube_recovery_subtitles)) {
        g_tube_recovery_launch = true;
        g_tube_recovery_subtitle_delay = prospero_subtitle_delay_ms;
        g_tube_recovery_secondary_delay = prospero_secondary_delay_ms;
        g_tube_recovery_subtitle_scale = prospero_subtitle_time_scale;
        std::memset(&sel, 0, sizeof sel);
        std::snprintf(sel.provider_id, sizeof sel.provider_id, "youtube");
        std::snprintf(sel.item_id, sizeof sel.item_id, "%s", queued.id);
        std::snprintf(sel.title, sizeof sel.title, "%s", queued.title);
        sel.is_live = queued.is_live;
    } else if (tube_take_next(&queued)) {
        g_tube_recovery_launch = false;""")
    replace(host, "    } else if (!evo_rmlui_provider_take_selection(&sel)) return;", """    } else {
        if (!evo_rmlui_provider_take_selection(&sel)) return;
        tube_recovery_finish(); g_tube_recovery_launch = false;
    }""")
    replace(host, "        if (g_start_success) {", """        if (g_tube_recovery_launch) {
            if (g_start_success && !tube_recovery_active()) {
                if (auto pb = Application::getInstance().getPlaybackController()) pb->stopPlayback();
                g_start_success = false;
            }
            if (g_start_success) {
                prospero_subtitle_enabled = g_tube_recovery_subtitles;
                prospero_subtitle_delay_ms = g_tube_recovery_subtitle_delay;
                prospero_secondary_delay_ms = g_tube_recovery_secondary_delay;
                prospero_subtitle_time_scale = g_tube_recovery_subtitle_scale;
            }
            tube_recovery_finish(); g_tube_recovery_launch = false;
        }
        if (g_start_success) {""")
    replace(host, """                sm->navigateTo(ScreenId::Player);
            }
        } else {""", """                sm->navigateTo(ScreenId::Player);
            }
            // ProsperoTube paid-content notice after successful start.
            if (m_providerId == "youtube") {
                if (const char* notice = tube_take_paid_content_notice()) toast("PAID PROMOTION", notice);
            }
            // End ProsperoTube paid-content notice.
            return; /* the browser must not consume another selection after navigation */
        } else {""")
    replace(host, '#include "evo_toast.h"', '#include "evo_toast.h"\n#include "evo_subtitle.h"')
    replace(host, '                toast("STREAM", have ? why : "That item could not be played");', '''                if (g_tube_recovery_launch) { tube_recovery_finish(); g_tube_recovery_launch = false; }
                toast("STREAM", have ? why : "That item could not be played");''')
    replace(host, "    if (rc != 0) {\n        g_start_running = false;", """    if (rc != 0) {
        tube_recovery_finish(); g_tube_recovery_launch = false;
        g_start_running = false;""")
    replace(host, "    if (m_opened && !m_navigatingToPlayer) {", """    if (!m_navigatingToPlayer) { tube_recovery_finish(); g_tube_recovery_launch = false; }
    if (m_opened && !m_navigatingToPlayer) {""")
    screen = player + "core/src/screens/PlayerScreen.cpp"
    replace(screen, "    uint64_t now = NowMs();\n    /*", """    if (evo_demux_input_failed() && !playback->isPaused() && !evo_pb_is_paused() &&
        !playback->isScrubbing() && !evo_demux_is_seeking()) {
        if (tube_queue_recovery(playback->getPositionSeconds(), playback->isLiveSource(), prospero_subtitle_enabled)) {
            toast("PLAYBACK", "Refreshing this stream and restoring playback...");
            playback->stopPlayback();
            if (auto sm = Application::getInstance().getScreenManager()) sm->returnFromPlayback();
            return;
        }
        if (!m_tubeFailureShown) {
            m_tubeFailureShown = true;
            toast("STREAM STOPPED", "Recovery is unavailable. Stop and select the video again.");
        }
    }
    uint64_t now = NowMs();
    /*""")
    replace(screen, "    m_osdVisibilityAlpha = 255;", "    m_osdVisibilityAlpha = 255;\n    m_tubeFailureShown = false;")
    replace(screen, "                              pb_prebuffer_hold || stalled);", "                              pb_prebuffer_hold || stalled || evo_demux_input_failed());")
    replace(player + "core/include/evo/screens/PlayerScreen.hpp", "    bool m_stallToastShown = false;", "    bool m_tubeFailureShown = false;\n    bool m_stallToastShown = false;")
