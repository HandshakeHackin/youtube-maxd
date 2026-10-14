"""Enable EVO's playback-only 60/120 Hz cadence matching for ProsperoTube."""

# Console selector sweep R92 found only 59.94/119.88 Hz; 25/50 fps stay at 59.94.


def apply_auto_frame_rate_overlay(replace, player):
    replace(player + "core/include/evo/services/SettingsService.hpp",
            "m_refreshRateMode = RefreshRateMode::Off;",
            "m_refreshRateMode = RefreshRateMode::PlaybackOnly;")
    # VideoOut reports zero for an unsupported mode, positive for support.
    replace(player + "media/src/evo_agc_runtime.c",
            "g_agc_dev.supports_120hz = (sup120 >= 0) ? 1 : 0;",
            "g_agc_dev.supports_120hz = (sup120 > 0) ? 1 : 0;")
