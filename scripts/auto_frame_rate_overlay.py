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
    replace(app, "            static bool s_hdr_refused = false;", """            static bool s_hdr_refused = false;
            static int s_hdr_target = -1;
            static unsigned s_hdr_attempts = 0;
            static long long s_hdr_retry_ms = 0;
            s_hdr_want = evo_agc_runtime_hdr_output_active();""")
    replace(app, "            if (want != s_hdr_want && !(want && s_hdr_refused)) {", """            if (want == s_hdr_want || s_hdr_target != want) {
                s_hdr_target = want;
                s_hdr_attempts = 0;
                s_hdr_retry_ms = 0;
            }
            if (want != s_hdr_want && s_hdr_attempts < 3 && now_ms() >= s_hdr_retry_ms) {""")
    replace(app, """                if (evo_agc_runtime_set_hdr_output(want) == 0) {
                    s_hdr_want = want;
                } else if (want) {
                    s_hdr_refused = true;
                    toast("HDR", "The display did not accept HDR10 - playing tone-mapped");
                }""", """                ++s_hdr_attempts;
                if (evo_agc_runtime_set_hdr_output(want) == 0) {
                    s_hdr_want = evo_agc_runtime_hdr_output_active();
                    s_hdr_attempts = 0;
                    s_hdr_retry_ms = 0;
                } else {
                    s_hdr_retry_ms = now_ms() + 1000LL * s_hdr_attempts;
                    if (want && !s_hdr_refused) {
                        s_hdr_refused = true;
                        toast("HDR", "The display did not accept HDR10 - playing tone-mapped");
                    }
                }""")
    replace(app, """         * the last frame presented. A refused switch is not retried until the
         * next playback session.""", """         * the last frame presented. Retry failures at most three times per
         * requested mode, with a bounded delay between attempts.""")
    replace(app, "         * Surround Studio. 25/50 fps judders at 120 Hz just as at 60, and",
            "         * Surround Studio. Playback menus keep the current mode. 25/50 fps\n"
            "         * judders at 120 Hz just as at 60, and")
    replace(app, "            } else if (!isPlayer) {\n                want = false;",
            "            } else if (!inPlaybackSession) {\n                want = false;")
    replace(app, "            } else if (m_playbackController && m_playbackController->isActive()) {",
            "            } else if (playbackActive) {")
    # Settings can switch modes outside this policy. Read applied state each
    # tick, and never cache a refused request as a successful switch.
    replace(app, "            static int  s_wait_frames = 0;", """            static int  s_wait_frames = 0;
            static int s_refresh_target = -1;
            static unsigned s_refresh_attempts = 0;
            static long long s_refresh_retry_ms = 0;
            s_want_120 = (evo_agc_runtime_is_120hz() != 0);""")
    replace(app, "            const bool hold = evo_agc_runtime_mode_switch_pending() && s_wait_frames < 30;", """            if (want == s_want_120 || s_refresh_target != static_cast<int>(want)) {
                s_refresh_target = static_cast<int>(want);
                s_refresh_attempts = 0;
                s_refresh_retry_ms = 0;
                s_wait_frames = 0;
            }
            const bool hold = evo_agc_runtime_mode_switch_pending() && s_wait_frames < 30;""")
    replace(app, "            } else if (want != s_want_120) {", """            } else if (want != s_want_120 && s_refresh_attempts < 3 &&
                       now_ms() >= s_refresh_retry_ms) {""")
    replace(app, """                evo_agc_runtime_set_120hz(want ? 1 : 0);
                s_want_120 = want;
                s_wait_frames = 0;""", """                ++s_refresh_attempts;
                if (evo_agc_runtime_set_120hz(want ? 1 : 0) == 0) {
                    s_want_120 = (evo_agc_runtime_is_120hz() != 0);
                    s_refresh_attempts = 0;
                    s_refresh_retry_ms = 0;
                } else {
                    s_refresh_retry_ms = now_ms() + 1000LL * s_refresh_attempts;
                }
                s_wait_frames = 0;""")
    # VideoOut reports zero for an unsupported mode, positive for support.
    replace(player + "media/src/evo_agc_runtime.c",
            "g_agc_dev.supports_120hz = (sup120 >= 0) ? 1 : 0;",
            "g_agc_dev.supports_120hz = (sup120 > 0) ? 1 : 0;")
