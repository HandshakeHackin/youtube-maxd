"""SponsorBlock actions using the player's existing toast and pad controls."""


def apply_sponsor_overlay(replace, player):
    screen = player + 'core/src/screens/PlayerScreen.cpp'
    replace(screen, '    if (!ended && !playback->isPaused() && !playback->isScrubbing()) {',
            '    if (!ended && !playback->isPaused() && !playback->isScrubbing() && !playback->isLiveSource() &&\n        !evo_demux_is_seeking() && !g_pp_pb.seek_discarding && !pb_prebuffer_hold) {')
    replace(screen, '    // Scrub hold update', '''    if (tube_sponsor_prompt(playback->getPositionSeconds()) &&
        (pressed & (PadButtons::Cross | PadButtons::Circle))) {
        const bool accept = (pressed & PadButtons::Cross) && !(pressed & PadButtons::Circle);
        if (accept && (playback->isLiveSource() || playback->isScrubbing() || evo_pb_is_complete() ||
            evo_demux_is_seeking() || g_pp_pb.seek_discarding || pb_prebuffer_hold)) return true;
        double target = tube_sponsor_confirm(playback->getPositionSeconds(), accept);
        m_tubeSponsorPromptAt = 0;
        toast("SPONSORBLOCK", accept ? "Skipping segment" : "Keeping this segment");
        if (tube_sponsor_finish(target, playback->getDurationSeconds())) {
            playback->stopPlayback();
            if (auto sm = Application::getInstance().getScreenManager()) sm->returnFromPlayback();
            return true;
        }
        if (target >= 0 && target < playback->getDurationSeconds()) playback->seekTo(target, evo_pb_is_paused() != 0);
        return true;
    }

    // Scrub hold update''')
    replace(screen, '    // Scrub hold update', '''    // ProsperoTube highlight input (extracted by host dispatch/player tests).
    if (pressed & PadButtons::R3) {
        if (playback->isLiveSource() || playback->isScrubbing() || evo_pb_is_complete() ||
            evo_demux_is_seeking() || g_pp_pb.seek_discarding || pb_prebuffer_hold) return true;
        double target = tube_highlight_target(playback->getPositionSeconds(), playback->getDurationSeconds());
        if (target >= 0 && target < playback->getDurationSeconds()) {
            playback->seekTo(target, evo_pb_is_paused() != 0);
            if (tube_sponsor_show_message()) toast("SPONSORBLOCK", "Jumped to highlight");
        } else {
            const char *prompt = tube_sponsor_prompt(playback->getPositionSeconds());
            toast("SPONSORBLOCK", prompt ? prompt : "No available upcoming highlight");
        }
        return true;
    }
    // End ProsperoTube highlight input.

    // Scrub hold update''')
    replace(screen, '            playback->seekTo(target); toast("SPONSORBLOCK", "Skipped an optional segment");', '''            playback->seekTo(target);
            if (tube_sponsor_show_message()) toast("SPONSORBLOCK", "Skipped an optional segment");''')
    replace(screen, '''        double target = tube_skip_target(playback->getPositionSeconds());
        if (target >= 0 && target < playback->getDurationSeconds()) {''', '''        double target = tube_skip_target(playback->getPositionSeconds());
        if (tube_sponsor_finish(target, playback->getDurationSeconds())) {
            if (tube_sponsor_show_message()) toast("SPONSORBLOCK", "Skipped the final segment");
            playback->stopPlayback();
            if (auto sm = Application::getInstance().getScreenManager()) sm->returnFromPlayback();
            return;
        }
        if (target >= 0 && target < playback->getDurationSeconds()) {''')
    replace(screen, '        double target = tube_skip_target(playback->getPositionSeconds());', '''        double target = tube_skip_target(playback->getPositionSeconds());
        const char *prompt = tube_sponsor_prompt(playback->getPositionSeconds());
        if (!prompt) m_tubeSponsorPromptAt = 0;
        if (prompt && (!m_tubeSponsorPromptAt || NowMs() - m_tubeSponsorPromptAt >= 2000)) {
            m_tubeSponsorPromptAt = NowMs(); toast("SPONSORBLOCK", prompt);
        }''')
    replace(player + 'core/include/evo/screens/PlayerScreen.hpp', '    bool m_showStatsForNerds', '    uint64_t m_tubeSponsorPromptAt = 0;\n    bool m_showStatsForNerds')
    kit = player + 'ui_kit/src/evo_hui_osd.cpp'
    replace(kit, '#include "evo_hui_osd.hpp"', '#include "evo_hui_osd.hpp"\n#include "tube_sponsor_draw.h"')
    replace(kit, '        const float tx = track.x + track.w * f;', '''        // ProsperoTube SponsorBlock marker draw (also extracted by host tests).
        tube_sponsor_marker_t markers[128];
        int marker_count = tube_sponsor_markers(raw_.duration_sec, markers, 128);
        for (int i = 0; i < marker_count; ++i) {
            double left, size;
            if (tube_sponsor_geometry(&markers[i], track.w, &left, &size))
                list.rounded_rect({track.x + (float)left, track.y, (float)size, track.h},
                                  1, Color::rgb(markers[i].rgb));
        }
        // End ProsperoTube SponsorBlock marker draw.
        const float tx = track.x + track.w * f;''')
    app = player + 'ui_rml/src/evo_rmlui_app.cpp'
    replace(app, '#include "evo_rmlui_app.h"', '#include "evo_rmlui_app.h"\n#include "tube_sponsor_draw.h"')
    replace(app, '''    ShowOnlyScreen(m_playback_doc);
    if (m_nav_doc) m_nav_doc->Hide();''', '''    // ProsperoTube SponsorBlock marker RML (also extracted by host tests).
    if (auto* el = m_playback_doc->GetElementById("sponsor-markers")) {
        tube_sponsor_marker_t markers[128];
        int marker_count = tube_sponsor_markers(m_last_state.duration_sec, markers, 128);
        std::string markup;
        for (int i = 0; i < marker_count; ++i) {
            double left, size; char element[160];
            if (!tube_sponsor_geometry(&markers[i], 1800, &left, &size)) continue;
            std::snprintf(element, sizeof element,
                "<div class=\\"sponsor-marker\\" style=\\"left:%.6f%%;width:%.6f%%;background-color:#%06x;\\"></div>",
                left / 18.0, size / 18.0, markers[i].rgb & 0xffffff);
            markup += element;
        }
        if (el->GetInnerRML() != markup) el->SetInnerRML(markup);
    }
    // End ProsperoTube SponsorBlock marker RML.
    ShowOnlyScreen(m_playback_doc);
    if (m_nav_doc) m_nav_doc->Hide();''')
    replace(player + 'assets/rml/playback.rml', '''                <div id="progress-fill">
                    <div id="progress-thumb"></div>
                </div>''', '''                <div id="progress-fill">
                    <div id="progress-thumb"></div>
                </div>
                <div id="sponsor-markers"></div>''')
    replace(player + 'assets/rml/playback.rcss', '#progress-thumb {', '''#sponsor-markers {
    position: absolute;
    top: 0;
    left: 0;
    width: 100%;
    height: 8dp;
}
.sponsor-marker {
    position: absolute;
    top: 0;
    height: 8dp;
}
#progress-thumb {
    z-index: 2;''')
