"""Seed the live refresh state from VideoOut output status when available."""


def apply_refresh_readback_overlay(replace, player):
    path = player + "media/src/evo_agc_runtime.c"
    replace(path, '''        if (vorc == 0) {
            g_agc_dev.display_dynamic_range = (int)vout.dynamic_range;''',
            '''        if (vorc == 0) {
            /* ResolutionStatus can retain the panel's 120-Hz capability while
             * OutputStatus reports the mode actually configured for this app. */
            g_agc_dev.current_refresh_rate = (int)vout.refresh_rate;
            g_agc_dev.is_120hz = (vout.refresh_rate == 13);
            if (g_agc_dev.is_120hz)
                g_agc_dev.supports_120hz = 1;
            g_agc_dev.display_dynamic_range = (int)vout.dynamic_range;''')
    replace(path, 'g_agc_dev.current_refresh_rate = (int)(vres.refresh_rate != 0 ? vres.refresh_rate : vout.refresh_rate);',
            'g_agc_dev.current_refresh_rate = (int)(vorc == 0 && vout.refresh_rate != 0 ? vout.refresh_rate : vres.refresh_rate);')
    replace(player + "core/src/Application.cpp",
            "static bool s_want_120 = false;",
            "static bool s_want_120 = (evo_agc_runtime_is_120hz() != 0);")
