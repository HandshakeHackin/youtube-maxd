"""Per-profile hidden-Cross behavior and controls timeout on the shipping player."""


def apply_player_controls_overlay(replace, player):
    screen = player + 'core/src/screens/PlayerScreen.cpp'
    replace(player + 'core/include/evo/screens/PlayerScreen.hpp',
            '    uint64_t m_controlsLastUsedMs',
            '    bool m_tubeSuppressControls = false;\n    uint64_t m_controlsLastUsedMs')
    replace(screen, '    m_osdVisibilityAlpha = 255;',
            '    m_osdVisibilityAlpha = 255;\n    m_tubeSuppressControls = false;')
    replace(screen, '''    if (pressed) {
        m_controlsLastUsedMs = NowMs();
    }''', '''    // ProsperoTube hidden Cross activity.
    const bool hiddenCross = m_osdVisibilityAlpha <= 0 && (pressed & PadButtons::Cross);
    const int crossMode = tube_player_ok_mode();
    if (pressed) {
        // Simultaneous navigation still exposes controls; only a lone Cross suppresses them.
        m_tubeSuppressControls = hiddenCross && pressed == PadButtons::Cross && crossMode >= 2;
        if (!m_tubeSuppressControls) m_controlsLastUsedMs = NowMs();
    }
    // End ProsperoTube hidden Cross activity.''')
    replace(screen, '''            // Toggle play / pause
            playback->togglePause();
            toast("PLAYBACK", playback->isPaused() ? "PAUSED" : "RESUMED");''', '''            // ProsperoTube hidden Cross action.
            if (hiddenCross && crossMode == 0) {
                // Timestamp above exposes controls without changing playback.
            } else if (hiddenCross && crossMode == 3) {
                const int current = playback->playbackSpeedPercent();
                const int target = current == 100 ? tube_last_playback_speed() : 100;
                if (playback->canChangePlaybackSpeed() && playback->changePlaybackSpeed(target)) {
                    char label[48]; std::snprintf(label,sizeof label,"%.3gx",target / 100.0);
                    toast("SPEED",label);
                } else toast("SPEED","Unable to change speed; try again when playback is ready");
            } else {
                playback->togglePause();
                toast("PLAYBACK", playback->isPaused() ? "PAUSED" : "RESUMED");
            }
            // End ProsperoTube hidden Cross action.''')
    replace(screen, '''    bool wantsControls = playback->isMusicMode() || ended ||
                         ((now - m_controlsLastUsedMs < 3500) || reallyPaused || m_showStatsForNerds) ||
                         playback->isScrubbing() || isBuffering;''', '''    // ProsperoTube controls visibility.
    const int hideMs = tube_player_hide_ms();
    bool wantsControls = playback->isMusicMode() || ended ||
                         (!m_tubeSuppressControls && (hideMs == 0 ||
                          now - m_controlsLastUsedMs < static_cast<uint64_t>(hideMs) || reallyPaused)) ||
                         m_showStatsForNerds || playback->isScrubbing() || isBuffering;
    // End ProsperoTube controls visibility.''')
    replace(player + 'ui_rml/src/evo_rmlui_provider.cpp', 'id != "settings:sleep-timer"',
            'id != "settings:sleep-timer" && id != "settings:player-ok" && id != "settings:player-hide"')
    picker = player + 'core/src/screens/AudioTrackPickerScreen.cpp'
    replace(player + 'core/include/evo/screens/AudioTrackPickerScreen.hpp',
            'KindAudio, KindSpeed, KindChat, KindSleep };',
            'KindAudio, KindSpeed, KindChat, KindSleep, KindCross, KindHide };')
    replace(picker, '    // End ProsperoTube chat option rows.', '''    // End ProsperoTube chat option rows.
    const char *crossLabels[] = {"Cross: show controls", "Cross: controls and pause", "Cross: pause only", "Cross: toggle speed"};
    for (int mode = 0; mode < 4; ++mode) {
        Entry e; e.kind = KindCross; e.streamIndex = mode; e.label = crossLabels[mode];
        e.detail = "When controls are hidden; saved for this profile";
        e.badge = tube_player_ok_mode() == mode ? "SELECTED" : "";
        m_tracks.push_back(std::move(e));
    }
    for (int seconds = 0; seconds <= 15; ++seconds) {
        Entry e; e.kind = KindHide; e.streamIndex = seconds;
        e.label = seconds ? "Hide controls after " + std::to_string(seconds) + " seconds" : "Never hide controls";
        e.detail = "Saved for this profile";
        e.badge = tube_player_hide_ms() == seconds * 1000 ? "SELECTED" : "";
        m_tracks.push_back(std::move(e));
    }''')
    replace(picker, '    if (e.kind == KindSleep) {', '''    if (e.kind == KindCross || e.kind == KindHide) {
        const bool saved = e.kind == KindCross ? tube_player_set_ok_mode(e.streamIndex) : tube_player_set_hide_seconds(e.streamIndex);
        toast("PLAYER",saved ? e.label.c_str() : "Unable to save preference for this profile");
    } else if (e.kind == KindSleep) {''')
