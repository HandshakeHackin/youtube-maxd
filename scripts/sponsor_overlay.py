"""SponsorBlock actions using the player's existing toast and pad controls."""


def apply_sponsor_overlay(replace, player):
    screen = player + 'core/src/screens/PlayerScreen.cpp'
    replace(screen, '    if (!ended && !playback->isPaused() && !playback->isScrubbing()) {',
            '    if (!ended && !playback->isPaused() && !playback->isScrubbing() && !playback->isLiveSource() &&\n        !evo_demux_is_seeking() && !g_pp_pb.seek_discarding && !pb_prebuffer_hold) {')
    replace(screen, '    // Scrub hold update', '''    if (tube_sponsor_prompt(playback->getPositionSeconds()) &&
        (pressed & (PadButtons::Cross | PadButtons::Circle))) {
        const bool accept = (pressed & PadButtons::Cross) && !(pressed & PadButtons::Circle);
        double target = tube_sponsor_confirm(playback->getPositionSeconds(), accept);
        m_tubeSponsorPromptAt = 0;
        toast("SPONSORBLOCK", accept ? "Skipping segment" : "Keeping this segment");
        if (tube_sponsor_finish(target, playback->getDurationSeconds())) {
            playback->stopPlayback();
            if (auto sm = Application::getInstance().getScreenManager()) sm->returnFromPlayback();
            return true;
        }
        if (target >= 0 && target < playback->getDurationSeconds()) playback->seekTo(target);
        return true;
    }

    // Scrub hold update''')
    replace(screen, '            playback->seekTo(target); toast("SPONSORBLOCK", "Skipped an optional segment");', '''            playback->seekTo(target);
            if (tube_sponsor_show_message()) toast("SPONSORBLOCK", "Skipped an optional segment");''')
    replace(screen, '        if (target >= 0 && target < playback->getDurationSeconds()) {', '''        if (tube_sponsor_finish(target, playback->getDurationSeconds())) {
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
