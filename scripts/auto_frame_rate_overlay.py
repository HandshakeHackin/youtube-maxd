"""Keep playback output modes stable across player menus."""

# Console selector sweep R92 found only 59.94/119.88 Hz; 25/50 fps stay at 59.94.


def apply_auto_frame_rate_overlay(replace, player):
    replace(player + "core/include/evo/services/SettingsService.hpp",
            "m_refreshRateMode = RefreshRateMode::Off;",
            "m_refreshRateMode = RefreshRateMode::PlaybackOnly;")
    app = player + "core/src/Application.cpp"
    replace(app,
            "        bool isSurround = (m_screenManager->getCurrentScreenId() == ScreenId::SurroundTest);",
            "        bool isSurround = (m_screenManager->getCurrentScreenId() == ScreenId::SurroundTest);\n"
            "        bool playbackActive = m_playbackController && m_playbackController->isActive();\n"
            "        bool inPlaybackSession = playbackActive &&\n"
            "            m_screenManager->isPlaybackScreen(m_screenManager->getCurrentScreenId());")
    replace(app, "const int want = (isPlayer && setting_on && (trc == 16 || trc == 18)) ? 1 : 0;",
            "const int want = (inPlaybackSession && setting_on &&\n"
            "                (trc == 16 || trc == 18 || (trc < 0 && s_hdr_want))) ? 1 : 0;")
    replace(app, "if (!isPlayer) s_hdr_refused = false;",
            "if (!inPlaybackSession) s_hdr_refused = false;")
    replace(app, "         * Surround Studio. 25/50 fps judders at 120 Hz just as at 60, and",
            "         * Surround Studio. Playback menus keep the current mode. 25/50 fps\n"
            "         * judders at 120 Hz just as at 60, and")
    replace(app, "            } else if (!isPlayer) {\n                want = false;",
            "            } else if (!inPlaybackSession) {\n                want = false;")
    replace(app, "            } else if (m_playbackController && m_playbackController->isActive()) {",
            "            } else if (playbackActive) {")
    # VideoOut reports zero for an unsupported mode, positive for support.
    replace(player + "media/src/evo_agc_runtime.c",
            "g_agc_dev.supports_120hz = (sup120 >= 0) ? 1 : 0;",
            "g_agc_dev.supports_120hz = (sup120 > 0) ? 1 : 0;")
