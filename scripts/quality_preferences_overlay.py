"""Open resolution and frame-rate selectors as settings submenus."""


def apply_quality_preferences_overlay(replace, player):
    # Generated provider ABI is rebuilt together; pinned upstream remains intact.
    replace(player + 'addons/include/evo_provider.h', '    int64_t bitrate_bps;        /* 0 if unknown */',
            '    double fps;                /* 0 if unknown; retained across YouTube recovery */\n    int64_t bitrate_bps;        /* 0 if unknown */')
    replace(player + 'ui_rml/src/evo_rmlui_provider.cpp', 'id != "settings:audio-languages"',
            'id != "settings:audio-languages" && id != "settings:quality" && id != "settings:frame-rate"')
